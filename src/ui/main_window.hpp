// AriaAgent — main window (DeepSeek-web-UI inspired: sidebar + bubble chat).
#pragma once

#include <memory>

#include <QMainWindow>

class QListView;
class QTextEdit;
class QPushButton;
class QLabel;
class QListWidget;
class QStackedWidget;
class QWidget;

class ChatViewModel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(ChatViewModel* vm, QWidget* parent = nullptr);

    // Re-read the theme from settings and restyle everything (called by the
    // Settings dialog after a theme change, and on first show).
    void apply_theme();

private:
    void on_send();
    void on_stop();
    void on_new_chat();
    void scroll_bottom();
    void toggle_trajectory();
    void toggle_todo();
    void refresh_todo();
    void show_message_menu(const QPoint& pos);
    void on_attach_file();
    void on_workspace_mode_toggle(bool checked);
    void on_open_settings();

    ChatViewModel* vm_;

    // sidebar
    QPushButton* new_chat_btn_;
    QListWidget* session_list_;
    QPushButton* settings_btn_;

    // chat area
    QListView*  chat_list_;
    QListView*  trajectory_list_;
    QListWidget* todo_list_;
    QTextEdit*  input_;
    QPushButton* send_btn_;
    QPushButton* traj_btn_;
    QPushButton* todo_btn_;
    QPushButton* plus_btn_;       // +  attachment / upload
    QPushButton* tool_btn_;       // 🛠  Workspace Write ↔ Read-Only toggle
    QPushButton* model_pick_;     // model picker -> SettingsDialog Model page
    QLabel*     phase_label_;
    QLabel*     model_label_;
    QStackedWidget* right_panel_;
    QWidget*    right_wrap_;       // container (title bar + panel) — collapsible
    QPushButton* close_panel_btn_; // ✕ collapse button on the panel title bar
    bool        trajectory_visible_{false};
    int         todo_sub_id_{0};
};
