// AriaAgent — main window.
// UI design follows DeepSeek's official harness web UI (deep dark theme,
// linear icons, large rounded input bar, model picker, bubble chat).
#include "main_window.hpp"
#include "viewmodel/chat_view_model.hpp"
#include "markdown_render.hpp"
#include "settings_dialog.hpp"
#include "theme.hpp"

#include "i18n/I18n.h"

#include "agent/todo_store.hpp"

#include <QApplication>
#include <QAbstractTextDocumentLayout>
#include <QFrame>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QListWidget>
#include <QMenu>
#include <QPainter>
#include <QSettings>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollBar>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QAbstractItemView>
#include <QFontMetrics>

#include <aria/adapters/qt6/qt_list_model_adapter.hpp>

namespace {

// ── Theme (shared with settings dialog + markdown renderer) ────────────────
// Palette is defined in qt/theme.hpp; g_theme holds the active one and is
// reloaded by MainWindow::apply_theme().
using agent_ui::Theme;
using agent_ui::g_theme;
using agent_ui::load_theme;

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

    // Build a QTextDocument for the message (markdown for assistant/tool,
    // plain text for user). Returns the doc; caller owns it.
    static QTextDocument* make_doc(const QString& text, bool isUser,
                                   bool isTool, const QFont& font) {
        auto* doc = new QTextDocument;
        doc->setDefaultFont(font);
        doc->setDocumentMargin(10);
        if (isUser || isTool) {
            doc->setPlainText(text);
        } else {
            agent_ui::render_markdown(text, *doc);
        }
        return doc;
    }

    QSize sizeHint(const QStyleOptionViewItem& opt,
                   const QModelIndex& idx) const override {
        const QString text = idx.data(RoleText).toString();
        const bool isUser  = idx.data(RoleAuthor).toString() == "You";
        const bool isTool  = idx.data(RoleIsTool).toBool();
        const int maxW = isTool ? 460 : 640;

        std::unique_ptr<QTextDocument> doc(make_doc(text, isUser, isTool, opt.font));
        doc->setTextWidth(maxW - 20);
        const int h = static_cast<int>(doc->size().height()) + 22;
        return QSize(maxW + 16, h);
    }

    void paint(QPainter* p, const QStyleOptionViewItem& opt,
               const QModelIndex& idx) const override {
        p->save();
        p->setRenderHint(QPainter::Antialiasing);

        const QString author = idx.data(RoleAuthor).toString();
        const QString text   = idx.data(RoleText).toString();
        const bool isUser    = author == "You";
        const bool isTool    = idx.data(RoleIsTool).toBool();
        const QString tool   = idx.data(RoleToolName).toString();

        const QRect r = opt.rect.adjusted(8, 4, -8, -4);
        QColor bubble = isTool ? QColor(g_theme.bubble_tool)
                               : (isUser ? QColor(g_theme.bubble_user) : QColor(g_theme.bubble_asst));
        const int maxW = isTool ? 460 : 640;
        const int bw = std::min(r.width() - 24, maxW);
        int bx = isUser ? (r.right() - bw) : r.left();
        const int by = r.y() + 4;

        // Optional label (assistant name / tool name)
        int headerH = 0;
        if (!isUser) {
            headerH = 18;
            QFont f = opt.font; f.setPointSizeF(f.pointSizeF() - 1.5);
            p->setFont(f);
            p->setPen(isTool ? QColor("#60a5fa") : QColor(g_theme.text_dim));
            const QString head = isTool ? (QString::fromStdString(agent::i18n::str("msg_tool_prefix")) + tool)
                                          : QString::fromStdString(agent::i18n::str("msg_agent"));
            p->drawText(bx + 4, by, bw, 16, Qt::AlignLeft | Qt::AlignVCenter, head);
        }
        const int bubbleTop = by + headerH;

        // Render the document to get its height, then draw bubble around it.
        std::unique_ptr<QTextDocument> doc(make_doc(text, isUser, isTool, opt.font));
        doc->setTextWidth(bw - 20);
        const int bh = static_cast<int>(doc->size().height()) + 12;

        QPainterPath path;
        path.addRoundedRect(QRectF(bx, bubbleTop, bw, bh), 12, 12);
        p->fillPath(path, bubble);

        // Draw the document inside the bubble (transparent bg → bubble shows).
        p->translate(bx + 10, bubbleTop + 6);
        QAbstractTextDocumentLayout::PaintContext ctx;
        ctx.palette.setColor(QPalette::Text, QColor(g_theme.text));
        ctx.palette.setColor(QPalette::Link, QColor(g_theme.accent));
        doc->documentLayout()->draw(p, ctx);

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

// ── Trajectory delegate: timeline of tool calls ─────────────────────────────
enum : int {
    RoleToolName2 = Qt::UserRole + 10,
    RoleToolArgs,
    RoleToolResult,
    RoleToolOk,
};

QVariant traj_data_fn(const UiToolCall& t, int role) {
    switch (role) {
        case RoleToolName2:  return QString::fromStdString(t.name);
        case RoleToolArgs:   return QString::fromStdString(t.args);
        case RoleToolResult: return QString::fromStdString(t.result);
        case RoleToolOk:     return t.ok;
        default: return {};
    }
}

class TrajectoryDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem& opt,
                   const QModelIndex& idx) const override {
        QFontMetrics fm(opt.font);
        const int w = 320;
        QString r = idx.data(RoleToolResult).toString();
        if (r.size() > 80) r = r.left(77) + "…";
        int lines = 1 + (static_cast<int>(r.size()) + 50) / 51;
        return QSize(w, lines * fm.lineSpacing() + 40);
    }

    void paint(QPainter* p, const QStyleOptionViewItem& opt,
               const QModelIndex& idx) const override {
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        const QRect r = opt.rect.adjusted(4, 2, -4, -2);
        const QString name = idx.data(RoleToolName2).toString();
        const QString args = idx.data(RoleToolArgs).toString();
        QString result = idx.data(RoleToolResult).toString();
        const bool ok = idx.data(RoleToolOk).toBool();
        if (result.size() > 100) result = result.left(97) + "…";

        p->setPen(QPen(QColor(g_theme.border), 2));
        p->drawLine(QPoint(r.left() + 8, r.top()), QPoint(r.left() + 8, r.bottom()));
        p->setPen(Qt::NoPen);
        p->setBrush(ok ? QColor(g_theme.accent) : QColor("#e24b4a"));
        p->drawEllipse(QPoint(r.left() + 8, r.top() + 10), 5, 5);

        const int bx = r.left() + 22;
        QFont bold = opt.font; bold.setBold(true);
        p->setFont(bold);
        p->setPen(QColor(g_theme.text));
        p->drawText(QRect(bx, r.top() + 2, r.width() - 30, 18),
                    Qt::AlignLeft, name + " " + args);

        QFont normal = opt.font; normal.setPointSizeF(normal.pointSizeF() - 0.5);
        p->setFont(normal);
        p->setPen(ok ? QColor(g_theme.text_dim) : QColor("#f09595"));
        p->drawText(QRect(bx, r.top() + 22, r.width() - 30, r.height() - 24),
                    Qt::AlignLeft | Qt::TextWordWrap, result);

        p->restore();
    }
};

} // namespace

// ── MainWindow ──────────────────────────────────────────────────────────────
void MainWindow::apply_theme() {
    g_theme = load_theme();
    const Theme& t = g_theme;

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
        "QListWidget::item:selected { background:%3; color:%4; font-weight:600; }"
        "QLabel#phase { color:%4; font-weight:600; font-size:13px; }"
        "QLabel#hint { color:%5; font-size:13px; }"
        "QLabel#workspaceTag { color:%2; font-size:13px; padding:3px 8px; background:%3; border-radius:6px; }"
    ).arg(t.bg, t.text, t.panel2, t.accent, t.border, t.panel));

    if (auto* sb = findChild<QFrame*>(QStringLiteral("sidebar"))) {
        sb->setStyleSheet(QStringLiteral("QFrame#sidebar { background:%1; border-right:1px solid %2; }")
                          .arg(t.panel, t.border));
    }
    if (right_wrap_) {
        right_wrap_->setStyleSheet(QStringLiteral(
            "QWidget#rightPanel { background:%1; border-left:1px solid %2; }")
            .arg(t.panel, t.border));
    }
    if (model_label_) {
        model_label_->setStyleSheet(QStringLiteral("color:%1; font-size:14px;").arg(t.text_dim));
    }
}

MainWindow::MainWindow(ChatViewModel* vm, QWidget* parent)
    : QMainWindow(parent), vm_(vm) {
    setWindowTitle(QString::fromStdString(agent::i18n::str("window_title")));
    resize(1280, 820);
    setMinimumSize(960, 640);

    // ── Sidebar ────────────────────────────────────────────────────────────
    auto* logo_box = new QHBoxLayout;
    logo_box->setSpacing(8);
    auto* logo_lab = new QLabel(QString::fromStdString(agent::i18n::str("app_name")), this);
    logo_lab->setStyleSheet(QStringLiteral("color:white; font-weight:700; font-size:16px;"));
    auto* tag_lab = new QLabel(QStringLiteral("HARNESS"), this);
    tag_lab->setStyleSheet(QStringLiteral("background:#3b82f6; color:white; font-size:10px;"
                                          " font-weight:700; padding:2px 6px; border-radius:4px;"));
    logo_box->addWidget(logo_lab);
    logo_box->addWidget(tag_lab);
    logo_box->addStretch();

    new_chat_btn_ = new QPushButton(QString::fromStdString(agent::i18n::str("new_chat")), this);
    new_chat_btn_->setObjectName(QStringLiteral("primary"));
    new_chat_btn_->setCursor(Qt::PointingHandCursor);
    new_chat_btn_->setMinimumHeight(40);

    session_list_ = new QListWidget(this);
    session_list_->setFixedWidth(240);
    session_list_->setContextMenuPolicy(Qt::CustomContextMenu);
    // Populate from the store.
    const auto sess = vm_->sessions();
    for (const auto& s : sess) {
        auto* it = new QListWidgetItem(QString::fromStdString(s.title), session_list_);
        it->setData(Qt::UserRole, QString::fromStdString(s.id));
        if (s.id == vm_->current_session_id()) session_list_->setCurrentItem(it);
    }

    settings_btn_ = new QPushButton(QString::fromStdString(agent::i18n::str("settings")), this);
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
    sidebar_w->setLayout(sidebar);

    // ── Chat area: top bar + bubble list + input ──────────────────────────
    auto* tag_ws = new QLabel(QString::fromStdString(agent::i18n::str("workspace")), this);
    tag_ws->setObjectName(QStringLiteral("workspaceTag"));

    model_label_ = new QLabel(QString::fromStdString(agent::i18n::str("app_subtitle")), this);

    phase_label_ = new QLabel(this);
    phase_label_->setObjectName(QStringLiteral("phase"));

    traj_btn_ = new QPushButton(QString::fromStdString(agent::i18n::str("trajectory")), this);
    traj_btn_->setCursor(Qt::PointingHandCursor);
    todo_btn_ = new QPushButton(QString::fromStdString(agent::i18n::str("todo")), this);
    todo_btn_->setCursor(Qt::PointingHandCursor);

    auto* top_bar = new QHBoxLayout;
    top_bar->setContentsMargins(0, 0, 0, 0);
    top_bar->setSpacing(10);
    top_bar->addWidget(tag_ws);
    top_bar->addSpacing(8);
    top_bar->addWidget(model_label_);
    top_bar->addStretch();
    top_bar->addWidget(traj_btn_);
    top_bar->addWidget(todo_btn_);
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
    input_->setPlaceholderText(QString::fromStdString(agent::i18n::str("input_placeholder")));
    input_->setFixedHeight(80);
    input_->setAcceptRichText(false);

    plus_btn_ = new QPushButton(QStringLiteral("+"), this);
    plus_btn_->setObjectName(QStringLiteral("primary"));
    plus_btn_->setFixedSize(30, 30);
    plus_btn_->setToolTip(QString::fromStdString(agent::i18n::str("attach_tooltip")));
    plus_btn_->setCursor(Qt::PointingHandCursor);

    tool_btn_ = new QPushButton(QStringLiteral("🛠 Workspace Write"), this);
    tool_btn_->setCursor(Qt::PointingHandCursor);
    tool_btn_->setToolTip(QString::fromStdString(agent::i18n::str("ws_tooltip")));

    model_pick_ = new QPushButton(QStringLiteral("DeepSeek-V4-Flash ▾"), this);
    model_pick_->setCursor(Qt::PointingHandCursor);
    model_pick_->setToolTip(QString::fromStdString(agent::i18n::str("attach_tooltip")));

    send_btn_ = new QPushButton(QStringLiteral("↑"), this);
    send_btn_->setObjectName(QStringLiteral("sendCircle"));
    send_btn_->setCursor(Qt::PointingHandCursor);
    // Send button doubles as stop button while busy (text toggles ↑ / ⏹).

    auto* input_tools = new QHBoxLayout;
    input_tools->setSpacing(8);
    input_tools->addWidget(plus_btn_);
    input_tools->addWidget(tool_btn_);
    input_tools->addStretch();

    auto* input_actions = new QHBoxLayout;
    input_actions->setSpacing(10);
    input_actions->addLayout(input_tools);
    input_actions->addStretch();
    input_actions->addWidget(model_pick_);
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

    // ── Trajectory / Todo panel (collapsible, right side) ─────────────────
    trajectory_list_ = new QListView(this);
    trajectory_list_->setModel(new aria::adapters::qt6::ObservableListModel<UiToolCall>(
        vm_->tool_trace,
        QHash<int,QByteArray>{{RoleToolName2,"name"},{RoleToolArgs,"args"},{RoleToolResult,"result"},{RoleToolOk,"ok"}},
        traj_data_fn, trajectory_list_));
    trajectory_list_->setItemDelegate(new TrajectoryDelegate(trajectory_list_));
    trajectory_list_->setSelectionMode(QAbstractItemView::NoSelection);
    trajectory_list_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    trajectory_list_->setFocusPolicy(Qt::NoFocus);
    trajectory_list_->setStyleSheet(QStringLiteral(
        "QListView { background:transparent; border:none; padding:10px; }"));

    todo_list_ = new QListWidget(this);
    todo_list_->setSelectionMode(QAbstractItemView::NoSelection);
    todo_list_->setStyleSheet(QStringLiteral(
        "QListWidget { background:transparent; border:none; padding:10px; }"
        "QListWidget::item { padding:10px 12px; border-radius:8px; }"));

    right_panel_ = new QStackedWidget(this);
    right_panel_->addWidget(trajectory_list_);   // page 0: trajectory
    right_panel_->addWidget(todo_list_);         // page 1: todo
    right_panel_->setFixedWidth(340);

    // Collapsible right-side container: title bar (label + ✕ collapse
    // button) above the stacked panel. Clicking ✕ hides the whole thing.
    right_wrap_ = new QWidget(this);
    right_wrap_->setObjectName(QStringLiteral("rightPanel"));
    auto* rv = new QVBoxLayout(right_wrap_);
    rv->setContentsMargins(0, 0, 0, 0);
    rv->setSpacing(0);
    auto* rbar = new QHBoxLayout;
    rbar->setContentsMargins(14, 8, 8, 4);
    auto* rtitle = new QLabel(QString::fromStdString(agent::i18n::str("panel_title")), right_wrap_);
    rtitle->setObjectName(QStringLiteral("panelTitle"));
    rtitle->setStyleSheet(QStringLiteral("font-weight:600; color:%1;")
                          .arg(QString::fromUtf8(g_theme.text)));
    close_panel_btn_ = new QPushButton(QStringLiteral("✕"), right_wrap_);
    close_panel_btn_->setCursor(Qt::PointingHandCursor);
    close_panel_btn_->setFixedSize(28, 28);
    close_panel_btn_->setToolTip(QString::fromStdString(agent::i18n::str("collapse_panel")));
    rbar->addWidget(rtitle);
    rbar->addStretch();
    rbar->addWidget(close_panel_btn_);
    rv->addLayout(rbar);
    rv->addWidget(right_panel_, 1);
    right_wrap_->setVisible(false);   // hidden until toggled

    auto* root = new QHBoxLayout;
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(sidebar_w);
    root->addWidget(chat_w, 1);
    root->addWidget(right_wrap_);
    auto* root_w = new QWidget(this);
    root_w->setLayout(root);
    setCentralWidget(root_w);

    // ── Connections ────────────────────────────────────────────────────────
    connect(send_btn_, &QPushButton::clicked, this, &MainWindow::on_send);
    connect(new_chat_btn_, &QPushButton::clicked, this, &MainWindow::on_new_chat);
    connect(traj_btn_, &QPushButton::clicked, this, &MainWindow::toggle_trajectory);
    connect(todo_btn_, &QPushButton::clicked, this, &MainWindow::toggle_todo);
    connect(close_panel_btn_, &QPushButton::clicked, this, [this] {
        right_wrap_->setVisible(false);
        trajectory_visible_ = false;
        traj_btn_->setStyleSheet(QString());
        todo_btn_->setStyleSheet(QString());
    });
    connect(plus_btn_, &QPushButton::clicked, this, &MainWindow::on_attach_file);
    connect(tool_btn_, &QPushButton::clicked, this, &MainWindow::on_workspace_mode_select);
    connect(model_pick_, &QPushButton::clicked, this, &MainWindow::on_open_settings);
    connect(settings_btn_, &QPushButton::clicked, this, &MainWindow::on_open_settings);

    // Reactive todo projection: refresh the list whenever the store changes.
    todo_sub_id_ = agent::TodoStore::instance().subscribe([this] {
        QMetaObject::invokeMethod(this, &MainWindow::refresh_todo, Qt::QueuedConnection);
    });
    refresh_todo();

    // Right-click a chat message → feedback menu (P2-4).
    chat_list_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(chat_list_, &QListView::customContextMenuRequested,
            this, &MainWindow::show_message_menu);

    // Session list: click to switch, context menu to delete.
    connect(session_list_, &QListWidget::itemClicked, this, [this](QListWidgetItem* it) {
        if (!it) return;
        vm_->switch_session(it->data(Qt::UserRole).toString().toStdString());
    });
    connect(session_list_, &QListWidget::customContextMenuRequested, this,
            [this](const QPoint& pos) {
        auto* it = session_list_->itemAt(pos);
        if (!it) return;
        QMenu menu(this);
        auto* del = menu.addAction(QString::fromStdString(agent::i18n::str("delete_session")));
        if (menu.exec(session_list_->mapToGlobal(pos)) == del) {
            vm_->delete_session(it->data(Qt::UserRole).toString().toStdString());
        }
    });
    // Refresh the sidebar whenever the session set changes.
    session_sub_ = vm_->session_changed.connect([this] {
        session_list_->clear();
        const auto sessions_now = vm_->sessions();
        QListWidgetItem* current = nullptr;
        for (const auto& s : sessions_now) {
            auto* it = new QListWidgetItem(QString::fromStdString(s.title), session_list_);
            it->setData(Qt::UserRole, QString::fromStdString(s.id));
            if (s.id == vm_->current_session_id()) {
                session_list_->setCurrentItem(it);
                current = it;
            }
        }
        if (current) {
            session_list_->scrollToItem(current, QAbstractItemView::EnsureVisible);
        }
    });
    // VM errors surface as a chat bubble (VM handles rendering); also flash
    // the phase label so failures are visible even when scrolled away.
    error_sub_ = vm_->error_occurred.connect([this](const std::string&) {
        QMetaObject::invokeMethod(this, [this] {
            phase_label_->setText(QString::fromStdString(agent::i18n::str("phase_error")));
        });
    });

    auto busy_sub = vm_->busy.observe([this](bool b, bool) {
        QMetaObject::invokeMethod(this, [this, b] {
            send_btn_->setEnabled(!b);
            input_->setEnabled(!b);
            send_btn_->setText(b ? QStringLiteral("⏹") : QStringLiteral("↑"));
            send_btn_->setToolTip(b ? QString::fromStdString(agent::i18n::str("stop"))
                                 : QString::fromStdString(agent::i18n::str("send")));
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

    // Everything is constructed — restyle now that all widgets exist.
    apply_theme();

    // Sync initial workspace level (0/1/2) → env + label.
    set_workspace_level(ws_level_);

    // Refresh labels from the i18n table, and keep them fresh on switch.
    apply_language();
    lang_sub_ = agent::i18n::on_language_changed([this](const std::string&) {
        QMetaObject::invokeMethod(this, &MainWindow::apply_language,
                                  Qt::QueuedConnection);
    });
}

void MainWindow::on_send() {
    if (vm_->busy.get()) return;
    const QString text = input_->toPlainText().trimmed();
    if (text.isEmpty()) return;
    input_->clear();
    vm_->send(text.toStdString());
}

void MainWindow::on_stop() {
    vm_->stop();
}

void MainWindow::on_new_chat() {
    vm_->new_session();   // creates + switches; sessionChanged refreshes sidebar
    phase_label_->setText(QStringLiteral(""));
}

void MainWindow::on_attach_file() {
    // Pick a single file and reference it as an inline @mention so the
    // agent (and tools that accept paths) can act on it.
    const QString path = QFileDialog::getOpenFileName(
        this, QString::fromStdString(agent::i18n::str("attach_dialog_title")), QString(),
        QString::fromStdString(agent::i18n::str("attach_all_files")));
    if (path.isEmpty()) return;
    QString cur = input_->toPlainText();
    if (!cur.isEmpty() && !cur.endsWith('\n')) cur += '\n';
    cur += QStringLiteral("@%1\n").arg(path);
    input_->setPlainText(cur);
    input_->setFocus();
}

void MainWindow::on_workspace_mode_select() {
    // Pop a 3-item menu matching DeepSeek's harness UX. The chosen
    // level is written to ARIA_WORKSPACE_WRITE (0/1/2) — tool
    // implementations and the approval gate both read it.
    QMenu menu(this);
    auto* ro  = menu.addAction(QString::fromStdString(agent::i18n::str("ws_read_only")));
    auto* wr  = menu.addAction(QString::fromStdString(agent::i18n::str("ws_workspace_write")));
    auto* all = menu.addAction(QString::fromStdString(agent::i18n::str("ws_full_access")));
    ro ->setData(0);
    wr ->setData(1);
    all->setData(2);
    for (QAction* a : {ro, wr, all}) {
        a->setCheckable(true);
        a->setChecked(a->data().toInt() == ws_level_);
    }
    QAction* picked = menu.exec(tool_btn_->mapToGlobal(
                                   QPoint(tool_btn_->width() / 2,
                                          tool_btn_->height())));
    if (!picked) return;
    set_workspace_level(picked->data().toInt());
}

void MainWindow::set_workspace_level(int level) {
    ws_level_ = (level < 0 || level > 2) ? 1 : level;
    switch (ws_level_) {
        case 0: tool_btn_->setText(QString::fromStdString(agent::i18n::str("ws_read_only")));         break;
        case 1: tool_btn_->setText(QString::fromStdString(agent::i18n::str("ws_workspace_write")));   break;
        case 2: tool_btn_->setText(QString::fromStdString(agent::i18n::str("ws_full_access")));       break;
    }
    char buf[2] = {char('0' + ws_level_), 0};
    qputenv("ARIA_WORKSPACE_WRITE", buf);
}

void MainWindow::apply_language() {
    setWindowTitle(QString::fromStdString(agent::i18n::str("window_title")));
    new_chat_btn_->setText(QString::fromStdString(agent::i18n::str("new_chat")));
    settings_btn_->setText(QString::fromStdString(agent::i18n::str("settings")));
    traj_btn_->setText(QString::fromStdString(agent::i18n::str("trajectory")));
    todo_btn_->setText(QString::fromStdString(agent::i18n::str("todo")));
    model_label_->setText(QString::fromStdString(agent::i18n::str("app_subtitle")));
    input_->setPlaceholderText(QString::fromStdString(agent::i18n::str("input_placeholder")));
    plus_btn_->setToolTip(QString::fromStdString(agent::i18n::str("attach_tooltip")));
    tool_btn_->setToolTip(QString::fromStdString(agent::i18n::str("ws_tooltip")));
    if (auto* t = findChild<QLabel*>(QStringLiteral("panelTitle"))) {
        t->setText(QString::fromStdString(agent::i18n::str("panel_title")));
    }
    close_panel_btn_->setToolTip(QString::fromStdString(agent::i18n::str("collapse_panel")));
    // Workspace level label uses i18n too.
    set_workspace_level(ws_level_);
}

void MainWindow::on_open_settings() {
    SettingsDialog dlg(this, /*initialPage=*/1);   // land straight on Model
    dlg.exec();
}

void MainWindow::toggle_trajectory() {
    // Clicking the active panel button again collapses the panel.
    if (right_wrap_->isVisible() && right_panel_->currentIndex() == 0) {
        right_wrap_->setVisible(false);
        trajectory_visible_ = false;
        traj_btn_->setStyleSheet(QString());
        todo_btn_->setStyleSheet(QString());
        return;
    }
    right_wrap_->setVisible(true);
    right_panel_->setCurrentIndex(0);
    trajectory_visible_ = true;
    traj_btn_->setStyleSheet(QStringLiteral("background:%1; color:white; border-radius:8px; padding:6px 12px;")
                             .arg(QString::fromUtf8(g_theme.accent)));
    todo_btn_->setStyleSheet(QString());
}

void MainWindow::toggle_todo() {
    if (right_wrap_->isVisible() && right_panel_->currentIndex() == 1) {
        right_wrap_->setVisible(false);
        trajectory_visible_ = false;
        traj_btn_->setStyleSheet(QString());
        todo_btn_->setStyleSheet(QString());
        return;
    }
    right_wrap_->setVisible(true);
    right_panel_->setCurrentIndex(1);
    trajectory_visible_ = true;
    todo_btn_->setStyleSheet(QStringLiteral("background:%1; color:white; border-radius:8px; padding:6px 12px;")
                             .arg(QString::fromUtf8(g_theme.accent)));
    traj_btn_->setStyleSheet(QString());
}

void MainWindow::refresh_todo() {
    todo_list_->clear();
    const auto items = agent::TodoStore::instance().snapshot();
    for (const auto& it : items) {
        const char* mark = it.status == agent::TodoStatus::Done ? "✅"
                          : it.status == agent::TodoStatus::InProgress ? "🔄" : "⬜";
        auto* li = new QListWidgetItem(QStringLiteral("%1 %2")
            .arg(QString::fromUtf8(mark), QString::fromStdString(it.content)));
        todo_list_->addItem(li);
    }
}

void MainWindow::show_message_menu(const QPoint& pos) {
    const QModelIndex idx = chat_list_->indexAt(pos);
    if (!idx.isValid()) return;
    const QString author = idx.data(RoleAuthor).toString();
    const bool isUser = author == "You";
    if (isUser) return;   // feedback is for assistant/tool messages

    const QString text = idx.data(RoleText).toString();
    QMenu menu(this);
    auto* good = menu.addAction(QString::fromStdString(agent::i18n::str("feedback_helpful")));
    auto* bad  = menu.addAction(QString::fromStdString(agent::i18n::str("feedback_not_helpful")));
    QAction* chosen = menu.exec(chat_list_->mapToGlobal(pos));
    if (!chosen) return;

    // Persist per-message feedback (keyed by a content hash) — P2-4.
    const QString key = QString::number(qHash(text), 16);
    QSettings s("AriaAgent", "AriaAgent");
    if (chosen == good)      s.setValue("feedback/" + key, "up");
    else if (chosen == bad)  s.setValue("feedback/" + key, "down");
}

void MainWindow::scroll_bottom() {
    QMetaObject::invokeMethod(chat_list_, [this] {
        auto* bar = chat_list_->verticalScrollBar();
        if (bar) bar->setValue(bar->maximum());
    }, Qt::QueuedConnection);
}