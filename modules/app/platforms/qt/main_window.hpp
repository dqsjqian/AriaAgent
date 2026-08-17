// AriaAgent — main window (app module, Qt shell).
// Acts as the VIEW CONTROLLER: it binds the module ViewModels to the
// widgets. VMs are requested lazily through the ViewModelProvider (never
// created up-front in main()); all user-facing strings come from the app
// module's AppText service (or from VM Properties) — never from i18n
// directly. When Aria grows a real ViewModelLocator/binding layer, this
// window will consume that instead of ViewModelProvider.
#pragma once

#include <memory>

#include <QMainWindow>
#include <QString>

#include <aria/subscription.hpp>

#include "module_api/ViewModelProvider.h"
#include "services/ServiceHub.h"

class QListView;
class QTextEdit;
class QPushButton;
class QLabel;
class QListWidget;
class QStackedWidget;
class QWidget;

class AppText;
class ChatViewModel;
class SessionListVm;
class TrajectoryVm;
class TodoVm;
class SettingsVm;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(ViewModelProvider& vms, ServiceHub& hub,
                        QWidget* parent = nullptr);

    // Re-read the theme from settings and restyle everything.
    void apply_theme();

    // Refresh all user-facing labels from AppText (startup + language change).
    void apply_language();

protected:
    // Enter/Ctrl+Enter send handling for the chat input box (P2: enter_behavior).
    bool eventFilter(QObject* obj, QEvent* ev) override;
    void showEvent(QShowEvent* ev) override;

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
    void on_workspace_mode_select();
    void on_choose_workspace();
    void set_workspace_level(int level);   // 0=read-only, 1=write, 2=full
    void update_workspace(const QString& path, int level);
    void open_message_link(const QString& href);
    void on_model_select();
    void refresh_model_selector();
    void on_open_settings();
    void refresh_session_list();

    // module VMs (owned by main.cpp; raw pointers here)
    AppText*       texts_;
    SettingsVm*    settings_vm_;
    SessionListVm* sessions_vm_;
    TrajectoryVm*  traj_vm_;
    TodoVm*        todo_vm_;
    ChatViewModel* chat_vm_;

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
    QPushButton* plus_btn_;
    QPushButton* tool_btn_;
    QPushButton* model_pick_;
    QLabel*     phase_label_;
    QLabel*     model_label_;
    QPushButton* workspace_label_;
    QString     workspace_root_;
    int         ws_level_{1};
    QStackedWidget* right_panel_;
    QWidget*    right_wrap_;
    QPushButton* close_panel_btn_;
    bool        trajectory_visible_{false};

    // aria subscriptions into the VMs (kept alive for the window's lifetime)
    aria::Subscription session_proj_sub_;
    aria::Subscription error_sub_;
    aria::Subscription lang_sub_;
    aria::Subscription settings_sub_;
    aria::Subscription todo_sub_;
};
