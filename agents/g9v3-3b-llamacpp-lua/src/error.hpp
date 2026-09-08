#pragma once
#include <stdexcept>
#include <string>
#include <utility>

namespace g9 {
enum class ExitCode : int {
    success = 0,
    invalid_input = 2,
    lua_failure = 3,
    model_failure = 4,
    context_failure = 5,
    inference_failure = 6,
    tool_protocol_failure = 7,
    policy_denied = 8,
    process_failure = 9,
    internal_failure = 10,
};
class Error final : public std::runtime_error {
public:
    Error(ExitCode code, std::string message) : std::runtime_error(std::move(message)), code_(code) {}
    [[nodiscard]] ExitCode code() const noexcept { return code_; }
private:
    ExitCode code_;
};
} // namespace g9
