#include <common.h>
#include "UpdateManifest.h"
#include <cassert>
#include <iostream>
#include <random>

using namespace filespacer;

int main() {
    // Deterministic malformed inputs; sanitizer builds also check bounds and overflow.
    std::mt19937 rng(42);
    for (int i = 0; i < 50000; ++i) {
        std::string input(rng() % 1100, '\0');
        for (char &c : input) c = static_cast<char>(rng() % 256);
        ReleaseManifest value;
        const bool accepted = parseUpdateManifest(input, &value);
        assert(!accepted);
    }
    std::cout << "Update manifest stress passed (50,000 malformed inputs).\n";
}
