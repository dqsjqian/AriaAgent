#include "ModulesManifest.h"
#include "i18n/AppLocale.h"
#include "module_api/ViewModelProvider.h"
#include "trajectory/services/tool_trace_store.hpp"
#include "trajectory/viewmodel/trajectory_vm.hpp"

#include <cstdio>
#include <memory>
#include <stdexcept>

namespace {

void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

template <typename Error, typename F>
void require_throws(F&& operation, const char* message) {
    try {
        operation();
    } catch (const Error&) {
        return;
    }
    throw std::runtime_error(message);
}

struct Count { int created = 0; };
class ProbeVm final : public aria::binding::ViewModel {};
class OtherVm final : public aria::binding::ViewModel {};

void verify_provider() {
    ServiceHub hub;
    auto count = std::make_shared<Count>();
    hub.register_instance(count);
    module_api::ModuleContext context(hub);
    ViewModelProvider provider(context);
    auto make_probe = [](module_api::ModuleContext& ctx) {
        ++ctx.service<Count>().created;
        return std::make_shared<ProbeVm>();
    };
    auto registration = provider.add("probe", make_probe);
    require(count->created == 0, "registering a module must not create its ViewModel");
    auto first = provider.get_as<ProbeVm>("probe");
    require(first && count->created == 1, "first access constructs from the injected context");
    require(provider.get("probe") == first && count->created == 1,
            "repeated shell access shares the cached ViewModel");
    require(!provider.get_as<OtherVm>("probe"), "wrong ViewModel type must not be cast unsafely");
    require(!provider.get("missing"), "unknown module does not create a ViewModel");

    auto fresh = provider.create("probe");
    require(fresh && fresh != first && count->created == 2, "fresh construction is explicit");
    require(provider.get("probe") == first, "fresh construction does not replace the cached instance");
    require_throws<std::invalid_argument>(
        [&] { auto duplicate = provider.add("probe", make_probe); }, "duplicate module id rejected");

    provider.clear_cache();
    auto replacement = provider.get("probe");
    require(replacement != first && count->created == 3, "cache reset allows a new module instance");
    registration.release();
    require(!provider.get("probe"), "releasing registration revokes the factory and cache");
    require(first != nullptr, "external ViewModel ownership survives module revocation");

    auto cyclic = provider.add("cycle", [&](module_api::ModuleContext&) {
        return provider.get("cycle");
    });
    require_throws<std::logic_error>([&] { (void)provider.get("cycle"); },
                                     "recursive module construction diagnosed");
    cyclic.release();
    auto recovered = provider.add("cycle", make_probe);
    require(provider.get("cycle") != nullptr, "failed creation leaves registry reusable");
}

void verify_feature_manifest(const char* resource_root) {
    ServiceHub hub;
    module_api::ModuleContext context(hub);
    ViewModelProvider provider(context);
    aria::SubscriptionBag registrations;

    // Registering the real feature set must be lazy: there are deliberately
    // no services yet, and no session/settings files may be opened here.
    populate_modules(provider, registrations);
    hub.register_instance(agent::make_app_locale(resource_root));
    auto trace = std::make_shared<agent::ToolTraceStore>();
    hub.register_instance(trace);
    auto vm = provider.get_as<TrajectoryVm>("trajectory");
    require(vm && &vm->trace() == &trace->trace(), "real feature resolves the app-owned service");
    require(provider.get_as<TrajectoryVm>("trajectory") == vm,
            "real feature retains the cached instance");
    registrations.clear();
    require(!provider.get("trajectory") && !provider.get("chat"),
            "manifest tokens revoke all features without eager construction");
}

} // namespace

int main(int argc, char** argv) {
    try {
        verify_provider();
        if (argc > 1) verify_feature_manifest(argv[1]);
        std::puts("module smoke passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "module smoke failed: %s\n", error.what());
        return 1;
    }
}
