#pragma once
#include "WinUtils.h"
#include <atlbase.h>
#include <commctrl.h>
#include <shobjidl.h>
#include <string>
#include <vector>

namespace filespacer {

// Shell hierarchy, native toolbar segments and a native edit control.
class PathBar : public WindowImpl {
public:
    static constexpr UINT OPEN_ITEM = NM_FIRST - 1001;
    struct OpenItemNotification {
        NMHDR header;
        IShellItem *item; // borrowed for the synchronous WM_NOTIFY call
        bool control;
    };

    static void init();
    void create(HWND parent, HWND owner, IShellItem *item, HFONT font, HFONT symbolFont);
    void destroy();
    void setItem(IShellItem *item);
    void setFont(HFONT font);
    void place(int left, int right, int height);
    void show(bool visible);
    bool isWindow(HWND window) const;
    bool hasFocus() const;
    bool handleTopLevelMessage(MSG *msg);

private:
    struct Segment {
        CComPtr<IShellItem> item;
        std::wstring name;
        int width = 0;
    };

    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam) override;
    static LRESULT CALLBACK toolbarProc(HWND window, UINT message, WPARAM wParam,
        LPARAM lParam, UINT_PTR subclassID, DWORD_PTR refData);
    void layout();
    void beginEdit();
    void endEdit(bool restoreFocus);
    void submit(bool control);
    void openItem(IShellItem *item, bool control);
    void showHiddenSegments();

    HWND owner = nullptr, toolbar = nullptr, edit = nullptr, previousFocus = nullptr;
    HFONT chevronFont = nullptr; // owned compact icon font
    CComPtr<IShellItem> currentItem;
    std::vector<Segment> segments;
    std::wstring address;
    size_t firstVisible = 0;
    bool editing = false;
};

} // namespace
