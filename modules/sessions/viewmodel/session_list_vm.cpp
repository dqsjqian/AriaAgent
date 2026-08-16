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
    // Order matters: the MainWindow observes `sessions` and calls
    // refresh_session_list() synchronously when it fires. If we wrote
    // `sessions` first, the observer's callback would read `current_id`
    // before this function's next line executed — i.e. the OLD current_id
    // — and would setCurrentItem() the wrong row (visual highlight lags
    // behind the chat pane, which already switched via the store). Set
    // current_id first so both Properties are coherent by the time the
    // observer runs.
    current_id = store_.current_id();
    sessions   = store_.list();
}
