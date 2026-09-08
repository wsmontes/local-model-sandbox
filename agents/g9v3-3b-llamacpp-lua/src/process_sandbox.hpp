#pragma once
#include "workspace.hpp"
#include <filesystem>
#include <string>
namespace g9 {
enum class SandboxMode { strict, best_effort, off };
struct SandboxCapabilities { bool filesystem=false; bool network=false; std::string backend; std::string detail; };
class ProcessSandbox {
public:
 static ProcessSandbox detect(SandboxMode mode,const Workspace& workspace,std::filesystem::path agent_root,std::filesystem::path temp_root);
 [[nodiscard]] const SandboxCapabilities& capabilities()const noexcept{return caps_;}
 [[nodiscard]] SandboxMode mode()const noexcept{return mode_;}
 [[nodiscard]] bool strict_ready()const noexcept{return mode_!=SandboxMode::strict||(caps_.filesystem&&caps_.network);}
 void apply_in_child() const;
private: SandboxMode mode_=SandboxMode::off;SandboxCapabilities caps_;std::filesystem::path workspace_,agent_root_,temp_root_;int landlock_abi_=0;
};
}
