#pragma once

#include <aria/app/app.hpp>

#include "module_api/ModuleContext.h"

// The application supplies its service context; Aria owns registration,
// lazy construction, caching, checked casts, and registration lifetime.
using ViewModelProvider = aria::app::ViewModelProvider<module_api::ModuleContext>;
