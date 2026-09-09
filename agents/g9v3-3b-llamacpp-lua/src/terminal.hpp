#pragma once
#include "agent_loop.hpp"
#include "cli.hpp"
#include "guardrails.hpp"
#include <filesystem>
#include <string_view>
namespace g9 {
enum class ApprovalChoice{deny,once,session};
class Terminal{public:explicit Terminal(ColorMode mode);bool color_enabled()const noexcept{return color_;}void banner(std::string_view model,const std::filesystem::path&workspace,GuardrailProfile profile,const SandboxCapabilities&sandbox,bool verbose);void event(const LoopEvent&e);ApprovalChoice approval(const ActionDescriptor&a);void answer(std::string_view text);void info(std::string_view text);void warning(std::string_view text);private:bool color_=false;std::string paint(std::string_view code,std::string_view text)const;};
}
