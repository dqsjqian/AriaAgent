// AriaAgent — main window.
// UI design follows DeepSeek's official harness web UI (deep dark theme,
// linear icons, large rounded input bar, model picker, bubble chat).
#include "ui/main_window.hpp"
#include "ui/chat_view_model.hpp"
#include "ui/settings_dialog.hpp"

#include <QApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QListWidget>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollBar>
#include <QStyledItemDelegate>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QAbstractItemView>
#include <QFontMetrics>

#include <aria/adapters/qt6/qt_list_model_adapter.hpp>

namespace {

// ── DeepSeek palette ────────────────────────────────────────────────────────
constexpr const char* kBg        = "#0f1117";
constexpr const char* kPanel     = "#1a1d27";
constexpr const char* kPanel2    = "#21252f";
constexpr const char* kBorder    = "#2a2f3a";
constexpr const char* kBubbleUser  = "#3b82f6";
constexpr const char* kBubbleAsst  = "#1f2330";
constexpr const char* kBubbleTool  = "#1e2940";
constexpr const char* kText      = "#e5e7eb";
constexpr const char* kTextDim   = "#8b93a3";
constexpr const char* kAccent    = "#3b82f6";

enum : int {
    RoleAuthor  = Qt::UserRole + 1,
    RoleText,
    RoleIsTool,
    RoleToolName,
};

// ── Bubble delegate: rounded message bubble ─────────────────────────────────
class BubbleDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem& opt,
                   const QModelIndex& idx) const override {
        const QString text = idx.data(RoleText).toString();
        const bool isTool = idx.data(RoleIsTool).toBool();
        const int maxW = isTool ? 460 : 640;
        QFontMetrics fm(opt.font);
        const int charW = std::max(1, fm.averageCharWidth());
        const int cpl = std::max(20, maxW / charW);
        int lines = 0;
        for (const auto& part : text.split('\n'))
            lines += std::max(1, (static_cast<int>(part.size()) + cpl - 1) / cpl);
        const int h = lines * fm.lineSpacing() + 28;
        return QSize(maxW + 16, h);
    }

    void paint(QPainter* p, const QStyleOptionViewItem& opt,
               const QModelIndex& idx) const override {
        p->save();
        p->setRenderHint(QPainter::Antialiasing);

        const QString author = idx.data(RoleAuthor).toString();
        const QString text   = idx.data(RoleText).toString();
        const bool isTool    = idx.data(RoleIsTool).toBool();
        const QString tool   = idx.data(RoleToolName).toString();

        const bool isUser = author == "You";
        const QRect r = opt.rect.adjusted(8, 4, -8, -4);

        QColor bubble = isTool ? QColor(kBubbleTool)
                               : (isUser ? QColor(kBubbleUser) : QColor(kBubbleAsst));
        const int maxW = isTool ? 460 : 640;
        QFontMetrics fm(opt.font);
        const int charW = std::max(1, fm.averageCharWidth());
        const int cpl = std::max(20, maxW / charW);
        int lines = 0;
        for (const auto& part : text.split('\n'))
            lines += std::max(1, (static_cast<int>(part.size()) + cpl - 1) / cpl);
        const int textH = lines * fm.lineSpacing();

        const int bw = std::min(r.width() - 24, maxW);
        const int bh = textH + 24;
        int bx = isUser ? (r.right() - bw) : r.left();   // right-/left-aligned
        const int by = r.y() + 4;

        // Optional label (assistant name / tool name)
        int headerH = 0;
        if (!isUser) {
            headerH = 18;
            QFont f = opt.font; f.setPointSizeF(f.pointSizeF() - 1.5);
            p->setFont(f);
            p->setPen(isTool ? QColor("#60a5fa") : QColor(kTextDim));
            const QString head = isTool ? ("🛠 " + tool) : "AriaAgent";
            p->drawText(bx + 4, by, bw, 16, Qt::AlignLeft | Qt::AlignVCenter, head);
        }
        const int bubbleTop = by + headerH;

        // Bubble background
        QPainterPath path;
        path.addRoundedRect(QRectF(bx, bubbleTop, bw, bh), 12, 12);
        p->fillPath(path, bubble);

        // Text
        p->setFont(opt.font);
        p->setPen(QColor(kText));
        p->drawText(QRect(bx + 12, bubbleTop + 6, bw - 24, textH),
                    Qt::AlignLeft | Qt::TextWordWrap, text);

        p->restore();
    }
};

// ── Bind aria list → Qt model ───────────────────────────────────────────────
QVariant chat_data_fn(const UiMessage& m, int role) {
    switch (role) {
        case RoleAuthor:  return QString::fromStdString(m.author);
        case RoleText:    return QString::fromStdString(m.text);
        case RoleIsTool:  return m.is_tool;
        case RoleToolName:return QString::fromStdString(m.tool_name);
        default: return {};
    }
}

} // namespace

// ── MainWindow ──────────────────────────────────────────────────────────────
MainWindow::MainWindow(ChatViewModel* vm, QWidget* parent)
    : QMainWindow(parent), vm_(vm) {
    setWindowTitle(QStringLiteral("AriaAgent"));
    resize(1280, 820);
    setMinimumSize(960, 640);

    setStyleSheet(QStringLiteral(
        "QMainWindow,QWidget { background:%1; color:%2; font-family:'Segoe UI','Microsoft YaHei UI','PingFang SC',sans-serif; font-size:14px; }"
        "QPushButton { background:transparent; color:%2; border:none;"
        "  border-radius:8px; padding:8px 14px; }"
        "QPushButton:hover { background:%3; }"
        "QPushButton:disabled { color:#4b5563; }"
        "QPushButton#primary { background:%4; color:white; font-weight:600; }"
        "QPushButton#primary:hover { background:#2563eb; }"
        "QPushButton#primary:disabled { background:#1f2a4a; color:#6c7693; }"
        "QPushButton#sendCircle { background:%4; color:white; border-radius:18px;"
        "  min-width:36px; max-width:36px; min-height:36px; max-height:36px; font-size:16px; }"
        "QPushButton#sendCircle:hover { background:#2563eb; }"
        "QPushButton#sendCircle:disabled { background:#1f2a4a; color:#6c7693; }"
        "QTextEdit { background:%3; border:1px solid %5; border-radius:14px;"
        "  padding:14px; color:%2; selection-background-color:%4; }"
        "QTextEdit:focus { border-color:%4; }"
        "QListWidget { background:transparent; border:none; }"
        "QListWidget::item { padding:10px 12px; border-radius:8px; }"
        "QListWidget::item:hover { background:%6; }"
        "QListWidget::item:selected { background:%6; color:white; }"
        "QLabel#phase { color:%4; font-weight:600; font-size:13px; }"
        "QLabel#hint { color:%5; font-size:13px; }"
        "QLabel#workspaceTag { color:%5; font-size:13px; padding:3px 8px; background:%3; border-radius:6px; }"
    ).arg(kBg, kText, kPanel2, kAccent, kBorder, kPanel));

    // ── Sidebar ────────────────────────────────────────────────────────────
    auto* logo_box = new QHBoxLayout;
    logo_box->setSpacing(8);
    auto* logo_lab = new QLabel(QStringLiteral("✦ AriaAgent"), this);
    logo_lab->setStyleSheet(QStringLiteral("color:white; font-weight:700; font-size:16px;"));
    auto* tag_lab = new QLabel(QStringLiteral("HARNESS"), this);
    tag_lab->setStyleSheet(QStringLiteral("background:#3b82f6; color:white; font-size:10px;"
                                          " font-weight:700; padding:2px 6px; border-radius:4px;"));
    logo_box->addWidget(logo_lab);
    logo_box->addWidget(tag_lab);
    logo_box->addStretch();

    new_chat_btn_ = new QPushButton(QStringLiteral("＋ 新对话"), this);
    new_chat_btn_->setObjectName(QStringLiteral("primary"));
    new_chat_btn_->setCursor(Qt::PointingHandCursor);
    new_chat_btn_->setMinimumHeight(40);

    session_list_ = new QListWidget(this);
    session_list_->addItem(QStringLiteral("💬 工作区"));
    session_list_->addItem(QStringLiteral("💬 新对话"));
    session_list_->setFixedWidth(240);
    session_list_->setCurrentRow(1);

    settings_btn_ = new QPushButton(QStringLiteral("⚙ 设置"), this);
    settings_btn_->setCursor(Qt::PointingHandCursor);

    auto* sidebar = new QVBoxLayout;
    sidebar->setContentsMargins(14, 16, 14, 14);
    sidebar->setSpacing(10);
    sidebar->addLayout(logo_box);
    sidebar->addSpacing(4);
    sidebar->addWidget(new_chat_btn_);
    sidebar->addWidget(session_list_, 1);
    sidebar->addWidget(settings_btn_);

    auto* sidebar_w = new QFrame(this);
    sidebar_w->setObjectName(QStringLiteral("sidebar"));
    sidebar_w->setFixedWidth(260);
    sidebar_w->setStyleSheet(QStringLiteral("QFrame#sidebar { background:%1; border-right:1px solid %2; }")
                             .arg(kPanel, kBorder));
    sidebar_w->setLayout(sidebar);

    // ── Chat area: top bar + bubble list + input ──────────────────────────
    auto* tag_ws = new QLabel(QStringLiteral("work"), this);
    tag_ws->setObjectName(QStringLiteral("workspaceTag"));

    model_label_ = new QLabel(QStringLiteral("● AriaAgent · LLM Agent Tool Framework"), this);
    model_label_->setStyleSheet(QStringLiteral("color:%1; font-size:14px;").arg(kTextDim));

    phase_label_ = new QLabel(this);
    phase_label_->setObjectName(QStringLiteral("phase"));

    auto* top_bar = new QHBoxLayout;
    top_bar->setContentsMargins(0, 0, 0, 0);
    top_bar->setSpacing(10);
    top_bar->addWidget(tag_ws);
    top_bar->addSpacing(8);
    top_bar->addWidget(model_label_);
    top_bar->addStretch();
    top_bar->addWidget(phase_label_);

    chat_list_ = new QListView(this);
    chat_list_->setModel(new aria::adapters::qt6::ObservableListModel<UiMessage>(
        vm_->messages,
        QHash<int,QByteArray>{{RoleAuthor,"author"},{RoleText,"text"},{RoleIsTool,"tool"},{RoleToolName,"toolname"}},
        chat_data_fn, chat_list_));
    chat_list_->setItemDelegate(new BubbleDelegate(chat_list_));
    chat_list_->setSelectionMode(QAbstractItemView::NoSelection);
    chat_list_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    chat_list_->setWordWrap(true);
    chat_list_->setFocusPolicy(Qt::NoFocus);
    chat_list_->setStyleSheet(QStringLiteral(
        "QListView { background:transparent; border:none; padding:8px; }"));

    // ── Input bar: DeepSeek-style big rounded box with tools left, model + send right ──
    input_ = new QTextEdit(this);
    input_->setPlaceholderText(QStringLiteral("给 AriaAgent 发送消息…  Enter 发送 · Shift+Enter 换行"));
    input_->setFixedHeight(80);
    input_->setAcceptRichText(false);

    auto* plus_btn  = new QPushButton(QStringLiteral("+"), this);
    auto* tool_btn  = new QPushButton(QStringLiteral("🛠 Workspace Write"), this);
    plus_btn->setObjectName(QStringLiteral("primary"));
    plus_btn->setFixedSize(30, 30);
    plus_btn->setToolTip(QStringLiteral("添加工具 / 上传"));
    tool_btn->setCursor(Qt::PointingHandCursor);

    auto* model_pick = new QPushButton(QStringLiteral("DeepSeek-V4-Flash ▾"), this);
    model_pick->setCursor(Qt::PointingHandCursor);

    send_btn_ = new QPushButton(QStringLiteral("↑"), this);
    send_btn_->setObjectName(QStringLiteral("sendCircle"));
    send_btn_->setCursor(Qt::PointingHandCursor);
    // Send button doubles as stop button while busy (text toggles ↑ / ⏹).

    auto* input_tools = new QHBoxLayout;
    input_tools->setSpacing(8);
    input_tools->addWidget(plus_btn);
    input_tools->addWidget(tool_btn);
    input_tools->addStretch();

    auto* input_actions = new QHBoxLayout;
    input_actions->setSpacing(10);
    input_actions->addLayout(input_tools);
    input_actions->addStretch();
    input_actions->addWidget(model_pick);
    input_actions->addWidget(send_btn_);

    auto* input_box = new QVBoxLayout;
    input_box->setContentsMargins(0, 0, 0, 0);
    input_box->setSpacing(8);
    input_box->addWidget(input_);
    input_box->addLayout(input_actions);

    auto* chat = new QVBoxLayout;
    chat->setContentsMargins(28, 18, 28, 18);
    chat->setSpacing(10);
    chat->addLayout(top_bar);
    chat->addWidget(chat_list_, 1);
    chat->addLayout(input_box);

    auto* chat_w = new QWidget(this);
    chat_w->setLayout(chat);

    auto* root = new QHBoxLayout;
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(sidebar_w);
    root->addWidget(chat_w, 1);
    auto* root_w = new QWidget(this);
    root_w->setLayout(root);
    setCentralWidget(root_w);

    // ── Connections ────────────────────────────────────────────────────────
    connect(send_btn_, &QPushButton::clicked, this, &MainWindow::on_send);
    connect(new_chat_btn_, &QPushButton::clicked, this, &MainWindow::on_new_chat);
    connect(settings_btn_, &QPushButton::clicked, this, [this] {
        SettingsDialog dlg(this);
        dlg.exec();   // modal; save() persists to QSettings + env
    });

    auto busy_sub = vm_->busy.observe([this](bool b, bool) {
        QMetaObject::invokeMethod(this, [this, b] {
            send_btn_->setEnabled(!b);
            input_->setEnabled(!b);
            send_btn_->setText(b ? QStringLiteral("⏹") : QStringLiteral("↑"));
            send_btn_->setToolTip(b ? QStringLiteral("停止") : QStringLiteral("发送"));
        });
    });
    auto phase_sub = vm_->phase_text.observe([this](const std::string& p, const std::string&) {
        QMetaObject::invokeMethod(this, [this, p] {
            phase_label_->setText(QString::fromStdString(p));
        });
    });
    (void)busy_sub; (void)phase_sub;

    // Stop button → send button reused as toggle
    connect(send_btn_, &QPushButton::clicked, this, [this] {
        if (vm_->busy.get()) vm_->stop();
    });

    auto* chat_model = qobject_cast<QAbstractListModel*>(chat_list_->model());
    connect(chat_model, &QAbstractListModel::rowsInserted,
            this, &MainWindow::scroll_bottom);
}

void MainWindow::on_send() {
    if (vm_->busy.get()) return;
    const QString text = input_->toPlainText().trimmed();
    if (text.isEmpty()) return;
    input_->clear();
    vm_->send(text);
}

void MainWindow::on_stop() {
    vm_->stop();
}

void MainWindow::on_new_chat() {
    vm_->stop();
    vm_->messages.clear();
    vm_->tool_trace.clear();
    vm_->phase_text = "";
    phase_label_->setText(QStringLiteral(""));
    session_list_->insertItem(0, QStringLiteral("💬 新对话"));
    session_list_->setCurrentRow(0);
}

void MainWindow::scroll_bottom() {
    QMetaObject::invokeMethod(chat_list_, [this] {
        auto* bar = chat_list_->verticalScrollBar();
        if (bar) bar->setValue(bar->maximum());
    }, Qt::QueuedConnection);
}