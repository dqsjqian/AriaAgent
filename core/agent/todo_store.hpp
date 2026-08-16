// AriaAgent — todo snapshot store (ported from harness packages/todo).
//
// Design: a TodoList is a FULL-SNAPSHOT value type. Any change replaces the
// whole list (last-wins). This keeps the state trivially serialisable,
// replayable and free of incremental merge bugs. The agent mutates it via
// the `todo` tool; the UI projects it reactively.
#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace agent {

enum class TodoStatus { Pending, InProgress, Done };

inline const char* todo_status_str(TodoStatus s) {
    switch (s) {
        case TodoStatus::Pending:    return "pending";
        case TodoStatus::InProgress: return "in_progress";
        case TodoStatus::Done:       return "done";
    }
    return "pending";
}

struct TodoItem {
    std::string content;
    TodoStatus  status{TodoStatus::Pending};
};

class TodoStore {
public:
    static TodoStore& instance() {
        static TodoStore s;
        return s;
    }

    // Snapshot access (copy).
    std::vector<TodoItem> snapshot() const {
        std::lock_guard<std::mutex> lk(mu_);
        return items_;
    }

    // Replace the whole list (last-wins), notify observers.
    void replace(std::vector<TodoItem> items) {
        {
            std::lock_guard<std::mutex> lk(mu_);
            items_ = std::move(items);
        }
        notify();
    }

    void clear() { replace({}); }

    // Observer API (UI subscribes; fired on UI thread by caller).
    using Observer = std::function<void()>;
    int subscribe(Observer obs) {
        std::lock_guard<std::mutex> lk(mu_);
        const int id = next_id_++;
        observers_[id] = std::move(obs);
        return id;
    }
    void unsubscribe(int id) {
        std::lock_guard<std::mutex> lk(mu_);
        observers_.erase(id);
    }

    // Serialisation.
    nlohmann::json to_json() const {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& it : snapshot())
            arr.push_back({{"content", it.content}, {"status", todo_status_str(it.status)}});
        return arr;
    }
    void from_json(const nlohmann::json& j) {
        std::vector<TodoItem> items;
        if (j.is_array()) {
            for (const auto& e : j) {
                TodoItem it;
                it.content = e.value("content", "");
                const std::string s = e.value("status", "pending");
                if (s == "done") it.status = TodoStatus::Done;
                else if (s == "in_progress") it.status = TodoStatus::InProgress;
                items.push_back(std::move(it));
            }
        }
        replace(std::move(items));
    }

private:
    TodoStore() = default;
    void notify() {
        std::lock_guard<std::mutex> lk(mu_);
        for (auto& [id, obs] : observers_) if (obs) obs();
    }

    mutable std::mutex mu_;
    std::vector<TodoItem> items_;
    std::map<int, Observer> observers_;
    int next_id_{1};
};

} // namespace agent
