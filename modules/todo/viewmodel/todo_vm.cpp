// AriaAgent — TodoVm implementation.
#include "todo/viewmodel/todo_vm.hpp"

TodoVm::TodoVm(agent::TodoStore& store,
                         std::shared_ptr<aria::i18n::LocaleContext> locale)
    : BaseVm(std::move(locale)), store_(store) {
    subscription_id_ = store_.subscribe([this] { changed.emit(); });
}

TodoVm::~TodoVm() {
    store_.unsubscribe(subscription_id_);
}
