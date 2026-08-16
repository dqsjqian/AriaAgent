// AriaAgent — Qt entry point (platform shell).
//
// The only Qt code that must exist: create QApplication, install the
// QtDispatcher into aria::runtime so the framework-agnostic ViewModel can
// marshal callbacks to the UI thread, and hand the VM to the Qt shell.
#include <QApplication>
#include <QMessageBox>

#include <aria/adapters/qt6/qt_dispatcher.hpp>
#include <aria/runtime/dispatcher.hpp>

#include "agent/fs_tools.hpp"
#include "agent/llm_client.hpp"
#include "agent/shell_tools.hpp"
#include "agent/todo_tools.hpp"
#include "agent/tool_registry.hpp"
#include "viewmodel/chat_view_model.hpp"
#include "main_window.hpp"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // Bridge aria's main dispatcher onto Qt's event loop so the pure-C++
    // VM can hop back to the UI thread without knowing Qt exists.
    aria::runtime::set_main_dispatcher(
        std::make_shared<aria::adapters::qt6::QtDispatcher>(&app));

    agent::ToolRegistry registry;
    agent::register_builtin_tools(registry);
    agent::register_shell_tools(registry);
    agent::register_fs_tools(registry);
    agent::register_todo_tools(registry);

    auto* vm = new ChatViewModel(std::move(registry));
    // The approval prompt is a VIEW concern: the VM only asks, the shell
    // answers. A different platform shell (iOS/Android/Web) injects its
    // own native prompt here and reuses the identical VM.
    vm->approval_ui = [](const std::string& tool, const std::string& args) {
        QMessageBox box(QMessageBox::Question,
            QStringLiteral("确认执行工具"),
            QStringLiteral("Agent 请求执行需要授权的工具:\n\n"
                           "<b>%1</b>\n<pre>%2</pre>\n\n是否允许?")
                .arg(QString::fromStdString(tool),
                     QString::fromStdString(args)),
            QMessageBox::Yes | QMessageBox::No);
        box.setDefaultButton(QMessageBox::No);   // fail-closed default
        return box.exec() == QMessageBox::Yes;
    };

    MainWindow win(vm);
    win.show();

    return app.exec();
}
