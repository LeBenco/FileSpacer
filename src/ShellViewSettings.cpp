#include "ShellViewSettings.h"
#include "DPI.h"
#include <atlbase.h>
#include <cstring>
#include <utility>

namespace filespacer {
namespace {
FolderViewSettings::Key stateKey(const PROPERTYKEY &key) {
    FolderViewSettings::Key result = {};
    std::memcpy(result.data(), &key.fmtid, 16);
    std::memcpy(result.data() + 16, &key.pid, 4);
    return result;
}
PROPERTYKEY propertyKey(const FolderViewSettings::Key &key) {
    PROPERTYKEY result = {};
    std::memcpy(&result.fmtid, key.data(), 16);
    std::memcpy(&result.pid, key.data() + 16, 4);
    return result;
}
}

bool sameView(const FolderViewSettings &a, const FolderViewSettings &b) {
    if (a.present != b.present || a.mode != b.mode || a.iconSize != b.iconSize
            || a.flags != b.flags || a.groupBy != b.groupBy
            || a.groupAscending != b.groupAscending
            || a.columns.size() != b.columns.size() || a.sort.size() != b.sort.size()) return false;
    for (size_t i = 0; i < a.columns.size(); ++i)
        if (a.columns[i].key != b.columns[i].key || a.columns[i].width != b.columns[i].width) return false;
    for (size_t i = 0; i < a.sort.size(); ++i)
        if (a.sort[i].key != b.sort[i].key || a.sort[i].direction != b.sort[i].direction) return false;
    return true;
}

FolderViewSettings captureOptions(IFolderView2 *view, const FolderViewSettings &previous) {
    FolderViewSettings state = previous; // Keep unsupported parts instead of writing empty values.
    FOLDERVIEWMODE mode;
    int iconSize;
    if (SUCCEEDED(view->GetViewModeAndIconSize(&mode, &iconSize))) {
        state.mode = mode; state.iconSize = iconSize;
        state.present |= FolderViewSettings::Mode;
    }
    DWORD flags;
    if (SUCCEEDED(view->GetCurrentFolderFlags(&flags))) {
        state.flags = flags & (FWF_AUTOARRANGE | FWF_SNAPTOGRID);
        state.present |= FolderViewSettings::Flags;
    }
    CComQIPtr<IColumnManager> manager(view);
    UINT count;
    if (manager && SUCCEEDED(manager->GetColumnCount(CM_ENUM_VISIBLE, &count)) && count) {
        std::vector<PROPERTYKEY> keys(count);
        std::vector<FolderViewSettings::Column> columns;
        bool complete = SUCCEEDED(manager->GetColumns(CM_ENUM_VISIBLE, keys.data(), count));
        if (complete) for (const auto &key : keys) {
            CM_COLUMNINFO info = {sizeof(info), CM_MASK_WIDTH};
            if (FAILED(manager->GetColumnInfo(key, &info))) { complete = false; break; }
            FolderViewSettings::Column column;
            column.key = stateKey(key);
            column.width = static_cast<uint32_t>(invScaleDPI(static_cast<int>(info.uWidth)));
            columns.push_back(column);
        }
        if (complete) { state.columns = std::move(columns); state.present |= FolderViewSettings::Columns; }
    }
    int sortCount;
    if (SUCCEEDED(view->GetSortColumnCount(&sortCount)) && sortCount > 0) {
        std::vector<SORTCOLUMN> columns(static_cast<size_t>(sortCount));
        if (SUCCEEDED(view->GetSortColumns(columns.data(), sortCount))) {
            state.sort.clear();
            for (const auto &column : columns) {
                FolderViewSettings::SortColumn sort;
                sort.key = stateKey(column.propkey); sort.direction = column.direction;
                state.sort.push_back(sort);
            }
            state.present |= FolderViewSettings::Sort;
        }
    }
    PROPERTYKEY group = {};
    BOOL ascending = FALSE;
    HRESULT groupResult = view->GetGroupBy(&group, &ascending);
    if (SUCCEEDED(groupResult)) {
        state.groupBy = groupResult == S_FALSE ? FolderViewSettings::Key{} : stateKey(group);
        state.groupAscending = groupResult == S_OK && ascending != FALSE;
        state.present |= FolderViewSettings::Group;
    }
    return state;
}

HRESULT restoreOptions(IFolderView2 *view, const FolderViewSettings &state) {
    HRESULT restored = S_OK;
    auto record = [&](HRESULT result) {
        if (FAILED(result) && SUCCEEDED(restored)) restored = result;
    };
    if (state.present & FolderViewSettings::Mode) {
        const HRESULT result = view->SetViewModeAndIconSize(static_cast<FOLDERVIEWMODE>(state.mode), state.iconSize);
        record(result);
    }
    if (state.present & FolderViewSettings::Flags) {
        const HRESULT result = view->SetCurrentFolderFlags(FWF_AUTOARRANGE | FWF_SNAPTOGRID, state.flags);
        record(result);
    }
    CComQIPtr<IColumnManager> manager(view);
    if ((state.present & FolderViewSettings::Columns) && !state.columns.empty()) {
        if (!manager) record(E_FAIL);
        else {
            std::vector<PROPERTYKEY> keys;
            for (const auto &column : state.columns) keys.push_back(propertyKey(column.key));
            const HRESULT result = manager->SetColumns(keys.data(), static_cast<UINT>(keys.size()));
            record(result);
            if (SUCCEEDED(result)) {
                for (const auto &column : state.columns) {
                    CM_COLUMNINFO info = {sizeof(info), CM_MASK_WIDTH};
                    info.uWidth = static_cast<UINT>(scaleDPI(static_cast<int>(column.width)));
                    const HRESULT widthResult = manager->SetColumnInfo(propertyKey(column.key), &info);
                    record(widthResult);
                }
            }
        }
    }
    if ((state.present & FolderViewSettings::Sort) && !state.sort.empty()) {
        std::vector<SORTCOLUMN> columns;
        for (const auto &column : state.sort)
            columns.push_back({propertyKey(column.key), static_cast<SORTDIRECTION>(column.direction)});
        const HRESULT result = view->SetSortColumns(columns.data(), static_cast<int>(columns.size()));
        record(result);
    }
    if (state.present & FolderViewSettings::Group) {
        const HRESULT result = view->SetGroupBy(propertyKey(state.groupBy), state.groupAscending);
        record(result);
    }
    return restored;
}

} // namespace filespacer
