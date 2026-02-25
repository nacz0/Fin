#include "App/Panels/ExplorerPanel.h"

#include "App/FinHelpers.h"
#include "fastener/fastener.h"

namespace fs = std::filesystem;

namespace fin {

void RenderExplorerPanel(
    fst::Context& ctx,
    fs::path& currentPath,
    const OpenDocumentFn& openDocument) {
    if (!fst::BeginDockableWindow(ctx, "Eksplorator")) {
        return;
    }

    const fst::Rect bounds = ctx.layout().currentBounds();
    beginScrollablePanelContent(ctx, "explorer_scroll", bounds);

    fst::Label(ctx, "Sciezka: " + currentPath.string());
    if (fst::Button(ctx, ".. (w gore)")) {
        fs::path parent = currentPath.parent_path();
        if (!parent.empty()) {
            currentPath = parent;
        }
    }
    fst::Separator(ctx);

    for (const ExplorerEntry& entry : listEntries(currentPath)) {
        bool selected = false;
        const std::string label = (entry.isDirectory ? "[DIR] " : "      ") + entry.name;
        if (fst::Selectable(ctx, label, selected)) {
            if (entry.isDirectory) {
                currentPath = entry.path;
            } else {
                openDocument(entry.path.string());
            }
        }
    }

    endScrollablePanelContent(ctx, "explorer_scroll", bounds);
    fst::EndDockableWindow(ctx);
}

} // namespace fin
