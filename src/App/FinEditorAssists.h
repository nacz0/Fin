#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "App/FinTypes.h"
#include "Core/AppConfig.h"
#include "fastener/fastener.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace fin {

void ApplyEditorAssists(
    const AppConfig& config,
    fst::InputState& input,
    std::vector<std::unique_ptr<DocumentTab>>& docs,
    int& activeTab,
    bool assistHasPreState,
    int assistPreTab,
    const std::string& assistPreText,
    const fst::TextPosition& assistPreCursor,
    bool completionVisible,
    const std::function<void()>& clampActiveTab,
    const std::function<void()>& closeCompletionPopup);

} // namespace fin
