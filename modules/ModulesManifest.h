// AriaAgent — ModulesManifest: explicit assembly list of feature modules.
//
// Static libraries strip unreferenced symbols at link time, so modules are
// registered here explicitly (no global-constructor auto-registration).
// Adding a new feature = add a modules/<m>/ directory + one line here.
#pragma once

#include "module_api/ModuleRegistry.h"

/// Register every business module (called once by the app shell).
void populate_modules(module_api::ModuleRegistry& registry);
