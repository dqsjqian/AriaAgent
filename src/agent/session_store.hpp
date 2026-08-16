// AriaAgent — session store: multi-session persistence (JSON on disk).
//
// Ported design from deepseek-harness packages/core/session:
// the conversation log is the single source of truth. Each session is a
// JSON file under ~/.ariaagent/sessions/<id>.json containing the full
// ordered message history (which the engine can replay for multi-turn).
#pragma once

#include <string>
#include <vector>

#include "agent/model.hpp"

namespace agent {

struct SessionMeta {
    std::string id;
    std::string title;
    int64_t     created_at{0};
    int64_t     updated_at{0};
};

// Pure C++ storage layer (no Qt). Thread-safe per call; the UI serialises
// access through the ViewModel.
class SessionStore {
public:
    SessionStore();

    // Directory used for persistence (creates it on demand).
    static std::string base_dir();

    // Create a new empty session, returns its id.
    std::string create(const std::string& title = {});

    // List all sessions, newest first.
    std::vector<SessionMeta> list() const;

    // Load a session's message log (empty if missing).
    MessageList load(const std::string& id) const;

    // Save the message log; updates title (first user text) + timestamps.
    void save(const std::string& id, const MessageList& messages,
              const std::string& title_hint = {});

    // Remove a session file.
    void remove(const std::string& id);

    // Delete ALL session files (used by "new chat" flows if desired).
    void clear_all();

private:
    std::string path_for(const std::string& id) const;
};

} // namespace agent
