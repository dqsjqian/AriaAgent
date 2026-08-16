// AriaAgent — main window (DeepSeek-web-UI inspired: sidebar + bubble chat).
#pragma once

#include <memory>

#include <QMainWindow>

class QListView;
class QTextEdit;
class QPushButton;
class QLabel;
class QListWidget;

class ChatViewModel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(ChatViewModel* vm, QWidget* parent = nullptr);

private:
    void on_send();
    void on_stop();
    void on_new_chat();
    void scroll_bottom();

    ChatViewModel* vm_;

    // sidebar
    QPushButton* new_chat_btn_;
    QListWidget* session_list_;
    QPushButton* settings_btn_;

    // chat area
    QListView*  chat_list_;
    QTextEdit*  input_;
    QPushButton* send_btn_;
    QLabel*     phase_label_;
    QLabel*     model_label_;
};
