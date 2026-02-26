#pragma once

#include "fastener/fastener.h"

#include <functional>

namespace fin {

using ApplyPresetThemeFn = std::function<void(int)>;
using ApplyCustomThemeFn = std::function<void(const fst::Theme&)>;

void RenderPersonalizationPanel(
    fst::Context& ctx,
    bool& showPersonalizationWindow,
    fst::Theme& editableTheme,
    int& selectedPreset,
    const ApplyPresetThemeFn& applyPresetTheme,
    const ApplyCustomThemeFn& applyCustomTheme);

} // namespace fin
