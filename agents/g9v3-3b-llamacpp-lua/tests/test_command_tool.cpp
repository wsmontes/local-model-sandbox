#include "command_tool.hpp"
#include "test_support.hpp"
#include <filesystem>
#include <fstream>
#include <unistd.h>
int main(){using namespace g9;namespace fs=std::filesystem;auto base=fs::temp_directory_path()/("g9-command-tool-"+std::to_string(::getpid()));fs::remove_all(base);fs::create_directories(base/"ws");fs::create_directories(base/"outside");std::ofstream(base/"outside/tool")<<"x";Workspace w(base/"ws",Access::read_write,{},{});auto sb=ProcessSandbox::detect(SandboxMode::off,w,base/"agent",base/"tmp");CommandTool tool(w,std::move(sb),base/"agent",base/"tmp");JsonValue args=JsonValue::object{{"command",JsonValue::array{JsonValue("../outside/tool")}}};CHECK_THROWS(tool.preflight(args));fs::remove_all(base);return test_support::finish();}
