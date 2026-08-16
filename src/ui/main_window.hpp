// AriaAgent — main window: chat list + tool-chain panel + input bar.
#pragma once

#include <memory>

#include <QMainWindow>

class QListView;
class QTextEdit;
class QLineEdit;
class QPushButton;
class QLabel;

class ChatViewModel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(ChatViewModel* vm, QWidget* parent = nullptr);

private:
    void on_send();
    void on_stop();
    void scroll_bottom();

    ChatViewModel* vm_;
    QListView*  chat_list_;
    QListView*  tool_list_;
    QLineEdit*  input_;
    QPushButton* send_btn_;
    QPushButton* stop_btn_;
    QLabel*     phase_label_;
};
