// AriaAgent — entry point.
#include <QApplication>

#include "agent/fs_tools.hpp"
#include "agent/llm_client.hpp"
#include "agent/shell_tools.hpp"
#include "agent/todo_tools.hpp"
#include "agent/tool_registry.hpp"
#include "ui/chat_view_model.hpp"
#include "ui/main_window.hpp"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    agent::ToolRegistry registry;
    agent::register_builtin_tools(registry);
    agent::register_shell_tools(registry);
    agent::register_fs_tools(registry);
    agent::register_todo_tools(registry);

    auto* vm = new ChatViewModel(std::move(registry));
    MainWindow win(vm);
    win.show();

    return app.exec();
}
