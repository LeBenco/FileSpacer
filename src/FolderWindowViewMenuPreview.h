// SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
// Based on ChromaFiler, Copyright (C) 2023 J. van't Hoog.
#pragma once
#include <common.h>
#include "FolderWindow.h"

namespace filespacer {
struct FolderWindow::ViewMenuPreview {
    struct Entry { UINT command; };
    static thread_local ViewMenuPreview *active;
    FolderWindow &owner;
    HMENU menu;
    HMENU displayedMenu = nullptr;
    CComPtr<IContextMenu> context;
    CComPtr<IShellView> source;
    CComPtr<IFolderView2> view;
    const ULONG_PTR generation;
    const ULONG_PTR serial;
    ShellViewState initial;
    std::vector<Entry> entries;
    UINT initialCommand = 0, currentCommand = 0, wantedCommand = 0;
    UINT pendingCommand = 0; // Entry outside the modes, run after the preview is cancelled.
    UINT chosenCommand = 0; // Mode retained when the menu closed: 0 for the initial one.
    bool tracking = true, posted = false, busy = false, prepared = false, capturedIcons = false;
    POINT initialSpacing = {};
    bool hoverEnabled = false;
    // The final apply() failed without any refill: no EnumDone replays it. The
    // session still withholds saving the preview, but its initial selection
    // no longer describes the view.
    bool restoreFailed = false;
    const ULONG_PTR enumerationBaseline; // Serial when the menu opened; earlier EnumDone are stale.
    std::vector<std::vector<uint8_t>> positionChunks;
    size_t nextPositionChunk = 0;
    POINT lastPoint = {}; // Last cursor position seen; moves without displacement are not hovers.

    HRESULT preparePositionChunks();

    void advancePositions();

    void finishPositions();

    ViewMenuPreview(FolderWindow &window, HMENU popup, IContextMenu *handler,
            IFolderView2 *folderView);

    bool current() const;

    bool prepare();

    bool capture();

    static LRESULT CALLBACK filter(int code, WPARAM wParam, LPARAM lParam);

    void pointAt(POINT point);

    void highlight(UINT command, HMENU popup);

    void restoreDetails();

    void apply(UINT command);

    void update();

    bool isEntry(UINT command) const;

    // Runs the entry chosen outside the modes once the initial mode is restored.
    bool invokePending();

    // While tracking, a refill completed by this EnumDone rebuilt the items
    // the restoration concerned: restore them again (idempotent).
    void enumerated(ULONG_PTR enumeration);

    // The menu closed while a refill observed by apply() was running: its
    // EnumDone replays the same restoration, or retries a failed cancellation,
    // then lets the completed view be saved.
    void replay(ULONG_PTR enumeration);

    void finish(UINT command);
};
} // namespace filespacer
