#pragma once

#include <aria/i18n/i18n.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace agent {

// Application policy only: all Agent strings share the app domain, and
// strings.xml is the Chinese fallback catalog. Parsing and reactivity live
// in Aria; the platform shell supplies the resource directory.
inline constexpr std::string_view kTextDomain = "app";

inline std::shared_ptr<aria::i18n::LocaleContext>
make_app_locale(std::string resource_directory, std::string language = "zh-CN") {
    auto catalog = std::make_shared<aria::i18n::XmlCatalog>(
        std::move(resource_directory), "zh-CN");
    return std::make_shared<aria::i18n::LocaleContext>(
        std::move(catalog),
        aria::i18n::LocaleOptions{.language = std::move(language),
                                  .default_language = "zh-CN",
                                  .common_domain = "",
                                  .fallback_order = aria::i18n::FallbackOrder::DomainFirst});
}

} // namespace agent
