// AriaAgent — TrajectoryVm: the tool-chain feature module.
//
// A thin projection over the ToolTraceStore service (shared with the chat
// module). The View binds the right-hand trajectory panel to trace().
#pragma once

#include "module_api/BaseVm.h"
#include "ui_types.hpp"

#include "trajectory/services/tool_trace_store.hpp"

class TrajectoryVm : public BaseVm {
public:
    explicit TrajectoryVm(agent::ToolTraceStore& store) : store_(store) {}

    aria::ObservableList<UiToolCall>& trace() { return store_.trace(); }
    const aria::ObservableList<UiToolCall>& trace() const { return store_.trace(); }

private:
    agent::ToolTraceStore& store_;
};
