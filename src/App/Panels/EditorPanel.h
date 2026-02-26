#pragma once

#include "App/FinTypes.h"
#include "Core/LSPClient.h"
#include "fastener/fastener.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace fin {

using ClampActiveTabFn = std::function<void()>;
using CloseTabFn = std::function<void(int)>;
using CloseCompletionPopupFn = std::function<void()>;

void RenderEditorPanel(
    fst::Context& ctx,
    std::vector<std::unique_ptr<DocumentTab>>& docs,
    int& activeTab,
    fst::TabControl& tabControl,
    float textScale,
    bool completionVisible,
    int completionOwnerTab,
    const std::string& completionOwnerDocumentPath,
    bool lspActive,
    LSPClient& lsp,
    const ClampActiveTabFn& clampActiveTab,
    const CloseTabFn& closeTab,
    const CloseCompletionPopupFn& closeCompletionPopup);

} // namespace fin
