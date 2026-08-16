// AriaAgent — TodoVm: the todo-board feature module.
//
// Projects the shared TodoStore service into reactive state so the View
// binds declaratively. Ported from the harness todo/plan/goal snapshot
// projection idea.
#pragma once

#include <vector>

#include <aria/aria.hpp>
#include <aria/detail/typed_signal.hpp>

#include "module_api/BaseVm.h"

#include "agent/todo_store.hpp"

class TodoVm : public BaseVm {
public:
    explicit TodoVm(agent::TodoStore& store);
    ~TodoVm() override;

    /// Snapshot of the current todo board (agent-visible, last-wins).
    std::vector<agent::TodoItem> items() const { return store_.snapshot(); }

    /// Fired whenever the board changes (agent tool ran todo_set/todo_add).
    aria::detail::TypedSignal<> changed;

private:
    agent::TodoStore& store_;
    int subscription_id_{0};
};
