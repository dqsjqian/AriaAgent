// AriaAgent — SessionListVm: the sessions feature module.
//
// A thin projection over the SessionStore service: it exposes the session
// list + current id as reactive Properties and forwards user actions to the
// store. The chat module shares the SAME store service — neither module
// references the other.
#pragma once

#include <string>
#include <vector>

#include <aria/aria.hpp>
#include <aria/subscription.hpp>

#include "module_api/BaseVm.h"

#include "agent/session_store.hpp"

class SessionListVm : public BaseVm {
public:
    explicit SessionListVm(agent::SessionStore& store);

    // ── Reactive surface (bound by the sidebar) ────────────────────────────
    aria::Property<std::vector<agent::SessionMeta>> sessions;
    aria::Property<std::string>                     current_id;

    // ── Actions (called by the View) ───────────────────────────────────────
    void new_session()    { store_.create_session(); }
    void switch_session(const std::string& id) { store_.switch_session(id); }
    void delete_session(const std::string& id) { store_.delete_session(id); }

private:
    void refresh();

    agent::SessionStore& store_;
    aria::Subscription   sub_;
};
