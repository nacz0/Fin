#include "App/Panels/SettingsPanel.h"

#include "fastener/fastener.h"

#include <algorithm>
#include <string>
#include <vector>

namespace fin {

void RenderSettingsPanel(
    fst::Context& ctx,
    bool& showSettingsWindow,
    AppConfig& config,
    float& textScale,
    const StartLspFn& startLsp,
    const StopLspFn& stopLsp,
    const ApplyThemeFn& applyTheme) {
    if (!showSettingsWindow) {
        return;
    }

    fst::DockableWindowOptions settingsOptions;
    settingsOptions.open = &showSettingsWindow;
    if (!fst::BeginDockableWindow(ctx, "Ustawienia", settingsOptions)) {
        return;
    }

    const fst::Rect bounds = ctx.layout().currentBounds();
    ctx.layout().beginContainer(bounds);

    if (fst::Checkbox(ctx, "Autouzupelnianie (LSP)", config.autocompleteEnabled)) {
        if (config.autocompleteEnabled) {
            if (!startLsp()) {
                config.autocompleteEnabled = false;
            }
        } else {
            stopLsp();
        }
    }
    (void)fst::Checkbox(ctx, "Budowanie przez clang++", config.clangBuildEnabled);
    (void)fst::Checkbox(ctx, "Auto-domykanie nawiasow", config.autoClosingBrackets);
    (void)fst::Checkbox(ctx, "Smart indent", config.smartIndentEnabled);

    std::vector<std::string> themeNames = {"Ciemny", "Jasny", "Retro"};
    int selectedTheme = std::clamp(config.theme, 0, 2);
    if (fst::ComboBox(ctx, "Motyw", selectedTheme, themeNames)) {
        config.theme = selectedTheme;
        applyTheme(config.theme);
    }

    fst::InputNumberOptions zoomOptions;
    zoomOptions.step = 0.1f;
    zoomOptions.decimals = 1;
    if (fst::InputNumber(ctx, "Zoom", textScale, 0.5f, 3.0f, zoomOptions)) {
        textScale = std::clamp(textScale, 0.5f, 3.0f);
        config.zoom = textScale;
    }

    ctx.layout().endContainer();
    fst::EndDockableWindow(ctx);
}

} // namespace fin
