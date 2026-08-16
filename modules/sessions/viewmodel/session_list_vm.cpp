// AriaAgent — SessionListVm implementation.
#include "sessions/viewmodel/session_list_vm.hpp"

SessionListVm::SessionListVm(agent::SessionStore& store) : store_(store) {
    sessions = store_.list();
    current_id = store_.current_id();
    // Keep the projection in sync whenever the store changes (create /
    // switch / delete, initiated by this VM or by the chat module).
    sub_ = store_.session_changed.connect([this] { refresh(); });
}

void SessionListVm::refresh() {
    sessions = store_.list();
    current_id = store_.current_id();
}
