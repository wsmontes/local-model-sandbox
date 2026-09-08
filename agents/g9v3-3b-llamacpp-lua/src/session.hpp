#pragma once
#include <optional>
#include <string>
#include <vector>
namespace g9 { enum class MessageKind{system,user,assistant,tool}; struct ConversationItem{MessageKind kind;std::string content;std::optional<std::string> tool_name;}; class Session{public:void add(ConversationItem i){items_.push_back(std::move(i));}void clear(){items_.clear();}const std::vector<ConversationItem>&items()const noexcept{return items_;}private:std::vector<ConversationItem>items_;}; }
