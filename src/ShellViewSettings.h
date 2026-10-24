#pragma once
#include <common.h>
#include "FolderStateStore.h"
#include <shobjidl.h>

namespace filespacer {
// Shared by FolderWindow and the real-Shell persistence regression. Widths in
// the existing storage format are 96-DPI units; icon sizes remain Shell units.
bool sameView(const FolderViewSettings &left, const FolderViewSettings &right);
FolderViewSettings captureOptions(IFolderView2 *view, const FolderViewSettings &previous);
// view must be non-null. Returns the first failed application of a stored part.
HRESULT restoreOptions(IFolderView2 *view, const FolderViewSettings &state);
} // namespace filespacer
