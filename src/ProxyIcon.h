#pragma once
#include "common.h"

#include <Windows.h>
#include <Uxtheme.h>

namespace filespacer {

class ItemWindow;

class ProxyIcon {
public:
    static void init();
    static void initTheme(HTHEME theme);
    static void initMetrics(const NONCLIENTMETRICS &metrics);
    static void uninit();

    ProxyIcon(ItemWindow *outer);

    void create(HWND parent, wchar_t *title, int top, int height);
    void destroy(); // may be called without create()

    bool isToolbarWindow(HWND hwnd) const;
    POINT getMenuPoint(HWND parent);

    void setTitle(wchar_t *title);
    void setIcon(HICON icon); // does not take ownership!
    void setActive(bool active);
    void setPressedState(bool pressed);
    void autoSize(LONG parentWidth, LONG captionLeft, LONG captionRight);
    void redrawToolbar();

    void beginRename();
    bool isRenaming();

    bool onControlCommand(HWND controlHwnd, WORD notif);
    bool drawTitle(const DRAWITEMSTRUCT *draw);
    void onThemeChanged();

private:
    RECT titleRect();

    void completeRename();
    void cancelRename();

    static LRESULT CALLBACK captionProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);

    // window subclasses
    static LRESULT CALLBACK renameBoxProc(HWND hwnd, UINT message,
        WPARAM wParam, LPARAM lParam, UINT_PTR subclassID, DWORD_PTR refData);

    ItemWindow * const outer;

    HWND caption = nullptr, toolbar = nullptr, renameBox = nullptr;
    HICON captionIcon = nullptr; // Borrowed from ItemWindow.
};

} // namespace
