#pragma once
#include "json.hpp"
#include <cstdint>
#include <string>
namespace g9 {
struct AgentRequest { std::string prompt; JsonValue context{JsonValue::array{}}; JsonValue options{JsonValue::object{}}; };
struct Usage { std::int64_t prompt_tokens = 0; std::int64_t completion_tokens = 0; };
struct AgentResponse { std::string output; std::string model = "ai9stars/G9v3-3B"; Usage usage; JsonValue metadata{JsonValue::object{}}; };
AgentRequest parse_request(const JsonValue& root);
JsonValue response_to_json(const AgentResponse& response);
JsonValue error_to_json(std::string message, JsonValue metadata = JsonValue{JsonValue::object{}});
} // namespace g9
