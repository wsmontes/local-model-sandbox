#include "patch_engine.hpp"
#include "test_support.hpp"
#include <filesystem>
#include <fstream>
#include <unistd.h>
static std::string rd(const std::filesystem::path&p){std::ifstream f(p);return{std::istreambuf_iterator<char>(f),{}};}int main(){using namespace g9;namespace fs=std::filesystem;auto r=fs::temp_directory_path()/("g9-p-"+std::to_string(::getpid()));fs::remove_all(r);fs::create_directories(r);std::ofstream(r/"a")<<"one\ntwo\n";Workspace w(r,Access::read_write,{},{});auto p=parse_patch("*** Begin Patch\n*** Update File: a\n@@\n-one\n+ONE\n*** Add File: b\n+new\n*** End Patch\n");apply_patch(w,validate_patch(w,p));CHECK_EQ(rd(r/"a"),std::string("ONE\ntwo\n"));CHECK_EQ(rd(r/"b"),std::string("new\n"));fs::remove_all(r);return test_support::finish();}
