#pragma once
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>
namespace g9 {
enum class RiskClass { read, write, execute, execute_approval, unsandboxed_execute, filesystem_destructive, vcs_read, vcs_write, vcs_destructive, forbidden };
enum class PolicyAction { allow, ask, deny };
enum class GuardrailProfile { review, balanced, autonomous_mode };
struct ActionDescriptor { std::string tool; RiskClass risk=RiskClass::read; std::vector<std::filesystem::path> targets; std::vector<std::string> command_argv; std::string equivalence_key; std::string summary; };
struct GuardrailDecision { PolicyAction action=PolicyAction::deny; std::string reason; };
class Guardrails {
public:
 explicit Guardrails(GuardrailProfile profile):profile_(profile){}
 [[nodiscard]] GuardrailDecision decide(const ActionDescriptor& action) const;
 void allow_for_session(std::string key){session_.insert(std::move(key));}
 [[nodiscard]] bool session_allows(std::string_view key) const{return session_.contains(std::string(key));}
 [[nodiscard]] GuardrailProfile profile() const noexcept{return profile_;}
private: GuardrailProfile profile_; std::unordered_set<std::string> session_;
};
std::string to_string(RiskClass risk); std::string to_string(GuardrailProfile profile);
}
