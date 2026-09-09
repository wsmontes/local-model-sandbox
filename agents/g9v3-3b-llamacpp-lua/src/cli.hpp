#pragma once
#include "guardrails.hpp"
#include "process_sandbox.hpp"
#include "workspace.hpp"
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
namespace g9 {
enum class ColorMode{automatic,always,never};enum class InvocationMode{chat,json_run,doctor,tools,version,help};
struct CliOptions{InvocationMode mode=InvocationMode::chat;std::filesystem::path program_path;std::filesystem::path workspace=".";std::optional<std::filesystem::path>agent_root,config,model,history_file,audit_log;std::optional<GuardrailProfile>profile;std::vector<ScopeRule>scopes;std::vector<std::string>denies;std::optional<std::uint32_t>max_steps;std::optional<bool>thinking;ColorMode color=ColorMode::automatic;std::optional<SandboxMode>command_sandbox;bool verbose=false;};
CliOptions parse_cli(int argc,char**argv);std::string usage_text();
}
