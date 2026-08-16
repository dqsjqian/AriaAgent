// AriaAgent — ToolTraceStore: shared tool-chain record service.
//
// Registered in the ServiceHub. The chat module appends records on every
// executed tool call; the trajectory module projects them for its panel.
// Modules talk through this service, not through each other.
#pragma once

#include <memory>

#include <aria/observable_list.hpp>

#include "ui_types.hpp"

namespace agent {

class ToolTraceStore {
public:
    aria::ObservableList<UiToolCall>& trace() { return trace_; }
    const aria::ObservableList<UiToolCall>& trace() const { return trace_; }

    void add(const ToolCallRecord& rec) {
        trace_.push_back(std::make_shared<UiToolCall>(
            UiToolCall{rec.name, rec.args, rec.result, rec.succeeded}));
    }
    void clear() { trace_.clear(); }

private:
    aria::ObservableList<UiToolCall> trace_;
};

} // namespace agent
