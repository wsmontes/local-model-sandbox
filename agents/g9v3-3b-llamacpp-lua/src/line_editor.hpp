#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
namespace g9 {
enum class ReadStatus { line, eof, interrupted };struct ReadLineResult { ReadStatus status=ReadStatus::eof; std::string text; };
class LineEditor {public:void set_completions(std::vector<std::string> values){completions_=std::move(values);}void set_history_file(std::optional<std::filesystem::path> path);ReadLineResult read(std::string_view prompt);void add_history(std::string_view line);private:std::vector<std::string>completions_,history_;std::optional<std::filesystem::path>history_file_;std::size_t history_limit_=200;void load_history();void save_history()const;};
}
