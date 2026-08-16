// AriaAgent — Qt entry point (platform shell).
//
// The only Qt code that must exist: create QApplication, install the
// QtDispatcher into aria::runtime so the framework-agnostic ViewModel can
// marshal callbacks to the UI thread, inject the i18n backend, and hand
// the VM to the Qt shell.
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>

#include <memory>

#include <aria/adapters/qt6/qt_dispatcher.hpp>
#include <aria/runtime/dispatcher.hpp>

#include "agent/fs_tools.hpp"
#include "agent/llm_client.hpp"
#include "agent/shell_tools.hpp"
#include "agent/todo_tools.hpp"
#include "agent/tool_registry.hpp"
#include "i18n/I18n.h"
#include "i18n/XmlI18nService.h"
#include "viewmodel/chat_view_model.hpp"
#include "main_window.hpp"

namespace {

// Locate the runtime i18n/ directory: beside the exe, inside a macOS .app
// bundle, or the source tree (development builds).
std::string resolve_i18n_dir() {
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        appDir + "/i18n",
        appDir + "/../Resources/i18n",                      // macOS .app bundle
        appDir + "/../../../../i18n",                       // build/… → source tree
        appDir + "/../../../i18n",
    };
    for (const auto& c : candidates) {
        if (QFileInfo(c + "/app/strings.xml").exists())
            return QDir(c).absolutePath().toStdString();
    }
    return QDir(appDir + "/i18n").absolutePath().toStdString();
}

} // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // Bridge aria's main dispatcher onto Qt's event loop so the pure-C++
    // VM can hop back to the UI thread without knowing Qt exists.
    aria::runtime::set_main_dispatcher(
        std::make_shared<aria::adapters::qt6::QtDispatcher>(&app));

    // i18n backend (lives for the whole app; VM/Views read via i18n::str()).
    static auto i18n_service = std::make_unique<agent::services::XmlI18nService>(
        resolve_i18n_dir(), "zh-CN", "zh-CN");
    agent::i18n::set_backend(i18n_service.get());

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
            QString::fromStdString(agent::i18n::str("approve_title")),
            QString::fromStdString(agent::i18n::str("approve_body"))
                .replace("%1", QString::fromStdString(tool))
                .replace("%2", QString::fromStdString(args)),
            QMessageBox::Yes | QMessageBox::No);
        box.setDefaultButton(QMessageBox::No);   // fail-closed default
        return box.exec() == QMessageBox::Yes;
    };

    MainWindow win(vm);
    win.show();

    return app.exec();
}
