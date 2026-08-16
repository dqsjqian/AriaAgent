// AriaAgent — session store implementation.
#include "agent/session_store.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

namespace agent {

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {
int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::string gen_id() {
    static uint64_t counter = 0;
    auto t = std::chrono::high_resolution_clock::now()
                 .time_since_epoch()
                 .count();
    return "s" + std::to_string(t) + "_" + std::to_string(counter++);
}

std::string home_dir() {
    if (const char* h = std::getenv("USERPROFILE"); h && *h) return h;
    if (const char* h = std::getenv("HOME"); h && *h) return h;
    return ".";
}
} // namespace

std::string SessionStore::base_dir() {
    return (fs::path(home_dir()) / ".ariaagent" / "sessions").string();
}

SessionStore::SessionStore() {
    std::error_code ec;
    fs::create_directories(base_dir(), ec);
}

std::string SessionStore::path_for(const std::string& id) const {
    // id is internally generated; still sanitise for safety.
    std::string safe = id;
    for (char& c : safe)
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-')
            c = '_';
    return (fs::path(base_dir()) / (safe + ".json")).string();
}

std::string SessionStore::create(const std::string& title) {
    const std::string id = gen_id();
    const int64_t t = now_ms();
    json doc;
    doc["id"] = id;
    doc["title"] = title.empty() ? "New chat" : title;
    doc["created_at"] = t;
    doc["updated_at"] = t;
    doc["messages"] = json::array();

    std::ofstream f(path_for(id), std::ios::trunc);
    f << doc.dump(2);
    return id;
}

std::vector<SessionMeta> SessionStore::list() const {
    std::vector<SessionMeta> out;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(base_dir(), ec)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".json") continue;
        std::ifstream f(entry.path());
        std::stringstream ss; ss << f.rdbuf();
        try {
            json doc = json::parse(ss.str());
            SessionMeta m;
            m.id = doc.value("id", entry.path().stem().string());
            m.title = doc.value("title", "New chat");
            m.created_at = doc.value("created_at", static_cast<int64_t>(0));
            m.updated_at = doc.value("updated_at", static_cast<int64_t>(0));
            out.push_back(std::move(m));
        } catch (...) { /* skip corrupt file */ }
    }
    std::sort(out.begin(), out.end(),
              [](const SessionMeta& a, const SessionMeta& b) {
                  return a.updated_at > b.updated_at;
              });
    return out;
}

MessageList SessionStore::load(const std::string& id) const {
    MessageList out;
    std::ifstream f(path_for(id));
    if (!f) return out;
    std::stringstream ss; ss << f.rdbuf();
    try {
        json doc = json::parse(ss.str());
        if (doc.contains("messages")) {
            for (const auto& jm : doc["messages"])
                out.push_back(ChatMessage::from_storage_json(jm));
        }
    } catch (...) { /* return what we have */ }
    return out;
}

void SessionStore::save(const std::string& id, const MessageList& messages,
                        const std::string& title_hint) {
    const int64_t t = now_ms();
    json doc;
    doc["id"] = id;
    doc["title"] = title_hint.empty() ? "New chat" : title_hint;
    doc["created_at"] = t;
    doc["updated_at"] = t;
    doc["messages"] = json::array();
    for (const auto& m : messages) doc["messages"].push_back(m.to_storage_json());

    std::ofstream f(path_for(id), std::ios::trunc);
    f << doc.dump(2);
}

void SessionStore::remove(const std::string& id) {
    std::error_code ec;
    fs::remove(path_for(id), ec);
}

void SessionStore::clear_all() {
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(base_dir(), ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json")
            fs::remove(entry.path(), ec);
    }
}

} // namespace agent
