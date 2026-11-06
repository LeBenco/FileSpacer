// Exercise small changes against the same real Shell host and production helpers.
#define wmain placementReferenceEntry
#include "PlacementTests.cpp"
#undef wmain
#include <algorithm>
#include "MixedFolder.h"
#include <set>

#define restoreOptions referenceRestoreOptions
#define captureOptions referenceCaptureOptions
#define sameView referenceSameView
#define viewSettingsKey referenceViewSettingsKey
#include "SettingsReference.inc"
#undef viewSettingsKey
#undef sameView
#undef captureOptions
#undef restoreOptions

namespace {
struct LayoutFixture {
    PlacementHost owner;
    CComPtr<IFolderView2> view;
    std::vector<std::unique_ptr<ITEMID_CHILD, decltype(&CoTaskMemFree)>> owned;
    std::vector<PCUITEMID_CHILD> ids;
    std::vector<POINT> base, target;
    CComHeapPtr<ITEMID_CHILD> focused;
    HWND keyboard = nullptr;
    unsigned generation = 0;

    explicit LayoutFixture(const wchar_t *path) {
        owner.create(path);
        view = owner.view();
        CComPtr<IEnumIDList> items;
        need(SUCCEEDED(view->Items(SVGIO_ALLVIEW, IID_PPV_ARGS(&items))), "No fixture enumeration");
        for (int i = 0; i < 5000; ++i) {
            CComHeapPtr<ITEMID_CHILD> id;
            need(items->Next(1, &id, nullptr) == S_OK && id, "Short fixture enumeration");
            ids.push_back(id);
            owned.emplace_back(id.Detach(), &CoTaskMemFree);
            POINT point = {30 + (i % 50) * 120, 30 + (i / 50) * 100};
            base.push_back(filespacer::scaleDPI(point));
            point.x += 17; point.y += 19;
            target.push_back(filespacer::scaleDPI(point));
        }
        need(SUCCEEDED(view->SelectAndPositionItems(2, ids.data(), nullptr,
            SVSI_SELECT | SVSI_NOTAKEFOCUS)), "Cannot seed selection");
        need(SUCCEEDED(owner.events->shell->SelectItem(ids[1],
            SVSI_SELECT | SVSI_FOCUSED | SVSI_NOTAKEFOCUS)), "Cannot seed focus");
        int index = -1;
        need(SUCCEEDED(view->GetFocusedItem(&index)) && index >= 0
            && SUCCEEDED(view->Item(index, &focused)) && focused, "Cannot capture focus");
        keyboard = GetFocus();
        generation = owner.events->generation;
    }
    bool current() const { return owner.events->generation == generation && IsWindow(owner.frame); }
    void reset() {
        const auto bytes = positionBlob(ids, base);
        need(SUCCEEDED(filespacer::restoreIconPositions(view, bytes, [&]() { return current(); })), "Reset failed");
        pump(0);
        verify(base);
    }
    void verify(const std::vector<POINT> &expected) {
        need(current(), "Fixture generation changed");
        for (size_t i = 0; i < ids.size(); ++i) {
            POINT actual = {};
            DWORD state = 0;
            need(SUCCEEDED(view->GetItemPosition(ids[i], &actual))
                && actual.x == expected[i].x && actual.y == expected[i].y, "Coordinate changed");
            need(SUCCEEDED(view->GetSelectionState(ids[i], &state))
                && ((state & SVSI_SELECT) != 0) == (i < 2), "Selected identity changed");
        }
        int index = -1;
        CComHeapPtr<ITEMID_CHILD> actualFocus;
        need(SUCCEEDED(view->GetFocusedItem(&index)) && index >= 0
            && SUCCEEDED(view->Item(index, &actualFocus)) && actualFocus
            && ILIsEqual(actualFocus, focused) && GetFocus() == keyboard, "Focus changed");
    }
};

void chunkPlacement(const wchar_t *path) {
    LayoutFixture fixture(path);
    const size_t strategies[] = {32, 128, 512, 5000};
    for (int repetition = 0; repetition < 3; ++repetition) {
        for (size_t arm = 0; arm < _countof(strategies); ++arm) {
            const size_t choice = repetition == 1 ? 3 - arm : (arm + static_cast<size_t>(repetition)) % 4;
            const size_t size = strategies[choice];
            std::vector<std::vector<uint8_t>> chunks;
            for (size_t first = 0; first < fixture.ids.size(); first += size) {
                const size_t last = (std::min)(first + size, fixture.ids.size());
                chunks.push_back(positionBlob(
                    std::vector<PCUITEMID_CHILD>(fixture.ids.begin() + first, fixture.ids.begin() + last),
                    std::vector<POINT>(fixture.target.begin() + first, fixture.target.begin() + last)));
            }
            fixture.reset();
            const double began = clockMs();
            double longest = 0;
            for (const auto &chunk : chunks) {
                const double step = clockMs();
                need(SUCCEEDED(filespacer::restoreIconPositions(fixture.view, chunk,
                    [&]() { return fixture.current(); })), "Chunk placement failed");
                pump(0);
                longest = (std::max)(longest, clockMs() - step);
            }
            const double elapsed = clockMs() - began;
            fixture.verify(fixture.target);
            std::printf("CHUNKS count=5000 size=%zu repetition=%d total_ms=%.3f longest_ms=%.3f coordinates=PASS selection=PASS focus=PASS\n",
                size, repetition, elapsed, longest);
            std::fflush(stdout);
        }
    }
}

void closingPlacement(const wchar_t *path) {
    LayoutFixture fixture(path);
    std::vector<std::vector<uint8_t>> chunks;
    for (size_t first = 0; first < fixture.ids.size(); first += 128) {
        const size_t last = (std::min)(first + 128, fixture.ids.size());
        chunks.push_back(positionBlob(
            std::vector<PCUITEMID_CHILD>(fixture.ids.begin() + first, fixture.ids.begin() + last),
            std::vector<POINT>(fixture.target.begin() + first, fixture.target.begin() + last)));
    }
    for (int repetition = 0; repetition < 3; ++repetition) {
        for (int arm = 0; arm < 2; ++arm) {
            const bool merged = ((repetition + arm) % 2) != 0;
            fixture.reset();
            auto valid = [&]() { return fixture.current(); };
            need(SUCCEEDED(filespacer::restoreIconPositions(fixture.view, chunks[0], valid)), "First chunk failed");
            pump(0);
            const double began = clockMs();
            if (merged) {
                std::vector<uint8_t> remaining;
                for (size_t i = 1; i < chunks.size(); ++i)
                    remaining.insert(remaining.end(), chunks[i].begin(), chunks[i].end());
                need(SUCCEEDED(filespacer::restoreIconPositions(fixture.view, remaining, valid)), "Merged suffix failed");
            } else {
                for (size_t i = 1; i < chunks.size(); ++i)
                    need(SUCCEEDED(filespacer::restoreIconPositions(fixture.view, chunks[i], valid)), "Suffix chunk failed");
            }
            pump(0);
            const double elapsed = clockMs() - began;
            fixture.verify(fixture.target);
            std::printf("CLOSING strategy=%s repetition=%d pending=4872 total_ms=%.3f coordinates=PASS selection=PASS focus=PASS\n",
                merged ? "merged" : "chunks128", repetition, elapsed);
            std::fflush(stdout);
        }
    }
}

HRESULT comparedOptions(IFolderView2 *view, const filespacer::FolderViewSettings &state) {
    return filespacer::restoreOptions(view, state);
}

void unchangedSettings(const wchar_t *path) {
    LayoutFixture fixture(path);
    const auto wanted = filespacer::captureOptions(fixture.view, {});
    need((wanted.present & (filespacer::FolderViewSettings::Mode | filespacer::FolderViewSettings::Sort))
        == (filespacer::FolderViewSettings::Mode | filespacer::FolderViewSettings::Sort), "Missing settings");
    for (int repetition = 0; repetition < 5; ++repetition) {
        for (int arm = 0; arm < 2; ++arm) {
            const bool compared = ((repetition + arm) % 2) != 0;
            fixture.reset();
            const double began = clockMs();
            const HRESULT applied = compared ? comparedOptions(fixture.view, wanted)
                : filespacer::referenceRestoreOptions(fixture.view, wanted);
            pump(0);
            const double elapsed = clockMs() - began;
            need(SUCCEEDED(applied), "Unchanged settings application failed");
            need(filespacer::sameView(filespacer::captureOptions(fixture.view, {}), wanted), "Settings differ");
            if (compared) fixture.verify(fixture.base);
            std::printf("SETTINGS strategy=%s repetition=%d total_ms=%.3f settings=PASS\n",
                compared ? "compare" : "reference", repetition, elapsed);
            std::fflush(stdout);
        }
    }
    auto changed = wanted;
    need(!changed.sort.empty(), "No reference sort");
    changed.sort[0].direction = changed.sort[0].direction == SORT_ASCENDING ? SORT_DESCENDING : SORT_ASCENDING;
    need(SUCCEEDED(comparedOptions(fixture.view, changed)), "Changed settings application failed");
    pump(0);
    need(filespacer::sameView(filespacer::captureOptions(fixture.view, {}), changed), "Changed sort was skipped");
    need(SUCCEEDED(comparedOptions(fixture.view, wanted)), "Settings roundtrip failed");
    pump(0);
    need(filespacer::sameView(filespacer::captureOptions(fixture.view, {}), wanted), "Settings roundtrip differs");
    fixture.reset();
    auto partial = wanted;
    partial.present = filespacer::FolderViewSettings::Mode;
    need(SUCCEEDED(comparedOptions(fixture.view, partial)), "Partial settings failed");
    pump(0);
    need(filespacer::sameView(filespacer::captureOptions(fixture.view, {}), wanted), "Partial settings changed other fields");
    std::puts("SETTINGS changed_sort=PASS roundtrip=PASS partial_snapshot=PASS");
}

void statusSizes(const wchar_t *path) {
    LayoutFixture fixture(path);
    need(SUCCEEDED(fixture.owner.events->shell->SelectItem(nullptr, SVSI_DESELECTOTHERS)), "Cannot clear fixture");
    need(SUCCEEDED(fixture.view->SelectAndPositionItems(static_cast<UINT>(fixture.ids.size()),
        fixture.ids.data(), nullptr, SVSI_SELECT | SVSI_NOTAKEFOCUS)), "Cannot select fixture");
    ULONGLONG expected = 0;
    for (int i = 0; i < 5000; ++i) expected += 1 + i % 127;
    for (int repetition = 0; repetition < 3; ++repetition) {
        for (int arm = 0; arm < 2; ++arm) {
            const bool chunked = ((repetition + arm) % 2) != 0;
            const double began = clockMs();
            CComPtr<IShellItemArray> selected;
            need(filespacer::nativeViewSelection(fixture.view, FALSE, &selected) == S_OK && selected,
                "Cannot obtain real selection");
            DWORD count = 0;
            need(SUCCEEDED(selected->GetCount(&count)) && count == 5000, "Wrong selection array size");
            const double arrayMs = clockMs() - began;
            double step = began, longest = 0;
            ULONGLONG sum = 0;
            for (DWORD i = 0; i < count; ++i) {
                CComPtr<IShellItem> item;
                need(SUCCEEDED(selected->GetItemAt(i, &item)), "Cannot obtain selected item");
                CComQIPtr<IShellItem2> properties(item);
                ULONGLONG size = 0;
                need(properties && SUCCEEDED(properties->GetUInt64(PKEY_Size, &size)), "Missing fixture size");
                sum += size;
                if (chunked && ((i + 1) % 64 == 0 || i + 1 == count)) {
                    longest = (std::max)(longest, clockMs() - step);
                    pump(0);
                    step = clockMs();
                }
            }
            if (!chunked) longest = (std::max)(longest, clockMs() - step);
            const double elapsed = clockMs() - began;
            need(sum == expected, "Wrong aggregate size");
            std::set<std::wstring> names;
            for (DWORD i = 0; i < count; ++i) {
                CComPtr<IShellItem> item;
                CComHeapPtr<wchar_t> name;
                need(SUCCEEDED(selected->GetItemAt(i, &item))
                    && SUCCEEDED(item->GetDisplayName(SIGDN_PARENTRELATIVEPARSING, &name)) && name
                    && names.emplace(name).second, "Duplicate selected identity");
            }
            for (int i = 0; i < 5000; ++i)
                need(names.count(L"item-" + std::to_wstring(100000 + i) + L".txt") == 1, "Wrong selected identity");
            std::printf("STATUS strategy=%s repetition=%d array_ms=%.3f total_ms=%.3f longest_work_ms=%.3f bytes=%llu identities=PASS\n",
                chunked ? "chunks64" : "synchronous", repetition, arrayMs, elapsed, longest, sum);
            std::fflush(stdout);
        }
    }
}

void notificationOverhead(const wchar_t *path) {
    LayoutFixture fixture(path);
    fixture.reset();
    constexpr UINT selectionTimer = 701, saveTimer = 702;
    for (int repetition = 0; repetition < 5; ++repetition) {
        for (int arm = 0; arm < 2; ++arm) {
            const bool coalesced = ((repetition + arm) % 2) != 0;
            unsigned scheduled = 0;
            const double began = clockMs();
            for (int event = 0; event < 5000; ++event) {
                CComPtr<IUnknown> source, current;
                need(SUCCEEDED(fixture.owner.events->shell->QueryInterface(IID_PPV_ARGS(&source)))
                    && SUCCEEDED(fixture.owner.events->shell->QueryInterface(IID_PPV_ARGS(&current)))
                    && source == current && fixture.current(), "Stale callback identity");
                if (coalesced && scheduled) continue;
                if (GetActiveWindow() == fixture.owner.frame)
                    need(SetTimer(fixture.owner.frame, selectionTimer, 100, nullptr) != 0, "Cannot schedule selection");
                auto view = fixture.owner.view();
                UINT mode = 0;
                need(view->GetAutoArrange() == S_FALSE && SUCCEEDED(view->GetCurrentViewMode(&mode))
                    && mode == FVM_ICON, "Notification changed profile");
                need(SetTimer(fixture.owner.frame, saveTimer, 250, nullptr) != 0, "Cannot schedule save");
                ++scheduled;
            }
            const double elapsed = clockMs() - began;
            KillTimer(fixture.owner.frame, selectionTimer);
            KillTimer(fixture.owner.frame, saveTimer);
            fixture.verify(fixture.base);
            std::printf("NOTIFICATIONS strategy=%s repetition=%d count=5000 scheduling_passes=%u total_ms=%.3f scope=callback_model_not_product\n",
                coalesced ? "once" : "each", repetition, scheduled, elapsed);
            std::fflush(stdout);
        }
    }
}

void combinedWorkflow(const wchar_t *path) {
    LayoutFixture fixture(path);
    const auto wanted = filespacer::captureOptions(fixture.view, {});
    constexpr size_t prefix = 256; // Identical completed work before closing, for both chunk sizes.
    for (int repetition = 0; repetition < 3; ++repetition) {
        for (unsigned arm = 0; arm < 8; ++arm) {
            const unsigned choice = repetition == 1 ? 7 - arm : (arm + static_cast<unsigned>(repetition)) % 8;
            const size_t size = (choice & 1u) ? 128 : 32;
            const bool merged = (choice & 2u) != 0, compared = (choice & 4u) != 0;
            std::vector<std::vector<uint8_t>> chunks;
            for (size_t first = 0; first < fixture.ids.size(); first += size) {
                const size_t last = (std::min)(first + size, fixture.ids.size());
                chunks.push_back(positionBlob(
                    std::vector<PCUITEMID_CHILD>(fixture.ids.begin() + first, fixture.ids.begin() + last),
                    std::vector<POINT>(fixture.target.begin() + first, fixture.target.begin() + last)));
            }
            fixture.reset();
            auto valid = [&]() { return fixture.current(); };
            const double began = clockMs();
            need(SUCCEEDED(compared ? comparedOptions(fixture.view, wanted)
                : filespacer::referenceRestoreOptions(fixture.view, wanted)), "Combined settings failed");
            pump(0);
            const double settingsEnd = clockMs();
            const size_t completed = prefix / size;
            for (size_t i = 0; i < completed; ++i) {
                need(SUCCEEDED(filespacer::restoreIconPositions(fixture.view, chunks[i], valid)), "Combined prefix failed");
                pump(0);
            }
            const double prefixEnd = clockMs();
            if (merged) {
                std::vector<uint8_t> remaining;
                for (size_t i = completed; i < chunks.size(); ++i)
                    remaining.insert(remaining.end(), chunks[i].begin(), chunks[i].end());
                need(SUCCEEDED(filespacer::restoreIconPositions(fixture.view, remaining, valid)), "Combined suffix failed");
            } else {
                for (size_t i = completed; i < chunks.size(); ++i)
                    need(SUCCEEDED(filespacer::restoreIconPositions(fixture.view, chunks[i], valid)), "Combined chunks failed");
            }
            pump(0);
            const double end = clockMs();
            fixture.verify(fixture.target);
            need(filespacer::sameView(filespacer::captureOptions(fixture.view, {}), wanted), "Combined settings differ");
            std::printf("COMBINED size=%zu merged=%d compared=%d repetition=%d settings_ms=%.3f prefix_ms=%.3f close_ms=%.3f total_ms=%.3f coordinates=PASS selection=PASS focus=PASS settings=PASS\n",
                size, merged ? 1 : 0, compared ? 1 : 0, repetition, settingsEnd - began,
                prefixEnd - settingsEnd, end - prefixEnd, end - began);
            std::fflush(stdout);
        }
    }
}
}

int wmain(int argc, wchar_t **argv) {
    if (argc != 3 || FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 2;
    QueryPerformanceFrequency(&frequency);
    filespacer::initDPI();
    WNDCLASSW type = {};
    type.hInstance = GetModuleHandleW(nullptr);
    type.lpfnWndProc = DefWindowProcW;
    type.lpszClassName = L"FileSpacer Placement Prototype";
    if (!RegisterClassW(&type)) { CoUninitialize(); return 2; }
    int result = 0;
    try {
        if (std::wcscmp(argv[2], L"placement") == 0) chunkPlacement(argv[1]);
        else if (std::wcscmp(argv[2], L"placement-close") == 0) closingPlacement(argv[1]);
        else if (std::wcscmp(argv[2], L"settings") == 0) unchangedSettings(argv[1]);
        else if (std::wcscmp(argv[2], L"status") == 0) statusSizes(argv[1]);
        else if (std::wcscmp(argv[2], L"notifications") == 0) notificationOverhead(argv[1]);
        else if (std::wcscmp(argv[2], L"combined") == 0) combinedWorkflow(argv[1]);
        else throw std::runtime_error("Unknown lightweight scenario");
    } catch (const std::exception &error) {
        std::fprintf(stderr, "FAIL lightweight Shell: %s\n", error.what());
        result = 1;
    }
    UnregisterClassW(type.lpszClassName, type.hInstance);
    CoUninitialize();
    return result;
}
