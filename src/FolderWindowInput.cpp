// SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
// Based on ChromaFiler, Copyright (C) 2023 J. van't Hoog.
#include <common.h>
#include "FolderWindow.h"
#include "UIStrings.h"
#include "FolderWindowInternal.h"
#include "MixedFolder.h"
#include "ShellViewColumns.h"
#include "GeomUtils.h"
#include "WinUtils.h"
#include "Settings.h"
#include "Peek.h"
#include <windowsx.h>
#include <shlobj.h>
#include <oleacc.h>
#include <algorithm>
#include <cstdlib>
#include <memory>
#include <vector>

namespace filespacer {

struct ViewGestureTarget {
    CComPtr<IAccessible> accessible;
    CComVariant child;
    LONG role = 0;
};

// Both Shell engines expose logical items through MSAA. Modern Details can
// hit a read-only text cell inside that item rather than the item itself.
static void viewGestureTargetAt(POINT point, ViewGestureTarget &target) {
    if (FAILED(AccessibleObjectFromPoint(point, &target.accessible, &target.child))) return;
    for (unsigned depth = 0; depth < 8 && target.accessible; ++depth) {
        CComVariant role, state;
        target.role = 0;
        if (FAILED(target.accessible->get_accRole(target.child, &role)) || role.vt != VT_I4) break;
        target.role = role.lVal;
        if (target.role != ROLE_SYSTEM_TEXT || FAILED(target.accessible->get_accState(target.child, &state))
                || state.vt != VT_I4
                || (state.lVal != 0 && !(static_cast<DWORD>(state.lVal) & STATE_SYSTEM_READONLY))) break;
        // Simple children belong to this IAccessible; full objects expose a
        // separate parent. List labels can have no state flags; editable text
        // is excluded by the focused control's dialog code before hit testing.
        if (target.child.vt == VT_I4 && target.child.lVal != CHILDID_SELF) {
            target.child = CComVariant(CHILDID_SELF);
        } else {
            CComPtr<IDispatch> parent;
            if (FAILED(target.accessible->get_accParent(&parent)) || !parent) break;
            CComQIPtr<IAccessible> accessible(parent);
            if (!accessible) break;
            target.accessible = accessible;
            target.child = CComVariant(CHILDID_SELF);
        }
    }
}

// A consumed press never reaches the tooltip's native subclass. Relay it
// to the tooltip before resolving the underlying item, as a native press does.
static void relayViewTooltipPress(MSG message) {
    wchar_t type[32] = {};
    if (!GetClassNameW(message.hwnd, type, _countof(type)) || lstrcmpW(type, WC_LISTVIEWW) != 0) return;
    const HWND tooltip = ListView_GetToolTips(message.hwnd);
    if (!tooltip || !IsWindowVisible(tooltip)) return;
    if (message.message == WM_LBUTTONDBLCLK) message.message = WM_LBUTTONDOWN;
    else if (message.message == WM_RBUTTONDBLCLK) message.message = WM_RBUTTONDOWN;
    else if (message.message == WM_MBUTTONDBLCLK) message.message = WM_MBUTTONDOWN;
    SendMessageW(tooltip, TTM_RELAYEVENT, 0, reinterpret_cast<LPARAM>(&message));
}

LONG gestureRoleAt(POINT point, int *itemIndex) {
    if (itemIndex) *itemIndex = -1;
    const HWND input = WindowFromPoint(point);
    wchar_t type[32] = {};
    if (input && GetClassNameW(input, type, _countof(type)) && lstrcmpW(type, WC_LISTVIEWW) == 0
            && !ListView_IsGroupViewEnabled(input)) {
        // Read the actual input control before entering any accessibility RPC
        // that could dispatch a queued second click reentrantly.
        RECT client = {}; LVHITTESTINFO hit = {}; hit.pt = screenToClient(input, point);
        if (!GetClientRect(input, &client) || !PtInRect(&client, hit.pt)) return 0;
        const int index = ListView_HitTest(input, &hit);
        if (index >= 0 && (hit.flags & LVHT_ONITEM)) {
            if (itemIndex) *itemIndex = index;
            return ROLE_SYSTEM_LISTITEM;
        }
        return ROLE_SYSTEM_LIST;
    }
    ViewGestureTarget target;
    viewGestureTargetAt(point, target);
    if (itemIndex) {
        *itemIndex = -1;
        HWND window = nullptr;
        // Reuse the standard provider's one-based child as a candidate instead
        // of a second full hit-test. The preview checks its native bounds.
        if (target.role == ROLE_SYSTEM_LISTITEM && target.child.vt == VT_I4 && target.child.lVal > 0
                && SUCCEEDED(WindowFromAccessibleObject(target.accessible, &window)) && window == WindowFromPoint(point))
            *itemIndex = target.child.lVal - 1;
    }
    return target.role;
}

bool FolderWindow::handleTopLevelMessage(MSG *msg) {
    CComPtr<ItemWindow> messageKeepAlive(this);
    if (!isWindowOperational())
        return false;
    if (spacingPopup && (msg->hwnd == spacingPopup || IsChild(spacingPopup, msg->hwnd))) {
        if (msg->message == WM_KEYDOWN && msg->wParam == VK_ESCAPE) {
            readSpacingSlider(true);
            closeSpacingSlider();
            return true;
        }
        return false; // The trackbar owns its navigation keys and mouse capture.
    }
    if (handleAltMouseMessage(msg))
        return true;
    if (!isWindowOperational())
        return false;
    watchIconDrag(msg);
    if (!isWindowOperational()) return false;
    if (activationClickPending && msg->message >= WM_MOUSEFIRST && msg->message <= WM_MOUSELAST
            && msg->message != WM_MOUSEMOVE) {
        // Only the button-down that caused the activation is a candidate.
        activationClickPending = false;
        if (msg->message == WM_LBUTTONDOWN && isBackgroundActivationClick(msg))
            return true;
        if (!isWindowOperational())
            return false;
    }
    // A native rename keeps its keys and ordinary wheel. Ctrl+wheel is consumed
    // without zooming: native zoom would bypass the position-preserving path.
    // The search and address fields deliberately share the window shortcuts.
    auto renaming = [&]() {
        const CComPtr<IShellView> view = shellView;
        HWND root = nullptr;
        const HWND focus = GetFocus();
        if (!view || !focus || FAILED(view->GetWindow(&root)) || !root
                || (focus != root && !IsChild(root, focus))) return false;
        return (SendMessage(focus, WM_GETDLGCODE, 0, 0) & (DLGC_HASSETSEL | DLGC_WANTALLKEYS)) != 0;
    };
    bool editorKey = false;
    if (msg->message == WM_KEYDOWN && GetKeyState(VK_MENU) >= 0) {
        const bool control = GetKeyState(VK_CONTROL) < 0;
        const bool shift = GetKeyState(VK_SHIFT) < 0;
        const bool refreshKey = !shift && ((!control && msg->wParam == VK_F5)
            || (control && msg->wParam == L'R'));
        const bool windowKey = control && !shift && (msg->wParam == L'N' || msg->wParam == L'W');
        const bool modeKey = control && shift && msg->wParam >= L'1' && msg->wParam <= L'8';
        // Only a key typed in this window, outside a rename, is a shortcut.
        if ((refreshKey || windowKey || modeKey) && GetAncestor(msg->hwnd, GA_ROOT) == hwnd) {
            if (renaming()) {
                // The accelerator table holds the same refresh and window keys:
                // the editor keeps them too, and the Shell view decides.
                editorKey = true;
            } else {
                if (!isWindowOperational()) return false;
                if (refreshKey) onCommand(IDM_REFRESH);
                else if (windowKey) onCommand(msg->wParam == L'N' ? IDM_NEW_WINDOW : IDM_CLOSE_WINDOW);
                else changeViewMode(static_cast<int>(msg->wParam - L'1'));
                return true;
            }
        }
        if (!isWindowOperational()) return false;
    }
    if (msg->message == WM_MOUSEWHEEL && (GET_KEYSTATE_WPARAM(msg->wParam) & MK_CONTROL)
            && !(GET_KEYSTATE_WPARAM(msg->wParam) & MK_SHIFT) && GetKeyState(VK_MENU) >= 0) {
        CComPtr<IShellView> view = shellView;
        HWND root = nullptr;
        const HWND target = WindowFromPoint({GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam)});
        if (view && SUCCEEDED(view->GetWindow(&root)) && isWindowOperational() && shellView == view
                && root && (msg->hwnd == root || IsChild(root, msg->hwnd)
                    || target == root || IsChild(root, target))) {
            // A wheel message is at least one step, as in Explorer. Windows merges
            // the notches received while a change is applied into one message:
            // its delta counts them, and one change then takes them all.
            const int delta = GET_WHEEL_DELTA_WPARAM(msg->wParam);
            const int notches = delta / WHEEL_DELTA;
            const int steps = notches ? notches : (delta > 0) - (delta < 0);
            if (steps && !renaming()) changeViewMode(steps, true);
            return true;
        }
    }
    if (msg->message == WM_KEYDOWN && msg->wParam == L'T'
            && GetKeyState(VK_CONTROL) < 0 && GetKeyState(VK_SHIFT) < 0
            && GetKeyState(VK_MENU) >= 0) {
        // This window command also works while editing the address or filter.
        onCommand(IDM_REOPEN_WINDOW);
        return true;
    }
    if (msg->message == WM_KEYDOWN && GetKeyState(VK_MENU) >= 0
            && (((msg->wParam == L'F' || msg->wParam == L'E') && GetKeyState(VK_CONTROL) < 0
                    && GetKeyState(VK_SHIFT) >= 0)
                || (msg->wParam == VK_F3 && GetKeyState(VK_CONTROL) >= 0 && GetKeyState(VK_SHIFT) >= 0)))
        return searchBox.focus();
    if (searchBox.hasFocus()) {
        if (pathBar.handleTopLevelMessage(msg))
            return true;
        if (!isWindowOperational())
            return false;
        // Keep native edit shortcuts away from the Shell's file commands.
        return searchBox.handleTopLevelMessage(msg);
    }
    if (!editorKey && ItemWindow::handleTopLevelMessage(msg))
        return true;
    if (!isWindowOperational() || pathBar.hasFocus())
        return false; // The Shell view must not consume Ctrl+C/Ctrl+V from the address.
    if (provisionalInputBlocked) return false;
    if (activateOnShiftRelease && msg->message == WM_KEYUP && msg->wParam == VK_SHIFT) {
        CComPtr<IShellView> view = shellView;
        if (view)
            checkHR(view->UIActivate(SVUIA_ACTIVATE_FOCUS));
        activateOnShiftRelease = false;
        // don't return true
    }
    if (!isWindowOperational())
        return false;
    CComPtr<IShellView> view = shellView;
    if (!view)
        return false;
    if (msg->message == WM_KEYDOWN && msg->wParam == VK_BACK
            && GetKeyState(VK_MENU) >= 0 && GetKeyState(VK_SHIFT) >= 0) {
        const bool closeSource = settings::getKeepSourceWindowOpen() == (GetKeyState(VK_CONTROL) < 0);
        const ULONG_PTR generation = viewGeneration;
        const HWND focus = GetFocus();
        HWND root = nullptr;
        if (SUCCEEDED(view->GetWindow(&root)) && isWindowOperational() && shellView == view
                && viewGeneration == generation && root && focus == msg->hwnd
                && (focus == root || IsChild(root, focus))) {
            const LRESULT code = SendMessageW(focus, WM_GETDLGCODE, VK_BACK, reinterpret_cast<LPARAM>(msg));
            if (!isWindowOperational() || shellView != view || viewGeneration != generation || GetFocus() != focus)
                return true;
            if (!(code & (DLGC_HASSETSEL | DLGC_WANTALLKEYS))) {
                // A held key would climb through each parent that takes the focus.
                if (!(HIWORD(msg->lParam) & KF_REPEAT)) openParent(closeSource);
                return true;
            }
        }
    }
    if (msg->message == WM_KEYDOWN && msg->wParam == VK_SPACE && settings::getPeekEnabled()
            && GetKeyState(VK_CONTROL) >= 0 && GetKeyState(VK_SHIFT) >= 0 && GetKeyState(VK_MENU) >= 0) {
        const ULONG_PTR generation = viewGeneration;
        const auto current = [&] { return isWindowOperational() && shellView == view && viewGeneration == generation; };
        HWND root = nullptr;
        const HWND focus = GetFocus();
        const HRESULT foundRoot = view->GetWindow(&root);
        if (!current()) return true;
        if (SUCCEEDED(foundRoot) && root && focus == msg->hwnd
                && (focus == root || IsChild(root, focus))) {
            const LRESULT code = SendMessageW(focus, WM_GETDLGCODE, VK_SPACE, reinterpret_cast<LPARAM>(msg));
            if (!current() || GetFocus() != focus) return true;
            // Native rename editors and controls that own Space keep the key.
            if (!(code & (DLGC_HASSETSEL | DLGC_WANTALLKEYS | DLGC_BUTTON))) {
                // The native selection first: PowerToys configuration files are
                // only read once there is something to preview, and never for repeats.
                CComQIPtr<IFolderView2> folderView(view);
                if (!current()) return true;
                CComPtr<IShellItemArray> selection;
                if (folderView && SUCCEEDED(nativeViewSelection(folderView, FALSE, &selection, current)) && selection) {
                    const auto paths = peekPaths(selection);
                    if (!current() || GetFocus() != focus)
                        return true;
                    if (!paths.empty()) {
                        // Repeats of a press that started a preview are consumed
                        // without re-reading the configuration or starting another.
                        if (msg->lParam & (1L << 30)) return peekPressActive;
                        peekPressActive = false;
                        const PeekInstallation installation = powerToysPeekEnabled() ? findPeek() : PeekInstallation{};
                        if (!current() || GetFocus() != focus) return true;
                        if (!installation.executable.empty()) {
                            peekPressActive = true;
                            const DWORD error = launchPeek(installation, paths);
                            if (error && current()) {
                                const auto detail = getErrorMessage(error);
                                const auto message = formatString(IDS_PEEK_ERROR,
                                    installation.executable.c_str(), error, detail.get());
                                MessageBoxW(hwnd, message.get(), getString(IDS_ERROR_CAPTION), MB_OK | MB_ICONERROR);
                            }
                            return true;
                        }
                    }
                }
            }
        }
        if (!isWindowOperational() || shellView != view || viewGeneration != generation)
            return true;
    }
    if (msg->message == WM_LBUTTONUP || msg->message == WM_KEYUP
            || msg->message == WM_MOUSEWHEEL || msg->message == WM_MOUSEHWHEEL) {
        HWND root = nullptr;
        if (SUCCEEDED(view->GetWindow(&root)) && isWindowOperational() && shellView == view
                && root && (msg->hwnd == root || IsChild(root, msg->hwnd)))
            scheduleStateSave(msg->message == WM_LBUTTONUP);
    }
    if (!isWindowOperational() || shellView != view) return true;
    const bool escape = msg->message == WM_CHAR && msg->wParam == VK_ESCAPE;
    HWND escapeFocus = escape ? GetFocus() : nullptr;
    // Let the Shell handle its own accelerators, including rename cancellation.
    ++navigationDepth;
    const HRESULT translated = view->TranslateAccelerator(msg);
    resumeDeferredClose();
    if (translated == S_OK)
        return true;
    if (!isWindowOperational())
        return true;
    if (escapeFocus && escapeFocus == msg->hwnd && escapeFocus == GetFocus()
            && shellView.p == view.p) {
        HWND viewWindow = nullptr;
        HRESULT hr = view->GetWindow(&viewWindow);
        if (!isWindowOperational())
            return true;
        if (SUCCEEDED(hr) && viewWindow
                && (escapeFocus == viewWindow || IsChild(viewWindow, escapeFocus))) {
            // Preserve text selection and controls that request this key themselves.
            LRESULT dialogCode = SendMessage(escapeFocus, WM_GETDLGCODE,
                VK_ESCAPE, reinterpret_cast<LPARAM>(msg));
            if (!isWindowOperational())
                return true;
            if (!(dialogCode & (DLGC_HASSETSEL | DLGC_WANTALLKEYS))
                    && escapeFocus == GetFocus() && shellView.p == view.p)
                clearSelection();
        }
    }
    // Keep normal dispatch so the Shell can also cancel a pending cut operation.
    return !isWindowOperational();
}

bool FolderWindow::handleResizeMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    const bool wasActive = resizeGesture.isActive();
    // Read before the gesture ends: releasing its capture reenters this function.
    const MSG press = backgroundPress;
    const bool handled = resizeGesture.handleMessage(message, wParam, lParam);
    if (!resizeGesture.isActive()) backgroundPress = {};
    if (resizeGesture.takeCompleted()) {
        deferredGestureClick = {};
        finishColumnResize();
        if (isWindowOperational()) persistViewState();
    } else if (wasActive && !resizeGesture.isActive() && resizingColumns) {
        // A click or cancellation without movement still ends layout deferral.
        finishColumnResize();
    }
    // A background press released without a drag is the Shell's own click:
    // return both messages to the view, which clears the selection and opens
    // its menu as it does for any right click.
    if (message == WM_RBUTTONUP && resizeGesture.takeClicked() && press.hwnd
            && isWindowOperational() && IsWindow(press.hwnd)) {
        const DWORD position = GetMessagePos();
        POINT point = {GET_X_LPARAM(position), GET_Y_LPARAM(position)};
        if (ScreenToClient(press.hwnd, &point)
                && PostMessageW(press.hwnd, press.message, press.wParam, press.lParam)) {
            returnedPress = press.hwnd;
            checkLE(PostMessageW(press.hwnd, WM_RBUTTONUP, wParam, MAKELPARAM(point.x, point.y)));
        }
    }
    return handled;
}

void FolderWindow::cancelAltGesture() {
    const bool wasActive = resizeGesture.isActive();
    resizeGesture.cancel();
    if (resizeGesture.takeCompleted() && isWindowOperational()) {
        finishColumnResize();
        persistViewState();
    } else if (wasActive && resizingColumns) {
        finishColumnResize();
    }
}

bool FolderWindow::handleAltMouseMessage(MSG *msg) {
    if (handleResizeMessage(msg->message, msg->wParam, msg->lParam)) return true;
    if (returnedPress && (msg->message == WM_RBUTTONDOWN || msg->message == WM_RBUTTONDBLCLK)) {
        // The press this window returned to the view is not a new gesture.
        const bool returned = msg->hwnd == returnedPress;
        returnedPress = nullptr;
        if (returned) return false;
    }
    // Auto-repeat (bit 30 of lParam: key already down) is not a new Alt press.
    if (msg->message == WM_SYSKEYDOWN && msg->wParam == VK_MENU && !(msg->lParam & (1 << 30)))
        suppressAltUp = false;
    if (msg->message == WM_SYSKEYUP && msg->wParam == VK_MENU && suppressAltUp) {
        suppressAltUp = false;
        return true;
    }
    if ((msg->message != WM_LBUTTONDOWN && msg->message != WM_LBUTTONDBLCLK
            && msg->message != WM_RBUTTONDOWN
            && msg->message != WM_RBUTTONDBLCLK
            && msg->message != WM_MBUTTONDOWN && msg->message != WM_MBUTTONDBLCLK)
            || GetCapture() || !shellView)
        return false;
    // The modern renderer recognizes double clicks without CS_DBLCLKS. Its
    // first press was consumed by the window gesture, so recognize the second
    // using Windows' time/position settings and invoke the public Shell verb.
    const bool rendererDoubleClick = msg->message == WM_LBUTTONDOWN && deferredGestureClick.hwnd == msg->hwnd
            && deferredGestureClick.hwnd && deferredGestureGeneration == viewGeneration
            && msg->time - deferredGestureClick.time <= GetDoubleClickTime()
            && std::abs(msg->pt.x - deferredGestureClick.pt.x) <= GetSystemMetrics(SM_CXDOUBLECLK) / 2
            && std::abs(msg->pt.y - deferredGestureClick.pt.y) <= GetSystemMetrics(SM_CYDOUBLECLK) / 2
            && (currentGestureModifiers() & GestureAlt)
            && !(GetClassLongPtrW(msg->hwnd, GCL_STYLE) & CS_DBLCLKS);
    // A window gesture consumes its first click. Prepare that click's selection
    // through MSAA before the native double-click. Replaying a button-down while
    // the second press is physically held can start an unintended Shell drag.
    if (msg->message == WM_LBUTTONDBLCLK || rendererDoubleClick) {
        const MSG click = deferredGestureClick;
        deferredGestureClick = {};
        const CComPtr<IShellView> view = shellView;
        HWND root = nullptr;
        const ULONG_PTR generation = viewGeneration;
        if (click.hwnd == msg->hwnd && click.hwnd && deferredGestureGeneration == generation
                && msg->time - click.time <= GetDoubleClickTime()
                && SUCCEEDED(view->GetWindow(&root)) && root
                && (click.hwnd == root || IsChild(root, click.hwnd))) {
            if (!isWindowOperational() || shellView != view || viewGeneration != generation) return true;
            // Same containment test as the gesture's start: a window that appeared
            // under the first press is neither queried nor selected through MSAA.
            const HWND hitWindow = WindowFromPoint(click.pt);
            if (hitWindow != root && !IsChild(root, hitWindow)) return false;
            relayViewTooltipPress(*msg);
            if (!isWindowOperational() || shellView != view || viewGeneration != generation) return true;
            // Classic views expose a synchronous public hit-test. Resolve the
            // actual item before entering the accessibility proxy, which may
            // dispatch pending native input while its RPC is in progress.
            int pointedIndex = -1; wchar_t type[32] = {};
            if (GetClassNameW(click.hwnd, type, _countof(type)) && lstrcmpW(type, WC_LISTVIEWW) == 0
                    && !ListView_IsGroupViewEnabled(click.hwnd)) {
                LVHITTESTINFO hit = {}; hit.pt = screenToClient(click.hwnd, click.pt);
                const int candidate = ListView_HitTest(click.hwnd, &hit);
                if (candidate >= 0 && (hit.flags & LVHT_ONITEM)) pointedIndex = candidate;
            }
            ViewGestureTarget target;
            if (pointedIndex < 0) viewGestureTargetAt(click.pt, target);
            const auto &accessible = target.accessible;
            const auto &child = target.child;
            CComVariant state;
            if (pointedIndex >= 0 || (accessible && target.role == ROLE_SYSTEM_LISTITEM)) {
                LONG flags = SELFLAG_NONE;
                if (rendererDoubleClick && (click.wParam & MK_SHIFT)) {
                    flags = SELFLAG_ADDSELECTION;
                } else if (click.wParam & MK_CONTROL) {
                    bool itemSelected = false;
                    if (pointedIndex >= 0) itemSelected = (ListView_GetItemState(click.hwnd, pointedIndex, LVIS_SELECTED) & LVIS_SELECTED) != 0;
                    else {
                        if (FAILED(accessible->get_accState(child, &state)) || state.vt != VT_I4) return false;
                        itemSelected = (state.lVal & STATE_SYSTEM_SELECTED) != 0;
                    }
                    flags |= itemSelected ? SELFLAG_REMOVESELECTION : SELFLAG_ADDSELECTION;
                } else {
                    flags |= SELFLAG_TAKESELECTION;
                }
                if (!isWindowOperational() || shellView != view || viewGeneration != generation) return true;
                CComQIPtr<IFolderView2> folderView(view);
                if (!isWindowOperational() || shellView != view || viewGeneration != generation) return true;
                if (!folderView) return false;
                const bool extendSelection = rendererDoubleClick && (click.wParam & MK_SHIFT);
                int rangeAnchor = -1;
                if (extendSelection) {
                    if (folderView->GetSelectionMarkedItem(&rangeAnchor) != S_OK || rangeAnchor < 0)
                        folderView->GetFocusedItem(&rangeAnchor);
                    if (!isWindowOperational() || shellView != view || viewGeneration != generation) return true;
                }
                // TAKEFOCUS in the MSAA proxy can synthesize Alt restoration
                // after the real key release. Resolve the pointed item through
                // a selection-only operation, then focus its public Shell index.
                std::vector<CComHeapPtr<ITEMID_CHILD>> previousSelection;
                const bool preserveSelection = (click.wParam & MK_CONTROL)
                    || (rendererDoubleClick && (click.wParam & MK_SHIFT));
                if (click.wParam & MK_CONTROL) {
                    int count = 0;
                    checkHR(folderView->ItemCount(SVGIO_SELECTION, &count));
                    if (!isWindowOperational() || shellView != view || viewGeneration != generation) return true;
                    if (count < 0) return false;
                    previousSelection.resize(static_cast<size_t>(count));
                    CComPtr<IEnumIDList> selectedItems;
                    checkHR(folderView->Items(SVGIO_SELECTION, IID_PPV_ARGS(&selectedItems)));
                    if (!isWindowOperational() || shellView != view || viewGeneration != generation) return true;
                    for (auto &id : previousSelection) {
                        if (!selectedItems) return false;
                        const HRESULT hr = selectedItems->Next(1, &id, nullptr);
                        if (!isWindowOperational() || shellView != view || viewGeneration != generation) return true;
                        if (hr != S_OK) { checkHR(hr); return false; }
                    }
                }
                if (pointedIndex >= 0) {
                    int count = 0;
                    if (FAILED(folderView->ItemCount(SVGIO_ALLVIEW, &count))
                            || !isWindowOperational() || shellView != view || viewGeneration != generation
                            || count != ListView_GetItemCount(click.hwnd) || pointedIndex >= count) return true;
                }
                if (!isWindowOperational() || shellView != view || viewGeneration != generation) return true;
                const HRESULT selectionResult = pointedIndex >= 0
                    ? folderView->SelectItem(pointedIndex, SVSI_SELECT | SVSI_DESELECTOTHERS | SVSI_NOTAKEFOCUS)
                    : accessible->accSelect(SELFLAG_TAKESELECTION, child);
                if (FAILED(selectionResult)) { checkHR(selectionResult); return true; }
                if (!isWindowOperational() || shellView != view || viewGeneration != generation) return true;
                int index = pointedIndex;
                const HRESULT found = pointedIndex >= 0 ? S_OK : folderView->GetSelectedItem(-1, &index);
                if (!isWindowOperational() || shellView != view || viewGeneration != generation) return true;
                CComHeapPtr<ITEMID_CHILD> pointed;
                if (found == S_OK && index >= 0) {
                    checkHR(folderView->Item(index, &pointed));
                    if (!isWindowOperational() || shellView != view || viewGeneration != generation) return true;
                }
                if (preserveSelection) {
                    checkHR(selectionClear.clear(view, [&]() {
                        return isWindowOperational() && shellView == view && viewGeneration == generation;
                    }));
                    if (!isWindowOperational() || shellView != view || viewGeneration != generation) return true;
                    for (const auto &id : previousSelection) {
                        checkHR(view->SelectItem(id, SVSI_SELECT | SVSI_NOTAKEFOCUS));
                        if (!isWindowOperational() || shellView != view || viewGeneration != generation) return true;
                    }
                    if (extendSelection && index >= 0) {
                        // Modern MSAA rejects EXTENDSELECTION. Apply the same
                        // view-order range through the public folder interface.
                        const int anchor = rangeAnchor >= 0 ? rangeAnchor : index;
                        for (int rangeIndex = (std::min)(anchor, index); rangeIndex <= (std::max)(anchor, index); ++rangeIndex) {
                            checkHR(folderView->SelectItem(rangeIndex, SVSI_SELECT | SVSI_NOTAKEFOCUS));
                            if (!isWindowOperational() || shellView != view || viewGeneration != generation) return true;
                        }
                    } else if (pointedIndex >= 0)
                        checkHR(folderView->SelectItem(pointedIndex,
                            ((flags & SELFLAG_REMOVESELECTION) ? SVSI_DESELECT : SVSI_SELECT) | SVSI_NOTAKEFOCUS));
                    else checkHR(accessible->accSelect(flags, child));
                    if (!isWindowOperational() || shellView != view || viewGeneration != generation) return true;
                }
                if (pointed) {
                    DWORD selection = 0;
                    if (pointedIndex >= 0)
                        selection = (flags & SELFLAG_REMOVESELECTION) ? 0 : SVSI_SELECT;
                    else checkHR(folderView->GetSelectionState(pointed, &selection));
                    if (!isWindowOperational() || shellView != view || viewGeneration != generation) return true;
                    const UINT mark = rendererDoubleClick ? 0 : SVSI_SELECTIONMARK;
                    checkHR(view->SelectItem(pointed, (selection & SVSI_SELECT) | SVSI_FOCUSED | mark | SVSI_NOTAKEFOCUS));
                }
                if (rendererDoubleClick && isWindowOperational() && shellView == view && viewGeneration == generation) {
                    ++navigationDepth;
                    const HRESULT invoked = folderView->InvokeVerbOnSelection("properties");
                    resumeDeferredClose();
                    // A native Ctrl-double-click on a selected item invokes
                    // its first-click selection, then restores that item.
                    if (flags == SELFLAG_REMOVESELECTION && pointed && isWindowOperational()
                            && shellView == view && viewGeneration == generation)
                        checkHR(view->SelectItem(pointed, SVSI_SELECT | SVSI_NOTAKEFOCUS));
                    checkHR(invoked);
                    return true;
                }
            }
            if (!isWindowOperational() || shellView != view || viewGeneration != generation) return true;
        }
        return false;
    }
    deferredGestureClick = {};
    const GestureButton button = msg->message == WM_LBUTTONDOWN ? GestureButton::Left
        : msg->message == WM_MBUTTONDOWN || msg->message == WM_MBUTTONDBLCLK ? GestureButton::Middle : GestureButton::Right;
    const unsigned modifiers = currentGestureModifiers();
    const WindowShortcuts bindings = settings::getWindowShortcuts();
    const bool moving = matchesShortcut(bindings.move, button, modifiers);
    if (!moving && !matchesShortcut(bindings.resize, button, modifiers)) return false;
    const HWND owner = hwnd;
    const CComPtr<IShellView> view = shellView;
    const ULONG_PTR generation = viewGeneration;
    auto current = [&]() {
        return isWindowOperational() && hwnd == owner && shellView == view
            && viewGeneration == generation;
    };
    HWND root = nullptr;
    if (FAILED(view->GetWindow(&root)) || !current() || !root
            || (msg->hwnd != root && !IsChild(root, msg->hwnd)))
        return false;
    const HWND focus = GetFocus();
    if (focus && (focus == root || IsChild(root, focus))) {
        const LRESULT code = SendMessage(focus, WM_GETDLGCODE, 0, 0);
        if (!current()) return true;
        if (code & DLGC_HASSETSEL) return false; // Native rename/text editing.
    }
    const POINT point = msg->pt;
    const HWND hitWindow = WindowFromPoint(point);
    if (hitWindow != root && !IsChild(root, hitWindow)) return false;
    relayViewTooltipPress(*msg);
    if (!current()) return true;
    const LONG role = gestureRoleAt(point);
    debugPrintf(L"ALT HWND=%p message=0x%04X role=%ld\n", owner, msg->message, role);
    if (!current()) return true;
    // Unknown roles, headers, scrollbars, rename edits and embedded buttons keep
    // their native input. Only list contents participate in FileSpacer gestures.
    if (role != ROLE_SYSTEM_LIST && role != ROLE_SYSTEM_LISTITEM) return false;
    const bool background = backgroundShortcut(moving ? bindings.move : bindings.resize);
    if (role == ROLE_SYSTEM_LISTITEM && background) return false;
    SetForegroundWindow(owner);
    if (!current()) return true;
    if (button == GestureButton::Left) {
        deferredGestureClick = *msg;
        deferredGestureGeneration = generation;
    }
    if (!moving || button != GestureButton::Left) {
        const bool begun = resizeGesture.begin(owner, owner, point, button, moving);
        if (begun && background) backgroundPress = *msg;
        if (begun && !moving) {
            resizingColumns = true;
            resizeDeferred = settings::getDetailsColumnsDeferred();
            if (columnLayout) columnLayout->suspend(ShellViewColumns::Hold::Resize, resizeDeferred);
        }
        return begun;
    }
    suppressAltUp = (modifiers & GestureAlt) != 0;
    if (DragDetect(msg->hwnd, point) && current()) {
        deferredGestureClick = {};
        if (GetCapture() == msg->hwnd) ReleaseCapture();
        SendMessage(owner, WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(point.x, point.y));
    }
    return true;
}

// A click that activates this window (WM_ACTIVATE with WA_CLICKACTIVE) is
// followed by its button-down message, posted to the clicked window only after
// the activation. When that first message lands on the empty background of the
// view, the application's own message loop drops it so the native view keeps
// its selection. Item clicks, other buttons and Alt gestures are not affected.
bool FolderWindow::cursorOverShellView() const {
    POINT cursor = {};
    HWND root = nullptr;
    if (!shellView || !GetCursorPos(&cursor) || FAILED(shellView->GetWindow(&root)) || !root)
        return false;
    const HWND target = WindowFromPoint(cursor);
    return target && (target == root || IsChild(root, target));
}

bool FolderWindow::isBackgroundActivationClick(const MSG *msg) const {
    if (!shellView || GetKeyState(VK_MENU) < 0 || GetAsyncKeyState(VK_MENU) < 0
            || !settings::getKeepSelectionOnActivate())
        return false;
    HWND root = nullptr;
    if (FAILED(shellView->GetWindow(&root)) || !root)
        return false;
    if (msg->hwnd != root && !IsChild(root, msg->hwnd))
        return false;
    return gestureRoleAt(msg->pt) == ROLE_SYSTEM_LIST;
}

} // namespace filespacer
