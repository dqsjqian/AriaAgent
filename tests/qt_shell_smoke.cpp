#include "ModulesManifest.h"
#include "agent/session_store.hpp"
#include "agent/todo_store.hpp"
#include "agent/tool_registry.hpp"
#include "app/viewmodel/app_text.hpp"
#include "chat/viewmodel/chat_view_model.hpp"
#include "i18n/AppLocale.h"
#include "main_window.hpp"
#include "sessions/viewmodel/session_list_vm.hpp"
#include "settings/platforms/qt/settings_dialog.hpp"
#include "settings/services/settings_store.hpp"
#include "settings/viewmodel/settings_vm.hpp"
#include "todo/viewmodel/todo_vm.hpp"
#include "trajectory/services/tool_trace_store.hpp"
#include "trajectory/viewmodel/trajectory_vm.hpp"

#include <aria/adapters/qt6/qt_dispatcher.hpp>
#include <aria/runtime/dispatcher.hpp>

#include <QApplication>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextEdit>

#include <cstdio>
#include <memory>
#include <stdexcept>

namespace {

void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

void drain_events() {
    for (int i = 0; i < 3; ++i) QCoreApplication::processEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

QPushButton* button_with_text(QWidget& parent, const std::string& text) {
    const auto expected = QString::fromStdString(text);
    for (auto* button : parent.findChildren<QPushButton*>()) {
        if (button->text() == expected) return button;
    }
    throw std::runtime_error("localized shell button not found");
}

struct DispatcherScope {
    std::shared_ptr<aria::runtime::IDispatcher> previous = aria::runtime::main_dispatcher();
    ~DispatcherScope() { aria::runtime::set_main_dispatcher(std::move(previous)); }
};

void verify_shell(QApplication& app, const char* resource_root) {
    QTemporaryDir temporary(QDir::tempPath() + "/ariaagent-qt-smoke-XXXXXX");
    require(temporary.isValid(), "create isolated shell data");
    const auto settings_path = temporary.filePath("settings.json").toStdString();

    // This subprocess must neither inherit provider credentials nor use the
    // platform-native AriaAgent settings. No tools or LLM requests are run.
    for (const auto* name : {"ARIA_LLM_BASE_URL", "ARIA_LLM_API_KEY", "ARIA_LLM_MODEL",
                             "ARIA_LLM_SYSTEM_PROMPT"}) qunsetenv(name);
    qputenv("ARIA_WORKSPACE_ROOT", temporary.path().toUtf8());
    auto shell_settings = std::make_shared<QSettings>(
        temporary.filePath("shell.ini"), QSettings::IniFormat);
    shell_settings->setFallbacksEnabled(false);
    require(shell_settings->fileName() == temporary.filePath("shell.ini"),
            "Qt settings use the explicitly supplied temporary file");

    DispatcherScope dispatcher_scope;
    aria::runtime::set_main_dispatcher(
        std::make_shared<aria::adapters::qt6::QtDispatcher>(&app));

    ServiceHub hub;
    auto locale = agent::make_app_locale(resource_root);
    hub.register_instance(locale);
    auto texts = std::make_shared<AppText>(locale);
    hub.register_instance(texts);
    hub.register_instance(std::make_shared<agent::ToolRegistry>());
    auto sessions = std::make_shared<agent::SessionStore>(
        locale, temporary.filePath("sessions").toStdString());
    hub.register_instance(sessions);
    hub.register_singleton<agent::ToolTraceStore>();
    hub.register_instance(std::make_shared<agent::SettingsStore>(settings_path));
    hub.register_instance(std::shared_ptr<agent::TodoStore>(
        &agent::TodoStore::instance(), [](agent::TodoStore*) {}));

    module_api::ModuleContext context(hub);
    ViewModelProvider provider(context);
    aria::SubscriptionBag registrations;
    populate_modules(provider, registrations);

    {
        MainWindow window(provider, hub, nullptr, shell_settings);
        window.show();
        drain_events();
        require(provider.get_as<ChatViewModel>("chat") != nullptr &&
                    provider.get_as<SessionListVm>("sessions") != nullptr &&
                    provider.get_as<TrajectoryVm>("trajectory") != nullptr &&
                    provider.get_as<TodoVm>("todo") != nullptr,
                "real feature manifest constructs every shell ViewModel");
        auto settings = provider.get_as<SettingsVm>("settings");
        require(settings != nullptr, "real settings ViewModel is cached");

        for (const auto* language : {"zh-CN", "en", "zh-CN"}) {
            settings->language = language;
            settings->save();
            drain_events();
            require(locale->language() == language, "settings changes the injected context");
            require(settings->title.get() == locale->tr(agent::kTextDomain, "settings_title"),
                    "settings ViewModel text follows the active language");
            (void)button_with_text(window, texts->text("new_chat"));
            (void)button_with_text(window, texts->text("settings"));
            auto* input = window.findChild<QTextEdit*>();
            require(input && input->placeholderText() ==
                        QString::fromStdString(texts->text("input_placeholder")),
                    "queued shell refresh updates the chat input");

            const auto previous_id = sessions->current_id();
            button_with_text(window, texts->text("new_chat"))->click();
            drain_events();
            require(sessions->current_id() != previous_id,
                    "localized new-chat action still reaches the session module");
            bool localized_title = false;
            for (const auto& session : sessions->list()) {
                if (session.id == sessions->current_id()) {
                    localized_title = session.title == locale->tr(agent::kTextDomain, "new_chat_default");
                }
            }
            require(localized_title, "new chat persists a title in the selected language");

            SettingsDialog dialog(settings.get(), texts.get(), &window, 0);
            dialog.show();
            drain_events();
            require(dialog.windowTitle() == QString::fromStdString(settings->title.get()),
                    "settings dialog is built with the localized title");
            auto* navigation = dialog.findChild<QListWidget*>();
            auto* pages = dialog.findChild<QStackedWidget*>();
            require(navigation && pages && pages->count() == 4 && navigation->count() == 4,
                    "all four settings pages are constructed");
            for (int page = 0; page < pages->count(); ++page) {
                navigation->setCurrentRow(page);
                require(pages->currentIndex() == page, "settings page navigation remains connected");
            }
            dialog.close();
        }
        window.close();
    }
    drain_events();
    locale->set_language("en");
    drain_events();
    registrations.clear();
    locale->set_language("zh-CN");
    drain_events();
    require(!provider.get("chat"), "registration release disposes the cached feature set");

    shell_settings->sync();
    require(shell_settings->status() == QSettings::NoError &&
                QFileInfo::exists(temporary.filePath("shell.ini")) &&
                QFileInfo::exists(QString::fromStdString(settings_path)),
            "both shell and feature preferences persist only in test storage");
    agent::SettingsValues saved;
    agent::SettingsStore(settings_path).load(saved);
    require(saved.language == "zh-CN", "settings persistence survives language round trips");
}

} // namespace

int main(int argc, char** argv) {
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    try {
        require(argc == 2, "pass the shipped resource directory");
        verify_shell(app, argv[1]);
        std::puts("Qt shell smoke passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Qt shell smoke failed: %s\n", error.what());
        return 1;
    }
}
