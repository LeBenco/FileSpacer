#pragma once
#include "WinUtils.h"
#include <commctrl.h>
#include <string>
#include <vector>
#include <memory>

namespace filespacer {

// Command 1 is Everything; command 2 + index is a recent local search.
HMENU createSearchMenu(bool everything, const std::vector<std::wstring> &history);

// Native single-line edit, with a themed frame and a search button.
class SearchBox : public WindowImpl {
public:
    static constexpr UINT APPLY = NM_FIRST - 1002;
    static constexpr UINT FOCUS_VIEW = NM_FIRST - 1003;
    static constexpr UINT RESIZE = NM_FIRST - 1004;
    static constexpr UINT DROPDOWN = NM_FIRST - 1005;
    static constexpr UINT SELECT = NM_FIRST - 1006;
    struct Notification {
        NMHDR header;
        const wchar_t *text; // borrowed during the synchronous WM_NOTIFY call
        int width = 0; // requested toolbar placeholder width, in pixels
        bool explicitSubmit = false;
        UINT command = 0;
    };
    static void init();
    void create(HWND parent, HWND owner);
    void destroy();
    void place(const RECT &rect, bool resizable = true);
    void setFolderName(const wchar_t *name);
    void setLiveSearch(bool enabled);
    void setDropdownEnabled(bool enabled, const wchar_t *everythingPath = L"");
    void showDropdown(HMENU menu); // Copies the existing menu's entries; menu remains borrowed.
    void hideDropdown();
    void setText(const wchar_t *text);
    std::wstring text() const;
    bool hasFocus() const;
    bool focus();
    bool handleTopLevelMessage(MSG *msg);

private:
    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam) override;
    static LRESULT CALLBACK splitterProc(HWND window, UINT message, WPARAM wParam,
        LPARAM lParam, UINT_PTR subclassID, DWORD_PTR refData);
    static LRESULT CALLBACK editProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
    static LRESULT CALLBACK listProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
    static LRESULT CALLBACK popupProc(HWND, UINT, WPARAM, LPARAM);
    void requestDropdown();
    void selectDropdown();
    void moveDropdownSelection(int direction);
    void requestEverythingIcon();
    void resetEverythingIcon();
    void updateFonts();
    void layout();
    void submit(bool explicitSubmit = true);
    HWND owner = nullptr, edit = nullptr, button = nullptr;
    HWND dropdownWindow = nullptr, dropdownList = nullptr;
    struct Suggestion { UINT command; std::wstring text; };
    std::vector<Suggestion> suggestions;
    bool dropdownEnabled = false;
    struct IconState;
    std::shared_ptr<IconState> everythingIcon;
    std::wstring everythingPath;
    HWND splitter = nullptr;
    HFONT textFont = nullptr, iconFont = nullptr;
    bool resizeEnabled = true;
    bool liveSearch = false;
    bool searchPending = false;
    int dragStartX = 0, dragStartWidth = 0;
};

} // namespace
