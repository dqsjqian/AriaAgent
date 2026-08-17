// AriaAgent — canonical workspace path resolution shared by local tools.
#include "agent/workspace.hpp"

namespace agent {
namespace {

bool is_descendant(const std::filesystem::path& root,
                   const std::filesystem::path& candidate) {
    const auto relative = candidate.lexically_relative(root);
    if (relative.empty() || relative == ".") return true;
    if (relative.is_absolute()) return false;
    const auto first = relative.begin();
    return first != relative.end() && *first != "..";
}

} // namespace

std::optional<std::filesystem::path> resolve_workspace_path(
    const ToolContext& ctx, const std::string& input, bool require_existing,
    std::string* error) {
    if (ctx.workspace_root.empty()) {
        if (error) *error = "workspace root is not configured";
        return std::nullopt;
    }
    if (input.empty()) {
        if (error) *error = "path is required";
        return std::nullopt;
    }

    std::error_code ec;
    const auto root = std::filesystem::weakly_canonical(ctx.workspace_root, ec);
    if (ec || !std::filesystem::is_directory(root, ec) || ec) {
        if (error) *error = "workspace root does not exist";
        return std::nullopt;
    }

    std::filesystem::path requested(input);
    if (requested.is_relative()) requested = root / requested;

    std::filesystem::path resolved;
    if (require_existing || std::filesystem::exists(requested, ec)) {
        if (!ec) resolved = std::filesystem::canonical(requested, ec);
    } else {
        ec.clear();
        const auto parent = requested.has_parent_path() ? requested.parent_path() : root;
        const auto canonical_parent = std::filesystem::canonical(parent, ec);
        if (!ec) resolved = canonical_parent / requested.filename();
    }
    if (ec) {
        if (error) *error = "path does not exist or cannot be resolved: " + input;
        return std::nullopt;
    }
    resolved = resolved.lexically_normal();
    if (ctx.workspace_access != 2 && !is_descendant(root, resolved)) {
        if (error) *error = "path outside workspace root requires Full Access: " + input;
        return std::nullopt;
    }
    return resolved;
}

} // namespace agent
