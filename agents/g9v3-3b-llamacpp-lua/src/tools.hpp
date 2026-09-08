#pragma once
#include "patch_engine.hpp"
#include "tool_registry.hpp"
#include "workspace.hpp"
#include <map>
#include <optional>
namespace g9 {
class CodingTools {
public:
 explicit CodingTools(Workspace& workspace):workspace_(workspace){}
 ActionDescriptor preflight(std::string_view tool,const JsonValue& args) const;
 ToolResult execute(std::string_view tool,const JsonValue& args);
 [[nodiscard]] JsonValue status() const;
private:
 Workspace& workspace_; mutable std::map<std::filesystem::path,std::optional<std::string>> baseline_;
 void remember(const std::filesystem::path&) const;
 ToolResult list_files(const JsonValue&); ToolResult find_files(const JsonValue&); ToolResult search_text(const JsonValue&); ToolResult read_file(const JsonValue&); ToolResult write_file(const JsonValue&); ToolResult replace_text(const JsonValue&); ToolResult apply_patch_tool(const JsonValue&); ToolResult delete_file(const JsonValue&); ToolResult workspace_status(const JsonValue&);
};
void register_file_tools(ToolRegistry& registry,CodingTools& tools);
} // namespace g9
