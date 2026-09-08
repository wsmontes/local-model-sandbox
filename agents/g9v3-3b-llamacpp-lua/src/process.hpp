#pragma once
#include "guardrails.hpp"
#include <chrono>
#include <filesystem>
#include <functional>
#include <map>
#include <stop_token>
#include <string>
#include <vector>
namespace g9 {
enum class CommandClass { known_safe, ask, denied, destructive };
struct ProcessSpec { std::vector<std::string> argv; std::filesystem::path cwd; std::chrono::milliseconds timeout{120000}; std::size_t max_output_bytes=131072; std::map<std::string,std::string> environment; };
struct ProcessResult { int exit_code=-1; bool timed_out=false,cancelled=false,stdout_truncated=false,stderr_truncated=false; std::string stdout_text,stderr_text; std::chrono::milliseconds duration{0}; };
class CommandPolicy { public: [[nodiscard]] CommandClass classify(const std::vector<std::string>& argv) const; [[nodiscard]] RiskClass risk(const std::vector<std::string>& argv) const; };
using ChildSetup=std::function<void()>;
ProcessResult run_process(const ProcessSpec& spec,ChildSetup child_setup={},std::stop_token stop={});
std::map<std::string,std::string> minimal_environment(std::filesystem::path sandbox_home,std::filesystem::path temp_dir);
} // namespace g9
