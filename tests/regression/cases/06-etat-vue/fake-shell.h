#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include "FolderStateStore.h"

using namespace filespacer;
using UINT = uint32_t;
using DWORD = uint32_t;
using BOOL = int;
using HRESULT = int32_t;
constexpr HRESULT S_OK = 0, S_FALSE = 1, E_FAIL = -1;
constexpr BOOL FALSE = 0;
bool SUCCEEDED(HRESULT result) { return result >= 0; }
bool FAILED(HRESULT result) { return result < 0; }
bool checkHR(HRESULT result) { return SUCCEEDED(result); }
enum FOLDERVIEWMODE { FVM_ICON = 1, FVM_LIST = 3, FVM_DETAILS = 4 };
enum SORTDIRECTION { SORT_ASCENDING = 1 };
constexpr UINT CM_ENUM_VISIBLE = 1, CM_MASK_WIDTH = 1;
constexpr DWORD FWF_AUTOARRANGE = 1, FWF_SNAPTOGRID = 4;
constexpr UINT SVGIO_ALLVIEW = 2, SVSI_POSITIONITEM = 0x80, SVSI_NOSTATECHANGE = 0x80000000u;
struct POINT { int32_t x, y; };
struct PROPERTYKEY { std::array<uint8_t, 16> fmtid; uint32_t pid; };
struct SORTCOLUMN { PROPERTYKEY propkey; SORTDIRECTION direction; };
struct CM_COLUMNINFO { UINT cbSize, dwMask, uWidth; };
struct ITEMID_CHILD {
    inline static int live = 0;
    int index;
    explicit ITEMID_CHILD(int value) : index(value) { ++live; }
    ~ITEMID_CHILD() { --live; }
};
using PCUITEMID_CHILD = const ITEMID_CHILD *;
using PCITEMID_CHILD = const ITEMID_CHILD *;
template<class T> struct CComHeapPtr {
    T *p = nullptr;
    CComHeapPtr() = default;
    CComHeapPtr(const CComHeapPtr &) = delete;
    ~CComHeapPtr() { delete p; }
    operator T *() const { return p; }
    T **operator&() { assert(!p); return &p; }
    T *Detach() { T *value = p; p = nullptr; return value; }
    void Attach(T *value) { assert(!p); p = value; }
};
int scaleDPI(int value) { return value; }
int invScaleDPI(int value) { return value; }

struct IColumnManager {
    bool available = true, countFails = false, keysFail = false;
    int infoFailsAt = -1;
    std::set<std::string> writes;
    HRESULT GetColumnCount(UINT, UINT *count) { *count = 2; return countFails ? E_FAIL : S_OK; }
    HRESULT GetColumns(UINT, PROPERTYKEY *keys, UINT count) {
        for (UINT i = 0; i < count; ++i) keys[i] = {{}, i + 1};
        return keysFail ? E_FAIL : S_OK;
    }
    HRESULT GetColumnInfo(PROPERTYKEY key, CM_COLUMNINFO *info) {
        if (static_cast<int>(key.pid) == infoFailsAt) return E_FAIL;
        info->uWidth = key.pid * 100;
        return S_OK;
    }
    HRESULT SetColumns(const PROPERTYKEY *, UINT count) {
        writes.insert("columns");
        return count ? S_OK : E_FAIL;
    }
    HRESULT SetColumnInfo(PROPERTYKEY, CM_COLUMNINFO *) { writes.insert("widths"); return S_OK; }
};
struct IFolderView {
    FOLDERVIEWMODE currentMode = FVM_ICON;
    bool autoArrange = false, currentModeFails = false;
    HRESULT GetAutoArrange() { return autoArrange ? S_OK : S_FALSE; }
    HRESULT GetCurrentViewMode(UINT *value) { *value = currentMode; return currentModeFails ? E_FAIL : S_OK; }
};
struct IFolderView2 : IColumnManager, IFolderView {
    bool modeFails = false, flagsFail = false, sortCountFails = false;
    bool sortFails = false, groupFails = false, countItemsFails = false;
    bool noGroup = false;
    int failItem = -1, failPosition = -1, items = 4;
    int positionReads = 0, restoreCalls = 0;
    std::vector<int> restoredIds;
    std::vector<POINT> restoredPositions;
    HRESULT GetViewModeAndIconSize(FOLDERVIEWMODE *mode, int *size) {
        *mode = currentMode; *size = 48;
        return modeFails ? E_FAIL : S_OK;
    }
    HRESULT GetCurrentFolderFlags(DWORD *flags) { *flags = FWF_SNAPTOGRID | (autoArrange ? FWF_AUTOARRANGE : 0); return flagsFail ? E_FAIL : S_OK; }
    HRESULT GetSortColumnCount(int *count) { *count = 1; return sortCountFails ? E_FAIL : S_OK; }
    HRESULT GetSortColumns(SORTCOLUMN *columns, int) {
        *columns = {{{}, 7}, SORT_ASCENDING};
        return sortFails ? E_FAIL : S_OK;
    }
    HRESULT GetGroupBy(PROPERTYKEY *key, BOOL *ascending) {
        *key = {{}, 9}; *ascending = 1;
        return groupFails ? E_FAIL : noGroup ? S_FALSE : S_OK;
    }
    HRESULT ItemCount(UINT, int *count) { *count = items; return countItemsFails ? E_FAIL : S_OK; }
    HRESULT Item(int index, ITEMID_CHILD **id) {
        *id = new ITEMID_CHILD(index);
        return index == failItem ? E_FAIL : S_OK;
    }
    HRESULT GetItemPosition(const ITEMID_CHILD *id, POINT *point) {
        ++positionReads;
        *point = {-999, -999};
        if (!id || id->index == failPosition) return E_FAIL;
        *point = {id->index * 10, id->index * 20};
        return S_OK;
    }
    HRESULT SetViewModeAndIconSize(FOLDERVIEWMODE value, int) { currentMode = value; writes.insert("mode"); return S_OK; }
    HRESULT SetCurrentFolderFlags(DWORD mask, DWORD flags) {
        if (mask & FWF_AUTOARRANGE) autoArrange = (flags & FWF_AUTOARRANGE) != 0;
        writes.insert("flags"); return S_OK;
    }
    HRESULT SetSortColumns(const SORTCOLUMN *, int) { writes.insert("sort"); return S_OK; }
    HRESULT SetGroupBy(PROPERTYKEY key, BOOL ascending) {
        writes.insert(key.pid == 0 && !ascending ? "ungrouped" : "group");
        return S_OK;
    }
    HRESULT SelectAndPositionItems(UINT count, const ITEMID_CHILD **ids, POINT *points, DWORD flags) {
        assert(flags == (SVSI_POSITIONITEM | SVSI_NOSTATECHANGE));
        ++restoreCalls;
        for (UINT i = 0; i < count; ++i) {
            restoredIds.push_back(ids[i] ? ids[i]->index : -1);
            restoredPositions.push_back(points[i]);
        }
        return S_OK;
    }
};
template<class T> struct CComQIPtr {
    T *p;
    explicit CComQIPtr(IFolderView2 *view) : p(view->available ? view : nullptr) {}
    explicit operator bool() const { return p != nullptr; }
    T *operator->() const { return p; }
};
