#pragma once
#include "contract.hpp"
#include "llama_runtime.hpp"
#include "lua_agent.hpp"
#include "session.hpp"
#include "tool_registry.hpp"
#include <functional>
#include <stop_token>
namespace g9 { struct LoopEvent{enum class Type{generation_started,generation_finished,tool_requested,approval_required,tool_finished,warning}type;std::string summary;JsonValue detail{JsonValue::object{}};};struct TaskResult{std::string output;Usage usage;std::uint32_t tool_steps=0;std::size_t files_changed=0;};using ApprovalCallback=std::function<PolicyAction(const ActionDescriptor&)>;using EventCallback=std::function<void(const LoopEvent&)>;class AgentLoop{public:AgentLoop(IAgentPolicy&policy,IGenerationRuntime&runtime,ToolRegistry&tools,Guardrails&guardrails,ApprovalCallback approval,EventCallback events):policy_(policy),runtime_(runtime),tools_(tools),guardrails_(guardrails),approval_(std::move(approval)),events_(std::move(events)){}TaskResult run(Session&session,const AgentRequest&request,std::stop_token stop={});private:IAgentPolicy&policy_;IGenerationRuntime&runtime_;ToolRegistry&tools_;Guardrails&guardrails_;ApprovalCallback approval_;EventCallback events_;}; }
