#pragma once
#include "contract.hpp"
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
struct lua_State;
namespace g9 {
struct ModelConfig { std::filesystem::path path{"models/ai9stars_G9v3-3B-Q4_K_M.gguf"}; std::uint32_t context_size=8192,batch_size=512; int gpu_layers=0,threads=0; };
struct GenerationConfig { bool thinking=false; std::uint32_t max_tokens=768; float temperature=0.7F,top_p=0.95F; int top_k=40; float min_p=0.0F,repeat_penalty=1.0F; int repeat_last_n=64; std::int64_t seed=-1; };
struct LoopConfig { std::uint32_t max_steps=32,max_tool_calls_per_step=8; std::size_t max_tool_result_bytes=65536; };
struct ChatMessage { std::string role; std::string content; };
class IAgentPolicy { public: virtual ~IAgentPolicy()=default; virtual AgentRequest before_prompt(const AgentRequest&)=0; virtual std::vector<ChatMessage> build_messages(const AgentRequest&)=0; virtual std::string render_prompt(const std::vector<ChatMessage>&,const GenerationConfig&,const JsonValue&,const JsonValue&)=0; virtual GenerationConfig generation_settings(const AgentRequest&)=0; virtual AgentResponse after_response(const AgentResponse&)=0; virtual const LoopConfig& loop() const noexcept=0; };
class LuaAgent final: public IAgentPolicy {
public:
 LuaAgent(const std::filesystem::path& config_path,std::function<void(std::string_view)> log_sink); ~LuaAgent(); LuaAgent(LuaAgent&&) noexcept; LuaAgent& operator=(LuaAgent&&) noexcept; LuaAgent(const LuaAgent&)=delete;LuaAgent& operator=(const LuaAgent&)=delete;
 const ModelConfig& model()const noexcept{return model_;} const GenerationConfig& generation()const noexcept{return generation_;} const LoopConfig& loop()const noexcept override{return loop_;}
 AgentRequest before_prompt(const AgentRequest& request) override; std::vector<ChatMessage> build_messages(const AgentRequest& request) override; std::string render_prompt(const std::vector<ChatMessage>& messages,const GenerationConfig& generation,const JsonValue& tool_schemas,const JsonValue& tool_history) override; GenerationConfig generation_settings(const AgentRequest& request) override; AgentResponse after_response(const AgentResponse& response) override;
private: lua_State* L_=nullptr; int config_ref_=-1; std::function<void(std::string_view)> log_sink_; ModelConfig model_;GenerationConfig generation_;LoopConfig loop_; void load_typed_config();
};
}
