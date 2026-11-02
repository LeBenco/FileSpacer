#pragma once
#include <common.h>
#include <atlbase.h>
#include <shobjidl.h>
#include <cstdint>
#include <functional>
#include <vector>

namespace filespacer {
// Missing baseline identities never authorize replacing a saved position.
// Product callers supply the parent for CompareIDs(SHCIDS_CANONICALONLY).
// Only a complete, unfiltered current snapshot may remove IDs. Canonical
// duplicates prefer the first entry: historical partial captures encode the
// visible observation before appending older, binary-distinct PIDLs.
HRESULT extendIconBaseline(const std::vector<uint8_t> &baseline,
    const std::vector<uint8_t> &observed, std::vector<uint8_t> &bytes,
    IShellFolder *identityFolder, const std::function<bool()> &current);
HRESULT mergeIconChanges(const std::vector<uint8_t> &reference,
    const std::vector<uint8_t> &baseline, const std::vector<uint8_t> &current,
    bool complete, std::vector<uint8_t> &bytes, IShellFolder *identityFolder,
    const std::function<bool()> &valid);
// Existing format: Shell-serialized child PIDL followed by a 96-DPI POINT.
// A filtered view returns S_FALSE with empty output; callers must not save it.
// Caller must exclude auto-arranged views from persistent captures.
// Incomplete enumeration preserves previously saved items not yet visible.
HRESULT captureIconPositions(IFolderView *view, const std::function<bool()> &current,
    bool enumerationComplete, const std::vector<uint8_t> &previous, std::vector<uint8_t> &bytes,
    bool filtered = false);
struct PreparedIconPosition {
    CComHeapPtr<ITEMID_CHILD> id;
    POINT point = {};
    PreparedIconPosition() = default;
    PreparedIconPosition(PreparedIconPosition &&other) noexcept : id(other.id.Detach()), point(other.point) {}
};
// Own decoded PIDLs; the application arrays borrow only from positions.
struct PreparedIconPositions {
    CComPtr<IFolderView> view;
    std::vector<PreparedIconPosition> positions;
    std::vector<PCUITEMID_CHILD> items;
    std::vector<POINT> points;
    bool ready = false;
};
HRESULT prepareIconPositions(IFolderView *view, const std::vector<uint8_t> &bytes,
    const std::function<bool()> &current, PreparedIconPositions &prepared);
HRESULT applyIconPositions(IFolderView *view, PreparedIconPositions &prepared,
    const std::function<bool()> &current);
// Decode the entire blob before moving anything; preserve selection and focus.
HRESULT restoreIconPositions(IFolderView *view, const std::vector<uint8_t> &bytes,
    const std::function<bool()> &current);
} // namespace filespacer
