// Explicit feature assembly keeps static-library factories reachable.
#pragma once

#include "module_api/ViewModelProvider.h"
#include <aria/subscription.hpp>

// Keep registrations alive for the app lifetime. Releasing a token revokes
// its factory and cached ViewModel through Aria's application layer.
void populate_modules(ViewModelProvider& provider, aria::SubscriptionBag& registrations);
