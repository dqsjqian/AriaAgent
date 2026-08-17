// AriaAgent — Qt entry point (app module shell).
//
// The only Qt code that must exist: create QApplication, install the
// QtDispatcher, inject the i18n backend, register the stable services in
// the ServiceHub, assemble the modules via the explicit manifest, and hand
// the composed ViewModels to the app shell.
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>

#include <memory>

#include <aria/adapters/qt6/qt_dispatcher.hpp>
#include <aria/runtime/dispatcher.hpp>

#include "agent/fs_tools.hpp"
#include "agent/llm_client.hpp"
#include "agent/session_store.hpp"
#include "agent/shell_tools.hpp"
#include "agent/skill_tools.hpp"
#include "agent/todo_store.hpp"
#include "agent/todo_tools.hpp"
#include "agent/tool_registry.hpp"
#include "services/ServiceHub.h"
#include "i18n/I18n.h"
#include "i18n/XmlI18nService.h"

#include "app/viewmodel/app_text.hpp"
#include "chat/viewmodel/chat_view_model.hpp"
#include "ModulesManifest.h"
#include "trajectory/services/tool_trace_store.hpp"
#include "settings/services/settings_store.hpp"
#include "sessions/viewmodel/session_list_vm.hpp"
#include "settings/viewmodel/settings_vm.hpp"
#include "todo/viewmodel/todo_vm.hpp"
#include "trajectory/viewmodel/trajectory_vm.hpp"

#include "main_window.hpp"

namespace {

// Locate the runtime i18n/ directory: beside the exe, inside a macOS .app
// bundle, or the source tree (development builds).
std::string resolve_project_root() {
    QDir current(QCoreApplication::applicationDirPath());
    while (current.cdUp()) {
        if (QFileInfo(current.filePath("CMakeLists.txt")).exists() &&
            QFileInfo(current.filePath("core/agent")).isDir()) {
            return current.absolutePath().toStdString();
        }
    }
    return QDir::currentPath().toStdString();
}

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
    // VMs can hop back to the UI thread without knowing Qt exists.
    aria::runtime::set_main_dispatcher(
        std::make_shared<aria::adapters::qt6::QtDispatcher>(&app));

    // ── ServiceHub: register the stable services ───────────────────────────
    ServiceHub hub;
    qputenv("ARIA_WORKSPACE_ROOT", QByteArray::fromStdString(resolve_project_root()));

    // i18n backend (lives for the whole app; VMs read via i18n::str).
    static auto i18n_service = std::make_unique<agent::services::XmlI18nService>(
        resolve_i18n_dir(), "zh-CN", "zh-CN");
    hub.register_instance<agent::services::II18nService>(std::move(i18n_service));
    agent::i18n::set_backend(&hub.i18n());

    // App module's UI-string service (consumed by the view controller).
    hub.register_singleton<AppText>();

    // Tool registry (populated once; chat module resolves it).
    auto tool_registry = std::make_shared<agent::ToolRegistry>();
    agent::register_builtin_tools(*tool_registry);
    agent::register_shell_tools(*tool_registry);
    agent::register_fs_tools(*tool_registry);
    agent::register_todo_tools(*tool_registry);
    agent::register_skill_tools(*tool_registry, resolve_project_root());
    hub.register_instance(tool_registry);

    // Cross-module state services.
    hub.register_singleton<agent::SessionStore>();
    hub.register_singleton<agent::ToolTraceStore>();
    hub.register_singleton<agent::SettingsStore>();
    // TodoStore is a singleton by design; share it (no ownership).
    hub.register_instance(std::shared_ptr<agent::TodoStore>(
        &agent::TodoStore::instance(), [](agent::TodoStore*) {}));

    // ── Modules (plugin manifest) + lazy VM provider ───────────────────────
    module_api::ModuleRegistry modules;
    populate_modules(modules);
    ViewModelProvider vms(hub, modules);

    // The approval prompt is a VIEW concern: the VM only asks, the view
    // controller answers (localized text from the app module's AppText).
    {
        auto chat_vm = vms.view_model_as<ChatViewModel>("chat");
        AppText& texts = hub.service<AppText>();
        chat_vm->approval_ui = [&texts](const std::string& tool,
                                        const std::string& args) {
            QMessageBox box(QMessageBox::Question,
                QString::fromStdString(texts.text("approve_title")),
                QString::fromStdString(texts.text("approve_body"))
                    .replace("%1", QString::fromStdString(tool))
                    .replace("%2", QString::fromStdString(args)),
                QMessageBox::Yes | QMessageBox::No);
            box.setDefaultButton(QMessageBox::No);   // fail-closed default
            return box.exec() == QMessageBox::Yes;
        };
    }

    // The view controller lazily grabs the VMs it binds.
    MainWindow win(vms, hub);
    win.show();

    return app.exec();
}
