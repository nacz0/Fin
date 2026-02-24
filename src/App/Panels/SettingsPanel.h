#pragma once

#include "Core/AppConfig.h"

#include <functional>

namespace fst {
class Context;
}

namespace fin {

using StartLspFn = std::function<bool()>;
using StopLspFn = std::function<void()>;
using ApplyThemeFn = std::function<void(int)>;

void RenderSettingsPanel(
    fst::Context& ctx,
    bool& showSettingsWindow,
    AppConfig& config,
    float& textScale,
    const StartLspFn& startLsp,
    const StopLspFn& stopLsp,
    const ApplyThemeFn& applyTheme);

} // namespace fin