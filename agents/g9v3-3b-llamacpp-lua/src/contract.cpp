#include "contract.hpp"
#include "error.hpp"
namespace g9 {
namespace { [[noreturn]] void invalid(const std::string& m) { throw Error(ExitCode::invalid_input, m); } }
AgentRequest parse_request(const JsonValue& root) {
    if (!root.is_object()) invalid("request must be a JSON object");
    const auto& obj = root.as_object(); const auto* prompt = object_get(obj, "prompt");
    if (!prompt || !prompt->is_string() || prompt->as_string().empty()) invalid("request.prompt must be a non-empty string");
    AgentRequest request; request.prompt = prompt->as_string();
    if (const auto* context = object_get(obj, "context")) { if (!context->is_array()) invalid("request.context must be an array"); request.context = *context; }
    if (const auto* options = object_get(obj, "options")) { if (!options->is_object()) invalid("request.options must be an object"); request.options = *options; }
    return request;
}
JsonValue response_to_json(const AgentResponse& r) {
    JsonValue::object usage{{"prompt_tokens", JsonValue(static_cast<double>(r.usage.prompt_tokens))},{"completion_tokens", JsonValue(static_cast<double>(r.usage.completion_tokens))}};
    return JsonValue::object{{"output",r.output},{"model",r.model},{"usage",JsonValue(std::move(usage))},{"metadata",r.metadata}};
}
JsonValue error_to_json(std::string message, JsonValue metadata) { return JsonValue::object{{"error",JsonValue(std::move(message))},{"metadata",std::move(metadata)}}; }
} // namespace g9
