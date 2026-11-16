#include "i18n/AppLocale.h"
#include "agent/session_store.hpp"
#include "module_api/BaseVm.h"
#include "app/viewmodel/app_text.hpp"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

class TestResources {
public:
    TestResources() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        root = std::filesystem::temp_directory_path() /
               ("ariaagent-localization-" + std::to_string(stamp));
        require(std::filesystem::create_directory(root), "create isolated resources");
        std::filesystem::create_directory(root / "app");
        write("strings.xml", R"(<resources>
<string name="send">发送</string>
<string name="fallback">默认文字</string>
<string name="empty"></string>
<string name="escaped">A &amp; B &lt; C &quot;quote&quot; &#65;</string>
<string name="new_chat_default">新对话</string>
</resources>)");
        write("strings_en.xml", R"(<resources>
<string name="send">Send</string>
<string name="new_chat_default">New chat</string>
</resources>)");
    }

    ~TestResources() {
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
    }

    std::filesystem::path root;

private:
    void write(const char* name, const char* contents) {
        std::ofstream stream(root / "app" / name, std::ios::binary);
        stream << contents;
        require(static_cast<bool>(stream), "write isolated catalog");
    }
};

class LocalizedVm final : public BaseVm {
public:
    explicit LocalizedVm(std::shared_ptr<aria::i18n::LocaleContext> locale)
        : BaseVm(std::move(locale)) { text(label, "send"); }
    aria::Property<std::string> label;
};

void verify_localization() {
    TestResources resources;
    auto locale = agent::make_app_locale(resources.root.string());
    auto text = [&](std::string_view key) { return locale->tr(agent::kTextDomain, key); };

    require(text("send") == "发送", "initial app-domain lookup");
    require(locale->tr(agent::kTextDomain, "send", "en") == "Send",
            "explicit language lookup");
    require(locale->language() == "zh-CN", "explicit lookup must not switch language");
    require(text("empty").empty(), "empty translations are present values");
    require(text("escaped") == "A & B < C \"quote\" A", "XML entity decoding");
    require(text("unknown") == "[app/unknown]", "missing-key placeholder");

    AppText texts(locale);
    LocalizedVm vm(locale);
    require(texts.text("send") == "发送" && vm.label.get() == "发送",
            "app text and ViewModel initialize from the catalog");
    vm.activate();
    vm.deactivate();
    int changes = 0;
    auto subscription = locale->on_language_changed(
        [&](const std::string&) { ++changes; });
    locale->set_language("en");
    require(texts.text("send") == "Send" && vm.label.get() == "Send",
            "language switch refreshes app text and inactive ViewModels");
    require(text("fallback") == "默认文字", "default-language fallback");
    require(changes == 1, "one notification per language change");
    locale->set_language("en");
    require(changes == 1, "same language must not notify again");
    subscription.release();
    locale->set_language("zh-CN");
    require(changes == 1 && vm.label.get() == "发送", "subscription release is scoped");
    {
        LocalizedVm temporary(locale);
        require(temporary.label.get() == "发送", "temporary ViewModel localized");
    }
    locale->set_language("en");
    require(vm.label.get() == "Send", "destroyed ViewModel does not retain callbacks");

    // Two app contexts must coexist without the old process-wide backend.
    auto second_locale = agent::make_app_locale(resources.root.string());
    AppText second_texts(second_locale);
    LocalizedVm second_vm(second_locale);
    require(second_texts.text("send") == "发送" && second_vm.label.get() == "发送",
            "independent contexts start with their own language");
    require(texts.text("send") == "Send" && vm.label.get() == "Send",
            "creating another context does not replace the first backend");
    second_locale->set_language("fr");
    require(second_vm.label.get() == "发送" && vm.label.get() == "Send",
            "fallback and language notifications stay within each context");

    {
        std::ofstream updated(resources.root / "app" / "strings_en.xml", std::ios::binary);
        updated << R"(<resources><string name="send">Send now</string>
<string name="new_chat_default">New chat</string></resources>)";
        require(static_cast<bool>(updated), "replace isolated English catalog");
    }
    require(locale->reload().has_value(), "reload updated catalog");
    require(texts.text("send") == "Send now" && vm.label.get() == "Send now",
            "catalog reload refreshes AppText and ViewModels without switching language");

    // The production default remains unchanged; tests explicitly select a
    // disposable directory and never open ~/.ariaagent/sessions.
    const auto session_directory = resources.root / "sessions";
    agent::SessionStore sessions(locale, session_directory.string());
    const auto first_id = sessions.current_id();
    require(!first_id.empty() && sessions.list().front().title == "New chat",
            "new session title uses the injected context");
    locale->set_language("zh-CN");
    sessions.create_session();
    require(sessions.current_id() != first_id, "new session has a distinct id");
    bool found_current = false;
    for (const auto& item : sessions.list()) {
        if (item.id == sessions.current_id()) {
            found_current = item.title == "新对话";
        }
    }
    require(found_current, "new session picks up language changes");
    sessions.persist_current("Explicit title");
    const auto current_id = sessions.current_id();
    agent::SessionStore reopened(locale, session_directory.string());
    bool preserved = false;
    for (const auto& item : reopened.list()) {
        if (item.id == current_id) preserved = item.title == "Explicit title";
    }
    require(preserved, "explicit session title survives reopening");
}

} // namespace

int main(int argc, char** argv) {
    try {
        verify_localization();
        if (argc > 1) {
            auto locale = agent::make_app_locale(argv[1]);
            require(locale->tr(agent::kTextDomain, "send") == "发送",
                    "shipped Chinese catalog parses");
            require(locale->tr(agent::kTextDomain, "send", "en") == "Send",
                    "shipped English catalog parses");
            require(locale->tr(agent::kTextDomain, "approve_body", "en").find("<b>%1</b>") !=
                        std::string::npos,
                    "shipped escaped markup and placeholders are preserved");
        }
        std::puts("localization smoke passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "localization smoke failed: %s\n", error.what());
        return 1;
    }
}
