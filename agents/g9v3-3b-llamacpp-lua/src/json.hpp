#pragma once
#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace g9 {
struct JsonValue {
    using array = std::vector<JsonValue>;
    using object = std::map<std::string, JsonValue, std::less<>>;
    using storage = std::variant<std::nullptr_t, bool, double, std::string, array, object>;
    storage value{nullptr};
    JsonValue() = default;
    JsonValue(std::nullptr_t) : value(nullptr) {}
    JsonValue(bool v) : value(v) {}
    JsonValue(double v) : value(v) {}
    JsonValue(int v) : value(static_cast<double>(v)) {}
    JsonValue(std::string v) : value(std::move(v)) {}
    JsonValue(const char* v) : value(std::string(v)) {}
    JsonValue(array v) : value(std::move(v)) {}
    JsonValue(object v) : value(std::move(v)) {}
    [[nodiscard]] bool is_null() const noexcept;
    [[nodiscard]] bool is_bool() const noexcept;
    [[nodiscard]] bool is_number() const noexcept;
    [[nodiscard]] bool is_string() const noexcept;
    [[nodiscard]] bool is_array() const noexcept;
    [[nodiscard]] bool is_object() const noexcept;
    [[nodiscard]] bool as_bool() const;
    [[nodiscard]] double as_number() const;
    [[nodiscard]] const std::string& as_string() const;
    [[nodiscard]] const array& as_array() const;
    [[nodiscard]] const object& as_object() const;
    [[nodiscard]] array& as_array();
    [[nodiscard]] object& as_object();
};
JsonValue parse_json(std::string_view text);
std::string serialize_json(const JsonValue& value);
const JsonValue* object_get(const JsonValue::object& object, std::string_view key);
std::string json_type_name(const JsonValue& value);
} // namespace g9
