#pragma once
#include "WinUtils.h"
#include <commctrl.h>
#include <string>

namespace filespacer {

// Native single-line edit, with a themed frame and a search button.
class SearchBox : public WindowImpl {
public:
    static constexpr UINT APPLY = NM_FIRST - 1002;
    static constexpr UINT FOCUS_VIEW = NM_FIRST - 1003;
    static constexpr UINT RESIZE = NM_FIRST - 1004;
    struct Notification {
        NMHDR header;
        const wchar_t *text; // borrowed during the synchronous WM_NOTIFY call
        int width = 0; // requested toolbar placeholder width, in pixels
    };
    static void init();
    void create(HWND parent, HWND owner);
    void destroy();
    void place(const RECT &rect, bool resizable = true);
    void setFolderName(const wchar_t *name);
    void setLiveSearch(bool enabled);
    bool hasFocus() const;
    bool focus();
    bool handleTopLevelMessage(MSG *msg);

private:
    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam) override;
    static LRESULT CALLBACK splitterProc(HWND window, UINT message, WPARAM wParam,
        LPARAM lParam, UINT_PTR subclassID, DWORD_PTR refData);
    void updateFonts();
    void layout();
    void submit();
    HWND owner = nullptr, edit = nullptr, button = nullptr;
    HWND splitter = nullptr;
    HFONT textFont = nullptr, iconFont = nullptr;
    bool resizeEnabled = true;
    bool liveSearch = false;
    bool searchPending = false;
    int dragStartX = 0, dragStartWidth = 0;
};

} // namespace
