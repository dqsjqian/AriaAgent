// AriaAgent — settings dialog implementation.
#include "ui/settings_dialog.hpp"

#include "ui/main_window.hpp"

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

const char* kSettingsOrg  = "AriaAgent";
const char* kSettingsApp  = "AriaAgent";

QString env_or(const char* name, const QString& fallback) {
    const char* v = std::getenv(name);
    return (v && *v) ? QString::fromUtf8(v) : fallback;
}

} // namespace

SettingsDialog::SettingsDialog(QWidget* parent, int initialPage) : QDialog(parent) {
    setWindowTitle(QStringLiteral("设置 — AriaAgent"));
    resize(720, 520);

    nav_ = new QListWidget(this);
    nav_->setFixedWidth(170);
    nav_->addItems({
        QStringLiteral("⚙ 通用"),
        QStringLiteral("◈ 模型"),
        QStringLiteral("🧩 插件"),
        QStringLiteral("✦ Agent 预设"),
    });
    nav_->setStyleSheet(QStringLiteral(
        "QListWidget { background:#1a1d27; border:none; border-radius:10px; padding:8px; }"
        "QListWidget::item { padding:12px 14px; border-radius:8px; font-size:14px; }"
        "QListWidget::item:hover { background:#21252f; }"
        "QListWidget::item:selected { background:#21252f; color:#3b82f6; }"));

    stack_ = new QStackedWidget(this);
    build_general_page(stack_);
    build_model_page(stack_);
    build_plugins_page(stack_);
    build_presets_page(stack_);

    connect(nav_, &QListWidget::currentRowChanged,
            stack_, &QStackedWidget::setCurrentIndex);
    nav_->setCurrentRow(initialPage);   // jump straight to the requested page (default: Model)

    auto* save_btn = new QPushButton(QStringLiteral("保存"), this);
    save_btn->setObjectName(QStringLiteral("primary"));
    save_btn->setCursor(Qt::PointingHandCursor);
    auto* cancel_btn = new QPushButton(QStringLiteral("取消"), this);
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
        QStringLiteral("You are a helpful assistant. You may call tools to answer questions.")));

    theme_combo_ = new QComboBox(card);
    theme_combo_->addItems({QStringLiteral("跟随系统"), QStringLiteral("浅色"), QStringLiteral("深色")});
    {
        QSettings s(kSettingsOrg, kSettingsApp);
        theme_combo_->setCurrentIndex(s.value("theme", 2).toInt());
    }

    enter_combo_ = new QComboBox(card);
    enter_combo_->addItems({QStringLiteral("Enter 发送"), QStringLiteral("Ctrl+Enter 发送")});

    stream_check_ = new QCheckBox(QStringLiteral("流式输出 (token-by-token)"), card);
    stream_check_->setChecked(true);

    form->addRow(QStringLiteral("System Prompt"), prompt_edit_);
    form->addRow(QStringLiteral("外观"), theme_combo_);
    form->addRow(QStringLiteral("回车行为"), enter_combo_);
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

    form->addRow(QStringLiteral("API 地址 (Base URL)"), base_url_edit_);
    form->addRow(QStringLiteral("API 密钥"), api_key_edit_);
    form->addRow(QStringLiteral("模型 (Model)"), model_edit_);

    auto* hint = new QLabel(QStringLiteral(
        "任何 OpenAI 兼容端点皆可: DeepSeek · OpenAI · Kimi · Qwen · GLM …\n"
        "换厂商只需改 API 地址与模型名,无需重新编译。"), card);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color:#8b93a3; font-size:12px;"));
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
        auto* cb = new QCheckBox(QStringLiteral("启用"), box);
        cb->setChecked(checked);
        auto* lab = new QLabel(desc, box);
        lab->setWordWrap(true);
        lab->setStyleSheet(QStringLiteral("color:#8b93a3; font-size:12px;"));
        bv->addWidget(cb);
        bv->addWidget(lab);
        v->addWidget(box);
    };

    make_plugin(QStringLiteral("计算器 (calculator)"),
                QStringLiteral("四则运算 / 幂运算,可回答数学计算问题。"), true);
    make_plugin(QStringLiteral("当前时间 (current_time)"),
                QStringLiteral("返回本地当前日期与时间。"), true);

    auto* hint = new QLabel(QStringLiteral(
        "内置工具在 src/agent/tool_registry.cpp 注册,\n"
        "新增工具只需实现 Tool{name, desc, schema, fn} 并 register_tool。"), card);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color:#8b93a3; font-size:12px;"));
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
        QStringLiteral("标准模式 — 平衡速度与能力,适合日常对话"),
        QStringLiteral("创造模式 — 鼓励发散性回答,适合创意写作"),
        QStringLiteral("极简模式 — 最快响应,最小开销,适合简单问答"),
    });
    {
        QSettings s(kSettingsOrg, kSettingsApp);
        preset_combo_->setCurrentIndex(s.value("preset", 0).toInt());
    }
    v->addWidget(preset_combo_);

    auto* hint = new QLabel(QStringLiteral(
        "预设会调整 Agent 的系统提示词与行为倾向:\n"
        "· 标准 —— 保持默认提示词\n"
        "· 创造 —— 注入「发散思考、勇于联想」的引导\n"
        "· 极简 —— 使用精简提示词,减少 token 开销"), card);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color:#8b93a3; font-size:12px;"));
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
