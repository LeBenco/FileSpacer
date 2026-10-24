// Real Shell views and the production SQLite store; no private controls or UI helper.
#define NOMINMAX
#include "ShellViewHost.h"
#include "ShellViewSettings.h"
#include "ShellViewSelection.h"
#include "ShellViewIcons.h"
#include "ShellViewFilter.h"
#include "DPI.h"
#include <shlobj.h>
#include <shlwapi.h>
#include <initguid.h>
#include <propkey.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <memory>
#include <string>
#include <utility>

using namespace filespacer;
namespace {
constexpr UINT READY = WM_APP + 1, DONE = WM_APP + 2, RUN = WM_APP + 3, DIAGNOSTIC = WM_APP + 4;
constexpr UINT WATCHDOG = 1;
FILE *journal = nullptr;
void log(const wchar_t *format, ...) {
    wchar_t line[2048];
    va_list args; va_start(args, format);
    _vsnwprintf_s(line, _countof(line), _TRUNCATE, format, args); va_end(args);
    wprintf(L"%s\n", line);
    if (journal) { fwprintf(journal, L"%s\n", line); fflush(journal); }
}
std::string utf8(const std::wstring &text) {
    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
        static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (!size && !text.empty()) return {};
    std::string result(static_cast<size_t>(size), '\0');
    if (size) WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
        static_cast<int>(text.size()), &result[0], size, nullptr, nullptr);
    return result;
}
FolderViewSettings::Key key(const PROPERTYKEY &property) {
    FolderViewSettings::Key result = {};
    std::memcpy(result.data(), &property.fmtid, 16);
    std::memcpy(result.data() + 16, &property.pid, 4);
    return result;
}
bool directory(const std::wstring &path) {
    return CreateDirectory(path.c_str(), nullptr) ||
        (GetLastError() == ERROR_ALREADY_EXISTS &&
            (GetFileAttributes(path.c_str()) & FILE_ATTRIBUTE_DIRECTORY) != 0);
}
class StateProbe : public IServiceProvider, public IExplorerBrowserEvents, public ICommDlgBrowser2 {
    LONG references = 1;
    ShellViewHost browser;
    CComPtr<IFolderFilter> nameFilter;
    CComPtr<IShellView> view;
    CComPtr<IUnknown> oldViewIdentity;
    std::wstring fixture, emptyFolder;
    std::string database;
    FolderViewSettings wanted, persisted;
    std::vector<uint8_t> selectionBlob, pendingSelection, emptySelectionBlob;
    std::vector<std::wstring> expectedSelection;
    std::wstring expectedFocus;
    FolderViewSettings iconSettings;
    std::vector<uint8_t> iconBlob, legacyIconBlob;
    struct ExpectedIconPosition {
        CComHeapPtr<ITEMID_CHILD> id;
        POINT point = {};
        ExpectedIconPosition() = default;
        ExpectedIconPosition(ExpectedIconPosition &&other) noexcept : id(other.id.Detach()), point(other.point) {}
    };
    std::vector<ExpectedIconPosition> expectedIcons;
    std::array<CComHeapPtr<ITEMID_CHILD>, 3> manualIconIds;
    const POINT manualPoints[3] = {{37, 53}, {219, 69}, {401, 211}};
    size_t firstIconPointOffset = 0;
    HWND focusTarget = nullptr;
    unsigned selectionCase = 0, selectionChanges = 0, selectionBaseline = 0;
    bool presentationPending = false;
    ULONG_PTR generation = 0, enumDone = 0, baseline = 0;
    bool navigation = false, ready = false, queued = false, running = false;
    bool requested = false, creating = false, closing = false, finished = false;
    const bool diagnostic;
    bool observingFailure = false;
    struct FilterCase { const wchar_t *query; int count; const wchar_t *single; };
    const FilterCase filterCases[5] = {
        {L"ÉLÉMENT-1.TXT", 1, L"élément-1.txt"},
        {L"élément-2.txt", 1, L"élément-2.txt"},
        {L"*.txt", 150, nullptr},
        {L"no-match-filespacer-unique", 0, nullptr},
        {L"", 150, nullptr}
    };
    size_t filterCase = 0;
    unsigned lifetimeCase = 0, staleNotifications = 0;
    ULONG_PTR retiredGeneration = 0;
    bool lifetimeTeardown = false;
    enum class Phase { Configure, VerifyConfigured, Recreate, Restore, VerifyRestored,
        VerifyRefresh, RestoreEmpty, VerifyEmpty, SelectionPrepare, SelectionRecreate,
        SelectionOptions, SelectionRestore, SelectionVerify, SelectionAfterUser,
        SelectionAfterRefresh, IconsConfigure, IconsSetup, IconsOptions,
        IconsRestore, IconsVerify, IconsRefresh, IconsEmptyOptions, IconsEmpty,
        FilterApply, FilterVerify, LifetimeStart, LifetimeVerify };
    Phase phase = Phase::Configure;
    int failures = 0;
    RECT bounds() const { RECT r = {}; GetClientRect(window, &r); return r; }
    bool expect(bool result, const wchar_t *description) {
        log(L"%s: %s", result ? L"PASS" : L"FAIL", description);
        if (!result) { ++failures; stop(false); }
        return result;
    }
    bool checked(HRESULT result, const wchar_t *description) {
        log(L"%s: HRESULT=0x%08lX", description, static_cast<unsigned long>(result));
        return expect(SUCCEEDED(result), description);
    }
    void queue() {
        if (closing || finished || !navigation || !ready || !enumDone || creating) return;
        if (running) { requested = true; return; }
        if (!queued) queued = PostMessage(window, RUN, generation, 0) != FALSE;
    }
    void stop(bool success) {
        if (finished) return;
        finished = true;
        exitCode = success && !failures ? 0 : 3;
        log(L"RESULT: %s persistence, selection, icons, filtering and lifetime; failures=%d", exitCode ? L"FAIL" : L"PASS", failures);
        if (diagnostic && !success && (phase == Phase::VerifyRestored || phase == Phase::VerifyEmpty)) {
            observingFailure = true;
            log(L"DIAG: failure retained, exit stays 3; observing subsequent public events without changing the view");
            PostMessage(window, DIAGNOSTIC, generation, 0);
            return;
        }
        PostMessage(window, WM_CLOSE, 0, 0);
    }
    void trace(const wchar_t *where) {
        if (!diagnostic) return;
        CComQIPtr<IFolderView2> current(view);
        int count = -1, iconSize = -1;
        FOLDERVIEWMODE mode = FVM_AUTO;
        HRESULT contents = E_NOINTERFACE, presentation = E_NOINTERFACE;
        if (current) {
            contents = current->ItemCount(SVGIO_ALLVIEW, &count);
            presentation = current->GetViewModeAndIconSize(&mode, &iconSize);
        }
        log(L"DIAG %s: generation=%llu, callbacks=%llu, handled=%llu, navigation=%d, ready=%d, creating=%d, items=%d (0x%08lX), mode=%d size=%d (0x%08lX)",
            where, static_cast<unsigned long long>(generation),
            static_cast<unsigned long long>(browser.enumerationSerial()),
            static_cast<unsigned long long>(enumDone), navigation, ready, creating,
            count, static_cast<unsigned long>(contents), static_cast<int>(mode), iconSize,
            static_cast<unsigned long>(presentation));
    }
    void observeFailedRestoration() {
        if (!observingFailure || closing) return;
        trace(L"after failed assertion / public event");
        CComQIPtr<IFolderView2> current(view);
        int count = -1;
        const HRESULT result = current ? current->ItemCount(SVGIO_ALLVIEW, &count) : E_NOINTERFACE;
        const int expected = phase == Phase::VerifyEmpty ? 0 : 150;
        if (SUCCEEDED(result) && count == expected) {
            const auto actual = captureOptions(current, {});
            log(L"DIAG: expected contents returned; exact stored settings=%s; original failure remains FAIL, exit 3",
                sameView(actual, persisted) ? L"yes" : L"no");
            PostMessage(window, WM_CLOSE, 0, 0);
        }
    }
    bool countIs(int expected, bool canWait = false) {
        CComQIPtr<IFolderView2> folderView(view);
        int actual = -1;
        const HRESULT hr = folderView ? folderView->ItemCount(SVGIO_ALLVIEW, &actual) : E_NOINTERFACE;
        log(L"Contents: actual=%d, expected=%d, HRESULT=0x%08lX, EnumDone=%llu, baseline=%llu",
            actual, expected, static_cast<unsigned long>(hr),
            static_cast<unsigned long long>(enumDone), static_cast<unsigned long long>(baseline));
        if (!checked(hr, L"read native item count")) return false;
        if (actual == expected) return true;
        if (canWait && enumDone <= baseline) {
            log(L"WAIT: real EnumDone after presentation mutation; no retry or timer continuation");
            return false;
        }
        return expect(false, L"native contents have the exact expected count after completion");
    }
    bool optionsAre(const FolderViewSettings &expected) {
        CComQIPtr<IFolderView2> folderView(view);
        if (!expect(folderView != nullptr, L"new native view exposes IFolderView2")) return false;
        const auto actual = captureOptions(folderView, {});
        log(L"Settings: parts=%u/%u, mode=%d/%d, size=%d/%d, flags=%u/%u, columns=%zu/%zu, sort=%zu/%zu",
            actual.present, expected.present, actual.mode, expected.mode,
            actual.iconSize, expected.iconSize, actual.flags, expected.flags,
            actual.columns.size(), expected.columns.size(), actual.sort.size(), expected.sort.size());
        return expect(sameView(actual, expected),
            L"native mode, icon size, flags, column keys/order/widths, sort and grouping match exactly");
    }
    bool afterPresentation(int expected) {
        baseline = browser.enumerationSerial();
        CComQIPtr<IFolderView2> folderView(view);
        int count = -1;
        if (!checked(folderView->ItemCount(SVGIO_ALLVIEW, &count), L"read contents immediately after presentation restoration")) return false;
        presentationPending = count != expected;
        log(L"Presentation completion: items=%d, expected=%d, baseline=%llu, needs later EnumDone=%d",
            count, expected, static_cast<unsigned long long>(baseline), presentationPending);
        return true;
    }
    bool presentationIsComplete() {
        if (presentationPending && enumDone <= baseline) {
            log(L"WAIT: restoration invalidated the initial completion; require a later EnumDone even if contents return first");
            return false;
        }
        presentationPending = false;
        return true;
    }
    bool itemName(IShellItem *item, std::wstring &name) {
        CComHeapPtr<wchar_t> text;
        if (!checked(item->GetDisplayName(SIGDN_PARENTRELATIVEPARSING, &text), L"read the real Shell item name")) return false;
        name = text;
        return true;
    }
    bool itemName(int index, std::wstring &name) {
        CComQIPtr<IFolderView2> folderView(view);
        CComPtr<IShellItem> item;
        return checked(folderView->GetItem(index, IID_PPV_ARGS(&item)), L"read the real Shell item by index")
            && expect(item != nullptr, L"native item lookup returned an object")
            && itemName(item, name);
    }
    bool selectionIs() {
        CComQIPtr<IFolderView2> folderView(view);
        int count = -1;
        if (!checked(folderView->ItemCount(SVGIO_SELECTION, &count), L"read restored selection count")) return false;
        if (!expect(count == static_cast<int>(expectedSelection.size()), L"selection contains the exact expected number of items")) return false;
        std::vector<std::wstring> names;
        if (count) {
            CComPtr<IShellItemArray> array;
            if (!checked(folderView->GetSelection(FALSE, &array), L"read the real selected Shell items")) return false;
            if (!expect(array != nullptr, L"nonempty native selection exposes a Shell item array")) return false;
            DWORD actualCount = 0;
            if (!checked(array->GetCount(&actualCount), L"read selected Shell array count")
                    || !expect(actualCount == static_cast<DWORD>(count), L"selection array agrees with the native count")) return false;
            for (DWORD index = 0; index < actualCount; ++index) {
                CComPtr<IShellItem> item;
                std::wstring name;
                if (!checked(array->GetItemAt(index, &item), L"read selected Shell array item")
                        || !expect(item != nullptr, L"selection array item is available") || !itemName(item, name)) return false;
                names.push_back(std::move(name));
            }
        }
        std::sort(names.begin(), names.end());
        if (!expect(names == expectedSelection, L"selection restores the exact saved identities, independently of view order")) return false;
        int focused = -1;
        std::wstring focusedName;
        if (!checked(folderView->GetFocusedItem(&focused), L"read restored focused item")
                || !expect(focused >= 0, L"native view exposes the restored focus")
                || !itemName(focused, focusedName)) return false;
        log(L"Selection case %u: selected=%d, focus=%s, expected focus=%s",
            selectionCase, count, focusedName.c_str(), expectedFocus.c_str());
        return expect(focusedName == expectedFocus, L"focused identity is exact without changing its selection state")
            && expect(GetFocus() == focusTarget, L"restoration does not take keyboard focus from the host edit control");
    }
    bool prepareSelection() {
        CComQIPtr<IFolderView2> folderView(view);
        SetFocus(focusTarget);
        if (!expect(GetFocus() == focusTarget, L"host edit control has keyboard focus before capture")) return false;
        auto current = [&, observedGeneration = generation, observedView = view]() {
            return !closing && view == observedView && browser.isViewNotification(observedGeneration);
        };
        if (!checked(folderView->SelectItem(0, SVSI_SELECT | SVSI_DESELECTOTHERS | SVSI_NOTAKEFOCUS),
                L"seed a selection before restoring the empty-folder snapshot")) return false;
        int selected = -1;
        if (!checked(folderView->ItemCount(SVGIO_SELECTION, &selected), L"read seeded selection count")
                || !expect(selected == 1, L"empty snapshot regression starts with one selected item")) return false;
        auto emptySnapshot = emptySelectionBlob;
        if (!checked(restoreReopenSelection(view, emptySnapshot, current), L"restore the empty-folder snapshot over an existing selection")
                || !expect(emptySnapshot.empty(), L"empty snapshot over a populated view is consumed once")
                || !checked(folderView->ItemCount(SVGIO_SELECTION, &selected), L"read selection after empty snapshot restoration")
                || !expect(selected == 0, L"empty snapshot clears an existing selection")
                || !expect(GetFocus() == focusTarget, L"deselect-all preserves keyboard focus on the host edit control")) return false;
        const std::vector<int> indices = selectionCase == 0 ? std::vector<int>{2, 17, 39}
            : selectionCase == 1 ? std::vector<int>{4, 28} : std::vector<int>{};
        const int focusIndex = selectionCase == 0 ? 17 : selectionCase == 1 ? 55 : 7;
        expectedSelection.clear();
        for (const int index : indices) {
            std::wstring name;
            if (!itemName(index, name)
                    || !checked(folderView->SelectItem(index, SVSI_SELECT | SVSI_NOTAKEFOCUS), L"select snapshot fixture item")) return false;
            expectedSelection.push_back(std::move(name));
        }
        std::sort(expectedSelection.begin(), expectedSelection.end());
        if (!itemName(focusIndex, expectedFocus)) return false;
        const DWORD focusFlags = SVSI_FOCUSED | SVSI_NOTAKEFOCUS | (selectionCase == 0 ? SVSI_SELECT : SVSI_DESELECT);
        if (!checked(folderView->SelectItem(focusIndex, focusFlags), L"set selected or unselected snapshot focus") || !selectionIs()) return false;
        if (!checked(captureReopenSelection(view, current, selectionBlob), L"capture through production reopen-selection code")) return false;
        if (!expect(selectionBlob.size() >= 20, L"capture retains the existing version-1 header layout")) return false;
        uint32_t header[3] = {};
        std::memcpy(header, selectionBlob.data(), sizeof(header));
        if (!expect(header[0] == 1 && header[1] == indices.size() && header[2] == 4,
                L"existing version, selection count and focus flag are preserved without a format migration")) return false;
        {
            FolderStateStore store(database.c_str());
            ClosedWindow snapshot;
            snapshot.key = "populated"; snapshot.parsingName = utf8(fixture);
            snapshot.displayName = "Selection regression"; snapshot.viewState = selectionBlob;
            if (!expect(store.ready() && store.rememberClosedWindow(snapshot), L"save the real selection blob through production closed-window history")) return false;
        }
        {
            FolderStateStore store(database.c_str());
            ClosedWindow stored, taken, remaining;
            if (!expect(store.ready() && store.lastClosedWindow(&stored) && stored.sequence > 0
                    && stored.key == "populated" && stored.viewState == selectionBlob, L"reopen SQLite and read the exact selection blob")) return false;
            if (!expect(store.takeClosedWindow(&taken) && taken.sequence == stored.sequence
                    && taken.viewState == selectionBlob, L"consume the saved closed-window entry atomically")) return false;
            pendingSelection = std::move(taken.viewState);
            if (!expect(store.lastClosedWindow(&remaining) && remaining.sequence == 0, L"consumed closure cannot be taken a second time")) return false;
            FolderState unchanged;
            if (!expect(store.read("populated", &unchanged) && sameView(unchanged.view, persisted)
                    && unchanged.x == 211 && unchanged.y == 193, L"closed-window selection history preserves folder settings and geometry")) return false;
        }
        return true;
    }
    bool rejectedSnapshotsPreserveSelection() {
        auto current = [&, observedGeneration = generation, observedView = view]() {
            return !closing && view == observedView && browser.isViewNotification(observedGeneration);
        };
        if (selectionCase == 0) {
            // Independent version-1 fixture carrying the old scroll anchor.
            // Its pixel offset is deliberately not part of this native contract.
            CComQIPtr<IFolderView> folderView(view);
            CComHeapPtr<ITEMID_CHILD> anchor;
            CComPtr<IStream> stream;
            stream.Attach(SHCreateMemStream(nullptr, 0));
            if (!expect(stream != nullptr, L"create legacy snapshot fixture stream")
                    || !checked(folderView->Item(100, &anchor), L"read real PIDL for the legacy anchor")) return false;
            auto header = std::vector<uint8_t>(selectionBlob.begin(), selectionBlob.begin() + 20);
            const uint32_t flags = 6;
            std::memcpy(header.data() + 8, &flags, sizeof(flags));
            if (!checked(IStream_Write(stream, header.data(), static_cast<ULONG>(header.size())), L"write original version-1 header")
                    || !checked(IStream_WritePidl(stream, anchor), L"write legacy anchor through the Shell serializer")
                    || !checked(IStream_Write(stream, selectionBlob.data() + 20, static_cast<ULONG>(selectionBlob.size() - 20)), L"append saved focus and selection PIDLs")) return false;
            STATSTG size = {};
            LARGE_INTEGER beginning = {};
            if (!checked(stream->Stat(&size, STATFLAG_NONAME), L"read legacy fixture size")
                    || !expect(size.cbSize.QuadPart <= INT32_MAX, L"legacy fixture fits the existing blob limit")
                    || !checked(stream->Seek(beginning, STREAM_SEEK_SET, nullptr), L"rewind legacy fixture")) return false;
            std::vector<uint8_t> legacy(static_cast<size_t>(size.cbSize.QuadPart));
            if (!checked(IStream_Read(stream, legacy.data(), static_cast<ULONG>(legacy.size())), L"read independently assembled legacy fixture")
                    || !checked(restoreReopenSelection(view, legacy, current), L"restore the existing version-1 legacy anchor format")
                    || !expect(legacy.empty(), L"legacy snapshot is consumed once") || !selectionIs()) return false;
        }
        auto invalid = selectionBlob;
        invalid.resize(sizeof(uint32_t));
        HRESULT result = restoreReopenSelection(view, invalid, current);
        if (!expect(FAILED(result) && invalid.empty(), L"truncated snapshot fails and is consumed before any selection mutation") || !selectionIs()) return false;
        invalid = selectionBlob;
        invalid.pop_back();
        result = restoreReopenSelection(view, invalid, current);
        if (!expect(FAILED(result) && invalid.empty(), L"truncated PIDL data fails before clearing the current selection") || !selectionIs()) return false;
        invalid = selectionBlob;
        const uint32_t unknownFlags = 8;
        std::memcpy(invalid.data() + 8, &unknownFlags, sizeof(unknownFlags));
        result = restoreReopenSelection(view, invalid, current);
        if (!expect(FAILED(result) && invalid.empty(), L"unknown snapshot flags fail without clearing the current selection") || !selectionIs()) return false;
        invalid = selectionBlob;
        result = restoreReopenSelection(view, invalid, []() { return false; });
        if (!expect(result == E_ABORT && invalid.empty(), L"cancelled restoration consumes the snapshot without touching the view")) return false;
        return selectionIs();
    }
    bool moveFixtureIcons(bool displaced) {
        CComQIPtr<IFolderView> folderView(view);
        for (size_t i = 0; i < manualIconIds.size(); ++i) {
            PCUITEMID_CHILD id = manualIconIds[i];
            POINT point = manualPoints[i];
            if (displaced) { point.x += 73; point.y += 121; }
            point = scaleDPI(point);
            if (!checked(folderView->SelectAndPositionItems(1, &id, &point,
                    SVSI_POSITIONITEM | SVSI_NOSTATECHANGE), L"position a real icon at explicit coordinates")) return false;
            POINT actual = {};
            if (!checked(folderView->GetItemPosition(id, &actual), L"read explicitly positioned icon")
                    || !expect(actual.x == point.x && actual.y == point.y,
                        L"native view honors the requested coordinates exactly")) return false;
        }
        return true;
    }
    bool decodeExpectedIcons() {
        expectedIcons.clear();
        CComPtr<IStream> stream;
        stream.Attach(SHCreateMemStream(iconBlob.data(), static_cast<UINT>(iconBlob.size())));
        if (!expect(stream != nullptr, L"open captured icon bytes for independent verification")) return false;
        LARGE_INTEGER zero = {};
        ULARGE_INTEGER offset = {};
        std::vector<std::string> identities;
        for (;;) {
            HRESULT result = stream->Seek(zero, STREAM_SEEK_CUR, &offset);
            if (FAILED(result)) return checked(result, L"read icon fixture stream offset");
            if (offset.QuadPart == iconBlob.size()) break;
            expectedIcons.emplace_back();
            auto &position = expectedIcons.back();
            result = IStream_ReadPidl(stream, &position.id);
            if (FAILED(result)) return checked(result, L"decode captured child PIDL with the Shell serializer");
            if (!position.id) return expect(false, L"captured icon has a child identity");
            identities.emplace_back(reinterpret_cast<const char *>(position.id.m_pData), ILGetSize(position.id));
            if (expectedIcons.size() == 1) {
                result = stream->Seek(zero, STREAM_SEEK_CUR, &offset);
                if (FAILED(result)) return checked(result, L"locate the first stored POINT");
                firstIconPointOffset = static_cast<size_t>(offset.QuadPart);
            }
            result = IStream_Read(stream, &position.point, sizeof(position.point));
            if (FAILED(result)) return checked(result, L"decode captured 96-DPI coordinates");
        }
        std::sort(identities.begin(), identities.end());
        return expect(expectedIcons.size() == 150, L"icon capture contains all 150 identities and coordinates")
            && expect(std::adjacent_find(identities.begin(), identities.end()) == identities.end(),
                L"all captured icon identities are distinct");
    }
    bool iconsAre() {
        CComQIPtr<IFolderView> folderView(view);
        for (size_t i = 0; i < expectedIcons.size(); ++i) {
            POINT actual = {};
            const HRESULT result = folderView->GetItemPosition(expectedIcons[i].id, &actual);
            if (FAILED(result)) return checked(result, L"read an icon position for exact comparison");
            const POINT expected = scaleDPI(expectedIcons[i].point);
            if (actual.x != expected.x || actual.y != expected.y) {
                log(L"Icon %zu: actual=(%ld,%ld), expected=(%ld,%ld)", i,
                    actual.x, actual.y, expected.x, expected.y);
                return expect(false, L"icon coordinates match the saved identity after DPI conversion");
            }
        }
        return expect(expectedIcons.size() == 150, L"all 150 icon identities have their exact saved coordinates");
    }
    bool saveIcons() {
        auto current = [&, observedGeneration = generation, observedView = view]() {
            return !closing && view == observedView && browser.isViewNotification(observedGeneration);
        };
        CComQIPtr<IFolderView> folderView(view);
        if (!checked(captureIconPositions(folderView, current, true, {}, iconBlob), L"capture all positions through production code")
                || !decodeExpectedIcons()) return false;
        {
            FolderStateStore store(database.c_str());
            FolderState patch; patch.icons = iconBlob;
            if (!expect(store.ready() && store.save("populated", patch, FolderState::Icons), L"save only the icon blob through production SQLite")) return false;
        }
        FolderState restored;
        return readStored("populated", restored)
            && expect((restored.present & FolderState::Icons) && restored.icons == iconBlob,
                L"a new SQLite connection reads the exact existing-format icon bytes")
            && expect(restored.x == 211 && restored.y == 193 && restored.width == 640 && restored.height == 480
                && sameView(restored.view, iconSettings), L"icon-only save preserves geometry and view settings");
    }
    bool rejectedIconBlobsPreserveState() {
        CComQIPtr<IFolderView> folderView(view);
        auto current = [&, observedGeneration = generation, observedView = view]() {
            return !closing && view == observedView && browser.isViewNotification(observedGeneration);
        };
        auto invalid = iconBlob;
        // A valid first record requests a different location; a later truncation
        // must prevent even that first move, not just fail after partial mutation.
        const POINT other = {1234, 5678};
        std::memcpy(invalid.data() + firstIconPointOffset, &other, sizeof(other));
        invalid.pop_back();
        HRESULT result = restoreIconPositions(folderView, invalid, current);
        if (!expect(FAILED(result), L"truncated icon blob fails before applying its valid first record")
                || !iconsAre() || !selectionIs()) return false;
        result = restoreIconPositions(folderView, iconBlob, []() { return false; });
        if (!expect(result == E_ABORT, L"cancelled icon restoration does not move any icon")
                || !iconsAre() || !selectionIs()) return false;
        std::vector<uint8_t> cancelled = {1};
        result = captureIconPositions(folderView, []() { return false; }, true, {}, cancelled);
        return expect(result == E_ABORT && cancelled.empty(), L"cancelled capture cannot expose a partial icon blob")
            && iconsAre() && selectionIs();
    }
    bool readStored(const std::string &identity, FolderState &state) {
        FolderStateStore store(database.c_str());
        if (!expect(store.ready(), L"reopen the real SQLite database")) return false;
        return expect(store.read(identity, &state), L"read the stored folder state from disk");
    }
    bool filteredNamesAre() {
        const auto &expected = filterCases[filterCase];
        std::vector<std::wstring> actualNames, expectedNames;
        for (int i = 0; i < expected.count; ++i) {
            std::wstring name;
            if (!itemName(i, name)) return false;
            actualNames.push_back(std::move(name));
            expectedNames.push_back(expected.single ? expected.single : L"élément-" + std::to_wstring(i) + L".txt");
        }
        std::sort(actualNames.begin(), actualNames.end());
        std::sort(expectedNames.begin(), expectedNames.end());
        return expect(actualNames == expectedNames, L"filter exposes the exact expected identities, including equal-count queries");
    }
    bool filteredSavePreservesIcons() {
        CComQIPtr<IFolderView2> folderView(view);
        auto current = [&, observedGeneration = generation, observedView = view]() {
            return !closing && view == observedView && browser.isViewNotification(observedGeneration);
        };
        std::vector<uint8_t> bytes = {99};
        const HRESULT result = captureIconPositions(folderView, current, true, iconBlob, bytes, true);
        if (!expect(result == S_FALSE && bytes.empty(), L"production capture refuses to persist a filtered subset, including zero results")) return false;
        // Match FolderWindow's result handling: only S_OK can dirty Icons.
        // Saving presentation while filtered must leave the full icon blob intact.
        FolderState patch; patch.view = captureOptions(folderView, {});
        uint32_t mask = FolderState::View;
        if (result == S_OK) { patch.icons = std::move(bytes); mask |= FolderState::Icons; }
        {
            FolderStateStore store(database.c_str());
            if (!expect(store.ready() && store.save("populated", patch, mask), L"persist presentation while the native view is filtered")) return false;
        }
        FolderState restored;
        return readStored("populated", restored)
            && expect(restored.icons == iconBlob && sameView(restored.view, iconSettings)
                && restored.x == 211 && restored.y == 193 && restored.width == 640 && restored.height == 480,
                L"filtered save preserves all complete-folder icon bytes, presentation and geometry in SQLite");
    }
    bool saveConfigured() {
        {
            FolderStateStore store(database.c_str());
            if (!expect(store.ready(), L"create isolated production SQLite store")) return false;
            FolderState geometry;
            geometry.present = FolderState::Position | FolderState::Size | FolderState::Maximized;
            geometry.x = 211; geometry.y = 193; geometry.width = 640; geometry.height = 480;
            if (!expect(store.save("populated", geometry, geometry.present), L"save unrelated window geometry")) return false;
            FolderState patch; patch.present = FolderState::View; patch.view = wanted;
            if (!expect(store.save("populated", patch, FolderState::View), L"save the real native view settings only")) return false;
            if (!expect(store.save("empty", patch, FolderState::View), L"save settings for the empty-folder fixture")) return false;
        } // Close the connection before reading through a new store.
        FolderState restored;
        if (!readStored("populated", restored)) return false;
        if (!expect(sameView(restored.view, wanted), L"SQLite round trip preserves every captured view field")) return false;
        if (!expect(restored.x == 211 && restored.y == 193 && restored.width == 640 && restored.height == 480
                && !restored.maximized, L"view-only save preserves unrelated window geometry")) return false;
        if (!expect(restored.present == (FolderState::Position | FolderState::Size
                | FolderState::Maximized | FolderState::View), L"stored presence mask preserves unrelated parts")) return false;
        persisted = restored.view;
        return true;
    }
    bool openView(const std::wstring &folder) {
        const FOLDERSETTINGS settings = {static_cast<UINT>(phase == Phase::Configure ? FVM_DETAILS : FVM_LIST),
            FWF_NOBROWSERVIEWSTATE | FWF_AUTOARRANGE};
        if (!checked(browser.initialize(window, bounds(), settings,
                static_cast<IServiceProvider *>(this), this), L"initialize production spatial host")) return false;
        auto concreteFilter = new FolderNameFilter(L"");
        nameFilter.Attach(concreteFilter);
        if (!checked(concreteFilter->initialize(), L"initialize production free-threaded name filter")
                || !checked(browser.setFilter(nameFilter), L"install production filter before native navigation")) return false;
        CComPtr<IShellItem> item;
        if (!checked(SHCreateItemFromParsingName(folder.c_str(), nullptr, IID_PPV_ARGS(&item)), L"bind fixture folder")) return false;
        return checked(browser.navigate(item), L"navigate production host");
    }
    bool replaceView(const std::wstring &folder) {
        const ULONG_PTR previous = generation;
        view.Release();
        if (!checked(browser.destroy(), L"destroy the previous browser and native view")) return false;
        if (!expect(!browser.isViewNotification(previous), L"previous observation generation is invalid")) return false;
        return openView(folder);
    }
    bool retireLifetimeView() {
        retiredGeneration = generation;
        oldViewIdentity.Release();
        if (view && !checked(view->QueryInterface(IID_PPV_ARGS(&oldViewIdentity)), L"retain the retiring native view identity")) return false;
        log(L"LIFETIME %u: after %s returned; generation=%llu, ready handled=%d, published EnumDone=%llu",
            lifetimeCase, lifetimeCase % 2 ? L"Refresh" : L"BrowseToObject",
            static_cast<unsigned long long>(generation), ready,
            static_cast<unsigned long long>(browser.enumerationSerial()));
        // No further completion wait: cancellation must tolerate either timing.
        lifetimeTeardown = true;
        if (!checked(browser.stopObserving(), L"cancel lifetime observation before teardown")
                || !expect(!browser.isViewNotification(retiredGeneration), L"retired observation is invalid before browser destruction")) return false;
        view.Release();
        if (!checked(browser.destroy(), L"destroy native browser immediately after the requested operation")
                || !expect(!browser, L"destroyed host no longer exposes a browser")) return false;
        CComPtr<IShellView> unavailable;
        const HRESULT result = browser.currentView(IID_PPV_ARGS(&unavailable));
        if (!expect(result == E_UNEXPECTED && !unavailable, L"destroyed host cannot return a native view")
                || !checked(browser.destroy(), L"repeated host teardown is harmless without destroying a second native browser")) return false;
        generation = enumDone = 0;
        navigation = ready = queued = presentationPending = false;
        lifetimeTeardown = false;
        return true;
    }
    void step() {
        if (finished || closing) return;
        log(L"PHASE %d; generation=%llu", static_cast<int>(phase), static_cast<unsigned long long>(generation));
        CComQIPtr<IFolderView2> folderView(view);
        if (!expect(folderView != nullptr, L"native folder view is available")) return;
        switch (phase) {
        case Phase::Configure:
            if (!countIs(150)) return;
            wanted = captureOptions(folderView, {});
            if (!expect(wanted.present == 31 && wanted.columns.size() >= 3 && !wanted.sort.empty(),
                    L"all persistent parts are available on the real filesystem view")) return;
            wanted.mode = FVM_DETAILS; wanted.iconSize = 16;
            std::swap(wanted.columns[1], wanted.columns[2]);
            wanted.columns.back().width += 23;
            wanted.sort = {{key(PKEY_ItemNameDisplay), SORT_DESCENDING}};
            wanted.groupBy = key(PKEY_ItemTypeText); wanted.groupAscending = false;
            phase = Phase::VerifyConfigured;
            if (!checked(restoreOptions(folderView, wanted), L"apply nondefault settings through production restoration")) return;
            // A column operation may already have emitted EnumDone before the
            // later sort starts. Such an older completion cannot finish this batch.
            baseline = browser.enumerationSerial();
            queue();
            return;
        case Phase::VerifyConfigured:
            if (!countIs(150, true) || !optionsAre(wanted) || !saveConfigured()) return;
            phase = Phase::Recreate;
            queue();
            return;
        case Phase::Recreate:
            if (!checked(view->QueryInterface(IID_PPV_ARGS(&oldViewIdentity)), L"retain previous COM identity for comparison")) return;
            phase = Phase::Restore;
            replaceView(fixture);
            return;
        case Phase::Restore:
        case Phase::RestoreEmpty: {
            const bool empty = phase == Phase::RestoreEmpty;
            if (!countIs(empty ? 0 : 150)) return;
            log(L"RESTORE: initial navigation, EnumReady and EnumDone completed before applying stored settings");
            trace(L"before deferred production restoreOptions");
            if (!checked(restoreOptions(folderView, persisted),
                    L"restore SQLite settings after initial enumeration, as FolderWindow does")) return;
            if (!afterPresentation(empty ? 0 : 150)) return;
            trace(L"after deferred production restoreOptions");
            phase = empty ? Phase::VerifyEmpty : Phase::VerifyRestored;
            queue();
            return;
        }
        case Phase::VerifyRestored: {
            if (!presentationIsComplete() || !countIs(150, true) || !optionsAre(persisted)) return;
            CComPtr<IUnknown> identity;
            if (!checked(view->QueryInterface(IID_PPV_ARGS(&identity)), L"read replacement COM identity")) return;
            if (!expect(identity != oldViewIdentity, L"restoration targets a genuinely new native view")) return;
            oldViewIdentity.Release();
            phase = Phase::VerifyRefresh;
            baseline = browser.enumerationSerial();
            if (!checked(view->Refresh(), L"refresh the restored native view")) return;
            queue();
            return;
        }
        case Phase::VerifyRefresh: {
            if (enumDone <= baseline) { log(L"WAIT: real EnumDone for Refresh"); return; }
            if (!countIs(150) || !optionsAre(persisted)) return;
            FolderState empty;
            if (!readStored("empty", empty)) return;
            if (!expect(empty.present == FolderState::View && sameView(empty.view, wanted),
                    L"empty-folder state is independent and contains only its requested view settings")) return;
            persisted = empty.view;
            phase = Phase::RestoreEmpty;
            replaceView(emptyFolder);
            return;
        }
        case Phase::VerifyEmpty: {
            if (!presentationIsComplete() || !countIs(0, true) || !optionsAre(persisted)) return;
            log(L"PASS: an enumerated empty folder is valid; zero items was not used as a readiness signal");
            auto current = [&, observedGeneration = generation, observedView = view]() {
                return !closing && view == observedView && browser.isViewNotification(observedGeneration);
            };
            std::vector<uint8_t> emptySnapshot;
            if (!checked(captureReopenSelection(view, current, emptySnapshot), L"capture a genuinely empty native folder")
                    || !expect(emptySnapshot.size() == 20, L"empty folder has a header-only snapshot without selected or focused items")) return;
            emptySelectionBlob = emptySnapshot;
            SetFocus(focusTarget);
            if (!expect(GetFocus() == focusTarget, L"host edit control has focus before empty-folder restoration")
                    || !checked(restoreReopenSelection(view, emptySnapshot, current), L"restore the empty-folder snapshot")
                    || !expect(GetFocus() == focusTarget, L"empty-folder restoration preserves keyboard focus")
                    || !expect(emptySnapshot.empty(), L"empty-folder snapshot is consumed once") || !countIs(0)) return;
            phase = Phase::SelectionPrepare;
            replaceView(fixture);
            return;
        }
        case Phase::SelectionPrepare:
            if (!countIs(150) || !prepareSelection()) return;
            phase = Phase::SelectionRecreate;
            queue();
            return;
        case Phase::SelectionRecreate:
            if (!checked(view->QueryInterface(IID_PPV_ARGS(&oldViewIdentity)), L"retain source selection-view identity")) return;
            phase = Phase::SelectionOptions;
            replaceView(fixture);
            return;
        case Phase::SelectionOptions:
            if (!countIs(150)) return;
            if (!checked(restoreOptions(folderView, persisted), L"restore settings before the pending selection")
                    || !afterPresentation(150)) return;
            phase = Phase::SelectionRestore;
            queue();
            return;
        case Phase::SelectionRestore: {
            if (!presentationIsComplete() || !countIs(150, true) || !optionsAre(persisted)) return;
            CComPtr<IUnknown> identity;
            if (!checked(view->QueryInterface(IID_PPV_ARGS(&identity)), L"read recreated selection-view identity")
                    || !expect(identity != oldViewIdentity, L"selection restoration targets a genuinely new native view")) return;
            oldViewIdentity.Release();
            SetFocus(focusTarget);
            if (!expect(GetFocus() == focusTarget, L"host edit control has focus before restoration")) return;
            selectionBaseline = selectionChanges;
            phase = Phase::SelectionVerify;
            auto current = [&, observedGeneration = generation, observedView = view]() {
                return !closing && view == observedView && browser.isViewNotification(observedGeneration);
            };
            if (!checked(restoreReopenSelection(view, pendingSelection, current), L"restore through production reopen-selection code")
                    || !expect(pendingSelection.empty(), L"restoration consumes the saved blob before any future callback")) return;
            queue();
            return;
        }
        case Phase::SelectionVerify: {
            if (selectionCase < 2 && selectionChanges <= selectionBaseline) {
                log(L"WAIT: public selection-change notification after nonempty restoration");
                return;
            }
            if (!countIs(150) || !selectionIs() || !rejectedSnapshotsPreserveSelection()) return;
            std::wstring name;
            if (!itemName(99, name)) return;
            expectedSelection = {name}; expectedFocus = name;
            if (!checked(folderView->SelectItem(99, SVSI_SELECT | SVSI_DESELECTOTHERS
                    | SVSI_FOCUSED | SVSI_NOTAKEFOCUS), L"make a subsequent selection after initial restoration")) return;
            auto current = [&, observedGeneration = generation, observedView = view]() {
                return !closing && view == observedView && browser.isViewNotification(observedGeneration);
            };
            const HRESULT replay = restoreReopenSelection(view, pendingSelection, current);
            if (!expect(replay == S_FALSE, L"a later initialization attempt cannot replay the consumed snapshot")) return;
            phase = Phase::SelectionAfterUser;
            queue();
            return;
        }
        case Phase::SelectionAfterUser:
            if (!selectionIs()) return;
            baseline = browser.enumerationSerial();
            phase = Phase::SelectionAfterRefresh;
            if (!checked(view->Refresh(), L"refresh after the subsequent selection")) return;
            queue();
            return;
        case Phase::SelectionAfterRefresh:
            if (enumDone <= baseline) { log(L"WAIT: real EnumDone after selection/Refresh"); return; }
            if (!countIs(150) || !selectionIs() || !optionsAre(persisted)) return;
            log(L"PASS: selection case %u survives a recreated view, SQLite and refresh without replay", selectionCase);
            if (++selectionCase < 3) {
                phase = Phase::SelectionPrepare;
                queue();
            } else {
                phase = Phase::IconsConfigure;
                queue();
            }
            return;
        case Phase::IconsConfigure:
            iconSettings = {};
            iconSettings.present = FolderViewSettings::Mode | FolderViewSettings::Flags | FolderViewSettings::Group;
            iconSettings.mode = FVM_ICON; iconSettings.iconSize = 32;
            log(L"ICON REQUEST: mode=%d size=%d", iconSettings.mode, iconSettings.iconSize);
            trace(L"before production icon settings restoration");
            if (!checked(restoreOptions(folderView, iconSettings), L"switch to ungrouped manually positioned icons")
                    || !afterPresentation(150)) return;
            trace(L"immediately after production icon settings restoration");
            phase = Phase::IconsSetup;
            queue();
            return;
        case Phase::IconsSetup: {
            if (!presentationIsComplete() || !countIs(150, true)
                    || !expect(folderView->GetAutoArrange() == S_FALSE, L"icon fixture has auto-arrange disabled")) return;
            FOLDERVIEWMODE mode = FVM_AUTO; int size = 0;
            if (!checked(folderView->GetViewModeAndIconSize(&mode, &size), L"read the manual icon presentation")) return;
            log(L"ICON READBACK: requested mode=%d size=%d; actual mode=%d size=%d; DPI=%d",
                iconSettings.mode, iconSettings.iconSize, static_cast<int>(mode), size, systemDPI);
            // Windows can report FVM_SMALLICON for a 32-pixel FVM_ICON request.
            // Only this fixture precondition accepts both icon layouts; saved
            // settings below must round-trip with their exact reported mode.
            const bool matches = (mode == FVM_ICON || mode == FVM_SMALLICON) && size == 32;
            if (!expect(matches, L"fixture uses native icons at exactly the requested size")) return;
            iconSettings = captureOptions(folderView, {});
            if (!expect((iconSettings.present & FolderViewSettings::Mode)
                    && iconSettings.mode == mode && iconSettings.iconSize == size,
                    L"capture retains the exact reported icon mode and size for persistence")) return;
            if (!expect((iconSettings.present & FolderViewSettings::Flags) && iconSettings.flags == 0,
                    L"manual icon fixture has neither auto-arrange nor snap-to-grid")) return;
            {
                FolderStateStore store(database.c_str());
                FolderState patch; patch.view = iconSettings;
                if (!expect(store.ready() && store.save("populated", patch, FolderState::View), L"persist the actual manual-icon settings")) return;
            }
            for (size_t i = 0; i < manualIconIds.size(); ++i)
                if (!checked(folderView->Item(static_cast<int>(i), &manualIconIds[i]), L"retain a real icon identity for manual positioning")
                        || !expect(manualIconIds[i] != nullptr, L"manual icon identity is available")) return;
            std::wstring name;
            if (!itemName(1, name)) return;
            expectedSelection = {name}; expectedFocus = name;
            SetFocus(focusTarget);
            if (!checked(folderView->SelectItem(1, SVSI_SELECT | SVSI_DESELECTOTHERS | SVSI_FOCUSED | SVSI_NOTAKEFOCUS),
                    L"establish selection and focus that positioning must preserve") || !moveFixtureIcons(false) || !selectionIs()) return;
            // Build a legacy-format fixture independently of production capture.
            CComPtr<IStream> legacy;
            legacy.Attach(SHCreateMemStream(nullptr, 0));
            if (!expect(legacy != nullptr, L"create the independent legacy icon stream")) return;
            for (size_t i = 0; i < manualIconIds.size(); ++i)
                if (!checked(IStream_WritePidl(legacy, manualIconIds[i]), L"serialize legacy child PIDL")
                        || !checked(IStream_Write(legacy, &manualPoints[i], sizeof(POINT)), L"serialize legacy 96-DPI point")) return;
            STATSTG bytes = {}; LARGE_INTEGER beginning = {};
            if (!checked(legacy->Stat(&bytes, STATFLAG_NONAME), L"read legacy icon fixture size")
                    || !expect(bytes.cbSize.QuadPart <= INT32_MAX, L"legacy icon fixture fits the stored blob limit")
                    || !checked(legacy->Seek(beginning, STREAM_SEEK_SET, nullptr), L"rewind legacy icon fixture")) return;
            legacyIconBlob.resize(static_cast<size_t>(bytes.cbSize.QuadPart));
            if (!checked(IStream_Read(legacy, legacyIconBlob.data(), static_cast<ULONG>(legacyIconBlob.size())), L"read independently serialized legacy icon bytes")
                    || !saveIcons()) return;
            for (size_t i = 0; i < manualIconIds.size(); ++i) {
                const auto found = std::find_if(expectedIcons.begin(), expectedIcons.end(), [&](const ExpectedIconPosition &position) {
                    return ILIsEqual(position.id, manualIconIds[i]) != FALSE;
                });
                if (!expect(found != expectedIcons.end() && found->point.x == manualPoints[i].x
                        && found->point.y == manualPoints[i].y, L"captured manual coordinates use the existing 96-DPI format")) return;
            }
            if (!checked(view->QueryInterface(IID_PPV_ARGS(&oldViewIdentity)), L"retain the source icon-view identity")) return;
            phase = Phase::IconsOptions;
            replaceView(fixture);
            return;
        }
        case Phase::IconsOptions:
            if (!countIs(150) || !checked(restoreOptions(folderView, iconSettings), L"restore settings before icon coordinates")
                    || !afterPresentation(150)) return;
            phase = Phase::IconsRestore;
            queue();
            return;
        case Phase::IconsRestore: {
            if (!presentationIsComplete() || !countIs(150, true) || !optionsAre(iconSettings)) return;
            CComPtr<IUnknown> identity;
            if (!checked(view->QueryInterface(IID_PPV_ARGS(&identity)), L"read the recreated icon-view identity")
                    || !expect(identity != oldViewIdentity, L"icon restoration targets a genuinely new native view")) return;
            oldViewIdentity.Release();
            FolderState restored;
            if (!readStored("populated", restored) || !expect(restored.icons == iconBlob, L"recreated view receives the SQLite icon blob")) return;
            SetFocus(focusTarget);
            if (!checked(view->SelectItem(manualIconIds[1], SVSI_SELECT | SVSI_DESELECTOTHERS | SVSI_FOCUSED | SVSI_NOTAKEFOCUS),
                    L"establish selection before icon restoration") || !moveFixtureIcons(true) || !selectionIs()) return;
            auto current = [&, observedGeneration = generation, observedView = view]() {
                return !closing && view == observedView && browser.isViewNotification(observedGeneration);
            };
            if (!checked(restoreIconPositions(folderView, restored.icons, current), L"restore SQLite icon positions through production code")) return;
            phase = Phase::IconsVerify;
            queue();
            return;
        }
        case Phase::IconsVerify: {
            if (!iconsAre() || !selectionIs() || !rejectedIconBlobsPreserveState() || !moveFixtureIcons(true)) return;
            auto current = [&, observedGeneration = generation, observedView = view]() {
                return !closing && view == observedView && browser.isViewNotification(observedGeneration);
            };
            if (!checked(restoreIconPositions(folderView, legacyIconBlob, current), L"restore the independent legacy-format icon fixture")
                    || !iconsAre() || !selectionIs() || !moveFixtureIcons(true) || !saveIcons()) return;
            // Match FolderWindow::refresh: persist the latest positions first,
            // then restore them only after this refresh's public completion.
            baseline = browser.enumerationSerial();
            phase = Phase::IconsRefresh;
            if (!checked(view->Refresh(), L"refresh after saving subsequently moved icons")) return;
            queue();
            return;
        }
        case Phase::IconsRefresh: {
            if (enumDone <= baseline) { log(L"WAIT: real EnumDone after icon refresh"); return; }
            if (!countIs(150)) return;
            FolderState restored;
            if (!readStored("populated", restored) || !expect(restored.icons == iconBlob, L"refresh reads the latest saved positions")) return;
            auto current = [&, observedGeneration = generation, observedView = view]() {
                return !closing && view == observedView && browser.isViewNotification(observedGeneration);
            };
            if (!checked(restoreIconPositions(folderView, restored.icons, current), L"restore latest positions after the completed refresh")
                    || !iconsAre() || !selectionIs() || !optionsAre(iconSettings)) return;
            phase = Phase::FilterApply;
            queue();
            return;
        }
        case Phase::FilterApply:
            log(L"FILTER %zu: query=[%s], expected=%d", filterCase, filterCases[filterCase].query, filterCases[filterCase].count);
            static_cast<FolderNameFilter *>(nameFilter.p)->setQuery(filterCases[filterCase].query);
            baseline = browser.enumerationSerial();
            phase = Phase::FilterVerify;
            if (!checked(view->Refresh(), L"refresh the same native view after updating the production query")) return;
            queue();
            return;
        case Phase::FilterVerify: {
            if (enumDone <= baseline) { log(L"WAIT: a new public EnumDone for the current filter query"); return; }
            if (!countIs(filterCases[filterCase].count) || !filteredNamesAre() || !optionsAre(iconSettings)) return;
            if (*filterCases[filterCase].query) {
                if (!filteredSavePreservesIcons()) return;
            } else {
                FolderState restored;
                if (!readStored("populated", restored) || !expect(restored.icons == iconBlob,
                        L"clearing the filter reloads the unchanged complete-folder positions")) return;
                SetFocus(focusTarget);
                if (!checked(view->SelectItem(manualIconIds[1], SVSI_SELECT | SVSI_DESELECTOTHERS | SVSI_FOCUSED | SVSI_NOTAKEFOCUS),
                        L"select a visible item before restoring positions after filter clearing")) return;
                auto current = [&, observedGeneration = generation, observedView = view]() {
                    return !closing && view == observedView && browser.isViewNotification(observedGeneration);
                };
                if (!checked(restoreIconPositions(folderView, restored.icons, current), L"restore complete icon positions after filter clearing")
                        || !iconsAre() || !selectionIs()) return;
            }
            if (++filterCase < _countof(filterCases)) {
                phase = Phase::FilterApply;
                queue();
            } else {
                phase = Phase::IconsEmptyOptions;
                replaceView(emptyFolder);
            }
            return;
        }
        case Phase::IconsEmptyOptions:
            if (!countIs(0) || !checked(restoreOptions(folderView, iconSettings), L"apply manual icon settings to the empty fixture")
                    || !afterPresentation(0)) return;
            phase = Phase::IconsEmpty;
            queue();
            return;
        case Phase::IconsEmpty: {
            if (!presentationIsComplete() || !countIs(0, true)
                    || !expect(folderView->GetAutoArrange() == S_FALSE, L"empty icon fixture has auto-arrange disabled")) return;
            auto current = [&, observedGeneration = generation, observedView = view]() {
                return !closing && view == observedView && browser.isViewNotification(observedGeneration);
            };
            std::vector<uint8_t> preserved, complete;
            if (!checked(captureIconPositions(folderView, current, false, iconBlob, preserved), L"exercise incomplete-capture preservation with a real empty view")
                    || !expect(preserved == iconBlob, L"incomplete capture retains every unseen saved identity")
                    || !checked(captureIconPositions(folderView, current, true, iconBlob, complete), L"capture a genuinely enumerated empty folder")
                    || !expect(complete.empty(), L"completed empty capture drops all obsolete positions")) return;
            FolderState empty;
            if (!readStored("empty", empty)) return;
            const auto previousView = empty.view;
            {
                FolderStateStore store(database.c_str());
                FolderState patch; patch.icons = complete;
                if (!expect(store.ready() && store.save("empty", patch, FolderState::Icons), L"persist an empty icon blob independently")) return;
            }
            if (!readStored("empty", empty) || !expect((empty.present & FolderState::Icons) && empty.icons.empty()
                    && sameView(empty.view, previousView), L"empty icon persistence preserves independent view settings")) return;
            SetFocus(focusTarget);
            if (!expect(GetFocus() == focusTarget, L"host edit has focus before empty icon restoration")
                    || !checked(restoreIconPositions(folderView, empty.icons, current), L"restore an empty icon blob without targeting an item")
                    || !expect(GetFocus() == focusTarget, L"empty icon restoration preserves keyboard focus")) return;
            phase = Phase::LifetimeStart;
            queue();
            return;
        }
        case Phase::LifetimeStart:
            if (lifetimeCase % 2) {
                if (!checked(view->Refresh(), L"request a native refresh before immediate teardown")) return;
            } else if (!replaceView(fixture)) return;
            if (!retireLifetimeView()) return;
            phase = Phase::LifetimeVerify;
            replaceView(fixture);
            return;
        case Phase::LifetimeVerify: {
            // queue() requires this replacement's navigation, EnumReady and EnumDone.
            if (!countIs(150)
                    || !expect(browser.isViewNotification(generation) && generation != retiredGeneration,
                        L"replacement observation is current and differs from the retired generation")
                    || !expect(!browser.isViewNotification(retiredGeneration), L"replacement host still rejects the retired generation")) return;
            if (oldViewIdentity) {
                CComPtr<IUnknown> identity;
                if (!checked(view->QueryInterface(IID_PPV_ARGS(&identity)), L"read replacement identity after immediate teardown")
                        || !expect(identity != oldViewIdentity, L"lifetime cycle creates a genuinely different native view")) return;
                oldViewIdentity.Release();
            } else log(L"LIFETIME: cancellation preceded publication of a view identity");
            if (++lifetimeCase < 8) {
                phase = Phase::LifetimeStart;
                queue();
            } else {
                log(L"LIFETIME: eight teardown/recreation cases passed; actual stale messages rejected=%u", staleNotifications);
                stop(true);
            }
            return;
        }
        }
    }
public:
    HWND window = nullptr;
    int exitCode = 3;
    StateProbe(std::wstring populated, std::wstring empty, std::string db, bool diagnosticMode)
        : fixture(std::move(populated)), emptyFolder(std::move(empty)), database(std::move(db)), diagnostic(diagnosticMode) {}
    virtual ~StateProbe() { shutdown(); }
    STDMETHODIMP QueryInterface(REFIID id, void **object) override {
        if (!object) return E_POINTER;
        *object = nullptr;
        if (id == IID_IUnknown || id == __uuidof(IServiceProvider)) *object = static_cast<IServiceProvider *>(this);
        else if (id == __uuidof(IExplorerBrowserEvents)) *object = static_cast<IExplorerBrowserEvents *>(this);
        else if (id == __uuidof(ICommDlgBrowser)) *object = static_cast<ICommDlgBrowser *>(this);
        else if (id == __uuidof(ICommDlgBrowser2)) *object = static_cast<ICommDlgBrowser2 *>(this);
        else return E_NOINTERFACE;
        AddRef(); return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&references); }
    STDMETHODIMP_(ULONG) Release() override {
        const ULONG count = InterlockedDecrement(&references);
        if (!count) delete this;
        return count;
    }
    STDMETHODIMP QueryService(REFGUID service, REFIID iid, void **object) override {
        if (!object) return E_POINTER;
        *object = nullptr;
        return service == SID_SExplorerBrowserFrame ? QueryInterface(iid, object) : E_NOINTERFACE;
    }
    STDMETHODIMP OnDefaultCommand(IShellView *) override { return S_FALSE; }
    STDMETHODIMP OnStateChange(IShellView *source, ULONG change) override {
        if (!closing && source == view && change == CDBOSC_SELCHANGE) {
            ++selectionChanges;
            log(L"EVENT selection changed: generation=%llu, count=%u",
                static_cast<unsigned long long>(generation), selectionChanges);
            if (phase == Phase::SelectionVerify) queue();
        }
        return S_OK;
    }
    STDMETHODIMP IncludeObject(IShellView *, PCUITEMID_CHILD) override { return S_OK; }
    STDMETHODIMP GetDefaultMenuText(IShellView *, LPWSTR, int) override { return S_FALSE; }
    STDMETHODIMP GetViewFlags(DWORD *flags) override {
        if (!flags) return E_POINTER;
        // Let ExplorerBrowser invoke the installed IFolderFilter.
        *flags = CDB2GVF_NOSELECTVERB;
        if (diagnostic) log(L"FILTER HOST: GetViewFlags=0x%08lX; IncludeObject filtering enabled", *flags);
        return S_OK;
    }
    STDMETHODIMP Notify(IShellView *, DWORD) override { return S_OK; }
    STDMETHODIMP OnNavigationPending(PCIDLIST_ABSOLUTE) override {
        if (closing || lifetimeTeardown) return E_ABORT;
        checked(browser.stopObserving(), L"cancel observation before navigation");
        generation = enumDone = 0; navigation = ready = queued = presentationPending = false;
        return S_OK;
    }
    STDMETHODIMP OnViewCreated(IShellView *source) override {
        if (closing || lifetimeTeardown) return E_ABORT;
        view = source;
        creating = true;
        trace(L"OnViewCreated before observer registration");
        const HRESULT result = browser.observeView(source, window, READY, DONE, generation);
        trace(L"OnViewCreated after observer registration");
        if (!checked(result, L"observe public events of the new native view")) { creating = false; return S_OK; }
        creating = false;
        queue();
        return S_OK;
    }
    STDMETHODIMP OnNavigationComplete(PCIDLIST_ABSOLUTE) override {
        if (lifetimeTeardown) return S_OK;
        navigation = true;
        trace(L"OnNavigationComplete");
        queue(); return S_OK;
    }
    STDMETHODIMP OnNavigationFailed(PCIDLIST_ABSOLUTE) override {
        if (lifetimeTeardown) log(L"LIFETIME: navigation cancellation reported during explicit teardown");
        else expect(false, L"navigation completed without failure");
        return S_OK;
    }
    bool start() {
        WNDCLASS definition = {};
        definition.hInstance = GetModuleHandle(nullptr); definition.lpfnWndProc = procedure;
        definition.lpszClassName = L"FileSpacerNativeStateRegression";
        definition.hCursor = LoadCursor(nullptr, IDC_ARROW);
        if (!RegisterClass(&definition)) return false;
        if (!CreateWindow(definition.lpszClassName, L"FileSpacer — persistance Shell native",
                WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1000, 700,
                nullptr, nullptr, definition.hInstance, this)) return false;
        focusTarget = CreateWindow(L"EDIT", L"Host keyboard focus", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            4, 4, 190, 22, window, nullptr, definition.hInstance, nullptr);
        if (!focusTarget) return false;
        ShowWindow(window, SW_SHOW);
        if (!SetTimer(window, WATCHDOG, 60000, nullptr)) return false;
        return openView(fixture);
    }
    bool translate(MSG *message) {
        CComPtr<IShellView> current = view;
        HWND root = nullptr;
        return current && SUCCEEDED(current->GetWindow(&root)) && root
            && (message->hwnd == root || IsChild(root, message->hwnd))
            && current->TranslateAccelerator(message) == S_OK;
    }
    void shutdown() {
        if (closing) return;
        closing = true;
        if (window) KillTimer(window, WATCHDOG);
        view.Release(); oldViewIdentity.Release();
        const HRESULT result = browser.destroy();
        nameFilter.Release();
        log(L"Destroy final browser: HRESULT=0x%08lX", static_cast<unsigned long>(result));
        if (FAILED(result)) exitCode = 3;
    }
    static LRESULT CALLBACK procedure(HWND target, UINT message, WPARAM wParam, LPARAM lParam) {
        auto owner = reinterpret_cast<StateProbe *>(GetWindowLongPtr(target, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            owner = static_cast<StateProbe *>(reinterpret_cast<CREATESTRUCT *>(lParam)->lpCreateParams);
            owner->window = target;
            SetWindowLongPtr(target, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(owner));
        }
        if (!owner) return DefWindowProc(target, message, wParam, lParam);
        try {
            switch (message) {
            case READY: case DONE:
                if (owner->browser.isViewNotification(wParam) && !owner->closing) {
                    if (message == READY) owner->ready = true;
                    else owner->enumDone = std::max(owner->enumDone, static_cast<ULONG_PTR>(lParam));
                    log(L"EVENT %s: generation=%llu, serial=%llu", message == READY ? L"EnumReady" : L"EnumDone",
                        static_cast<unsigned long long>(wParam), static_cast<unsigned long long>(owner->enumDone));
                    owner->trace(message == READY ? L"handle EnumReady" : L"handle EnumDone");
                    if (owner->observingFailure) PostMessage(target, DIAGNOSTIC, wParam, 0);
                    owner->queue();
                } else {
                    ++owner->staleNotifications;
                    log(L"LIFETIME: rejected actual stale notification; generation=%llu", static_cast<unsigned long long>(wParam));
                }
                return 0;
            case DIAGNOSTIC:
                if (owner->browser.isViewNotification(wParam)) owner->observeFailedRestoration();
                return 0;
            case RUN:
                if (wParam != owner->generation || owner->closing) return 0;
                owner->queued = false;
                if (owner->running) { owner->requested = true; return 0; }
                owner->running = true; owner->requested = false;
                owner->step();
                owner->running = false;
                if (owner->requested) owner->queue();
                return 0;
            case WM_SIZE:
                if (owner->browser) owner->browser.setBounds(owner->bounds());
                return 0;
            case WM_TIMER:
                if (wParam == WATCHDOG) {
                    if (owner->observingFailure) {
                        owner->trace(L"watchdog after retained failure");
                        log(L"DIAG: watchdog closes the failed test; no continuation, exit 3");
                        PostMessage(target, WM_CLOSE, 0, 0);
                        return 0;
                    }
                    log(L"FAIL: 60 s watchdog at phase %d; no timer continuation", static_cast<int>(owner->phase));
                    owner->expect(false, L"native test finished before watchdog");
                }
                return 0;
            case WM_CLOSE:
                if (!owner->finished) owner->expect(false, L"test was not closed before completion");
                owner->shutdown();
                DestroyWindow(target);
                return 0;
            case WM_DESTROY: PostQuitMessage(owner->exitCode); return 0;
            case WM_NCDESTROY:
                SetWindowLongPtr(target, GWLP_USERDATA, 0); owner->window = nullptr;
                return DefWindowProc(target, message, wParam, lParam);
            default: break;
            }
        } catch (const std::bad_alloc &) {
            owner->running = false;
            owner->expect(false, L"native test has enough memory");
            if (owner->observingFailure) PostMessage(target, WM_CLOSE, 0, 0);
            return 0;
        }
        return DefWindowProc(target, message, wParam, lParam);
    }
};
} // namespace

int wmain(int argc, wchar_t **argv) {
    const bool diagnostic = argc == 2 && wcscmp(argv[1], L"--diagnostic") == 0;
    if (argc > 1 && !diagnostic) return 3;
    wchar_t executable[32768];
    const DWORD size = GetModuleFileName(nullptr, executable, _countof(executable));
    if (!size || size >= _countof(executable)) return 3;
    std::wstring base(executable, size); base.resize(base.find_last_of(L"\\/"));
    if (!directory(base + L"\\logs") || !directory(base + L"\\fixtures")) return 3;
    SYSTEMTIME time; GetLocalTime(&time);
    wchar_t suffix[100];
    swprintf_s(suffix, L"state-%04u%02u%02u-%02u%02u%02u-%lu", time.wYear, time.wMonth, time.wDay,
        time.wHour, time.wMinute, time.wSecond, GetCurrentProcessId());
    const std::wstring run = base + L"\\fixtures\\" + suffix;
    const std::wstring populated = run + L"\\peuple", empty = run + L"\\vide";
    const std::wstring logPath = base + L"\\logs\\" + suffix + L".log";
    if (_wfopen_s(&journal, logPath.c_str(), L"w, ccs=UTF-8") != 0) return 3;
    log(L"FileSpacer native Shell state/selection/icons/filtering/lifetime 2g; production host/settings/selection/filter/SQLite; PID=%lu", GetCurrentProcessId());
    if (diagnostic) log(L"DIAGNOSTIC 2g: existing exact state checks, immediate host teardown and native view recreation");
    log(L"LOG: %s", logPath.c_str());
    log(L"No private control access, PowerShell, UIA, polling or production database.");
    int exitCode = 3;
    if (directory(run) && directory(populated) && directory(empty)) {
        bool created = true;
        for (int index = 0; index < 150 && created; ++index) {
            const std::wstring file = populated + L"\\élément-" + std::to_wstring(index) + L".txt";
            HANDLE handle = CreateFile(file.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (handle == INVALID_HANDLE_VALUE) created = false;
            else { DWORD written = 0; created = WriteFile(handle, "x", 1, &written, nullptr) && written == 1; if (!CloseHandle(handle)) created = false; }
        }
        const HRESULT ole = OleInitialize(nullptr);
        if (created && SUCCEEDED(ole)) {
            initDPI();
            try {
                CComPtr<StateProbe> host;
                host.Attach(new StateProbe(populated, empty, utf8(run + L"\\state.sqlite3"), diagnostic));
                if (host->start()) {
                    MSG message = {}; BOOL received;
                    while ((received = GetMessage(&message, nullptr, 0, 0)) > 0)
                        if (!host->translate(&message)) { TranslateMessage(&message); DispatchMessage(&message); }
                    exitCode = received < 0 ? 3 : static_cast<int>(message.wParam);
                }
                host->shutdown();
                if (host->exitCode == 3) exitCode = 3;
                if (host->window) DestroyWindow(host->window);
            } catch (const std::bad_alloc &) { log(L"FAIL: allocation failed"); }
        } else log(L"FAIL: fixture creation or OleInitialize; HRESULT=0x%08lX", static_cast<unsigned long>(ole));
        if (SUCCEEDED(ole)) OleUninitialize();
    } else log(L"FAIL: fixture directories");
    log(L"PROCESS EXIT: %d", exitCode);
    fclose(journal); journal = nullptr;
    return exitCode;
}
