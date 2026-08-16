// AriaAgent — main window implementation.
#include "ui/main_window.hpp"
#include "ui/chat_view_model.hpp"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPushButton>
#include <QScrollBar>
#include <QSplitter>
#include <QTextEdit>
#include <QVBoxLayout>

#include <aria/adapters/qt6/qt_list_model_adapter.hpp>

namespace {

// Role map for the chat list: Display = "author: text".
const QHash<int, QByteArray> kChatRoles = {
    {Qt::DisplayRole, "display"},
    {Qt::UserRole,    "raw"},
};

QVariant chat_role_fn(const UiMessage& m, int role) {
    if (role == Qt::DisplayRole) {
        QString prefix = QString::fromStdString(m.author);
        QString text = QString::fromStdString(m.text);
        if (m.is_tool) {
            prefix = QStringLiteral("🛠 ") + prefix;
        }
        return prefix + QStringLiteral("  ") + text;
    }
    if (role == Qt::UserRole) {
        return QString::fromStdString(m.author + "|" + m.text);
    }
    return {};
}

const QHash<int, QByteArray> kToolRoles = {
    {Qt::DisplayRole, "display"},
};

QVariant tool_role_fn(const UiToolCall& t, int role) {
    if (role == Qt::DisplayRole) {
        QString line = QString::fromStdString(t.name) + "(" +
                       QString::fromStdString(t.args) + ")  =>  " +
                       QString::fromStdString(t.result);
        return t.ok ? line : QStringLiteral("✗ ") + line;
    }
    return {};
}

} // namespace

MainWindow::MainWindow(ChatViewModel* vm, QWidget* parent)
    : QMainWindow(parent), vm_(vm) {
    setWindowTitle(QStringLiteral("AriaAgent — LLM Agent Tool Framework"));
    resize(1000, 680);

    chat_list_ = new QListView(this);
    chat_list_->setWordWrap(true);
    chat_list_->setSelectionMode(QAbstractItemView::NoSelection);
    chat_list_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    chat_list_->setUniformItemSizes(false);

    tool_list_ = new QListView(this);
    tool_list_->setWordWrap(true);
    tool_list_->setSelectionMode(QAbstractItemView::NoSelection);
    tool_list_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    // aria → Qt model binding (adapter handles thread marshalling)
    auto* chat_model = new aria::adapters::qt6::ObservableListModel<UiMessage>(
        vm_->messages, kChatRoles, chat_role_fn, chat_list_);
    chat_list_->setModel(chat_model);

    auto* tool_model = new aria::adapters::qt6::ObservableListModel<UiToolCall>(
        vm_->tool_trace, kToolRoles, tool_role_fn, tool_list_);
    tool_list_->setModel(tool_model);

    // ── input bar ──
    input_ = new QLineEdit(this);
    input_->setPlaceholderText(QStringLiteral("Ask the agent… (Enter to send)"));
    send_btn_ = new QPushButton(QStringLiteral("Send"), this);
    stop_btn_ = new QPushButton(QStringLiteral("Stop"), this);
    stop_btn_->setEnabled(false);
    phase_label_ = new QLabel(QStringLiteral("idle"), this);
    phase_label_->setStyleSheet(QStringLiteral("color:#7DD3FC;font-weight:600;"));

    auto* input_row = new QHBoxLayout;
    input_row->addWidget(input_, 1);
    input_row->addWidget(send_btn_);
    input_row->addWidget(stop_btn_);
    input_row->addWidget(phase_label_);

    // ── layout: chat (left) | tools (right) ──
    auto* left = new QVBoxLayout;
    left->addWidget(new QLabel(QStringLiteral("Conversation"), this));
    left->addWidget(chat_list_, 1);
    left->addLayout(input_row);

    auto* right = new QVBoxLayout;
    right->addWidget(new QLabel(QStringLiteral("Tool chain"), this));
    right->addWidget(tool_list_, 1);

    auto* panels = new QWidget(this);
    auto* split = new QHBoxLayout(panels);
    split->setContentsMargins(0, 0, 0, 0);
    auto* left_w = new QWidget(panels);
    left_w->setLayout(left);
    auto* right_w = new QWidget(panels);
    right_w->setLayout(right);
    split->addWidget(left_w, 3);
    split->addWidget(right_w, 2);

    setCentralWidget(panels);

    // ── connections ──
    connect(send_btn_, &QPushButton::clicked, this, &MainWindow::on_send);
    connect(stop_btn_, &QPushButton::clicked, this, &MainWindow::on_stop);
    connect(input_, &QLineEdit::returnPressed, this, &MainWindow::on_send);

    // Reactive binding: busy/phase → widgets
    auto busy_sub = vm_->busy.observe([this](bool b, bool) {
        QMetaObject::invokeMethod(this, [this, b] {
            send_btn_->setEnabled(!b);
            stop_btn_->setEnabled(b);
            input_->setEnabled(!b);
        });
    });
    auto phase_sub = vm_->phase_text.observe([this](const std::string& p, const std::string&) {
        QMetaObject::invokeMethod(this, [this, p] {
            phase_label_->setText(QString::fromStdString(p));
        });
    });
    (void)busy_sub; (void)phase_sub;
    // Auto-scroll chat on new items (poll-less: reuse list model rowsInserted)
    connect(chat_model, &QAbstractListModel::rowsInserted,
            this, &MainWindow::scroll_bottom);

    // Key bindings
    connect(chat_list_, &QListView::activated, this, &MainWindow::scroll_bottom);
}

void MainWindow::on_send() {
    const QString text = input_->text().trimmed();
    if (text.isEmpty()) return;
    input_->clear();
    vm_->send(text);
}

void MainWindow::on_stop() {
    vm_->stop();
}

void MainWindow::scroll_bottom() {
    QMetaObject::invokeMethod(chat_list_, [this] {
        auto* bar = chat_list_->verticalScrollBar();
        if (bar) bar->setValue(bar->maximum());
    }, Qt::QueuedConnection);
}
