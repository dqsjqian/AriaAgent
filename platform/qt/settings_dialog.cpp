// AriaAgent — settings dialog implementation.
#include "settings_dialog.hpp"

#include "main_window.hpp"
#include "theme.hpp"

#include "i18n/I18n.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTextStream>
#include <QVBoxLayout>

namespace {

using agent_ui::g_theme;

const char* kSettingsOrg  = "AriaAgent";
const char* kSettingsApp  = "AriaAgent";

QString env_or(const char* name, const QString& fallback) {
    const char* v = std::getenv(name);
    return (v && *v) ? QString::fromUtf8(v) : fallback;
}

} // namespace

SettingsDialog::SettingsDialog(QWidget* parent, int initialPage) : QDialog(parent) {
    setWindowTitle(QString::fromStdString(agent::i18n::str("settings_title")));
    resize(720, 520);

    nav_ = new QListWidget(this);
    nav_->setFixedWidth(170);
    nav_->addItems({
        QString::fromStdString(agent::i18n::str("nav_general")),
        QString::fromStdString(agent::i18n::str("nav_model")),
        QString::fromStdString(agent::i18n::str("nav_plugins")),
        QString::fromStdString(agent::i18n::str("nav_presets")),
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
    nav_->setCurrentRow(initialPage);   // jump straight to the requested page (default: Model)

    auto* save_btn = new QPushButton(QString::fromStdString(agent::i18n::str("save")), this);
    save_btn->setObjectName(QStringLiteral("primary"));
    save_btn->setCursor(Qt::PointingHandCursor);
    auto* cancel_btn = new QPushButton(QString::fromStdString(agent::i18n::str("cancel")), this);
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
    prompt_edit_->setText(env_or("ARIA_LLM_SYSTEM_PROMPT",
        QString::fromStdString(agent::i18n::str("default_system_prompt"))));

    theme_combo_ = new QComboBox(card);
    theme_combo_->addItems({
        QString::fromStdString(agent::i18n::str("gen_theme_follow")),
        QString::fromStdString(agent::i18n::str("gen_theme_light")),
        QString::fromStdString(agent::i18n::str("gen_theme_dark")),
    });
    {
        QSettings s(kSettingsOrg, kSettingsApp);
        theme_combo_->setCurrentIndex(s.value("theme", 2).toInt());
    }

    lang_combo_ = new QComboBox(card);
    lang_combo_->addItem(QString::fromStdString(agent::i18n::str("lang_zh")), QStringLiteral("zh-CN"));
    lang_combo_->addItem(QString::fromStdString(agent::i18n::str("lang_en")),   QStringLiteral("en"));
    {
        QSettings s(kSettingsOrg, kSettingsApp);
        const QString cur = s.value("language", "zh-CN").toString();
        const int idx = lang_combo_->findData(cur);
        lang_combo_->setCurrentIndex(idx >= 0 ? idx : 0);
    }

    enter_combo_ = new QComboBox(card);
    enter_combo_->addItems({
        QString::fromStdString(agent::i18n::str("gen_enter_send")),
        QString::fromStdString(agent::i18n::str("gen_ctrl_enter")),
    });

    stream_check_ = new QCheckBox(
        QString::fromStdString(agent::i18n::str("gen_streaming")), card);
    stream_check_->setChecked(true);

    form->addRow(QString::fromStdString(agent::i18n::str("gen_system_prompt")), prompt_edit_);
    form->addRow(QString::fromStdString(agent::i18n::str("gen_language")), lang_combo_);
    form->addRow(QString::fromStdString(agent::i18n::str("gen_appearance")), theme_combo_);
    form->addRow(QString::fromStdString(agent::i18n::str("gen_enter_behavior")), enter_combo_);
    form->addRow(QString(), stream_check_);

    stack->addWidget(card);
}

// ── Model ───────────────────────────────────────────────────────────────────
void SettingsDialog::build_model_page(QStackedWidget* stack) {
    auto* card = new QWidget(this);
    auto* form = new QFormLayout(card);
    form->setSpacing(14);

    QSettings s(kSettingsOrg, kSettingsApp);
    base_url_edit_ = new QLineEdit(card);
    base_url_edit_->setPlaceholderText(QStringLiteral("https://api.deepseek.com"));
    base_url_edit_->setText(s.value("base_url",
        env_or("ARIA_LLM_BASE_URL", QStringLiteral("https://api.deepseek.com"))).toString());

    api_key_edit_ = new QLineEdit(card);
    api_key_edit_->setEchoMode(QLineEdit::Password);
    api_key_edit_->setPlaceholderText(QStringLiteral("sk-…"));
    api_key_edit_->setText(s.value("api_key").toString());

    model_edit_ = new QLineEdit(card);
    model_edit_->setPlaceholderText(QStringLiteral("deepseek-chat"));
    model_edit_->setText(s.value("model",
        env_or("ARIA_LLM_MODEL", QStringLiteral("deepseek-chat"))).toString());

    form->addRow(QString::fromStdString(agent::i18n::str("model_base_url")), base_url_edit_);
    form->addRow(QString::fromStdString(agent::i18n::str("model_api_key")), api_key_edit_);
    form->addRow(QString::fromStdString(agent::i18n::str("model_name")), model_edit_);

    auto* hint = new QLabel(QString::fromStdString(agent::i18n::str("model_hint")), card);
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

    auto make_plugin = [&](const QString& name, const QString& desc, bool checked) {
        auto* box = new QGroupBox(name, card);
        auto* bv = new QVBoxLayout(box);
        auto* cb = new QCheckBox(QString::fromStdString(agent::i18n::str("plugins_enable")), box);
        cb->setChecked(checked);
        auto* lab = new QLabel(desc, box);
        lab->setWordWrap(true);
        lab->setStyleSheet(QStringLiteral("color:%1; font-size:12px;").arg(QString::fromUtf8(g_theme.text_dim)));
        bv->addWidget(cb);
        bv->addWidget(lab);
        v->addWidget(box);
    };

    make_plugin(QString::fromStdString(agent::i18n::str("plugins_calculator")),
                QString::fromStdString(agent::i18n::str("plugins_calculator_desc")), true);
    make_plugin(QString::fromStdString(agent::i18n::str("plugins_time")),
                QString::fromStdString(agent::i18n::str("plugins_time_desc")), true);

    auto* hint = new QLabel(QString::fromStdString(agent::i18n::str("plugins_hint")), card);
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
        QString::fromStdString(agent::i18n::str("preset_standard")),
        QString::fromStdString(agent::i18n::str("preset_creative")),
        QString::fromStdString(agent::i18n::str("preset_minimal")),
    });
    {
        QSettings s(kSettingsOrg, kSettingsApp);
        preset_combo_->setCurrentIndex(s.value("preset", 0).toInt());
    }
    v->addWidget(preset_combo_);

    auto* hint = new QLabel(QString::fromStdString(agent::i18n::str("presets_hint")), card);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color:%1; font-size:12px;").arg(QString::fromUtf8(g_theme.text_dim)));
    v->addWidget(hint);
    v->addStretch();

    stack->addWidget(card);
}

// ── Persistence ─────────────────────────────────────────────────────────────
void SettingsDialog::save() {
    QSettings s(kSettingsOrg, kSettingsApp);
    s.setValue("base_url", base_url_edit_->text().trimmed());
    s.setValue("api_key",  api_key_edit_->text().trimmed());
    s.setValue("model",    model_edit_->text().trimmed());
    s.setValue("system_prompt", prompt_edit_->text());
    s.setValue("theme", theme_combo_->currentIndex());
    if (preset_combo_) s.setValue("preset", preset_combo_->currentIndex());
    if (lang_combo_) {
        s.setValue("language", lang_combo_->currentData().toString());
        agent::i18n::set_language(lang_combo_->currentData().toString().toStdString());
    }

    // Make the config visible to create_llm_client() at next run.
    qputenv("ARIA_LLM_BASE_URL", base_url_edit_->text().trimmed().toUtf8());
    qputenv("ARIA_LLM_MODEL",    model_edit_->text().trimmed().toUtf8());
    if (!api_key_edit_->text().trimmed().isEmpty()) {
        qputenv("ARIA_LLM_API_KEY", api_key_edit_->text().trimmed().toUtf8());
    }
    qputenv("ARIA_LLM_SYSTEM_PROMPT", prompt_edit_->text().toUtf8());

    // Preset → system prompt override (unless the user edited the prompt).
    if (preset_combo_) {
        const int p = preset_combo_->currentIndex();
        const char* prompt = (p == 1) ? "You are a creative assistant. Think "
                                       "divergently, explore unusual angles, "
                                       "and offer original ideas."
                                     : (p == 2) ? "You are a concise assistant. "
                                       "Answer directly and briefly, without "
                                       "excess detail."
                                     : nullptr;
        if (prompt) {
            qputenv("ARIA_LLM_SYSTEM_PROMPT", prompt);
            s.setValue("system_prompt", QString::fromUtf8(prompt));
        }
    }

    // Theme change must take effect immediately on the main window.
    if (auto* w = qobject_cast<MainWindow*>(parent())) {
        w->apply_theme();
    }
}

// ── Accessors ───────────────────────────────────────────────────────────────
QString SettingsDialog::baseUrl() const { return base_url_edit_->text().trimmed(); }
QString SettingsDialog::apiKey()  const { return api_key_edit_->text().trimmed(); }
QString SettingsDialog::model()   const { return model_edit_->text().trimmed(); }
QString SettingsDialog::systemPrompt() const { return prompt_edit_->text(); }
