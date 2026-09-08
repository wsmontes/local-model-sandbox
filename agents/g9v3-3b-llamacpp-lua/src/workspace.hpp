#pragma once
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
namespace g9 {
enum class Access { none, read, read_write };
enum class Operation { read, write, remove, execute_cwd };
struct ScopeRule { std::filesystem::path relative_path; Access access; };
struct PathDecision { bool allowed=false; std::filesystem::path canonical_path; Access effective_access=Access::none; std::string reason; };
class Workspace {
public:
    Workspace(std::filesystem::path root, Access default_access, std::vector<ScopeRule> scopes, std::vector<std::string> deny_globs, bool follow_directory_symlinks=false);
    [[nodiscard]] const std::filesystem::path& root() const noexcept { return root_; }
    [[nodiscard]] PathDecision resolve(std::filesystem::path relative, Operation operation, bool target_may_not_exist=false) const;
    [[nodiscard]] bool is_denied(std::filesystem::path relative) const;
    [[nodiscard]] const std::vector<ScopeRule>& scopes() const noexcept { return scopes_; }
private:
    std::filesystem::path root_; Access default_access_; std::vector<ScopeRule> scopes_; std::vector<std::string> deny_globs_; bool follow_directory_symlinks_;
};
bool glob_match(std::string_view pattern, std::string_view path);
} // namespace g9
