#include "App/Panels/PersonalizationPanel.h"

#include "App/FinHelpers.h"
#include "fastener/fastener.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace fin {

namespace {

enum class ColorPreviewKind {
    WindowBackground,
    PanelBackground,
    PopupBackground,
    Primary,
    PrimaryText,
    Secondary,
    Text,
    Border,
    InputBackground,
    InputBorder,
    InputFocused,
    Button,
    ButtonText,
    Status,
    Selection,
    SelectionText,
    ScrollbarTrack,
    ScrollbarThumb,
    ScrollbarThumbHover,
    Shadow,
    TooltipBackground,
    TooltipText,
    TooltipBorder,
    DockPreviewOverlay,
    DockTab,
    DockSplitter,
    DockSplitterHover,
    Swatch
};

struct ColorTarget {
    const char* name;
    fst::Color* value;
    ColorPreviewKind preview = ColorPreviewKind::Swatch;
    bool supportsAlpha = false;
};

uint64_t HashFNV1a(const void* data, size_t size, uint64_t seed) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    uint64_t hash = seed;
    for (size_t i = 0; i < size; ++i) {
        hash ^= static_cast<uint64_t>(bytes[i]);
        hash *= 1099511628211ull;
    }
    return hash;
}

void HashColor(uint64_t& hash, const fst::Color& color) {
    const uint8_t rgba[4] = {color.r, color.g, color.b, color.a};
    hash = HashFNV1a(rgba, sizeof(rgba), hash);
}

void HashFloat(uint64_t& hash, float value) {
    uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value), "float size mismatch");
    std::memcpy(&bits, &value, sizeof(bits));
    hash = HashFNV1a(&bits, sizeof(bits), hash);
}

uint64_t ThemeFingerprint(const fst::Theme& theme) {
    uint64_t hash = 1469598103934665603ull;
    const fst::ThemeColors& c = theme.colors;

    HashColor(hash, c.windowBackground);
    HashColor(hash, c.panelBackground);
    HashColor(hash, c.popupBackground);
    HashColor(hash, c.primary);
    HashColor(hash, c.primaryHover);
    HashColor(hash, c.primaryActive);
    HashColor(hash, c.primaryText);
    HashColor(hash, c.secondary);
    HashColor(hash, c.secondaryHover);
    HashColor(hash, c.secondaryActive);
    HashColor(hash, c.text);
    HashColor(hash, c.textDisabled);
    HashColor(hash, c.textSecondary);
    HashColor(hash, c.border);
    HashColor(hash, c.borderHover);
    HashColor(hash, c.borderFocused);
    HashColor(hash, c.inputBackground);
    HashColor(hash, c.inputBorder);
    HashColor(hash, c.inputFocused);
    HashColor(hash, c.buttonBackground);
    HashColor(hash, c.buttonHover);
    HashColor(hash, c.buttonActive);
    HashColor(hash, c.buttonText);
    HashColor(hash, c.success);
    HashColor(hash, c.warning);
    HashColor(hash, c.error);
    HashColor(hash, c.info);
    HashColor(hash, c.selection);
    HashColor(hash, c.selectionText);
    HashColor(hash, c.scrollbarTrack);
    HashColor(hash, c.scrollbarThumb);
    HashColor(hash, c.scrollbarThumbHover);
    HashColor(hash, c.shadow);
    HashColor(hash, c.tooltipBackground);
    HashColor(hash, c.tooltipText);
    HashColor(hash, c.tooltipBorder);
    HashColor(hash, c.dockPreviewOverlay);
    HashColor(hash, c.dockTabActive);
    HashColor(hash, c.dockTabInactive);
    HashColor(hash, c.dockTabHover);
    HashColor(hash, c.dockSplitter);
    HashColor(hash, c.dockSplitterHover);

    const fst::ThemeMetrics& m = theme.metrics;
    HashFloat(hash, m.paddingSmall);
    HashFloat(hash, m.paddingMedium);
    HashFloat(hash, m.paddingLarge);
    HashFloat(hash, m.marginSmall);
    HashFloat(hash, m.marginMedium);
    HashFloat(hash, m.marginLarge);
    HashFloat(hash, m.itemSpacing);
    HashFloat(hash, m.buttonHeight);
    HashFloat(hash, m.inputHeight);
    HashFloat(hash, m.sliderHeight);
    HashFloat(hash, m.checkboxSize);
    HashFloat(hash, m.scrollbarWidth);
    HashFloat(hash, m.borderWidth);
    HashFloat(hash, m.borderRadius);
    HashFloat(hash, m.borderRadiusSmall);
    HashFloat(hash, m.borderRadiusLarge);
    HashFloat(hash, m.fontSize);
    HashFloat(hash, m.fontSizeSmall);
    HashFloat(hash, m.fontSizeLarge);
    HashFloat(hash, m.fontSizeHeading);
    HashFloat(hash, m.animationDuration);
    HashFloat(hash, m.shadowSize);
    HashFloat(hash, m.shadowOffset);

    return hash;
}

float RelativeLuminance(const fst::Color& color) {
    return 0.2126f * static_cast<float>(color.r) +
           0.7152f * static_cast<float>(color.g) +
           0.0722f * static_cast<float>(color.b);
}

fst::Color ContrastTextColor(const fst::Color& background) {
    return RelativeLuminance(background) < 145.0f
               ? fst::Color::fromHex(0xf8fafc)
               : fst::Color::fromHex(0x1f2937);
}

void DrawCenteredText(
    fst::IDrawList& dl,
    fst::Font* font,
    const fst::Rect& bounds,
    std::string_view text,
    const fst::Color& color) {
    if (!font || text.empty()) {
        return;
    }
    const fst::Vec2 textSize = font->measureText(text);
    const fst::Vec2 pos(
        bounds.x() + std::max(0.0f, (bounds.width() - textSize.x) * 0.5f),
        bounds.y() + std::max(0.0f, (bounds.height() - textSize.y) * 0.5f));
    dl.addText(font, pos, text, color);
}

void DrawColorPreview(
    fst::Context& ctx,
    const fst::Theme& theme,
    const ColorTarget& target,
    const fst::Rect& bounds) {
    fst::IDrawList& dl = *ctx.activeDrawList();
    fst::Font* font = ctx.font();
    if (!font) {
        return;
    }
    const fst::Color previewColor = *target.value;

    const float outerRadius = std::max(4.0f, theme.metrics.borderRadiusSmall);
    const float innerRadius = std::max(3.0f, theme.metrics.borderRadiusSmall - 1.0f);
    dl.addRectFilled(bounds, theme.colors.panelBackground.darker(0.06f), outerRadius);
    dl.addRect(bounds, theme.colors.border, outerRadius);

    const fst::Rect content = bounds.shrunk(10.0f);
    const fst::Rect titleRect(content.x(), content.y(), content.width(), font ? font->lineHeight() : 14.0f);
    DrawCenteredText(dl, font, titleRect, "Podglad", theme.colors.textSecondary);

    const float sampleTop = titleRect.bottom() + std::max(6.0f, theme.metrics.paddingSmall);
    const float sampleHeight = std::max(40.0f, content.bottom() - sampleTop);
    const fst::Rect sample(content.x(), sampleTop, content.width(), sampleHeight);

    const auto drawSampleLabel = [&](std::string_view text, const fst::Color& color) {
        const fst::Rect labelRect(sample.x(), sample.bottom() - 24.0f, sample.width(), 18.0f);
        DrawCenteredText(dl, font, labelRect, text, color);
    };

    switch (target.preview) {
        case ColorPreviewKind::WindowBackground: {
            dl.addRectFilled(sample, previewColor, innerRadius);
            dl.addRect(sample, theme.colors.border, innerRadius);
            drawSampleLabel("Tlo okna", ContrastTextColor(previewColor));
            break;
        }
        case ColorPreviewKind::PanelBackground: {
            dl.addRectFilled(sample, theme.colors.windowBackground, innerRadius);
            const fst::Rect panel = sample.shrunk(14.0f);
            dl.addRectFilled(panel, previewColor, innerRadius);
            dl.addRect(panel, theme.colors.border, innerRadius);
            drawSampleLabel("Panel", ContrastTextColor(previewColor));
            break;
        }
        case ColorPreviewKind::PopupBackground: {
            dl.addRectFilled(sample, theme.colors.windowBackground, innerRadius);
            const fst::Rect popup(sample.x() + 14.0f, sample.y() + 14.0f, sample.width() - 28.0f, sample.height() - 28.0f);
            dl.addRectFilled(popup, previewColor, innerRadius);
            dl.addRect(popup, theme.colors.border, innerRadius);
            drawSampleLabel("Popup", ContrastTextColor(previewColor));
            break;
        }
        case ColorPreviewKind::Primary: {
            dl.addRectFilled(sample, theme.colors.windowBackground, innerRadius);
            const fst::Rect button(sample.x() + 16.0f, sample.center().y - 16.0f, sample.width() - 32.0f, 32.0f);
            dl.addRectFilled(button, previewColor, innerRadius);
            dl.addRect(button, theme.colors.border, innerRadius);
            DrawCenteredText(dl, font, button, "Primary", theme.colors.primaryText);
            break;
        }
        case ColorPreviewKind::PrimaryText: {
            dl.addRectFilled(sample, theme.colors.windowBackground, innerRadius);
            const fst::Rect button(sample.x() + 16.0f, sample.center().y - 16.0f, sample.width() - 32.0f, 32.0f);
            dl.addRectFilled(button, theme.colors.primary, innerRadius);
            dl.addRect(button, theme.colors.border, innerRadius);
            DrawCenteredText(dl, font, button, "Primary text", previewColor);
            break;
        }
        case ColorPreviewKind::Secondary: {
            dl.addRectFilled(sample, theme.colors.windowBackground, innerRadius);
            const fst::Rect button(sample.x() + 16.0f, sample.center().y - 16.0f, sample.width() - 32.0f, 32.0f);
            dl.addRectFilled(button, previewColor, innerRadius);
            dl.addRect(button, theme.colors.border, innerRadius);
            DrawCenteredText(dl, font, button, "Secondary", ContrastTextColor(previewColor));
            break;
        }
        case ColorPreviewKind::Text: {
            dl.addRectFilled(sample, theme.colors.inputBackground, innerRadius);
            dl.addRect(sample, theme.colors.inputBorder, innerRadius);
            const float x = sample.x() + 12.0f;
            const float y = sample.y() + 10.0f;
            dl.addText(font, fst::Vec2(x, y), "Przykladowy tekst", previewColor);
            dl.addText(font, fst::Vec2(x, y + 20.0f), "Druga linia", theme.colors.textSecondary);
            break;
        }
        case ColorPreviewKind::Border: {
            dl.addRectFilled(sample, theme.colors.inputBackground, innerRadius);
            dl.addRect(sample, previewColor, innerRadius);
            const fst::Rect inner = sample.shrunk(8.0f);
            dl.addRect(inner, previewColor.withAlpha(static_cast<uint8_t>(180)), std::max(2.0f, innerRadius - 1.0f));
            drawSampleLabel("Obramowanie", theme.colors.textSecondary);
            break;
        }
        case ColorPreviewKind::InputBackground: {
            dl.addRectFilled(sample, theme.colors.windowBackground, innerRadius);
            const fst::Rect input(sample.x() + 14.0f, sample.center().y - 16.0f, sample.width() - 28.0f, 32.0f);
            dl.addRectFilled(input, previewColor, innerRadius);
            dl.addRect(input, theme.colors.inputBorder, innerRadius);
            DrawCenteredText(dl, font, input, "Input", theme.colors.text);
            break;
        }
        case ColorPreviewKind::InputBorder: {
            dl.addRectFilled(sample, theme.colors.windowBackground, innerRadius);
            const fst::Rect input(sample.x() + 14.0f, sample.center().y - 16.0f, sample.width() - 28.0f, 32.0f);
            dl.addRectFilled(input, theme.colors.inputBackground, innerRadius);
            dl.addRect(input, previewColor, innerRadius);
            DrawCenteredText(dl, font, input, "Input", theme.colors.text);
            break;
        }
        case ColorPreviewKind::InputFocused: {
            dl.addRectFilled(sample, theme.colors.windowBackground, innerRadius);
            const fst::Rect input(sample.x() + 14.0f, sample.center().y - 16.0f, sample.width() - 28.0f, 32.0f);
            dl.addRectFilled(input, theme.colors.inputBackground, innerRadius);
            dl.addRect(input.expanded(1.5f), previewColor.withAlpha(static_cast<uint8_t>(140)), innerRadius + 1.5f);
            dl.addRect(input, previewColor, innerRadius);
            DrawCenteredText(dl, font, input, "Focused", theme.colors.text);
            break;
        }
        case ColorPreviewKind::Button: {
            dl.addRectFilled(sample, theme.colors.windowBackground, innerRadius);
            const fst::Rect button(sample.x() + 16.0f, sample.center().y - 16.0f, sample.width() - 32.0f, 32.0f);
            dl.addRectFilled(button, previewColor, innerRadius);
            dl.addRect(button, theme.colors.border, innerRadius);
            DrawCenteredText(dl, font, button, "Przycisk", theme.colors.buttonText);
            break;
        }
        case ColorPreviewKind::ButtonText: {
            dl.addRectFilled(sample, theme.colors.windowBackground, innerRadius);
            const fst::Rect button(sample.x() + 16.0f, sample.center().y - 16.0f, sample.width() - 32.0f, 32.0f);
            dl.addRectFilled(button, theme.colors.buttonBackground, innerRadius);
            dl.addRect(button, theme.colors.border, innerRadius);
            DrawCenteredText(dl, font, button, "Przycisk", previewColor);
            break;
        }
        case ColorPreviewKind::Status: {
            dl.addRectFilled(sample, theme.colors.windowBackground, innerRadius);
            const fst::Rect badge(sample.x() + 26.0f, sample.center().y - 14.0f, sample.width() - 52.0f, 28.0f);
            dl.addRectFilled(badge, previewColor, 14.0f);
            DrawCenteredText(dl, font, badge, "STATUS", ContrastTextColor(previewColor));
            break;
        }
        case ColorPreviewKind::Selection: {
            dl.addRectFilled(sample, theme.colors.inputBackground, innerRadius);
            dl.addRect(sample, theme.colors.inputBorder, innerRadius);
            const fst::Rect selected(sample.x() + 12.0f, sample.y() + 12.0f, sample.width() - 24.0f, 28.0f);
            dl.addRectFilled(selected, previewColor, std::max(2.0f, innerRadius - 1.0f));
            DrawCenteredText(dl, font, selected, "Zaznaczenie", theme.colors.selectionText);
            break;
        }
        case ColorPreviewKind::SelectionText: {
            dl.addRectFilled(sample, theme.colors.inputBackground, innerRadius);
            dl.addRect(sample, theme.colors.inputBorder, innerRadius);
            const fst::Rect selected(sample.x() + 12.0f, sample.y() + 12.0f, sample.width() - 24.0f, 28.0f);
            dl.addRectFilled(selected, theme.colors.selection, std::max(2.0f, innerRadius - 1.0f));
            DrawCenteredText(dl, font, selected, "Zaznaczenie", previewColor);
            break;
        }
        case ColorPreviewKind::ScrollbarTrack: {
            dl.addRectFilled(sample, theme.colors.windowBackground, innerRadius);
            const fst::Rect track(sample.right() - 24.0f, sample.y() + 8.0f, 10.0f, sample.height() - 16.0f);
            dl.addRectFilled(track, previewColor, 5.0f);
            const fst::Rect thumb(track.x(), track.y() + 18.0f, track.width(), 28.0f);
            dl.addRectFilled(thumb, theme.colors.scrollbarThumb, 5.0f);
            break;
        }
        case ColorPreviewKind::ScrollbarThumb: {
            dl.addRectFilled(sample, theme.colors.windowBackground, innerRadius);
            const fst::Rect track(sample.right() - 24.0f, sample.y() + 8.0f, 10.0f, sample.height() - 16.0f);
            dl.addRectFilled(track, theme.colors.scrollbarTrack, 5.0f);
            const fst::Rect thumb(track.x(), track.y() + 18.0f, track.width(), 28.0f);
            dl.addRectFilled(thumb, previewColor, 5.0f);
            break;
        }
        case ColorPreviewKind::ScrollbarThumbHover: {
            dl.addRectFilled(sample, theme.colors.windowBackground, innerRadius);
            const fst::Rect track(sample.right() - 24.0f, sample.y() + 8.0f, 10.0f, sample.height() - 16.0f);
            dl.addRectFilled(track, theme.colors.scrollbarTrack, 5.0f);
            const fst::Rect thumb(track.x(), track.y() + 18.0f, track.width(), 28.0f);
            dl.addRectFilled(thumb, previewColor, 5.0f);
            dl.addRect(thumb.expanded(1.0f), previewColor.withAlpha(static_cast<uint8_t>(170)), 6.0f);
            break;
        }
        case ColorPreviewKind::Shadow: {
            dl.addRectFilled(sample, theme.colors.windowBackground, innerRadius);
            const fst::Rect card(sample.x() + 20.0f, sample.y() + 18.0f, sample.width() - 40.0f, sample.height() - 36.0f);
            dl.addShadow(card, previewColor, std::max(4.0f, theme.metrics.shadowSize * 0.8f), innerRadius);
            dl.addRectFilled(card, theme.colors.panelBackground, innerRadius);
            dl.addRect(card, theme.colors.border, innerRadius);
            drawSampleLabel("Cien", theme.colors.textSecondary);
            break;
        }
        case ColorPreviewKind::TooltipBackground: {
            dl.addRectFilled(sample, theme.colors.windowBackground, innerRadius);
            const fst::Rect tip(sample.x() + 14.0f, sample.y() + 14.0f, sample.width() - 28.0f, sample.height() - 28.0f);
            dl.addRectFilled(tip, previewColor, innerRadius);
            dl.addRect(tip, theme.colors.tooltipBorder, innerRadius);
            DrawCenteredText(dl, font, tip, "Tooltip", theme.colors.tooltipText);
            break;
        }
        case ColorPreviewKind::TooltipText: {
            dl.addRectFilled(sample, theme.colors.windowBackground, innerRadius);
            const fst::Rect tip(sample.x() + 14.0f, sample.y() + 14.0f, sample.width() - 28.0f, sample.height() - 28.0f);
            dl.addRectFilled(tip, theme.colors.tooltipBackground, innerRadius);
            dl.addRect(tip, theme.colors.tooltipBorder, innerRadius);
            DrawCenteredText(dl, font, tip, "Tooltip", previewColor);
            break;
        }
        case ColorPreviewKind::TooltipBorder: {
            dl.addRectFilled(sample, theme.colors.windowBackground, innerRadius);
            const fst::Rect tip(sample.x() + 14.0f, sample.y() + 14.0f, sample.width() - 28.0f, sample.height() - 28.0f);
            dl.addRectFilled(tip, theme.colors.tooltipBackground, innerRadius);
            dl.addRect(tip, previewColor, innerRadius);
            DrawCenteredText(dl, font, tip, "Tooltip", theme.colors.tooltipText);
            break;
        }
        case ColorPreviewKind::DockPreviewOverlay: {
            dl.addRectFilled(sample, theme.colors.panelBackground, innerRadius);
            dl.addRect(sample, theme.colors.border, innerRadius);
            const fst::Rect overlay(sample.x() + 14.0f, sample.y() + 12.0f, sample.width() - 28.0f, sample.height() - 24.0f);
            dl.addRectFilled(overlay, previewColor, innerRadius);
            drawSampleLabel("Dock preview", theme.colors.textSecondary);
            break;
        }
        case ColorPreviewKind::DockTab: {
            dl.addRectFilled(sample, theme.colors.windowBackground, innerRadius);
            const fst::Rect tabs(sample.x() + 10.0f, sample.y() + 10.0f, sample.width() - 20.0f, 32.0f);
            dl.addRectFilled(tabs, theme.colors.panelBackground, innerRadius);
            const fst::Rect activeTab(tabs.x() + 6.0f, tabs.y() + 4.0f, 82.0f, tabs.height() - 8.0f);
            dl.addRectFilled(activeTab, previewColor, std::max(2.0f, innerRadius - 1.0f));
            DrawCenteredText(dl, font, activeTab, "Tab", ContrastTextColor(previewColor));
            break;
        }
        case ColorPreviewKind::DockSplitter: {
            dl.addRectFilled(sample, theme.colors.windowBackground, innerRadius);
            const float splitX = sample.x() + sample.width() * 0.55f;
            const fst::Rect left(sample.x(), sample.y(), splitX - sample.x() - 2.0f, sample.height());
            const fst::Rect right(splitX + 2.0f, sample.y(), sample.right() - splitX - 2.0f, sample.height());
            const fst::Rect splitter(splitX - 2.0f, sample.y(), 4.0f, sample.height());
            dl.addRectFilled(left, theme.colors.panelBackground, innerRadius);
            dl.addRectFilled(right, theme.colors.panelBackground, innerRadius);
            dl.addRectFilled(splitter, previewColor);
            break;
        }
        case ColorPreviewKind::DockSplitterHover: {
            dl.addRectFilled(sample, theme.colors.windowBackground, innerRadius);
            const float splitX = sample.x() + sample.width() * 0.55f;
            const fst::Rect left(sample.x(), sample.y(), splitX - sample.x() - 3.0f, sample.height());
            const fst::Rect right(splitX + 3.0f, sample.y(), sample.right() - splitX - 3.0f, sample.height());
            const fst::Rect splitter(splitX - 3.0f, sample.y(), 6.0f, sample.height());
            dl.addRectFilled(left, theme.colors.panelBackground, innerRadius);
            dl.addRectFilled(right, theme.colors.panelBackground, innerRadius);
            dl.addRectFilled(splitter, previewColor);
            break;
        }
        case ColorPreviewKind::Swatch:
        default: {
            dl.addRectFilled(sample, previewColor, innerRadius);
            dl.addRect(sample, theme.colors.border, innerRadius);
            break;
        }
    }
}

} // namespace

void RenderPersonalizationPanel(
    fst::Context& ctx,
    bool& showPersonalizationWindow,
    fst::Theme& editableTheme,
    int& selectedPreset,
    const ApplyPresetThemeFn& applyPresetTheme,
    const ApplyCustomThemeFn& applyCustomTheme) {
    if (!showPersonalizationWindow) {
        return;
    }

    static uint64_t lastAppliedFingerprint = 0;
    static bool hasAppliedFingerprint = false;
    static int selectedColorIndex = 0;
    static bool openThemeColors = true;
    static bool openMetrics = true;
    static bool openStatusAndDocking = false;

    fst::DockableWindowOptions windowOptions;
    windowOptions.open = &showPersonalizationWindow;
    if (!fst::BeginDockableWindow(ctx, "Personalizacja", windowOptions)) {
        return;
    }

    const fst::Rect bounds = ctx.layout().currentBounds();
    beginScrollablePanelContent(ctx, "personalization_scroll", bounds);

    static const std::vector<std::string> presetNames = {"Ciemny", "Jasny", "Retro"};
    selectedPreset = std::clamp(selectedPreset, 0, 2);
    const int presetBefore = selectedPreset;
    (void)fst::ComboBox(ctx, "Motyw bazowy", selectedPreset, presetNames);
    selectedPreset = std::clamp(selectedPreset, 0, 2);

    bool loadedPreset = false;
    auto loadPreset = [&](int preset) {
        selectedPreset = std::clamp(preset, 0, 2);
        applyPresetTheme(selectedPreset);
        editableTheme = ctx.theme();
        lastAppliedFingerprint = ThemeFingerprint(editableTheme);
        hasAppliedFingerprint = true;
        loadedPreset = true;
    };

    if (selectedPreset != presetBefore) {
        loadPreset(selectedPreset);
    }

    if (fst::Button(ctx, "Wczytaj motyw bazowy")) {
        loadPreset(selectedPreset);
    }
    if (fst::Button(ctx, "Odwiez z aktywnego motywu")) {
        editableTheme = ctx.theme();
        lastAppliedFingerprint = ThemeFingerprint(editableTheme);
        hasAppliedFingerprint = true;
    }

    fst::Separator(ctx);
    if (fst::CollapsingHeader(ctx, "Kolory (zaawansowane)", openThemeColors)) {
        std::vector<ColorTarget> colorTargets = {
            {"Tlo okna", &editableTheme.colors.windowBackground, ColorPreviewKind::WindowBackground},
            {"Tlo panelu", &editableTheme.colors.panelBackground, ColorPreviewKind::PanelBackground},
            {"Tlo popup", &editableTheme.colors.popupBackground, ColorPreviewKind::PopupBackground},
            {"Primary", &editableTheme.colors.primary, ColorPreviewKind::Primary},
            {"Primary hover", &editableTheme.colors.primaryHover, ColorPreviewKind::Primary},
            {"Primary active", &editableTheme.colors.primaryActive, ColorPreviewKind::Primary},
            {"Primary text", &editableTheme.colors.primaryText, ColorPreviewKind::PrimaryText},
            {"Secondary", &editableTheme.colors.secondary, ColorPreviewKind::Secondary},
            {"Secondary hover", &editableTheme.colors.secondaryHover, ColorPreviewKind::Secondary},
            {"Secondary active", &editableTheme.colors.secondaryActive, ColorPreviewKind::Secondary},
            {"Tekst", &editableTheme.colors.text, ColorPreviewKind::Text},
            {"Tekst disabled", &editableTheme.colors.textDisabled, ColorPreviewKind::Text},
            {"Tekst secondary", &editableTheme.colors.textSecondary, ColorPreviewKind::Text},
            {"Obramowanie", &editableTheme.colors.border, ColorPreviewKind::Border},
            {"Obramowanie hover", &editableTheme.colors.borderHover, ColorPreviewKind::Border},
            {"Obramowanie focused", &editableTheme.colors.borderFocused, ColorPreviewKind::Border},
            {"Tlo inputu", &editableTheme.colors.inputBackground, ColorPreviewKind::InputBackground},
            {"Obramowanie inputu", &editableTheme.colors.inputBorder, ColorPreviewKind::InputBorder},
            {"Input focused", &editableTheme.colors.inputFocused, ColorPreviewKind::InputFocused},
            {"Przycisk", &editableTheme.colors.buttonBackground, ColorPreviewKind::Button},
            {"Przycisk hover", &editableTheme.colors.buttonHover, ColorPreviewKind::Button},
            {"Przycisk active", &editableTheme.colors.buttonActive, ColorPreviewKind::Button},
            {"Tekst przycisku", &editableTheme.colors.buttonText, ColorPreviewKind::ButtonText},
            {"Success", &editableTheme.colors.success, ColorPreviewKind::Status},
            {"Warning", &editableTheme.colors.warning, ColorPreviewKind::Status},
            {"Error", &editableTheme.colors.error, ColorPreviewKind::Status},
            {"Info", &editableTheme.colors.info, ColorPreviewKind::Status},
            {"Selection", &editableTheme.colors.selection, ColorPreviewKind::Selection},
            {"Selection text", &editableTheme.colors.selectionText, ColorPreviewKind::SelectionText},
            {"Scrollbar track", &editableTheme.colors.scrollbarTrack, ColorPreviewKind::ScrollbarTrack},
            {"Scrollbar thumb", &editableTheme.colors.scrollbarThumb, ColorPreviewKind::ScrollbarThumb},
            {"Scrollbar thumb hover", &editableTheme.colors.scrollbarThumbHover, ColorPreviewKind::ScrollbarThumbHover},
            {"Cien", &editableTheme.colors.shadow, ColorPreviewKind::Shadow, true},
            {"Tooltip tlo", &editableTheme.colors.tooltipBackground, ColorPreviewKind::TooltipBackground},
            {"Tooltip tekst", &editableTheme.colors.tooltipText, ColorPreviewKind::TooltipText},
            {"Tooltip border", &editableTheme.colors.tooltipBorder, ColorPreviewKind::TooltipBorder},
            {"Dock preview overlay", &editableTheme.colors.dockPreviewOverlay, ColorPreviewKind::DockPreviewOverlay, true},
            {"Dock tab active", &editableTheme.colors.dockTabActive, ColorPreviewKind::DockTab},
            {"Dock tab inactive", &editableTheme.colors.dockTabInactive, ColorPreviewKind::DockTab},
            {"Dock tab hover", &editableTheme.colors.dockTabHover, ColorPreviewKind::DockTab},
            {"Dock splitter", &editableTheme.colors.dockSplitter, ColorPreviewKind::DockSplitter},
            {"Dock splitter hover", &editableTheme.colors.dockSplitterHover, ColorPreviewKind::DockSplitterHover},
        };

        if (!colorTargets.empty()) {
            selectedColorIndex = std::clamp(selectedColorIndex, 0, static_cast<int>(colorTargets.size()) - 1);
            const int maxColorIndex = static_cast<int>(colorTargets.size()) - 1;
            std::vector<std::string> colorNames;
            colorNames.reserve(colorTargets.size());
            for (const ColorTarget& target : colorTargets) {
                colorNames.push_back(target.name);
            }
            (void)fst::ComboBox(ctx, "Edytowany kolor", selectedColorIndex, colorNames);
            selectedColorIndex = std::clamp(selectedColorIndex, 0, maxColorIndex);

            ColorTarget& current = colorTargets[static_cast<size_t>(selectedColorIndex)];
            fst::Label(ctx, std::string("Aktualnie: ") + current.name);

            const float panelWidth = std::max(260.0f, bounds.width() - 20.0f);
            const float spacing = std::max(6.0f, ctx.theme().metrics.itemSpacing);
            const float previewWidth = std::clamp(panelWidth * 0.35f, 150.0f, 230.0f);
            const float pickerWidth = std::clamp(panelWidth - previewWidth - spacing, 180.0f, 320.0f);
            const float lineHeight = ctx.font() ? ctx.font()->lineHeight() : 14.0f;
            const float pickerHeight = (pickerWidth - 30.0f) + lineHeight + ctx.theme().metrics.paddingSmall + 20.0f;

            fst::BeginHorizontal(ctx, spacing);
            fst::ColorPickerOptions pickerOptions;
            pickerOptions.showAlpha = current.supportsAlpha;
            pickerOptions.style = fst::Style().withWidth(pickerWidth);
            (void)fst::ColorPicker(ctx, "Picker", *current.value, pickerOptions);
            const fst::Rect previewBounds = fst::Allocate(ctx, previewWidth, std::max(170.0f, pickerHeight));
            DrawColorPreview(ctx, editableTheme, current, previewBounds);
            fst::EndHorizontal(ctx);

            int r = current.value->r;
            int g = current.value->g;
            int b = current.value->b;
            int a = current.value->a;
            (void)fst::SliderInt(ctx, "R", r, 0, 255);
            (void)fst::SliderInt(ctx, "G", g, 0, 255);
            (void)fst::SliderInt(ctx, "B", b, 0, 255);
            if (current.supportsAlpha) {
                (void)fst::SliderInt(ctx, "A", a, 0, 255);
            }

            current.value->r = static_cast<uint8_t>(std::clamp(r, 0, 255));
            current.value->g = static_cast<uint8_t>(std::clamp(g, 0, 255));
            current.value->b = static_cast<uint8_t>(std::clamp(b, 0, 255));
            if (current.supportsAlpha) {
                current.value->a = static_cast<uint8_t>(std::clamp(a, 0, 255));
            } else {
                current.value->a = 255;
            }
        }
    }

    if (fst::CollapsingHeader(ctx, "Metryki i rozmiary", openMetrics)) {
        fst::SliderOptions metricSlider;
        metricSlider.decimals = 1;

        (void)fst::Slider(ctx, "Padding small", editableTheme.metrics.paddingSmall, 0.0f, 16.0f, metricSlider);
        (void)fst::Slider(ctx, "Padding medium", editableTheme.metrics.paddingMedium, 0.0f, 24.0f, metricSlider);
        (void)fst::Slider(ctx, "Padding large", editableTheme.metrics.paddingLarge, 0.0f, 40.0f, metricSlider);
        (void)fst::Slider(ctx, "Margin small", editableTheme.metrics.marginSmall, 0.0f, 16.0f, metricSlider);
        (void)fst::Slider(ctx, "Margin medium", editableTheme.metrics.marginMedium, 0.0f, 24.0f, metricSlider);
        (void)fst::Slider(ctx, "Margin large", editableTheme.metrics.marginLarge, 0.0f, 40.0f, metricSlider);
        (void)fst::Slider(ctx, "Item spacing", editableTheme.metrics.itemSpacing, 0.0f, 24.0f, metricSlider);
        (void)fst::Slider(ctx, "Wysokosc przycisku", editableTheme.metrics.buttonHeight, 20.0f, 64.0f, metricSlider);
        (void)fst::Slider(ctx, "Wysokosc inputu", editableTheme.metrics.inputHeight, 20.0f, 64.0f, metricSlider);
        (void)fst::Slider(ctx, "Wysokosc suwaka", editableTheme.metrics.sliderHeight, 12.0f, 40.0f, metricSlider);
        (void)fst::Slider(ctx, "Rozmiar checkboxa", editableTheme.metrics.checkboxSize, 12.0f, 36.0f, metricSlider);
        (void)fst::Slider(ctx, "Szerokosc scrollbara", editableTheme.metrics.scrollbarWidth, 6.0f, 30.0f, metricSlider);
        (void)fst::Slider(ctx, "Szerokosc obramowania", editableTheme.metrics.borderWidth, 0.0f, 4.0f, metricSlider);
        (void)fst::Slider(ctx, "Promien rogow", editableTheme.metrics.borderRadius, 0.0f, 24.0f, metricSlider);
        (void)fst::Slider(ctx, "Promien rogow small", editableTheme.metrics.borderRadiusSmall, 0.0f, 24.0f, metricSlider);
        (void)fst::Slider(ctx, "Promien rogow large", editableTheme.metrics.borderRadiusLarge, 0.0f, 32.0f, metricSlider);
        (void)fst::Slider(ctx, "Font size", editableTheme.metrics.fontSize, 10.0f, 24.0f, metricSlider);
        (void)fst::Slider(ctx, "Font size small", editableTheme.metrics.fontSizeSmall, 8.0f, 20.0f, metricSlider);
        (void)fst::Slider(ctx, "Font size large", editableTheme.metrics.fontSizeLarge, 12.0f, 34.0f, metricSlider);
        (void)fst::Slider(ctx, "Font heading", editableTheme.metrics.fontSizeHeading, 16.0f, 42.0f, metricSlider);
        (void)fst::Slider(ctx, "Anim duration", editableTheme.metrics.animationDuration, 0.0f, 0.6f, metricSlider);
        (void)fst::Slider(ctx, "Shadow size", editableTheme.metrics.shadowSize, 0.0f, 20.0f, metricSlider);
        (void)fst::Slider(ctx, "Shadow offset", editableTheme.metrics.shadowOffset, 0.0f, 12.0f, metricSlider);
    }

    if (fst::CollapsingHeader(ctx, "Status i docking", openStatusAndDocking)) {
        (void)fst::ColorPicker(ctx, "Success", editableTheme.colors.success);
        (void)fst::ColorPicker(ctx, "Warning", editableTheme.colors.warning);
        (void)fst::ColorPicker(ctx, "Error", editableTheme.colors.error);
        (void)fst::ColorPicker(ctx, "Info", editableTheme.colors.info);
        (void)fst::ColorPicker(ctx, "Dock splitter", editableTheme.colors.dockSplitter);
        (void)fst::ColorPicker(ctx, "Dock splitter hover", editableTheme.colors.dockSplitterHover);
    }

    if (!loadedPreset) {
        const uint64_t currentFingerprint = ThemeFingerprint(editableTheme);
        if (!hasAppliedFingerprint || currentFingerprint != lastAppliedFingerprint) {
            applyCustomTheme(editableTheme);
            lastAppliedFingerprint = currentFingerprint;
            hasAppliedFingerprint = true;
        }
    }

    fst::LabelSecondary(ctx, "Zmiany sa stosowane na zywo.");

    endScrollablePanelContent(ctx, "personalization_scroll", bounds);
    fst::EndDockableWindow(ctx);
}

} // namespace fin
