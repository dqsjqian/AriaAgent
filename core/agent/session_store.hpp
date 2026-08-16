// AriaAgent — SessionStore: session service (multi-session persistence +
// current-session state). Registered in the ServiceHub and shared by the
// sessions (SessionListVm) and chat (ChatViewModel) modules — the two
// modules never reference each other, only this service.
//
// Design (ported from deepseek-harness packages/core/session): the
// conversation log is the single source of truth. Each session is a JSON
// file under ~/.ariaagent/sessions/<id>.json containing the full ordered
// message history (which the engine can replay for multi-turn).
#pragma once

#include <string>
#include <vector>

#include <aria/detail/typed_signal.hpp>

#include "agent/model.hpp"

namespace agent {

struct SessionMeta {
    std::string id;
    std::string title;
    int64_t     created_at{0};
    int64_t     updated_at{0};

    bool operator==(const SessionMeta& o) const {
        return id == o.id && title == o.title &&
               created_at == o.created_at && updated_at == o.updated_at;
    }
};

// Pure C++ storage + current-session state (no Qt). The UI serialises
// access through the ViewModels (single-threaded on the UI thread).
class SessionStore {
public:
    SessionStore();

    // Directory used for persistence (creates it on demand).
    static std::string base_dir();

    // ── Store (session files) ──────────────────────────────────────────────
    /// Create a new empty session file, returns its id.
    std::string create(const std::string& title = {});

    /// List all sessions, newest first.
    std::vector<SessionMeta> list() const;

    /// Load a session's message log (empty if missing).
    MessageList load(const std::string& id) const;

    /// Remove a session file.
    void remove(const std::string& id);

    // ── Current-session state (shared by sessions + chat modules) ──────────
    const std::string& current_id() const { return current_id_; }
    MessageList&       current_history() { return current_history_; }
    const MessageList& current_history() const { return current_history_; }

    /// Create a fresh session and switch to it (emits session_changed).
    void create_session();

    /// Switch the current session (persists the outgoing one first;
    /// emits session_changed).
    void switch_session(const std::string& id);

    /// Delete a session; if it was current, switch to the newest remaining
    /// (emits session_changed).
    void delete_session(const std::string& id);

    /// Save the current session log (title derived from first user text).
    void persist_current(const std::string& title_hint = {});

    /// Fired after create/switch/delete — modules reload their projections.
    aria::detail::TypedSignal<> session_changed;

private:
    std::string path_for(const std::string& id) const;
    std::string derive_title() const;

    std::string   current_id_;
    MessageList   current_history_;
};

} // namespace agent
