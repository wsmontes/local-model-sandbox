#pragma once
#include "lua_agent.hpp"
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <stop_token>
#include <string>
#include <string_view>
namespace g9 {
struct GenerationResult { std::string text; std::int64_t prompt_tokens=0,completion_tokens=0; bool hit_eog=false,cancelled=false; };
class IGenerationRuntime { public: virtual ~IGenerationRuntime()=default; virtual GenerationResult generate(std::string_view,const GenerationConfig&,std::stop_token={})=0; };
class LlamaRuntime final: public IGenerationRuntime { public: LlamaRuntime(std::filesystem::path model_path,const ModelConfig& config,std::function<void(std::string_view)> log_sink); ~LlamaRuntime(); LlamaRuntime(LlamaRuntime&&) noexcept;LlamaRuntime& operator=(LlamaRuntime&&) noexcept;LlamaRuntime(const LlamaRuntime&)=delete;LlamaRuntime& operator=(const LlamaRuntime&)=delete; GenerationResult generate(std::string_view rendered_prompt,const GenerationConfig& config,std::stop_token stop={}) override; std::string model_description()const; std::uint64_t model_parameter_count()const; private: struct Impl; std::unique_ptr<Impl> impl_; };
}
