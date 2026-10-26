#include <common.h>
#define FILESPACER_SIMULATION_LIBRARY
#include "PlacementSimulation.cpp"

namespace filespacer {
namespace simulation {
struct ShapeStats { size_t x, y, cells; };
ShapeStats shapeStats(const Scene &scene) {
    std::vector<LONG> xs = {scene.work.left, scene.work.right};
    std::vector<LONG> ys = {scene.work.top, scene.work.bottom};
    for (const auto &r : scene.windows) {
        xs.push_back(r.left); xs.push_back(r.right); ys.push_back(r.top); ys.push_back(r.bottom);
    }
    unique(xs); unique(ys);
    return {candidates(scene, true).size(), candidates(scene, false).size(), xs.size() * ys.size()};
}
void addBars(Scene &scene, bool horizontal) {
    const LONG end = horizontal ? scene.work.bottom : scene.work.right;
    const LONG extent = horizontal ? scene.size.cy : scene.size.cx;
    const LONG thickness = horizontal ? 160 : 320;
    for (LONG start = 0; start < end; start += extent + thickness - 1) {
        const LONG last = (std::min)(end, start + thickness);
        scene.windows.push_back(horizontal ? RECT{0, start, scene.work.right, last}
            : RECT{start, 0, last, scene.work.bottom});
    }
}
RECT randomInterior(const Scene &scene, std::mt19937 &random) {
    const auto axis = [&](LONG end, LONG target, LONG minimum, LONG maximum) {
        LONG low = 0, high = end;
        // Where feasible, every edge and its target-size translation is a valid
        // origin. This increases the candidate set instead of relying on density.
        if (end - 2 * target >= minimum) { low = target; high = end - target; }
        const LONG size = randomBetween(random, minimum, (std::min)(maximum, high - low));
        const LONG start = randomBetween(random, low, high - size);
        return std::make_pair(start, start + size);
    };
    const auto x = axis(scene.work.right, scene.size.cx, 320, 920);
    const auto y = axis(scene.work.bottom, scene.size.cy, 240, 760);
    return {x.first, y.first, x.second, y.second};
}
Scene worstScene(SIZE screen, SIZE target, const std::string &pattern, unsigned count, unsigned seed) {
    std::mt19937 random(seed);
    Scene scene = {{0, 0, screen.cx, screen.cy}, target,
        {(screen.cx - target.cx) * 37 / 101, (screen.cy - target.cy) * 43 / 103}, {}};
    if (pattern == "covered") {
        const LONG split = screen.cx / 2 + randomBetween(random, -99, 99);
        scene.windows = {{0, 0, split, screen.cy}, {split, 0, screen.cx, screen.cy}};
    } else if (pattern == "last-fit") {
        const LONG x = screen.cx - target.cx, y = screen.cy - target.cy;
        scene.windows = {{0, 0, x, screen.cy}, {x, 0, screen.cx, y}};
    } else {
        addBars(scene, false);
        if (pattern == "mesh") addBars(scene, true);
    }
    require(scene.windows.size() <= count, "too few windows for adversarial scaffold");
    while (scene.windows.size() < count) {
        RECT best = {};
        size_t bestScore = 0;
        // Keep the most structurally expensive of 40 random choices: unique
        // critical origins first, compressed edge counts second. No timing here.
        for (unsigned attempt = 0; attempt < 40; ++attempt) {
            RECT next = randomInterior(scene, random);
            if (pattern == "last-fit") {
                next.left = (std::min)(next.left, screen.cx - target.cx - 1);
                next.right = (std::min)(next.right, screen.cx - target.cx);
            }
            scene.windows.push_back(next);
            const auto stats = shapeStats(scene);
            const size_t score = stats.x * stats.y * 1000000 + stats.cells;
            scene.windows.pop_back();
            if (score > bestScore) { best = next; bestScore = score; }
        }
        scene.windows.push_back(best);
    }
    return scene;
}
struct WorstTiming { double total, sweep; Choice choice; const char *path; };
WorstTiming timeHybrid(const Scene &scene) {
    const auto start = Clock::now();
    WorstTiming timing = {};
    {
        Scene state = scene;
        const bool free = findFree(state, timing.choice, timing.path);
        timing.sweep = elapsed(start);
        if (!free) { ExactSearch search(state); timing.choice = search.find(); }
        checksum ^= timing.choice.area + static_cast<Area>(timing.choice.point.x);
    }
    // Includes copy, allocations, the failed sweep, fallback and destruction.
    timing.total = elapsed(start);
    return timing;
}
WorstTiming timeExact(const Scene &scene) {
    const auto start = Clock::now();
    WorstTiming timing = {};
    timing.path = "exact";
    {
        Scene state = scene;
        ExactSearch search(state);
        timing.choice = search.find();
        checksum ^= timing.choice.area + static_cast<Area>(timing.choice.point.x);
    }
    timing.total = elapsed(start);
    return timing;
}
void worstBenchmark(const std::string &output, unsigned seeds, unsigned repetitions) {
    std::ofstream results(output + "/worst-measurements.csv"), geometry(output + "/worst-windows.csv");
    require(results.good() && geometry.good(), "cannot create worst-case CSV files");
    results << "scene,width,height,target_width,target_height,pattern,n,seed,candidates_x,candidates_y,compressed_cells,repeat,hybrid_us,sweep_us,exact_us,path,area,distance\n";
    results << std::fixed << std::setprecision(4);
    geometry << "scene,index,left,top,right,bottom\n";
    unsigned sceneId = 0;
    const SIZE screens[] = {{1920, 1080}, {3840, 2160}};
    const SIZE targets[] = {{320, 240}, {580, 435}, {960, 720}};
    for (const auto &screen : screens) for (unsigned count : {20U, 40U, 60U}) {
        for (const auto &target : targets) for (const std::string pattern : {"covered", "slits", "mesh", "last-fit"}) {
            for (unsigned sample = 0; sample < seeds; ++sample) {
                const unsigned seed = 19217 + sample * 104729 + count * 97;
                const Scene scene = worstScene(screen, target, pattern, count, seed);
                const auto stats = shapeStats(scene);
                ++sceneId;
                for (size_t i = 0; i < scene.windows.size(); ++i) {
                    const auto &r = scene.windows[i];
                    geometry << sceneId << ',' << i << ',' << r.left << ',' << r.top << ',' << r.right << ',' << r.bottom << '\n';
                }
                // Check the intended difficult path and warm code/data pages,
                // while retaining a fresh algorithm cache for every timed call.
                const auto oracle = timeExact(scene).choice;
                require(unionArea(scene, oracle.point) == oracle.area, "adversarial score failed union oracle");
                if (pattern == "last-fit") {
                    require(oracle.area == 0 && oracle.point.x == screen.cx - target.cx
                        && oracle.point.y == screen.cy - target.cy, "unique last-fit origin was lost");
                } else require(oracle.area > 0, "adversarial scene accidentally has a free placement");
                for (unsigned repeat = 0; repeat < repetitions; ++repeat) {
                    WorstTiming hybrid, exact;
                    if ((sceneId + repeat) % 2) { hybrid = timeHybrid(scene); exact = timeExact(scene); }
                    else { exact = timeExact(scene); hybrid = timeHybrid(scene); }
                    require(hybrid.choice.area == oracle.area && exact.choice.area == oracle.area
                        && hybrid.choice.distance == oracle.distance && exact.choice.distance == oracle.distance,
                        "adversarial methods disagree with exact reference");
                    const bool usedFallback = std::string(hybrid.path) == "positive-exact";
                    require(usedFallback == (pattern != "last-fit"), "wrong adversarial execution path");
                    results << sceneId << ',' << screen.cx << ',' << screen.cy << ',' << target.cx << ',' << target.cy
                        << ',' << pattern << ',' << count << ',' << seed << ',' << stats.x << ',' << stats.y
                        << ',' << stats.cells << ',' << repeat << ',' << hybrid.total << ',' << hybrid.sweep
                        << ',' << exact.total << ',' << hybrid.path << ',' << hybrid.choice.area << ',' << hybrid.choice.distance << '\n';
                }
            }
            results.flush();
            std::cout << screen.cx << 'x' << screen.cy << " n=" << count << " target=" << target.cx << 'x' << target.cy
                << ' ' << pattern << " complete\n" << std::flush;
        }
    }
    std::cout << "WORST PASS: " << sceneId << " scenes, " << repetitions << " repeats per method; checksum=" << checksum << '\n';
}
} // namespace simulation
} // namespace filespacer

int main(int argc, char **argv) {
    try {
        filespacer::simulation::verify();
        if (argc == 2 && std::string(argv[1]) == "--verify-only") return 0;
        if (argc != 4) throw std::runtime_error("Usage: WorstCases output-directory seeds repetitions");
        const unsigned seeds = static_cast<unsigned>(std::stoul(argv[2]));
        const unsigned repetitions = static_cast<unsigned>(std::stoul(argv[3]));
        if (!seeds || !repetitions || seeds > 100 || repetitions > 100) throw std::runtime_error("Seeds/repetitions must be in 1..100");
        filespacer::simulation::worstBenchmark(argv[1], seeds, repetitions);
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
