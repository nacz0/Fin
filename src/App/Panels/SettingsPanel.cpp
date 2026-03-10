#include "App/Panels/SettingsPanel.h"

#include "App/FinHelpers.h"
#include "App/FinI18n.h"
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
    if (!fst::BeginDockableWindow(ctx, fst::i18n("window.settings"), settingsOptions)) {
        return;
    }

    const fst::Rect bounds = ctx.layout().currentBounds();
    beginScrollablePanelContent(ctx, "settings_scroll", bounds);

    if (fst::Checkbox(ctx, fst::i18n("settings.autocomplete_lsp"), config.autocompleteEnabled)) {
        if (config.autocompleteEnabled) {
            if (!startLsp()) {
                config.autocompleteEnabled = false;
            }
        } else {
            stopLsp();
        }
    }
    (void)fst::Checkbox(ctx, fst::i18n("settings.build_clang"), config.clangBuildEnabled);
    (void)fst::Checkbox(ctx, fst::i18n("settings.auto_brackets"), config.autoClosingBrackets);
    (void)fst::Checkbox(ctx, fst::i18n("settings.smart_indent"), config.smartIndentEnabled);
    (void)fst::Checkbox(ctx, fst::i18n("settings.minimap"), config.minimapEnabled);

    std::vector<std::string> themeNames = {
        fst::i18n("theme.dark"),
        fst::i18n("theme.light"),
        fst::i18n("theme.retro"),
        fst::i18n("theme.classic"),
    };
    int selectedTheme = std::clamp(config.theme, 0, 3);
    if (fst::ComboBox(ctx, fst::i18n("settings.theme"), selectedTheme, themeNames)) {
        config.theme = selectedTheme;
        applyTheme(config.theme);
    }

    std::vector<std::string> localeNames = {
        fst::i18n("locale.polish"),
        fst::i18n("locale.english"),
    };
    std::vector<std::string> localeCodes = {"pl", "en"};
    config.language = NormalizeLocale(config.language);
    int selectedLocale = config.language == "en" ? 1 : 0;
    if (fst::ComboBox(ctx, fst::i18n("settings.language"), selectedLocale, localeNames)) {
        selectedLocale = std::clamp(selectedLocale, 0, static_cast<int>(localeCodes.size()) - 1);
        config.language = localeCodes[static_cast<size_t>(selectedLocale)];
        SetLocale(config.language);
    }

    fst::InputNumberOptions zoomOptions;
    zoomOptions.step = 0.1f;
    zoomOptions.decimals = 1;
    if (fst::InputNumber(ctx, fst::i18n("settings.zoom"), textScale, 0.5f, 3.0f, zoomOptions)) {
        textScale = std::clamp(textScale, 0.5f, 3.0f);
        config.zoom = textScale;
    }

    endScrollablePanelContent(ctx, "settings_scroll", bounds);
    fst::EndDockableWindow(ctx);
}

} // namespace fin
