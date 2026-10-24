#pragma once
#include <common.h>

#include <memory>
#include <string>
#include <windows.h>

namespace filespacer {

// https://stackoverflow.com/a/47459319
template <typename T>
const T* tempPtr(const T&& x) { return &x; }

using wstr_ptr = std::unique_ptr<wchar_t[]>;

std::wstring quoteCommandArgument(const wchar_t *argument);

// UTF-16 to UTF-8. flags = 0 replaces invalid surrogates; WC_ERR_INVALID_CHARS rejects them.
inline std::string toUtf8(const wchar_t *text, DWORD flags = 0) {
    if (!text || !*text) return {};
    const int size = WideCharToMultiByte(CP_UTF8, flags, text, -1, nullptr, 0, nullptr, nullptr);
    if (!size) return {};
    std::string result(static_cast<size_t>(size), '\0');
    if (!WideCharToMultiByte(CP_UTF8, flags, text, -1, &result[0], size, nullptr, nullptr)) return {};
    result.pop_back();
    return result;
}

/* win32 wrappers that return values directly instead of error codes */
RECT windowRect(HWND hwnd);
RECT clientRect(HWND hwnd); // client rect top-left is always at (0,0)
SIZE clientSize(HWND hwnd);
POINT screenToClient(HWND hwnd, POINT screenPt);
POINT clientToScreen(HWND hwnd, POINT clientPt);

class WindowImpl {
protected:
    static LRESULT CALLBACK windowProc(HWND, UINT, WPARAM, LPARAM);
    virtual LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    virtual bool useCustomFrame() const;
    HWND hwnd = nullptr;
};

} // namespace
