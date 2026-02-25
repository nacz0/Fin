#include "App/FinDockingUi.h"

#include <algorithm>
#include <string>

namespace fin {

namespace {

const ManagedDockWindow* FindManagedDockWindow(const ManagedDockWindows& managedDockWindows, DockWindowId id) {
    const auto it = std::find_if(
        managedDockWindows.begin(),
        managedDockWindows.end(),
        [&](const ManagedDockWindow& windowInfo) {
            return id == windowInfo.id;
        });
    if (it == managedDockWindows.end()) {
        return nullptr;
    }
    return &(*it);
}

void RememberCurrentDockNodes(
    fst::Context& ctx,
    const ManagedDockWindows& managedDockWindows,
    std::unordered_map<int, fst::DockNode::Id>& lastDockNodeByWindow) {
    for (const ManagedDockWindow& windowInfo : managedDockWindows) {
        const std::string windowTitle = DockWindowTitle(windowInfo.id);
        const fst::WidgetId windowId = ctx.makeId(windowTitle);
        fst::DockNode* node = ctx.docking().getWindowDockNode(windowId);
        if (node && node->hasWindow(windowId)) {
            lastDockNodeByWindow[DockWindowKey(windowInfo.id)] = node->id;
        }
    }
}

} // namespace

std::string DockWindowTitle(DockWindowId id) {
    switch (id) {
        case DockWindowId::Explorer:
            return fst::i18n("window.explorer");
        case DockWindowId::Editor:
            return fst::i18n("window.editor");
        case DockWindowId::Console:
            return fst::i18n("window.console");
        case DockWindowId::LspDiagnostics:
            return fst::i18n("window.lsp_diagnostics");
        case DockWindowId::Terminal:
            return fst::i18n("window.terminal");
        case DockWindowId::Settings:
            return fst::i18n("window.settings");
        case DockWindowId::Personalization:
            return fst::i18n("window.personalization");
    }
    return std::string();
}

int DockWindowKey(DockWindowId id) {
    return static_cast<int>(id);
}

ManagedDockWindows CreateManagedDockWindows(
    bool& showExplorerTab,
    bool& showEditorTab,
    bool& showConsoleTab,
    bool& showLspDiagnosticsTab,
    bool& showTerminalTab,
    bool& showSettingsWindow,
    bool& showPersonalizationTab) {
    return {{
        {DockWindowId::Explorer, &showExplorerTab, DockWindowId::Editor, fst::DockDirection::Left},
        {DockWindowId::Editor, &showEditorTab, DockWindowId::Explorer, fst::DockDirection::Right},
        {DockWindowId::Console, &showConsoleTab, DockWindowId::Editor, fst::DockDirection::Bottom},
        {DockWindowId::LspDiagnostics, &showLspDiagnosticsTab, DockWindowId::Editor, fst::DockDirection::Bottom},
        {DockWindowId::Terminal, &showTerminalTab, DockWindowId::Editor, fst::DockDirection::Bottom},
        {DockWindowId::Settings, &showSettingsWindow, DockWindowId::Editor, fst::DockDirection::Center},
        {DockWindowId::Personalization, &showPersonalizationTab, DockWindowId::Settings, fst::DockDirection::Center},
    }};
}

void RequestDockTab(int& pendingDockTabFocus, DockWindowId windowId, bool* visibilityFlag) {
    if (visibilityFlag) {
        *visibilityFlag = true;
    }
    pendingDockTabFocus = DockWindowKey(windowId);
}

void FocusDockTab(fst::Context& ctx, DockWindowId dockWindowId) {
    const std::string windowTitle = DockWindowTitle(dockWindowId);
    if (windowTitle.empty()) {
        return;
    }

    const fst::WidgetId widgetId = ctx.makeId(windowTitle);
    fst::DockNode* node = ctx.docking().getWindowDockNode(widgetId);
    if (!node) {
        return;
    }

    const auto it = std::find(node->dockedWindows.begin(), node->dockedWindows.end(), widgetId);
    if (it == node->dockedWindows.end()) {
        return;
    }

    node->selectedTabIndex = static_cast<int>(std::distance(node->dockedWindows.begin(), it));
}

void SetDockWindowVisibility(
    fst::Context& ctx,
    const ManagedDockWindows& managedDockWindows,
    std::unordered_map<int, fst::DockNode::Id>& lastDockNodeByWindow,
    int& pendingDockTabFocus,
    DockWindowId windowKind,
    bool visible) {
    RememberCurrentDockNodes(ctx, managedDockWindows, lastDockNodeByWindow);

    const ManagedDockWindow* windowInfo = FindManagedDockWindow(managedDockWindows, windowKind);
    if (!windowInfo) {
        return;
    }

    if (visible == *windowInfo->visible) {
        if (visible) {
            pendingDockTabFocus = DockWindowKey(windowKind);
        }
        return;
    }

    const std::string windowTitle = DockWindowTitle(windowInfo->id);
    const fst::WidgetId windowId = ctx.makeId(windowTitle);
    fst::DockNode* node = ctx.docking().getWindowDockNode(windowId);
    if (!visible && node && node->hasWindow(windowId) && node->dockedWindows.size() <= 1) {
        return;
    }

    *windowInfo->visible = visible;
    if (visible) {
        pendingDockTabFocus = DockWindowKey(windowKind);
    }
}

void SyncDockWindowVisibility(
    fst::Context& ctx,
    const ManagedDockWindows& managedDockWindows,
    std::unordered_map<int, fst::DockNode::Id>& lastDockNodeByWindow) {
    RememberCurrentDockNodes(ctx, managedDockWindows, lastDockNodeByWindow);

    for (const ManagedDockWindow& windowInfo : managedDockWindows) {
        if (*windowInfo.visible) {
            continue;
        }

        const std::string windowTitle = DockWindowTitle(windowInfo.id);
        const fst::WidgetId windowId = ctx.makeId(windowTitle);
        fst::DockNode* node = ctx.docking().getWindowDockNode(windowId);
        if (node && node->hasWindow(windowId)) {
            if (node->dockedWindows.size() <= 1) {
                *windowInfo.visible = true;
                continue;
            }
            node->removeWindow(windowId);
        }
    }

    for (const ManagedDockWindow& windowInfo : managedDockWindows) {
        if (!*windowInfo.visible) {
            continue;
        }

        const std::string windowTitle = DockWindowTitle(windowInfo.id);
        const fst::WidgetId windowId = ctx.makeId(windowTitle);
        fst::DockNode* node = ctx.docking().getWindowDockNode(windowId);
        if (node && node->hasWindow(windowId)) {
            continue;
        }

        fst::DockNode* targetNode = nullptr;
        const auto remembered = lastDockNodeByWindow.find(DockWindowKey(windowInfo.id));
        if (remembered != lastDockNodeByWindow.end()) {
            targetNode = ctx.docking().getDockNode(remembered->second);
        }

        if (targetNode) {
            ctx.docking().dockWindow(windowId, targetNode->id, fst::DockDirection::Center);
            continue;
        }

        const std::string anchorTitle = DockWindowTitle(windowInfo.fallbackAnchorId);
        const fst::WidgetId anchorId = ctx.makeId(anchorTitle);
        if (fst::DockNode* anchorNode = ctx.docking().getWindowDockNode(anchorId)) {
            ctx.docking().dockWindow(windowId, anchorNode->id, windowInfo.fallbackDirection);
            continue;
        }

        if (fst::DockNode* rootNode = ctx.docking().getDockSpace("##MainDockSpace")) {
            ctx.docking().dockWindow(windowId, rootNode->id, fst::DockDirection::Center);
        }
    }
}

} // namespace fin
