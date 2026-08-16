// AriaAgent — settings dialog (DeepSeek-harness-style: left nav + right cards).
#pragma once

#include <QDialog>

class QListWidget;
class QStackedWidget;
class QLineEdit;
class QComboBox;
class QCheckBox;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr, int initialPage = 1);

    // Current effective config (reads env defaults, overwritten by saved).
    QString baseUrl() const;
    QString apiKey() const;
    QString model() const;
    QString systemPrompt() const;

private:
    void build_general_page(QStackedWidget* stack);
    void build_model_page(QStackedWidget* stack);
    void build_plugins_page(QStackedWidget* stack);
    void build_presets_page(QStackedWidget* stack);
    void save();

    QListWidget*   nav_;
    QStackedWidget* stack_;

    // model page
    QLineEdit*  base_url_edit_;
    QLineEdit*  api_key_edit_;
    QLineEdit*  model_edit_;

    // general page
    QLineEdit*  prompt_edit_;
    QComboBox*  theme_combo_;
    QComboBox*  enter_combo_;
    QCheckBox*  stream_check_;

    // presets page
    QComboBox*  preset_combo_;
};
