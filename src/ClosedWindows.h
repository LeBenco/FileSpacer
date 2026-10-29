#pragma once
#include <common.h>
#include "FolderStateStore.h"
#include <windows.h>
#include <shobjidl.h>
#include <atlbase.h>
#include <cstdint>
#include <string>
#include <vector>

namespace filespacer {

// Everything the Jump Lists are built from, in build order. Every field is
// length-prefixed, so two different models never serialize alike and an equal
// fingerprint can only come from an unchanged model (false positives are
// harmless: they only trigger a rebuild).
struct JumpListModel {
    std::wstring text;
    void add(const std::wstring &field) { text += std::to_wstring(field.size()); text += L':'; text += field; }
    void add(const wchar_t *field) { add(std::wstring(field ? field : L"")); }
    uint64_t fingerprint() const {
        uint64_t hash = 14695981039346656037ull; // FNV-1a over the UTF-16 units.
        for (const wchar_t unit : text) {
            hash = (hash ^ (unit & 0xFF)) * 1099511628211ull;
            hash = (hash ^ (unit >> 8)) * 1099511628211ull;
        }
        return hash;
    }
};

enum class FolderOpenResult;
bool getClosedWindowInfo(IShellItem *item, const wchar_t *title, ClosedWindow *window);
std::wstring closedWindowName(const ClosedWindow &window);
std::wstring closedWindowPath(const ClosedWindow &window);
FolderOpenResult reopenLastWindow(HWND owner);
void updateReopenJumpLists();
void refreshReopenJumpLists();
// stateKey tags the registry key with the folder's stored state key, so a
// later start can drop the key once that state is gone and no window is open.
void scheduleReopenJumpList(const std::wstring &appID, const std::string &stateKey);
// Pinned Quick access folders, through the Explorer namespace and property shared
// by the toolbar menu and the Jump Lists. On failure *stage names the step
// (IDS_QUICK_ACCESS_OPEN/KEY/ENUM/READ); strict fails on an unreadable entry
// instead of skipping it.
HRESULT enumerateQuickAccessFolders(std::vector<CComPtr<IShellItem>> *folders, UINT *stage,
    bool strict);

} // namespace
