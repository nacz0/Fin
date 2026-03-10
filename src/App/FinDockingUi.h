#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "fastener/fastener.h"

#include <array>
#include <string>
#include <unordered_map>

namespace fin {

enum class DockWindowId {
    Explorer = 0,
    Editor = 1,
    Console = 2,
    LspDiagnostics = 3,
    Terminal = 4,
    Settings = 5,
    Personalization = 6,
};

struct ManagedDockWindow {
    DockWindowId id;
    bool* visible;
    DockWindowId fallbackAnchorId;
    fst::DockDirection fallbackDirection;
};

using ManagedDockWindows = std::array<ManagedDockWindow, 7>;

std::string DockWindowTitle(DockWindowId id);
int DockWindowKey(DockWindowId id);

ManagedDockWindows CreateManagedDockWindows(
    bool& showExplorerTab,
    bool& showEditorTab,
    bool& showConsoleTab,
    bool& showLspDiagnosticsTab,
    bool& showTerminalTab,
    bool& showSettingsWindow,
    bool& showPersonalizationTab);

void RequestDockTab(int& pendingDockTabFocus, DockWindowId windowId, bool* visibilityFlag = nullptr);
void FocusDockTab(fst::Context& ctx, DockWindowId dockWindowId);

void SetDockWindowVisibility(
    fst::Context& ctx,
    const ManagedDockWindows& managedDockWindows,
    std::unordered_map<int, fst::DockNode::Id>& lastDockNodeByWindow,
    int& pendingDockTabFocus,
    DockWindowId windowKind,
    bool visible);

void SyncDockWindowVisibility(
    fst::Context& ctx,
    const ManagedDockWindows& managedDockWindows,
    std::unordered_map<int, fst::DockNode::Id>& lastDockNodeByWindow);

} // namespace fin
