#pragma once
#include "process.hpp"
#include "process_sandbox.hpp"
#include "tool_registry.hpp"
#include "workspace.hpp"
#include <filesystem>
namespace g9 {
class CommandTool {
public:
 CommandTool(Workspace& workspace,ProcessSandbox sandbox,std::filesystem::path agent_root,std::filesystem::path temp_root):workspace_(workspace),sandbox_(std::move(sandbox)),agent_root_(std::move(agent_root)),temp_root_(std::move(temp_root)){}
 ActionDescriptor preflight(const JsonValue& args) const; ToolResult execute(const JsonValue& args) const; const ProcessSandbox& sandbox()const noexcept{return sandbox_;}
private:Workspace&workspace_;ProcessSandbox sandbox_;std::filesystem::path agent_root_,temp_root_;CommandPolicy policy_;
};
void register_command_tool(ToolRegistry& registry,CommandTool& tool);
}
