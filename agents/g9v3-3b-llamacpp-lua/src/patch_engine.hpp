#pragma once
#include "workspace.hpp"
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
namespace g9 {
enum class PatchKind { add, update, remove };
struct PatchHunk { std::string before; std::string after; };
struct PatchOperation { PatchKind kind; std::filesystem::path path; std::string content; std::vector<PatchHunk> hunks; };
struct PatchPlan { std::vector<PatchOperation> operations; };
struct PatchFileStat { std::filesystem::path path; std::size_t added_lines=0, removed_lines=0; PatchKind kind; };
struct PatchResult { std::vector<PatchFileStat> files; bool rollback_attempted=false; bool rollback_complete=true; };
PatchPlan parse_patch(std::string_view text); PatchPlan validate_patch(const Workspace&,const PatchPlan&); PatchResult apply_patch(const Workspace&,const PatchPlan&);
} // namespace g9
