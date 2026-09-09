#pragma once
#include "tool_registry.hpp"
#include <string>
#include <string_view>
#include <vector>
namespace g9 {
struct ParsedToolCall { std::string name; JsonValue arguments{JsonValue::object{}}; std::string raw_text; };
struct ParsedAssistantTurn { std::string visible_text; std::vector<ParsedToolCall> tool_calls; bool malformed_tool_markup=false; std::string parse_error; };
JsonValue tool_definitions_to_prompt_json(const std::vector<ToolDefinition>& tools);
ParsedAssistantTurn parse_g9_assistant_turn(std::string_view text,const ToolRegistry& registry);
std::string make_g9_tool_response(std::string_view tool_name,const ToolResult& result);
std::string make_g9_error_response(std::string_view error);
std::string tool_format_hint();
}
