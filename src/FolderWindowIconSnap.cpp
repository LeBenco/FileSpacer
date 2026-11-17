// SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
// Based on ChromaFiler, Copyright (C) 2023 J. van't Hoog.
#include <common.h>
#include "FolderWindow.h"
#include "FolderWindowInternal.h"
#include "ShellViewOperations.h"
#include "ShellViewIcons.h"
#include "MixedFolder.h"
#include "GeomUtils.h"
#include "GDIUtils.h"
#include "WinUtils.h"
#include "UIStrings.h"
#include "DPI.h"
#include <commctrl.h>
#include <oleacc.h>
#include <algorithm>
#include <map>
#include <set>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace filespacer {

// Observe only a left-button gesture that began on an icon. The thread hook
// also sees the messages consumed by the Shell's nested drag/drop loop.
struct FolderWindow::IconSnap {
    static thread_local IconSnap *active;
    FolderWindow &owner;
    CComPtr<IShellView> view;
    const ULONG_PTR generation, serial;
    const DWORD placementFlags;
    POINT start, drop = {};
    HWND body = nullptr;
    bool moved = false, finished = false, dropped = false, snap = false, heldS = false, placement = false;
    CComPtr<IIconDropPolicySite> dropSite;
    // A native target can reenter after the source gesture was finalized.
    std::shared_ptr<HRESULT> dropResult;
    PreparedIconPositions movingPositions;
    ShellIconGrid grid;
    CComHeapPtr<ITEMID_CHILD> originalFocus;
    int focusIndex = -1, nativeDropIndex = -1;
    struct ImageTarget { RECT bounds, itemBounds; int index; };
    std::map<std::pair<LONG,LONG>, ImageTarget> images;
    std::vector<bool> movingItems;
    bool pointsCaptured = false;
    FOLDERVIEWMODE previewMode = FVM_AUTO;
    HHOOK hook = nullptr;
    std::unique_ptr<std::remove_pointer<HHOOK>::type, decltype(&UnhookWindowsHookEx)> keyboardHook{nullptr, UnhookWindowsHookEx};
    bool markerPending = false;
    HWND list = nullptr, marker = nullptr;
    int anchor = -1, iconSize = 0, itemCount = 0;
    POINT press = {}, anchorPoint = {}, spacing = {}, iconOffset = {};
    std::vector<int> orderedItems, ranks, selectedBefore;
    RECT layoutClient = {};
    RECT markerRect = {}, shownRect = {};
    bool prepared = false, previewReady = false;
    std::vector<POINT> dragged;
    std::vector<POINT> initialPoints;
    std::vector<RECT> dropTargets;
    std::vector<std::pair<RECT,int>> itemTargets;
    POINT targetOrigin = {};
    RECT targetClient = {};
    bool targetsReady = false;
    std::vector<std::pair<LONG,LONG>> destinations;
    std::set<std::pair<LONG,LONG>> stationary;
#ifdef FILESPACER_DEBUG
    ULONGLONG previewTicks = 0, previewMaximum = 0, previewSetupTicks = 0;
    unsigned previewUpdates = 0;
#endif

    IconSnap(FolderWindow &window, POINT point, HWND root, int itemIndex, DWORD flags) : owner(window), view(window.shellView),
        generation(window.viewGeneration), serial(++window.iconSnapSerial), placementFlags(flags), start(point), body(root), anchor(itemIndex) {
        heldS = GetAsyncKeyState(L'S') < 0;
        // Learn the control from the actual input target, never a child search.
        const HWND target = WindowFromPoint(point);
        wchar_t type[32] = {};
        POINT origin = {};
        if (target && IsChild(body, target) && GetClassNameW(target, type, _countof(type))
                && lstrcmpW(type, WC_LISTVIEWW) == 0 && ListView_GetOrigin(target, &origin)) {
            const POINT position = screenToClient(target, point);
            RECT icon = {}, label = {};
            if (anchor >= 0 && anchor < ListView_GetItemCount(target) && ListView_GetItemPosition(target, anchor, &anchorPoint)
                    && ListView_GetItemRect(target, anchor, &icon, LVIR_ICON) && ListView_GetItemRect(target, anchor, &label, LVIR_LABEL)
                    && (PtInRect(&icon, position) || PtInRect(&label, position))) {
                list = target;
                press = {position.x + origin.x, position.y + origin.y};
            }
        }
    }
    ~IconSnap() { stop(); }
    bool permanent() const { return (placementFlags & (FWF_AUTOARRANGE | FWF_SNAPTOGRID)) != 0; }
    bool automatic() const { return (placementFlags & FWF_AUTOARRANGE) != 0; }
    bool current() const {
        return owner.isWindowOperational() && owner.shellView == view
            && owner.viewGeneration == generation;
    }
    void releaseDropPolicy(HRESULT outcome) {
        if (dropResult) { *dropResult = outcome; dropResult.reset(); }
        if (dropSite) { dropSite->SetIconDropPolicy(nullptr); dropSite.Release(); }
    }
    void finishDrop() {
        // The native drag has ended and positioning has been attempted. No
        // cached target may retain this gesture's transfer veto afterward.
        if (hook) { UnhookWindowsHookEx(hook); hook = nullptr; }
        releaseDropPolicy(S_FALSE);
    }
    void stop() {
        finished = true;
        markerPending = false;
        if (active == this) active = nullptr;
        if (hook) { UnhookWindowsHookEx(hook); hook = nullptr; }
        keyboardHook.reset();
        releaseDropPolicy(E_ABORT);
        if (marker && GetPropW(marker, L"FileSpacer.SnapMarker") == this) DestroyWindow(marker);
        marker = nullptr;
#ifdef FILESPACER_DEBUG
        if (previewUpdates) {
            LARGE_INTEGER frequency = {}; QueryPerformanceFrequency(&frequency);
            debugPrintf(L"GRID PREVIEW updates=%u total_us=%llu max_us=%llu setup_us=%llu\n", previewUpdates,
                previewTicks * 1000000 / frequency.QuadPart, previewMaximum * 1000000 / frequency.QuadPart,
                previewSetupTicks * 1000000 / frequency.QuadPart);
            previewUpdates = 0;
        }
#endif
    }
    void hideMarker() {
        if (marker) ShowWindow(marker, SW_HIDE);
        SetRectEmpty(&shownRect);
    }
    void queueMarker() {
        if (!markerPending && current() && !finished)
            markerPending = PostMessageW(owner.hwnd, MSG_ICON_SNAP, serial, 1) != FALSE;
    }
    struct Policy : IIconDropPolicy {
        LONG references = 1;
        const DWORD thread = GetCurrentThreadId();
        std::weak_ptr<IconSnap> gesture;
        const std::shared_ptr<HRESULT> result;
        explicit Policy(const std::shared_ptr<IconSnap> &value) : gesture(value), result(value->dropResult) {}
        STDMETHODIMP QueryInterface(REFIID iid, void **out) override {
            if (!out) return E_POINTER;
            *out = nullptr;
            if (iid != IID_IUnknown && iid != __uuidof(IIconDropPolicy)) return E_NOINTERFACE;
            *out = static_cast<IIconDropPolicy *>(this); AddRef(); return S_OK;
        }
        STDMETHODIMP_(ULONG) AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&references)); }
        STDMETHODIMP_(ULONG) Release() override {
            const ULONG count = static_cast<ULONG>(InterlockedDecrement(&references));
            if (!count) delete this;
            return count;
        }
        STDMETHODIMP Placement(PCUITEMID_CHILD target, POINTL point, DWORD keys) override {
            if (thread != GetCurrentThreadId()) return E_ABORT;
            if (*result != E_PENDING) {
                return *result;
            }
            auto session = gesture.lock();
            if (!session) return E_ABORT;
            CComPtr<ItemWindow> keepAlive(&session->owner);
            return session->placementForTarget(target, {point.x, point.y}, keys);
        }
    };
    bool capturePoints() {
        if (pointsCaptured) return current() && ListView_GetItemCount(list) == itemCount;
        if (!list || !current()) return false;
        itemCount = ListView_GetItemCount(list);
        if (anchor < 0 || anchor >= itemCount) return false;
        initialPoints.clear(); movingItems.clear();
        initialPoints.reserve(static_cast<size_t>(itemCount)); movingItems.reserve(static_cast<size_t>(itemCount));
        for (int i = 0; i < itemCount; ++i) {
            POINT point = {};
            if (!ListView_GetItemPosition(list, i, &point) || !current()) return false;
            initialPoints.push_back(point);
            movingItems.push_back((ListView_GetItemState(list, i, LVIS_SELECTED) & LVIS_SELECTED) != 0);
        }
        focusIndex = ListView_GetNextItem(list, -1, LVNI_FOCUSED);
        pointsCaptured = movingItems[anchor];
        return pointsCaptured;
    }
    void clearDropHighlight() {
        if (!list || !current()) return;
        int index = ListView_GetNextItem(list, -1, LVNI_DROPHILITED);
        if (index < 0) {
            POINT cursor = {};
            if (!GetCursorPos(&cursor)) return;
            overImage(cursor); // Refresh the visible rectangle cache if needed.
            const POINT point = screenToClient(list, cursor);
            for (const auto &target : itemTargets) {
                if (PtInRect(&target.first, point)
                        && (ListView_GetItemState(list, target.second, LVIS_DROPHILITED) & LVIS_DROPHILITED)) {
                    index = target.second; break;
                }
            }
            if (index < 0) return;
        }
        // Clearing the visual state leaves the Shell's target cache intact.
        // Remember its valid feedback so releasing S can restore it in place.
        nativeDropIndex = index;
        if (current()) ListView_SetItemState(list, index, 0, LVIS_DROPHILITED);
    }
    bool overImage(POINT cursor) {
        if (!list) return false;
        POINT origin = {}, point = screenToClient(list, cursor); RECT client = {};
        if (!ListView_GetOrigin(list, &origin) || !GetClientRect(list, &client)) return false;
        if (!targetsReady || origin.x != targetOrigin.x || origin.y != targetOrigin.y || !EqualRect(&client, &targetClient)) {
            dropTargets.clear(); itemTargets.clear();
            for (const auto &image : images) {
                if (movingItems[image.second.index]) continue;
                RECT rectangle = image.second.bounds, visible = {};
                OffsetRect(&rectangle, -origin.x, -origin.y);
                if (IntersectRect(&visible, &rectangle, &client)) dropTargets.push_back(rectangle);
                rectangle = image.second.itemBounds;
                OffsetRect(&rectangle, -origin.x, -origin.y);
                if (IntersectRect(&visible, &rectangle, &client)) itemTargets.emplace_back(rectangle, image.second.index);
            }
            targetsReady = true; targetOrigin = origin; targetClient = client;
        }
        return std::any_of(dropTargets.begin(), dropTargets.end(), [&](const RECT &rectangle) { return PtInRect(&rectangle, point) != FALSE; });
    }
    HRESULT placementForTarget(PCUITEMID_CHILD, POINT cursor, DWORD keys) try {
        if (!current() || (finished && !dropped)) return E_ABORT;
        if (permanent()) return S_FALSE;
        const bool force = finished ? snap : heldS;
        if (!force && ((keys & MK_CONTROL) || GetKeyState(VK_MENU) < 0)) return S_FALSE;
        if (!prepared) previewReady = preparePreview();
        if (!previewReady || !current()) return E_ABORT;
        const bool place = force || !overImage(cursor);
        if (place) clearDropHighlight();
        return place ? S_OK : S_FALSE;
    } catch (const std::bad_alloc &) { return E_OUTOFMEMORY; }
    bool applyPlacement() try {
        if (!prepared) previewReady = preparePreview();
        if (!previewReady || !current() || ListView_GetItemCount(list) != itemCount) return false;
        POINT origin = {}, point = screenToClient(list, drop);
        if (!ListView_GetOrigin(list, &origin)) return false;
        const LONGLONG dx = static_cast<LONGLONG>(point.x) + origin.x - press.x;
        const LONGLONG dy = static_cast<LONGLONG>(point.y) + origin.y - press.y;
        movingPositions.items.clear(); movingPositions.points.clear();
        for (const auto &position : movingPositions.positions) {
            const LONGLONG x = position.point.x + dx, y = position.point.y + dy;
            if (x < LONG_MIN || x > LONG_MAX || y < LONG_MIN || y > LONG_MAX) return false;
            POINT destination = {static_cast<LONG>(x), static_cast<LONG>(y)};
            movingPositions.items.push_back(position.id); movingPositions.points.push_back(destination);
        }
        movingPositions.ready = true;
        auto valid = [&]() { return current(); };
        ++owner.navigationDepth;
        owner.restoringState = true;
        HRESULT result = snap && previewMode == FVM_ICON ? grid.apply(movingPositions, valid)
            : applyIconPositions(movingPositions.view, movingPositions, valid);
        if (!current()) { owner.resumeDeferredClose(); return false; }
        CComQIPtr<IFolderView2> folder(view);
        if (SUCCEEDED(result) && current() && folder) {
            int selectedCount = 0;
            result = folder->ItemCount(SVGIO_SELECTION, &selectedCount);
            bool changed = selectedCount != static_cast<int>(movingPositions.items.size());
            if (SUCCEEDED(result) && current() && !changed) for (const auto id : movingPositions.items) {
                DWORD state = 0;
                result = folder->GetSelectionState(id, &state);
                if (FAILED(result) || !current()) break;
                if (!(state & SVSI_SELECT)) { changed = true; break; }
            }
            if (SUCCEEDED(result) && current() && changed) {
                result = owner.selectionClear.clear(view, valid);
                if (SUCCEEDED(result) && current())
                    result = folder->SelectAndPositionItems(static_cast<UINT>(movingPositions.items.size()),
                        movingPositions.items.data(), movingPositions.points.data(), SVSI_POSITIONITEM | SVSI_SELECT | SVSI_NOTAKEFOCUS);
            }
            int focused = -1;
            if (SUCCEEDED(result) && current() && originalFocus && SUCCEEDED(folder->GetFocusedItem(&focused))
                    && current() && focused != focusIndex) {
                DWORD state = 0;
                result = folder->GetSelectionState(originalFocus, &state);
                if (SUCCEEDED(result) && current())
                    result = view->SelectItem(originalFocus, (state & SVSI_SELECT) | SVSI_FOCUSED | SVSI_NOTAKEFOCUS);
            }
        }
        if (current()) owner.restoringState = false;
        owner.resumeDeferredClose();
        return current() && checkHR(result);
    } catch (const std::bad_alloc &) { return false; }
    bool snapKey(bool down) {
        if (permanent() || (finished && !heldS)) return false;
        const bool changed = down != heldS;
        heldS = down;
        if (finished && !down) PostMessageW(owner.hwnd, MSG_ICON_SNAP, serial, 0);
        else if (changed) queueMarker();
        return true;
    }
    static LRESULT CALLBACK keys(int code, WPARAM key, LPARAM state) {
        if ((code == HC_ACTION || code == HC_NOREMOVE) && (key == L'S' || key == VK_ESCAPE) && active) {
            auto session = active->owner.iconSnap;
            if (session && session->current()) {
                const bool down = !(static_cast<ULONG_PTR>(state) & (ULONG_PTR(1) << 31));
                if (key == VK_ESCAPE && down && !session->finished) {
                    session->snap = false;
                    session->finished = true;
                    PostMessageW(session->owner.hwnd, MSG_ICON_SNAP, session->serial, 0);
                } else if (key == L'S' && session->snapKey(down)) {
                    // Keep this press, including repeats after the drop, out of
                    // the Shell's type-to-select until its matching release.
                    return 1;
                }
            }
        }
        return CallNextHookEx(nullptr, code, key, state);
    }
    bool preparePreview() {
        prepared = true;
        if (!list || !IsWindow(list) || !IsChild(body, list) || !current()) return false;
        CComQIPtr<IFolderView2> folder(view);
        FOLDERVIEWMODE mode = FVM_AUTO; DWORD flags = 0; UINT areas = UINT_MAX;
        const auto style = GetWindowLongPtrW(list, GWL_STYLE);
        if (!folder || FAILED(folder->GetViewModeAndIconSize(&mode, &iconSize)) || !current()
                || !iconPlacementMode(mode)
                || FAILED(folder->GetSpacing(&spacing)) || !current()
                || iconSize <= 0 || iconSize > MAXSHORT / 3 || spacing.x < iconSize || spacing.x > MAXSHORT / 3 || spacing.y <= 0
                || FAILED(folder->GetCurrentFolderFlags(&flags)) || !current()
                || (flags & (FWF_DESKTOP | FWF_NOSCROLL))
                || (flags & (FWF_AUTOARRANGE | FWF_SNAPTOGRID)) != placementFlags
                || (style & (LVS_NOSCROLL | LVS_ALIGNMASK))
                || (GetWindowLongPtrW(list, GWL_EXSTYLE) & WS_EX_LAYOUTRTL)
                || ListView_IsGroupViewEnabled(list)) return false;
        ListView_GetNumberOfWorkAreas(list, &areas);
        if (areas || !current()) return false;
        if (mode == FVM_ICON) {
            const HIMAGELIST icons = ListView_GetImageList(list, LVSIL_NORMAL);
            int width = 0, height = 0;
            if (!icons || !ImageList_GetIconSize(icons, &width, &height) || !current()
                    || width <= 0 || height != width || width > MAXSHORT / 3 || spacing.x < width) return false;
            iconSize = width;
        }
        if (!capturePoints()) return false;
        previewMode = mode;
        movingPositions.view = folder;
        if (focusIndex >= 0 && (FAILED(folder->Item(focusIndex, &originalFocus)) || !current())) return false;
        POINT snapshotOrigin = {};
        if (!ListView_GetOrigin(list, &snapshotOrigin)) return false;
        // One snapshot per gesture. No folder enumeration or native alignment
        // is performed on mouse movement, including after releasing/repressing S.
        for (int i = 0; i < itemCount; ++i) {
            const POINT point = initialPoints[i];
            const bool moving = movingItems[i];
            if (moving) {
                dragged.push_back(point);
                if (!permanent()) {
                    movingPositions.positions.emplace_back();
                    auto &position = movingPositions.positions.back();
                    if (FAILED(folder->Item(i, &position.id)) || !current()) return false;
                    position.point = point;
                }
            } else if (permanent() && !automatic()) stationary.emplace(point.x, point.y);
            if (!permanent()) {
                RECT image = {}, itemBounds = {};
                if (!ListView_GetItemRect(list, i, &itemBounds, LVIR_SELECTBOUNDS) || !current()) return false;
                OffsetRect(&itemBounds, snapshotOrigin.x, snapshotOrigin.y);
                if (mode == FVM_ICON) {
                    if (point.x > LONG_MAX - iconSize || point.y > LONG_MAX - iconSize) return false;
                    image = {point.x, point.y, point.x + iconSize, point.y + iconSize};
                } else {
                    if (!ListView_GetItemRect(list, i, &image, LVIR_ICON) || !current()) return false;
                    OffsetRect(&image, snapshotOrigin.x, snapshotOrigin.y);
                }
                const auto inserted = images.emplace(std::make_pair(point.x, point.y), ImageTarget{image, itemBounds, i});
                if (!inserted.second && !moving) inserted.first->second = ImageTarget{image, itemBounds, i};
            }
        }
        POINT actualAnchor = {}; RECT icon = {};
        if (!GetClientRect(list, &layoutClient) || !ListView_GetItemRect(list, anchor, &icon, LVIR_ICON)
                || !ListView_GetItemPosition(list, anchor, &actualAnchor) || !current()) return false;
        iconOffset = {icon.left + snapshotOrigin.x - actualAnchor.x, icon.top + snapshotOrigin.y - actualAnchor.y};
        if (!permanent() && mode == FVM_ICON) iconOffset = {};
        if (automatic()) {
            orderedItems.reserve(static_cast<size_t>(itemCount));
            for (int i = 0; i < itemCount; ++i) orderedItems.push_back(i);
            std::sort(orderedItems.begin(), orderedItems.end(), [&](int a, int b) {
                const POINT first = initialPoints[a], second = initialPoints[b];
                return first.y != second.y ? first.y < second.y : first.x < second.x;
            });
            ranks.resize(static_cast<size_t>(itemCount));
            selectedBefore.push_back(0);
            for (int i = 0; i < itemCount; ++i) {
                const int itemIndex = orderedItems[i];
                ranks[itemIndex] = i;
                selectedBefore.push_back(selectedBefore.back() + (movingItems[itemIndex] ? 1 : 0));
            }
        }
        dropTargets.reserve(initialPoints.size() * 2);
        destinations.reserve(dragged.size());
        return current();
    }
    bool prepareMarker() {
        // Prepare one icon-sized surface per gesture. Windows retains its pixels
        // while it moves; no repaint or bitmap allocation is needed on motion.
        marker = CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW, WC_STATICW, L"",
            WS_POPUP | WS_DISABLED, 0, 0, iconSize, iconSize, owner.hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (!marker) return false;
        if (!SetPropW(marker, L"FileSpacer.SnapMarker", this)) { DestroyWindow(marker); marker = nullptr; return false; }
        std::unique_ptr<std::remove_pointer<HDC>::type, decltype(&DeleteDC)> dc(CreateCompatibleDC(nullptr), DeleteDC);
        BITMAPINFO info = {{sizeof(BITMAPINFOHEADER), iconSize, -iconSize, 1, 32, BI_RGB}};
        void *pixels = nullptr;
        std::unique_ptr<std::remove_pointer<HBITMAP>::type, decltype(&DeleteObject)> bitmap(
            dc ? CreateDIBSection(dc.get(), &info, DIB_RGB_COLORS, &pixels, nullptr, 0) : nullptr, DeleteObject);
        if (!bitmap || !pixels) return false;
        const int border = (std::max)(1, scaleDPI(1));
        const COLORREF outline = GetSysColor(COLOR_GRAYTEXT);
        for (int y = 0; y < iconSize; ++y) for (int x = 0; x < iconSize; ++x) {
            const bool edge = x < border || y < border || x >= iconSize - border || y >= iconSize - border;
            const DWORD alpha = edge ? 255u : 128u;
            const COLORREF colour = edge ? outline : RGB(224, 224, 224);
            // UpdateLayeredWindow requires premultiplied BGRA pixels.
            static_cast<DWORD *>(pixels)[y * iconSize + x] = (alpha << 24)
                | ((GetRValue(colour) * alpha / 255u) << 16)
                | ((GetGValue(colour) * alpha / 255u) << 8) | (GetBValue(colour) * alpha / 255u);
        }
        const HGDIOBJ previous = SelectObject(dc.get(), bitmap.get());
        if (!previous || previous == HGDI_ERROR) return false;
        POINT origin = {}; SIZE size = {iconSize, iconSize};
        BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        const bool drawn = UpdateLayeredWindow(marker, nullptr, nullptr, &size, dc.get(), &origin, 0, &blend, ULW_ALPHA) != FALSE;
        SelectObject(dc.get(), previous);
        if (!drawn) return false;
        markerRect = {0, 0, iconSize, iconSize};
        auto region = std::unique_ptr<std::remove_pointer<HRGN>::type, decltype(&DeleteObject)>(
            CreateRectRgnIndirect(&markerRect), DeleteObject);
        if (!region || !SetWindowRgn(marker, region.get(), FALSE)) return false;
        region.release(); // The native window owns its clipping region.
        return current();
    }
    bool overDropTarget(POINT cursor, POINT origin, const RECT &client) {
        if (!targetsReady || origin.x != targetOrigin.x || origin.y != targetOrigin.y || !EqualRect(&client, &targetClient)) {
            dropTargets.clear();
            // LVM_HITTEST traverses the whole free-position view. Cache the
            // native visible icon/label rectangles for subsequent mouse moves
            // instead. Off-screen points need no native rectangle query.
            for (size_t i = 0; i < initialPoints.size(); ++i) {
                const LONGLONG x = static_cast<LONGLONG>(initialPoints[i].x) - origin.x;
                const LONGLONG y = static_cast<LONGLONG>(initialPoints[i].y) - origin.y;
                if (static_cast<int>(i) != anchor && (x < client.left - 2LL * spacing.x || x > client.right + 2LL * spacing.x
                        || y < client.top - 2LL * spacing.y || y > client.bottom + 2LL * spacing.y)) continue;
                for (int part : {LVIR_ICON, LVIR_LABEL}) {
                    RECT rectangle = {};
                    if (!ListView_GetItemRect(list, static_cast<int>(i), &rectangle, part) || !current()) {
                        previewReady = false; return true;
                    }
                    dropTargets.push_back(rectangle);
                }
            }
            targetOrigin = origin; targetClient = client; targetsReady = true;
        }
        return std::any_of(dropTargets.begin(), dropTargets.end(), [&](const RECT &rectangle) {return PtInRect(&rectangle, cursor) != FALSE;});
    }
    void updateMarker(POINT cursor) try {
#ifdef FILESPACER_DEBUG
        LARGE_INTEGER began = {}, ended = {}; QueryPerformanceCounter(&began);
        struct Timing {
            IconSnap &session; LARGE_INTEGER &began, &ended;
            bool preparing = false;
            ~Timing() {
                QueryPerformanceCounter(&ended);
                const ULONGLONG ticks = ended.QuadPart - began.QuadPart;
                if (preparing) { session.previewSetupTicks += ticks; return; }
                session.previewTicks += ticks;
                session.previewMaximum = (std::max)(session.previewMaximum, ticks);
                ++session.previewUpdates;
            }
        } timing{*this, began, ended};
#endif
        if (!moved || finished || !current() || !list || WindowFromPoint(cursor) != list
                || (permanent() && (GetKeyState(VK_CONTROL) < 0 || GetKeyState(VK_MENU) < 0))) {
            hideMarker(); return;
        }
        if (!prepared) {
#ifdef FILESPACER_DEBUG
            timing.preparing = true;
#endif
            previewReady = preparePreview();
        }
        if (!previewReady || !current() || ListView_GetItemCount(list) != itemCount) { hideMarker(); return; }
        if (!permanent()) {
            if (heldS || !overImage(cursor)) clearDropHighlight();
            else if (nativeDropIndex >= 0 && nativeDropIndex < itemCount) {
                const POINT point = initialPoints[nativeDropIndex];
                const auto found = images.find({point.x, point.y});
                POINT origin = {}, position = screenToClient(list, cursor);
                if (found != images.end() && ListView_GetOrigin(list, &origin)) {
                    position.x += origin.x; position.y += origin.y;
                    if (PtInRect(&found->second.bounds, position))
                        ListView_SetItemState(list, nativeDropIndex, LVIS_DROPHILITED, LVIS_DROPHILITED);
                }
            }
            if (!heldS || previewMode != FVM_ICON) {
                hideMarker();
                if (!heldS && !overImage(cursor) && GetKeyState(VK_CONTROL) >= 0 && GetKeyState(VK_MENU) >= 0)
                    SetCursor(LoadCursorW(nullptr, IDC_ARROW));
                return;
            }
            SetCursor(LoadCursorW(nullptr, IDC_ARROW));
        }
        if (!marker && !prepareMarker()) { hideMarker(); return; }
        POINT origin = {}; const POINT position = screenToClient(list, cursor); RECT client = {};
        if (!ListView_GetOrigin(list, &origin) || !GetClientRect(list, &client)
                || (permanent() && (automatic() ? !EqualRect(&client, &layoutClient) : overDropTarget(position, origin, client)))) {
            hideMarker(); return;
        }
        const LONGLONG dx = static_cast<LONGLONG>(position.x) + origin.x - press.x;
        const LONGLONG dy = static_cast<LONGLONG>(position.y) + origin.y - press.y;
        const auto destination = [&](POINT &point) {
            const LONGLONG x = point.x + dx, y = point.y + dy;
            if (x < LONG_MIN || x > LONG_MAX || y < LONG_MIN || y > LONG_MAX) return false;
            if (permanent()) {
                // The existing grid supplies its phase in every icon-based view.
                // Round the drag delta, keeping each icon's original grid slot.
                const LONGLONG gridX = dx + (dx < 0 ? -(spacing.x / 2) : spacing.x / 2);
                const LONGLONG gridY = dy + (dy < 0 ? -(spacing.y / 2) : spacing.y / 2);
                const LONGLONG alignedX = point.x + gridX - gridX % spacing.x;
                const LONGLONG alignedY = point.y + gridY - gridY % spacing.y;
                if (alignedX < LONG_MIN || alignedX > LONG_MAX || alignedY < LONG_MIN || alignedY > LONG_MAX) return false;
                point = {static_cast<LONG>(alignedX), static_cast<LONG>(alignedY)};
                return true;
            }
            point = {static_cast<LONG>(x), static_cast<LONG>(y)};
            return grid.align(point);
        };
        POINT point = anchorPoint;
        if (automatic()) {
            // Read the insertion decision already made by the Shell. Item indexes
            // do not change when Windows rearranges their positions.
            LVINSERTMARK insertion = {sizeof(insertion)};
            if (!ListView_GetInsertMark(list, &insertion) || insertion.iItem < 0 || insertion.iItem >= itemCount) {
                hideMarker(); return;
            }
            const int cut = ranks[insertion.iItem] + ((insertion.dwFlags & LVIM_AFTER) ? 1 : 0);
            const int slot = cut - selectedBefore[cut] + selectedBefore[ranks[anchor]];
            POINT actual = {};
            if (slot < 0 || slot >= itemCount || !ListView_GetItemPosition(list, insertion.iItem, &actual)
                    || actual.x != initialPoints[insertion.iItem].x || actual.y != initialPoints[insertion.iItem].y) {
                hideMarker(); return;
            }
            point = initialPoints[orderedItems[slot]];
        } else if (permanent()) {
            destinations.clear();
            for (POINT target : dragged) {
                if (!destination(target) || stationary.count({target.x, target.y})) { hideMarker(); return; }
                destinations.emplace_back(target.x, target.y);
            }
            std::sort(destinations.begin(), destinations.end());
            if (std::adjacent_find(destinations.begin(), destinations.end()) != destinations.end()
                    || !destination(point)) { hideMarker(); return; }
        } else if (!destination(point)) { hideMarker(); return; }
        // Frame the icon's destination, preserving its original cursor offset
        // and accounting for native scrolling.
        const int width = rectWidth(markerRect), height = rectHeight(markerRect);
        const LONGLONG horizontal = static_cast<LONGLONG>(point.x) + iconOffset.x - origin.x;
        const LONGLONG vertical = static_cast<LONGLONG>(point.y) + iconOffset.y - origin.y;
        if (horizontal < LONG_MIN || horizontal > LONG_MAX - width || vertical < LONG_MIN || vertical > LONG_MAX - height
                || horizontal - markerRect.left < INT_MIN || horizontal - markerRect.left > INT_MAX
                || vertical - markerRect.top < INT_MIN || vertical - markerRect.top > INT_MAX) { hideMarker(); return; }
        const int x = static_cast<int>(horizontal), y = static_cast<int>(vertical);
        RECT next = {x, y, x + width, y + height}, clipped = {};
        if (!IntersectRect(&clipped, &client, &next) || !EqualRect(&clipped, &next)) { hideMarker(); return; }
        if (!EqualRect(&next, &shownRect)) {
            MapWindowPoints(list, nullptr, reinterpret_cast<POINT *>(&next), 2);
            if (!SetWindowPos(marker, HWND_TOPMOST, next.left - markerRect.left, next.top - markerRect.top, 0, 0,
                    SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW)) { hideMarker(); return; }
            shownRect = {x, y, x + width, y + height};
        }
    } catch (const std::bad_alloc &) {
        previewReady = false; hideMarker();
    }
    static LRESULT CALLBACK messages(int code, WPARAM removed, LPARAM data) {
        if (code == HC_ACTION && removed == PM_REMOVE && active) {
            auto session = active->owner.iconSnap;
            if (session) {
                CComPtr<ItemWindow> keepAlive(&session->owner);
                auto message = reinterpret_cast<MSG *>(data);
                if (!session->current()) {
                    session->stop();
                } else if (!session->finished) {
                    if (message->message == WM_MOUSEMOVE || message->message == WM_LBUTTONUP) {
                        session->moved = session->moved
                            || std::abs(message->pt.x - session->start.x) >= GetSystemMetrics(SM_CXDRAG)
                            || std::abs(message->pt.y - session->start.y) >= GetSystemMetrics(SM_CYDRAG);
                    }
                    if (session->moved && !session->pointsCaptured) {
                        try { session->capturePoints(); } catch (const std::bad_alloc &) { session->stop(); }
                    }
                    if ((message->message == WM_KEYDOWN && message->wParam == VK_ESCAPE)
                            || message->message == WM_CANCELMODE) {
                        session->stop();
                    } else if (message->message == WM_LBUTTONUP) {
                        session->hideMarker();
                        session->finished = true;
                        session->dropped = true;
                        session->drop = message->pt;
                        const HWND target = WindowFromPoint(session->drop);
                        const bool inside = target == session->body || IsChild(session->body, target);
                        session->snap = session->moved && session->heldS && !session->permanent();
                        session->placement = session->moved && session->list && session->pointsCaptured && !session->permanent() && inside
                            && (session->snap || (GetKeyState(VK_CONTROL) >= 0 && GetKeyState(VK_MENU) >= 0
                                && !session->overImage(session->drop)));
                        if (session->dropResult) *session->dropResult = session->placement || session->snap ? S_OK : S_FALSE;
                        if (session->placement || session->snap) {
                            // Finish the Shell drag as cancellation before it can
                            // transfer a file. Apply only icon coordinates later.
                            // A late translation must not deliver Escape's
                            // character to the list and clear its selection.
                            message->hwnd = session->owner.hwnd;
                            message->message = WM_KEYDOWN;
                            message->wParam = VK_ESCAPE;
                            message->lParam = 1;
                        }
                        // The native drop completes before the ordinary message
                        // loop handles this; never reposition inside the hook.
                        if (session->current())
                            PostMessageW(session->owner.hwnd, MSG_ICON_SNAP, session->serial, 0);
                    } else if (message->message == WM_MOUSEMOVE) {
                        session->queueMarker();
                    }
                }
            }
        }
        return CallNextHookEx(nullptr, code, removed, data);
    }
};

thread_local FolderWindow::IconSnap *FolderWindow::IconSnap::active = nullptr;

void FolderWindow::watchIconDrag(const MSG *message) {
    if (message->message != WM_LBUTTONDOWN) return;
    iconSnap.reset();
    if (!viewReady || !initialStateApplied || !enumerationComplete || restoringState
            || provisionalInputBlocked || provisionalCaptureBusy || viewMenuPreview || viewMenuRestore
            || !searchText.empty() || !shellView) return;
    CComPtr<ItemWindow> keepAlive(this);
    CComPtr<IShellView> source = shellView;
    const ULONG_PTR generation = viewGeneration;
    HWND body = nullptr;
    CComQIPtr<IFolderView2> view(source);
    DWORD flags = 0; FOLDERVIEWMODE mode = FVM_AUTO; int size = 0;
    int itemIndex = -1;
    if (!view || FAILED(source->GetWindow(&body)) || !body
            || (message->hwnd != body && !IsChild(body, message->hwnd))
            || FAILED(view->GetViewModeAndIconSize(&mode, &size)) || !iconPlacementMode(mode)
            || FAILED(view->GetCurrentFolderFlags(&flags))
            || (!(flags & FWF_AUTOARRANGE) && !spatialView(view))
            || gestureRoleAt(message->pt, &itemIndex) != ROLE_SYSTEM_LISTITEM
            || !isWindowOperational() || shellView != source || viewGeneration != generation) return;
    if (IconSnap::active) IconSnap::active->stop();
    auto session = std::make_shared<IconSnap>(*this, message->pt, body, itemIndex, flags & (FWF_AUTOARRANGE | FWF_SNAPTOGRID));
    if (mode == FVM_ICON && !(flags & (FWF_AUTOARRANGE | FWF_SNAPTOGRID)) && session->list) {
        ++navigationDepth;
        restoringState = true;
        const HRESULT result = session->grid.prepare(source, session->list, [&]() {
            return isWindowOperational() && shellView == source && viewGeneration == generation;
        });
        if (isWindowOperational() && shellView == source && viewGeneration == generation) restoringState = false;
        resumeDeferredClose();
        if (!session->current()) return;
        checkHR(result); // Keep the drop veto even if grid preparation fails.
    }
    session->hook = SetWindowsHookExW(WH_GETMESSAGE, IconSnap::messages, nullptr, GetCurrentThreadId());
    session->keyboardHook.reset(SetWindowsHookExW(WH_KEYBOARD, IconSnap::keys, nullptr, GetCurrentThreadId()));
    if (!session->hook || !session->keyboardHook) return;
    iconSnap = session;
    IconSnap::active = session.get();
    if (!session->permanent() && SUCCEEDED(view->GetFolder(IID_PPV_ARGS(&session->dropSite)))) {
        session->dropResult = std::make_shared<HRESULT>(E_PENDING);
        CComPtr<IIconDropPolicy> policy;
        policy.Attach(new(std::nothrow) IconSnap::Policy(session));
        if (policy) session->dropSite->SetIconDropPolicy(policy);
    }
}

void FolderWindow::realignIcons(bool selectedOnly) {
    if (!selectedOnly && (viewMenuPreview || viewMenuRestore)) {
        iconRealignPending = true;
        return;
    }
    if (!viewReady || !initialStateApplied || !enumerationComplete || restoringState
            || provisionalInputBlocked || provisionalCaptureBusy || viewMenuPreview || viewMenuRestore
            || !searchText.empty() || GetCapture()) return;
    iconRealignPending = false;
    CComPtr<ItemWindow> keepAlive(this);
    CComPtr<IShellView> source = shellView;
    const ULONG_PTR generation = viewGeneration;
    auto current = [&]() {
        return isWindowOperational() && shellView == source && viewGeneration == generation;
    };
    CComQIPtr<IFolderView2> view(source);
    if (!view || !current() || !spatialView(view) || !current()) return;
    ++navigationDepth;
    restoringState = true;
    struct Resume {
        FolderWindow &owner;
        decltype(current) &valid;
        ~Resume() {
            if (valid()) owner.restoringState = false;
            owner.resumeDeferredClose();
        }
    } resume{*this, current};
    const HRESULT result = alignIconsToGrid(source, selectedOnly, current);
    if (!current() || result == S_FALSE) return;
    if (!checkHR(result)) setStatusText(getErrorMessage(result).get());
    viewStateDirty(FolderState::Icons);
    SetTimer(hwnd, TIMER_SAVE_STATE, 250, nullptr);
}

LRESULT FolderWindow::handleIconSnap(WPARAM wParam, LPARAM lParam) {
        auto session = iconSnap;
        if (session && session->serial == wParam && lParam == 1) {
            session->markerPending = false;
            POINT cursor = {};
            if (!session->finished && session->current() && GetCursorPos(&cursor)) session->updateMarker(cursor);
            return 0;
        }
        if (session && session->serial == wParam && session->finished) {
            session->hideMarker();
            const bool place = session->placement, dropped = session->dropped;
            session->placement = session->dropped = false;
            if (session->current() && place && !GetCapture()) {
                if (session->applyPlacement() && session->snap && session->previewMode != FVM_ICON)
                    realignIcons(true); // Existing native support for Small icons and Tiles.
            }
            session->snap = false;
            session->finishDrop();
            if (!session->heldS || session->permanent()) {
                session->stop();
                if (iconSnap == session) iconSnap.reset();
            }
            if (session->current() && dropped && session->moved && !GetCapture())
                scheduleStateSave(true);
        }
        return 0;
    }

void FolderWindow::deactivateIconDrag() {
        if (iconSnap && iconSnap->placement) {
            // Preserve the accepted placement, as for WM_CANCELMODE. Its S
            // release can now go to another window, so retire the key hook.
            iconSnap->heldS = false;
            iconSnap->keyboardHook.reset();
        } else {
            auto gesture = std::move(iconSnap);
            if (gesture) gesture->stop();
        }
}

bool FolderWindow::hasPendingIconPlacement() const {
    return iconSnap && iconSnap->placement;
}
} // namespace filespacer
