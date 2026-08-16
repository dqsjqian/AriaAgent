// AriaAgent — settings dialog implementation (settings module, Qt shell).
#include "settings_dialog.hpp"

#include "app/viewmodel/app_text.hpp"
#include "settings/viewmodel/settings_vm.hpp"
#include "theme.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace {

using agent_ui::g_theme;

} // namespace

SettingsDialog::SettingsDialog(SettingsVm* vm, AppText* texts,
                               QWidget* parent, int initialPage)
    : QDialog(parent), vm_(vm), texts_(texts) {
    setWindowTitle(QString::fromStdString(vm_->title.get()));
    resize(720, 520);

    nav_ = new QListWidget(this);
    nav_->setFixedWidth(170);
    nav_->addItems({
        QString::fromStdString(vm_->nav_general.get()),
        QString::fromStdString(vm_->nav_model.get()),
        QString::fromStdString(vm_->nav_plugins.get()),
        QString::fromStdString(vm_->nav_presets.get()),
    });
    nav_->setStyleSheet(QStringLiteral(
        "QListWidget { background:%1; border:none; border-radius:10px; padding:8px; }"
        "QListWidget::item { padding:12px 14px; border-radius:8px; font-size:14px; }"
        "QListWidget::item:hover { background:%2; }"
        "QListWidget::item:selected { background:%2; color:%3; }")
        .arg(QString::fromUtf8(g_theme.panel),
             QString::fromUtf8(g_theme.panel2),
             QString::fromUtf8(g_theme.accent)));

    stack_ = new QStackedWidget(this);
    build_general_page(stack_);
    build_model_page(stack_);
    build_plugins_page(stack_);
    build_presets_page(stack_);

    connect(nav_, &QListWidget::currentRowChanged,
            stack_, &QStackedWidget::setCurrentIndex);
    nav_->setCurrentRow(initialPage);

    auto* save_btn = new QPushButton(QString::fromStdString(vm_->save_text.get()), this);
    save_btn->setObjectName(QStringLiteral("primary"));
    save_btn->setCursor(Qt::PointingHandCursor);
    auto* cancel_btn = new QPushButton(QString::fromStdString(vm_->cancel_text.get()), this);
    cancel_btn->setCursor(Qt::PointingHandCursor);

    auto* btns = new QHBoxLayout;
    btns->addStretch();
    btns->addWidget(cancel_btn);
    btns->addWidget(save_btn);

    auto* body = new QHBoxLayout;
    body->setSpacing(14);
    body->addWidget(nav_);
    body->addWidget(stack_, 1);

    auto* root = new QVBoxLayout;
    root->setContentsMargins(18, 18, 18, 14);
    root->addLayout(body, 1);
    root->addLayout(btns);
    setLayout(root);

    connect(save_btn, &QPushButton::clicked, this, [this] { save(); accept(); });
    connect(cancel_btn, &QPushButton::clicked, this, &QDialog::reject);
}

// ── General ─────────────────────────────────────────────────────────────────
void SettingsDialog::build_general_page(QStackedWidget* stack) {
    auto* card = new QWidget(this);
    auto* form = new QFormLayout(card);
    form->setSpacing(14);

    prompt_edit_ = new QLineEdit(card);
    prompt_edit_->setText(QString::fromStdString(vm_->system_prompt.get()));

    theme_combo_ = new QComboBox(card);
    theme_combo_->addItems({
        QString::fromStdString(vm_->label_theme_follow.get()),
        QString::fromStdString(vm_->label_theme_light.get()),
        QString::fromStdString(vm_->label_theme_dark.get()),
    });
    theme_combo_->setCurrentIndex(vm_->theme.get());

    lang_combo_ = new QComboBox(card);
    lang_combo_->addItem(QString::fromStdString(vm_->lang_zh.get()), QStringLiteral("zh-CN"));
    lang_combo_->addItem(QString::fromStdString(vm_->lang_en.get()),   QStringLiteral("en"));
    const int li = lang_combo_->findData(QString::fromStdString(vm_->language.get()));
    lang_combo_->setCurrentIndex(li >= 0 ? li : 0);

    enter_combo_ = new QComboBox(card);
    enter_combo_->addItems({
        QString::fromStdString(vm_->label_enter_send.get()),
        QString::fromStdString(vm_->label_ctrl_enter.get()),
    });
    enter_combo_->setCurrentIndex(vm_->enter_behavior.get());

    stream_check_ = new QCheckBox(
        QString::fromStdString(vm_->label_streaming.get()), card);
    stream_check_->setChecked(vm_->streaming.get());

    form->addRow(QString::fromStdString(vm_->label_system_prompt.get()), prompt_edit_);
    form->addRow(QString::fromStdString(vm_->label_language.get()), lang_combo_);
    form->addRow(QString::fromStdString(vm_->label_appearance.get()), theme_combo_);
    form->addRow(QString::fromStdString(vm_->label_enter_behavior.get()), enter_combo_);
    form->addRow(QString(), stream_check_);

    stack->addWidget(card);
}

// ── Model ───────────────────────────────────────────────────────────────────
void SettingsDialog::build_model_page(QStackedWidget* stack) {
    auto* card = new QWidget(this);
    auto* form = new QFormLayout(card);
    form->setSpacing(14);

    base_url_edit_ = new QLineEdit(card);
    base_url_edit_->setPlaceholderText(QStringLiteral("https://api.deepseek.com"));
    base_url_edit_->setText(QString::fromStdString(vm_->base_url.get()));

    api_key_edit_ = new QLineEdit(card);
    api_key_edit_->setEchoMode(QLineEdit::Password);
    api_key_edit_->setPlaceholderText(QStringLiteral("sk-…"));
    api_key_edit_->setText(QString::fromStdString(vm_->api_key.get()));

    model_edit_ = new QLineEdit(card);
    model_edit_->setPlaceholderText(QStringLiteral("deepseek-chat"));
    model_edit_->setText(QString::fromStdString(vm_->model.get()));

    form->addRow(QString::fromStdString(vm_->label_base_url.get()), base_url_edit_);
    form->addRow(QString::fromStdString(vm_->label_api_key.get()), api_key_edit_);
    form->addRow(QString::fromStdString(vm_->label_model_name.get()), model_edit_);

    auto* hint = new QLabel(QString::fromStdString(vm_->hint_model.get()), card);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color:%1; font-size:12px;").arg(QString::fromUtf8(g_theme.text_dim)));
    form->addRow(QString(), hint);

    stack->addWidget(card);
}

// ── Plugins ─────────────────────────────────────────────────────────────────
void SettingsDialog::build_plugins_page(QStackedWidget* stack) {
    auto* card = new QWidget(this);
    auto* v = new QVBoxLayout(card);
    v->setSpacing(12);

    auto make_plugin = [&](const QString& name, const QString& desc) {
        auto* box = new QGroupBox(name, card);
        auto* bv = new QVBoxLayout(box);
        auto* cb = new QCheckBox(QString::fromStdString(vm_->plugins_enable.get()), box);
        cb->setChecked(true);
        auto* lab = new QLabel(desc, box);
        lab->setWordWrap(true);
        lab->setStyleSheet(QStringLiteral("color:%1; font-size:12px;").arg(QString::fromUtf8(g_theme.text_dim)));
        bv->addWidget(cb);
        bv->addWidget(lab);
        v->addWidget(box);
    };

    make_plugin(QString::fromStdString(vm_->plugins_calculator.get()),
                QString::fromStdString(vm_->plugins_calculator_desc.get()));
    make_plugin(QString::fromStdString(vm_->plugins_time.get()),
                QString::fromStdString(vm_->plugins_time_desc.get()));

    auto* hint = new QLabel(QString::fromStdString(vm_->hint_plugins.get()), card);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color:%1; font-size:12px;").arg(QString::fromUtf8(g_theme.text_dim)));
    v->addWidget(hint);
    v->addStretch();

    stack->addWidget(card);
}

// ── Presets ─────────────────────────────────────────────────────────────────
void SettingsDialog::build_presets_page(QStackedWidget* stack) {
    auto* card = new QWidget(this);
    auto* v = new QVBoxLayout(card);
    v->setSpacing(12);

    preset_combo_ = new QComboBox(card);
    preset_combo_->addItems({
        QString::fromStdString(vm_->preset_standard.get()),
        QString::fromStdString(vm_->preset_creative.get()),
        QString::fromStdString(vm_->preset_minimal.get()),
    });
    preset_combo_->setCurrentIndex(vm_->preset.get());
    v->addWidget(preset_combo_);

    auto* hint = new QLabel(QString::fromStdString(vm_->hint_presets.get()), card);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color:%1; font-size:12px;").arg(QString::fromUtf8(g_theme.text_dim)));
    v->addWidget(hint);
    v->addStretch();

    stack->addWidget(card);
}

// ── Persistence (delegated to the SettingsVm → SettingsStore) ───────────────
void SettingsDialog::save() {
    vm_->base_url       = base_url_edit_->text().trimmed().toStdString();
    vm_->api_key        = api_key_edit_->text().trimmed().toStdString();
    vm_->model          = model_edit_->text().trimmed().toStdString();
    vm_->system_prompt  = prompt_edit_->text().toStdString();
    vm_->theme          = theme_combo_->currentIndex();
    vm_->language       = lang_combo_->currentData().toString().toStdString();
    vm_->enter_behavior = enter_combo_->currentIndex();
    vm_->streaming      = stream_check_->isChecked();
    vm_->preset         = preset_combo_->currentIndex();

    vm_->save();   // persists + injects env + switches language + notifies shell
}
