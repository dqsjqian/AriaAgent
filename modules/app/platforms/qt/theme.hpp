// AriaAgent — shared theme definition (dark / light / system).
//
// The palette is defined once here so every painted surface — main window
// stylesheets, bubble/trajectory delegates, markdown renderer and the
// settings dialog — reads the *same* active theme. MainWindow::apply_theme()
// reloads `g_theme` and repaints everything after a settings change.
#pragma once

#include <QApplication>
#include <QPalette>
#include <QSettings>
#include <QString>

namespace agent_ui {

struct Theme {
    const char* bg;          // window background
    const char* panel;       // sidebar / panels
    const char* panel2;      // raised surfaces (input, tags)
    const char* border;      // borders / subtle strokes
    const char* bubble_user;
    const char* bubble_asst;
    const char* bubble_tool;
    const char* text;        // primary text
    const char* text_dim;    // secondary text
    const char* accent;
};

inline constexpr Theme kThemeDark = {
    "#0f1117", "#1a1d27", "#21252f", "#2a2f3a",
    "#3b82f6", "#1f2330", "#1e2940",
    "#e5e7eb", "#8b93a3", "#3b82f6",
};

inline constexpr Theme kThemeLight = {
    "#f5f6f8", "#ffffff", "#eef0f4", "#d5dae3",
    "#3b82f6", "#f0f2f6", "#e8edf7",
    "#1f2430", "#6b7280", "#3b82f6",
};

// Active theme (global). MainWindow::apply_theme() refreshes it on startup
// and whenever the user changes the theme in Settings.
inline Theme g_theme = kThemeDark;

inline bool theme_is_light() {
    const QString bg = QString::fromUtf8(g_theme.bg);
    return QColor(bg).lightness() > 128;
}

/// Resolve a palette from a theme index (0=system, 1=light, 2=dark).
/// The SettingsVm owns the value; the view controller passes it here.
inline Theme resolve_theme(int t) {
    if (t == 1) return kThemeLight;
    if (t == 0) {
        const QColor win = QApplication::palette().color(QPalette::Window);
        return win.lightness() > 128 ? kThemeLight : kThemeDark;
    }
    return kThemeDark;
}

} // namespace agent_ui
