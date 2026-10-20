static void require(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}
int main(int argc, char **argv) {
    std::string scenario = argc > 1 ? argv[1] : "success";
    try {
        IFolderView2 oldView, newView;
        if (scenario == "all-fail") {
            oldView.modeFails = oldView.flagsFail = oldView.countFails = true;
            oldView.sortCountFails = oldView.groupFails = oldView.countItemsFails = true;
        } else if (scenario == "columns-fail") oldView.keysFail = true;
        else if (scenario == "width-fail") oldView.infoFailsAt = 2;
        else if (scenario == "sort-fail") oldView.sortFails = true;
        else if (scenario == "group-fail") oldView.groupFails = true;
        else if (scenario == "partial-items") { oldView.failItem = 1; oldView.failPosition = 2; }
        else if (scenario == "no-items") oldView.items = 0;
        else if (scenario == "count-fail") oldView.countItemsFails = true;
        else if (scenario == "no-manager") oldView.available = false;
        else if (scenario == "no-group") oldView.noGroup = true;
        {
            FolderWindow::ShellViewState snapshot;
            FolderWindow::getViewState(&oldView, &snapshot);
            FolderWindow::setViewState(&newView, snapshot);
        }
        require(ITEMID_CHILD::live == 0, "PIDL ownership leak");
        std::set<std::string> expected = {"mode", "flags", "columns", "widths", "sort", "group"};
        if (scenario == "all-fail") expected.clear();
        if (scenario == "columns-fail" || scenario == "width-fail" || scenario == "no-manager") {
            expected.erase("columns"); expected.erase("widths");
        }
        if (scenario == "sort-fail") expected.erase("sort");
        if (scenario == "group-fail") expected.erase("group");
        if (scenario == "no-group") { expected.erase("group"); expected.insert("ungrouped"); }
        require(newView.writes == expected, "restored a missing part or lost a valid part");
        std::vector<int> expectedIds = {0, 1, 2, 3};
        if (scenario == "partial-items") expectedIds = {0, 3};
        if (scenario == "all-fail" || scenario == "no-items" || scenario == "count-fail") expectedIds.clear();
        require(newView.restoredIds == expectedIds, "restored invalid item/position pairs");
        require(newView.restoreCalls == (expectedIds.empty() ? 0 : 1), "unexpected item restoration call");
        for (size_t i = 0; i < expectedIds.size(); ++i)
            require(newView.restoredPositions[i].x == expectedIds[i] * 10
                && newView.restoredPositions[i].y == expectedIds[i] * 20, "invalid or mismatched position");
        if (scenario == "partial-items") require(oldView.positionReads == 3, "position read after failed Item");
        std::cout << "PASS " << scenario << '\n';
    } catch (const std::exception &error) {
        std::cerr << "FAIL " << scenario << ": " << error.what() << '\n';
        return 1;
    }
}
