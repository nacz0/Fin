#include "App/FinI18n.h"

#include "fastener/fastener.h"

#include <algorithm>
#include <array>
#include <cctype>

namespace fin {

namespace {

struct TranslationEntry {
    const char* key;
    const char* pl;
    const char* en;
};

constexpr TranslationEntry kTranslations[] = {
    {"locale.polish", "Polski", "Polish"},
    {"locale.english", "Angielski", "English"},

    {"window.explorer", "Eksplorator", "Explorer"},
    {"window.editor", "Edytor", "Editor"},
    {"window.console", "Konsola", "Console"},
    {"window.lsp_diagnostics", "Diagnostyka LSP", "LSP Diagnostics"},
    {"window.terminal", "Terminal", "Terminal"},
    {"window.settings", "Ustawienia", "Settings"},
    {"window.personalization", "Personalizacja", "Personalization"},
    {"window.completion", "Autouzupelnianie", "Autocomplete"},

    {"menu.file", "Plik", "File"},
    {"menu.edit", "Edycja", "Edit"},
    {"menu.build", "Buduj", "Build"},
    {"menu.view", "Widok", "View"},
    {"menu.file.new", "Nowy", "New"},
    {"menu.file.open", "Otworz...", "Open..."},
    {"menu.file.save", "Zapisz", "Save"},
    {"menu.file.close_tab", "Zamknij karte", "Close tab"},
    {"menu.file.exit", "Zakoncz", "Exit"},
    {"menu.edit.find", "Szukaj", "Find"},
    {"menu.edit.autocomplete", "Autouzupelnianie", "Autocomplete"},
    {"menu.build.run", "Kompiluj i uruchom", "Build and run"},
    {"menu.view.explorer", "Eksplorator", "Explorer"},
    {"menu.view.editor", "Edytor", "Editor"},
    {"menu.view.console", "Konsola", "Console"},
    {"menu.view.lsp_diagnostics", "Diagnostyka LSP", "LSP Diagnostics"},
    {"menu.view.terminal", "Terminal", "Terminal"},
    {"menu.view.settings", "Ustawienia", "Settings"},
    {"menu.view.personalization", "Personalizacja", "Personalization"},
    {"menu.view.minimap", "Minimapa", "Minimap"},
    {"menu.view.theme_dark", "Motyw: Ciemny", "Theme: Dark"},
    {"menu.view.theme_light", "Motyw: Jasny", "Theme: Light"},
    {"menu.view.theme_retro", "Motyw: Retro", "Theme: Retro"},
    {"menu.view.theme_classic", "Motyw: Klasyczny IDE", "Theme: Classic IDE"},

    {"theme.dark", "Ciemny", "Dark"},
    {"theme.light", "Jasny", "Light"},
    {"theme.retro", "Retro", "Retro"},
    {"theme.classic", "Klasyczny IDE", "Classic IDE"},

    {"settings.autocomplete_lsp", "Autouzupelnianie (LSP)", "Autocomplete (LSP)"},
    {"settings.build_clang", "Budowanie przez clang++", "Build with clang++"},
    {"settings.auto_brackets", "Auto-domykanie nawiasow", "Auto-close brackets"},
    {"settings.smart_indent", "Smart indent", "Smart indent"},
    {"settings.minimap", "Minimapa edytora", "Editor minimap"},
    {"settings.theme", "Motyw", "Theme"},
    {"settings.zoom", "Zoom", "Zoom"},
    {"settings.language", "Jezyk", "Language"},

    {"status.ready", "Gotowy", "Ready"},
    {"status.compilation_ready", "Gotowy.", "Ready."},
    {"status.lsp_start_failed", "Nie mozna uruchomic clangd (LSP).", "Cannot start clangd (LSP)."},
    {"status.lsp_started", "LSP uruchomiony.", "LSP started."},
    {"status.enter_file_path", "Podaj sciezke pliku.", "Provide a file path."},
    {"status.file_not_found", "Plik nie istnieje: {0}", "File does not exist: {0}"},
    {"status.not_a_file", "To nie jest plik: {0}", "This is not a file: {0}"},
    {"status.unsupported_file_type", "Tego typu pliku nie mozna otworzyc w edytorze: {0}", "This file type cannot be opened in the editor: {0}"},
    {"status.opened", "Otworzono: {0}", "Opened: {0}"},
    {"status.invalid_file_path", "Nieprawidlowa sciezka pliku.", "Invalid file path."},
    {"status.saved", "Zapisano: {0}", "Saved: {0}"},
    {"status.no_active_tab", "Brak aktywnej karty.", "No active tab."},
    {"status.save_first", "Najpierw zapisz plik.", "Save the file first."},
    {"status.no_compiler", "Brak kompilatora clang++ i g++ w PATH.", "No clang++ or g++ compiler found in PATH."},
    {"status.compiling", "Kompilacja ({0})...", "Compiling ({0})..."},
    {"status.compiling_fallback", "Kompilacja fallback przez {0}.", "Fallback compile via {0}."},
    {"status.compiling_using", "Kompilacja przez {0}.", "Compiling with {0}."},
    {"status.no_suggestions", "Brak podpowiedzi.", "No suggestions."},
    {"status.compilation_finished", "Kompilacja zakonczona.", "Compilation finished."},
    {"statusbar.line", "Lin", "Ln"},
    {"statusbar.col", "Kol", "Col"},
    {"statusbar.diag", "Diag", "Diag"},
    {"statusbar.new_file", "(nowy plik)", "(new file)"},
    {"tab.new_cpp", "Nowy.cpp", "New.cpp"},

    {"completion.fetching", "Pobieranie podpowiedzi...", "Fetching suggestions..."},
    {"completion.none", "Brak podpowiedzi.", "No suggestions."},
    {"completion.source.local", "Zrodlo: lokalne", "Source: local"},
    {"completion.source.lsp", "Zrodlo: LSP", "Source: LSP"},
    {"completion.details", "Szczegoly: {0}", "Details: {0}"},
    {"completion.hint", "Strzalki = wybor, Enter = wstaw, Esc = zamknij", "Arrows = select, Enter = insert, Esc = close"},

    {"find.next", "Dalej", "Next"},
    {"find.previous", "Wstecz", "Previous"},

    {"explorer.path", "Sciezka: {0}", "Path: {0}"},
    {"explorer.up", ".. (w gore)", ".. (up)"},
    {"explorer.dir_prefix", "[DIR] ", "[DIR] "},

    {"console.compilation_result", "Wynik kompilacji", "Compilation result"},
    {"console.errors", "Bledy: {0}", "Errors: {0}"},
    {"console.warnings", "Ostrzezenia: {0}", "Warnings: {0}"},
    {"console.compiler_issues", "Problemy kompilatora:", "Compiler issues:"},
    {"console.duplicates_skipped", "Pominieto duplikaty: {0}", "Skipped duplicates: {0}"},
    {"console.no_location", "(bez lokalizacji)", "(no location)"},
    {"console.level.error", "ERROR", "ERROR"},
    {"console.level.warn", "WARN", "WARN"},
    {"console.no_jump_location", "Brak lokalizacji do przejscia.", "No location to jump to."},
    {"console.no_compiler_errors", "Brak bledow kompilatora.", "No compiler errors."},
    {"console.compilation_output", "Wyjscie kompilacji:", "Compilation output:"},

    {"lsp.no_active_document", "Brak aktywnego dokumentu.", "No active document."},
    {"lsp.file", "Plik: {0}", "File: {0}"},
    {"lsp.errors", "Bledy: {0}", "Error: {0}"},
    {"lsp.warnings", "Ostrzezenia: {0}", "Warning: {0}"},
    {"lsp.infos", "Info: {0}", "Info: {0}"},
    {"lsp.no_diagnostics", "Brak diagnostyk LSP dla aktywnej karty.", "No LSP diagnostics for the active tab."},
    {"lsp.severity.error", "BLAD", "ERROR"},
    {"lsp.severity.warn", "OSTRZ", "WARN"},
    {"lsp.severity.info", "INFO", "INFO"},

    {"personalization.base_theme", "Motyw bazowy", "Base theme"},
    {"personalization.load_base_theme", "Wczytaj motyw bazowy", "Load base theme"},
    {"personalization.refresh_from_active", "Odwiez z aktywnego motywu", "Refresh from active theme"},
    {"personalization.colors_advanced", "Kolory (zaawansowane)", "Colors (advanced)"},
    {"personalization.edited_color", "Edytowany kolor", "Edited color"},
    {"personalization.picker", "Probnik", "Picker"},
    {"personalization.current", "Aktualnie: {0}", "Current: {0}"},
    {"personalization.metrics_sizes", "Metryki i rozmiary", "Metrics and sizes"},
    {"personalization.status_docking", "Status i docking", "Status and docking"},
    {"personalization.live_apply", "Zmiany sa stosowane na zywo.", "Changes are applied live."},

    {"preview.title", "Podglad", "Preview"},
    {"preview.window_background", "Tlo okna", "Window background"},
    {"preview.panel", "Panel", "Panel"},
    {"preview.popup", "Popup", "Popup"},
    {"preview.primary", "Glowny", "Primary"},
    {"preview.primary_text", "Tekst glowny", "Primary text"},
    {"preview.secondary", "Drugorzedny", "Secondary"},
    {"preview.sample_text", "Przykladowy tekst", "Sample text"},
    {"preview.sample_second_line", "Druga linia", "Second line"},
    {"preview.border", "Obramowanie", "Border"},
    {"preview.input", "Pole", "Input"},
    {"preview.focused", "Aktywne", "Focused"},
    {"preview.button", "Przycisk", "Button"},
    {"preview.status", "STATUS", "STATUS"},
    {"preview.selection", "Zaznaczenie", "Selection"},
    {"preview.tooltip", "Podpowiedz", "Tooltip"},
    {"preview.dock_preview", "Dock preview", "Dock preview"},
    {"preview.tab", "Karta", "Tab"},

    {"color.window_background", "Tlo okna", "Window background"},
    {"color.panel_background", "Tlo panelu", "Panel background"},
    {"color.popup_background", "Tlo popup", "Popup background"},
    {"color.primary", "Glowny", "Primary"},
    {"color.primary_hover", "Glowny hover", "Primary hover"},
    {"color.primary_active", "Glowny active", "Primary active"},
    {"color.primary_text", "Tekst glowny", "Primary text"},
    {"color.secondary", "Drugorzedny", "Secondary"},
    {"color.secondary_hover", "Drugorzedny hover", "Secondary hover"},
    {"color.secondary_active", "Drugorzedny active", "Secondary active"},
    {"color.text", "Tekst", "Text"},
    {"color.text_disabled", "Tekst disabled", "Text disabled"},
    {"color.text_secondary", "Tekst secondary", "Text secondary"},
    {"color.border", "Obramowanie", "Border"},
    {"color.border_hover", "Obramowanie hover", "Border hover"},
    {"color.border_focused", "Obramowanie focused", "Border focused"},
    {"color.input_background", "Tlo inputu", "Input background"},
    {"color.input_border", "Obramowanie inputu", "Input border"},
    {"color.input_focused", "Input focused", "Input focused"},
    {"color.button", "Przycisk", "Button"},
    {"color.button_hover", "Przycisk hover", "Button hover"},
    {"color.button_active", "Przycisk active", "Button active"},
    {"color.button_text", "Tekst przycisku", "Button text"},
    {"color.success", "Sukces", "Success"},
    {"color.warning", "Ostrzezenie", "Warning"},
    {"color.error", "Blad", "Error"},
    {"color.info", "Informacja", "Info"},
    {"color.selection", "Zaznaczenie", "Selection"},
    {"color.selection_text", "Tekst zaznaczenia", "Selection text"},
    {"color.scrollbar_track", "Tor scrollbara", "Scrollbar track"},
    {"color.scrollbar_thumb", "Uchwyt scrollbara", "Scrollbar thumb"},
    {"color.scrollbar_thumb_hover", "Uchwyt scrollbara hover", "Scrollbar thumb hover"},
    {"color.shadow", "Cien", "Shadow"},
    {"color.tooltip_background", "Tooltip tlo", "Tooltip background"},
    {"color.tooltip_text", "Tooltip tekst", "Tooltip text"},
    {"color.tooltip_border", "Obramowanie podpowiedzi", "Tooltip border"},
    {"color.dock_preview_overlay", "Nakladka dock preview", "Dock preview overlay"},
    {"color.dock_tab_active", "Karta dock aktywna", "Dock tab active"},
    {"color.dock_tab_inactive", "Karta dock nieaktywna", "Dock tab inactive"},
    {"color.dock_tab_hover", "Karta dock hover", "Dock tab hover"},
    {"color.dock_splitter", "Separator dock", "Dock splitter"},
    {"color.dock_splitter_hover", "Separator dock hover", "Dock splitter hover"},

    {"metrics.padding_small", "Padding maly", "Padding small"},
    {"metrics.padding_medium", "Padding sredni", "Padding medium"},
    {"metrics.padding_large", "Padding duzy", "Padding large"},
    {"metrics.margin_small", "Margines maly", "Margin small"},
    {"metrics.margin_medium", "Margines sredni", "Margin medium"},
    {"metrics.margin_large", "Margines duzy", "Margin large"},
    {"metrics.item_spacing", "Odstep elementow", "Item spacing"},
    {"metrics.button_height", "Wysokosc przycisku", "Button height"},
    {"metrics.input_height", "Wysokosc inputu", "Input height"},
    {"metrics.slider_height", "Wysokosc suwaka", "Slider height"},
    {"metrics.checkbox_size", "Rozmiar checkboxa", "Checkbox size"},
    {"metrics.scrollbar_width", "Szerokosc scrollbara", "Scrollbar width"},
    {"metrics.border_width", "Szerokosc obramowania", "Border width"},
    {"metrics.border_radius", "Promien rogow", "Corner radius"},
    {"metrics.border_radius_small", "Promien rogow small", "Corner radius small"},
    {"metrics.border_radius_large", "Promien rogow large", "Corner radius large"},
    {"metrics.font_size", "Rozmiar czcionki", "Font size"},
    {"metrics.font_size_small", "Rozmiar czcionki maly", "Font size small"},
    {"metrics.font_size_large", "Rozmiar czcionki duzy", "Font size large"},
    {"metrics.font_heading", "Rozmiar naglowka", "Font heading"},
    {"metrics.animation_duration", "Czas animacji", "Animation duration"},
    {"metrics.shadow_size", "Rozmiar cienia", "Shadow size"},
    {"metrics.shadow_offset", "Przesuniecie cienia", "Shadow offset"},
};

} // namespace

std::string NormalizeLocale(const std::string& locale) {
    if (locale.empty()) {
        return "pl";
    }

    std::string lower = locale;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    if (lower.rfind("en", 0) == 0) {
        return "en";
    }
    if (lower.rfind("pl", 0) == 0) {
        return "pl";
    }
    return "pl";
}

void InitializeI18n(const std::string& preferredLocale) {
    fst::I18n& i18n = fst::I18n::instance();
    i18n.clear();
    i18n.setFallbackLocale("en");
    i18n.setReturnKeyIfMissing(true);

    for (const TranslationEntry& entry : kTranslations) {
        i18n.addTranslation("pl", entry.key, entry.pl);
        i18n.addTranslation("en", entry.key, entry.en);
    }

    i18n.setLocale(NormalizeLocale(preferredLocale));
}

void SetLocale(const std::string& locale) {
    fst::I18n::instance().setLocale(NormalizeLocale(locale));
}

std::string GetLocale() {
    return NormalizeLocale(fst::I18n::instance().getLocale());
}

bool IsEnglishLocale() {
    return GetLocale() == "en";
}

} // namespace fin
