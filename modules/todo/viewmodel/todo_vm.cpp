// AriaAgent — TodoVm implementation.
#include "todo/viewmodel/todo_vm.hpp"

TodoVm::TodoVm(agent::TodoStore& store) : store_(store) {
    subscription_id_ = store_.subscribe([this] { changed.emit(); });
}

TodoVm::~TodoVm() {
    store_.unsubscribe(subscription_id_);
}
