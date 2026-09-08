#pragma once
#include "guardrails.hpp"
#include "json.hpp"
#include <functional>
#include <optional>
#include <string>
#include <vector>
namespace g9 {
struct ToolDefinition { std::string name; std::string description; JsonValue input_schema; RiskClass base_risk=RiskClass::read; };
struct ToolCall { std::string name; JsonValue arguments{JsonValue::object{}}; };
enum class ToolDisposition { completed, approval_required, denied, failed };
struct ToolResult { ToolDisposition disposition=ToolDisposition::failed; bool ok=false; JsonValue payload{JsonValue::object{}}; std::string display_summary; std::optional<ActionDescriptor> approval; };
using ToolPreflight=std::function<ActionDescriptor(const JsonValue&)>; using ToolExecutor=std::function<ToolResult(const JsonValue&)>;
class ToolRegistry {
public:
 explicit ToolRegistry(Guardrails& guardrails):guardrails_(guardrails){}
 void add(ToolDefinition definition,ToolPreflight preflight,ToolExecutor executor);
 [[nodiscard]] std::vector<ToolDefinition> definitions() const;
 [[nodiscard]] const ToolDefinition* definition(std::string_view name) const;
 [[nodiscard]] ToolResult execute(const ToolCall& call,bool approval_granted=false) const;
private:
 struct Entry{ToolDefinition definition;ToolPreflight preflight;ToolExecutor executor;}; std::vector<Entry> entries_; Guardrails& guardrails_;
 bool validate(const ToolDefinition&,const JsonValue&,std::string&) const;
};
std::string to_string(ToolDisposition d);
} // namespace g9
