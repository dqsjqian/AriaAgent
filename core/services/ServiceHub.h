// AriaAgent — ServiceHub: stable service layer aggregate.
//
// Ported from AiTools Workbench (core/infra/ServiceHub.h). Holds the DI
// Container; modules obtain services only via ModuleContext::service<T>()
// and never know concrete sibling implementations. This is what makes the
// feature modules genuinely decoupled plugins.
#pragma once

#include <memory>

#include <aria/runtime/container.hpp>

#include "i18n/II18nService.h"

class ServiceHub {
public:
    ServiceHub();
    ~ServiceHub();

    ServiceHub(const ServiceHub&) = delete;
    ServiceHub& operator=(const ServiceHub&) = delete;

    /// Fetch a stable service (resolved via the DI Container).
    template <typename T>
    T& service() { return *container_.resolve<T>(); }

    template <typename T, typename Impl = T>
    void register_singleton() { container_.register_singleton<T, Impl>(); }

    template <typename T>
    void register_instance(std::shared_ptr<T> ptr) {
        container_.register_instance<T>(std::move(ptr));
    }

    [[nodiscard]] aria::runtime::Container& container() { return container_; }

    // Convenience direct access (high-frequency services).
    [[nodiscard]] agent::services::II18nService& i18n() {
        return service<agent::services::II18nService>();
    }

private:
    aria::runtime::Container container_;
};
