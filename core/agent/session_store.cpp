// AriaAgent — session store implementation.
#include "agent/session_store.hpp"

#include "i18n/I18n.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {

int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::string home_dir() {
    const char* h = std::getenv("USERPROFILE");
    if (!h || !*h) h = std::getenv("HOME");
    return h ? h : ".";
}

} // namespace

namespace agent {

SessionStore::SessionStore() {
    // Start with the most recent session, or create a fresh one.
    auto existing = list();
    if (!existing.empty()) {
        switch_session(existing.front().id);
    } else {
        create_session();
    }
}

std::string SessionStore::base_dir() {
    return home_dir() + "/.ariaagent/sessions";
}

std::string SessionStore::path_for(const std::string& id) const {
    return base_dir() + "/" + id + ".json";
}

std::string SessionStore::create(const std::string& title) {
    static std::atomic<int64_t> counter{0};
    const int64_t t = now_ms();
    const std::string id = "s" + std::to_string(t) + "_" +
                           std::to_string(counter.fetch_add(1));
    (void)title;
    json doc;
    doc["id"] = id;
    doc["title"] = title.empty() ? agent::i18n::str("new_chat_default") : title;
    doc["created_at"] = t;
    doc["updated_at"] = t;
    doc["messages"] = json::array();
    std::ofstream f(path_for(id), std::ios::trunc);
    f << doc.dump(2);
    return id;
}

std::vector<SessionMeta> SessionStore::list() const {
    std::vector<SessionMeta> out;
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::is_directory(base_dir(), ec)) return out;
    for (const auto& e : fs::directory_iterator(base_dir(), ec)) {
        if (e.path().extension() != ".json") continue;
        std::ifstream f(e.path());
        if (!f.is_open()) continue;
        try {
            const auto doc = json::parse(f);
            SessionMeta m;
            m.id = doc.value("id", e.path().stem().string());
            m.title = doc.value("title", agent::i18n::str("new_chat_default"));
            m.created_at = doc.value("created_at", static_cast<int64_t>(0));
            m.updated_at = doc.value("updated_at", static_cast<int64_t>(0));
            out.push_back(std::move(m));
        } catch (const std::exception&) {
            continue;
        }
    }
    std::sort(out.begin(), out.end(),
              [](const SessionMeta& a, const SessionMeta& b) {
                  return a.created_at > b.created_at;
              });
    return out;
}

MessageList SessionStore::load(const std::string& id) const {
    MessageList out;
    std::ifstream f(path_for(id));
    if (!f.is_open()) return out;
    try {
        const auto doc = json::parse(f);
        for (const auto& e : doc.value("messages", json::array())) {
            out.push_back(ChatMessage::from_storage_json(e));
        }
    } catch (const std::exception&) {
        out.clear();
    }
    return out;
}

void SessionStore::remove(const std::string& id) {
    std::error_code ec;
    std::filesystem::remove(path_for(id), ec);
}

// ── Current-session state ───────────────────────────────────────────────────
void SessionStore::create_session() {
    current_id_ = create();
    current_history_.clear();
    session_changed.emit();
}

void SessionStore::switch_session(const std::string& id) {
    if (id.empty() || id == current_id_) return;
    persist_current();              // save the outgoing session first
    current_id_ = id;
    current_history_ = load(id);
    session_changed.emit();
}

void SessionStore::delete_session(const std::string& id) {
    remove(id);
    if (id == current_id_) {
        current_id_ = "";
        current_history_.clear();
        auto remaining = list();
        if (!remaining.empty()) {
            current_id_ = remaining.front().id;
            current_history_ = load(current_id_);
        } else {
            current_id_ = create();
            current_history_.clear();
        }
    }
    session_changed.emit();
}

void SessionStore::persist_current(const std::string& title_hint) {
    if (current_id_.empty()) return;
    const std::string t = title_hint.empty() ? derive_title() : title_hint;
    const int64_t ts = now_ms();
    json doc;
    doc["id"] = current_id_;
    doc["title"] = t.empty() ? agent::i18n::str("new_chat_default") : t;
    // Preserve the original created_at (never overwrite — sorting key).
    {
        std::ifstream f(path_for(current_id_));
        if (f.is_open()) {
            try {
                const auto old = json::parse(f);
                doc["created_at"] = old.value("created_at", ts);
            } catch (const std::exception&) {
                doc["created_at"] = ts;
            }
        } else {
            doc["created_at"] = ts;
        }
    }
    doc["updated_at"] = ts;
    doc["messages"] = json::array();
    for (const auto& m : current_history_) doc["messages"].push_back(m.to_storage_json());
    std::ofstream f(path_for(current_id_), std::ios::trunc);
    f << doc.dump(2);
}

std::string SessionStore::derive_title() const {
    for (const auto& m : current_history_) {
        if (m.role == Role::User && !m.content.empty()) {
            auto t = m.content;
            if (t.size() > 24) t = t.substr(0, 24) + "…";
            return t;
        }
    }
    return {};
}

} // namespace agent
