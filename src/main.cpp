// AriaAgent — entry point.
#include <QApplication>
#include <QMessageBox>
#include <QTimer>

#include "agent/fs_tools.hpp"
#include "agent/llm_client.hpp"
#include "agent/shell_tools.hpp"
#include "agent/tool_registry.hpp"
#include "ui/chat_view_model.hpp"
#include "ui/main_window.hpp"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    agent::ToolRegistry registry;
    agent::register_builtin_tools(registry);
    agent::register_shell_tools(registry);
    agent::register_fs_tools(registry);

    auto* vm = new ChatViewModel(std::move(registry));
    MainWindow win(vm);
    win.show();

    // Surface startup errors (e.g. missing API key) once the loop runs.
    QTimer::singleShot(0, [&win] {
        const char* key = std::getenv("ARIA_LLM_API_KEY");
        if (!key) key = std::getenv("DEEPSEEK_API_KEY");
        if (!key) key = std::getenv("OPENAI_API_KEY");
        if (!key || !*key) {
            QMessageBox::warning(&win,
                QStringLiteral("No LLM API key configured"),
                QStringLiteral("Set ARIA_LLM_API_KEY (any OpenAI-compatible "
                               "provider) before asking the agent.\n\n"
                               "  export ARIA_LLM_API_KEY=sk-...\n\n"
                               "Default endpoint: https://api.deepseek.com\n"
                               "Override with ARIA_LLM_BASE_URL / ARIA_LLM_MODEL.\n\n"
                               "The app will start anyway."));
        }
    });

    return app.exec();
}
