// AriaAgent — settings dialog (settings module, Qt shell).
// DeepSeek-harness-style: left nav + right cards. All text comes from the
// SettingsVm Properties (auto-localized); edits write back to the VM and
// save() persists through the SettingsStore service.
#pragma once

#include <QDialog>

class QListWidget;
class QStackedWidget;
class QLineEdit;
class QComboBox;
class QCheckBox;

class AppText;
class SettingsVm;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(SettingsVm* vm, AppText* texts,
                            QWidget* parent = nullptr, int initialPage = 1);

private:
    void build_general_page(QStackedWidget* stack);
    void build_model_page(QStackedWidget* stack);
    void build_plugins_page(QStackedWidget* stack);
    void build_presets_page(QStackedWidget* stack);
    void save();

    SettingsVm* vm_;
    AppText*    texts_;

    QListWidget*   nav_;
    QStackedWidget* stack_;

    // model page
    QLineEdit*  base_url_edit_;
    QLineEdit*  api_key_edit_;
    QComboBox*  model_edit_;

    // general page
    QLineEdit*  prompt_edit_;
    QComboBox*  theme_combo_;
    QComboBox*  lang_combo_;
    QComboBox*  enter_combo_;
    QCheckBox*  stream_check_;

    // presets page
    QComboBox*  preset_combo_;
};
