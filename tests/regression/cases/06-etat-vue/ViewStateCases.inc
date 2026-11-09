static void require(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}
int main(int argc, char **argv) {
    std::string scenario = argc > 1 ? argv[1] : "success";
    try {
        IFolderView2 oldView, newView;
        // Restoration must change the destination; an identical presentation
        // intentionally performs no writes and has its own scenario below.
        if (scenario != "identical" && scenario != "partial-identical") { newView.currentMode = FVM_DETAILS; newView.autoArrange = true; }
        if (scenario == "partial-mode-matched") newView.currentMode = oldView.currentMode;
        if (scenario == "all-fail") {
            oldView.modeFails = oldView.flagsFail = oldView.countFails = true;
            oldView.sortCountFails = oldView.groupFails = oldView.countItemsFails = true;
        } else if (scenario == "columns-fail") oldView.keysFail = true;
        else if (scenario == "width-fail") oldView.infoFailsAt = 2;
        else if (scenario == "sort-fail") oldView.sortFails = true;
        else if (scenario == "group-fail") oldView.groupFails = true;
        else if (scenario == "no-manager") oldView.available = false;
        else if (scenario == "no-group") oldView.noGroup = true;
        {
            // The rename snapshot records only the settings read from the old view;
            // icon positions go through the shared ShellViewIcons blob (bench 18).
            FolderWindow::ShellViewState snapshot;
            snapshot.view = captureOptions(&oldView, {});
            if (scenario == "partial-identical" || scenario == "partial-mode-matched")
                snapshot.view.present &= FolderViewSettings::Mode | FolderViewSettings::Flags;
            restoreOptions(&newView, snapshot.view);
        }
        std::set<std::string> expected = {"mode", "flags", "columns", "widths", "sort", "group"};
        if (scenario == "all-fail" || scenario == "identical") expected.clear();
        if (scenario == "partial-identical") expected.clear();
        if (scenario == "partial-mode-matched") expected = {"flags"};
        if (scenario == "columns-fail" || scenario == "width-fail" || scenario == "no-manager") {
            expected.erase("columns"); expected.erase("widths");
        }
        if (scenario == "sort-fail") expected.erase("sort");
        if (scenario == "group-fail") expected.erase("group");
        if (scenario == "no-group") { expected.erase("group"); expected.insert("ungrouped"); }
        require(newView.writes == expected, "restored a missing part or lost a valid part");
        if (scenario != "all-fail")
            require(newView.currentMode == oldView.currentMode && newView.autoArrange == oldView.autoArrange,
                "restored mode or arrangement differs");
        std::cout << "PASS " << scenario << '\n';
    } catch (const std::exception &error) {
        std::cerr << "FAIL " << scenario << ": " << error.what() << '\n';
        return 1;
    }
}
