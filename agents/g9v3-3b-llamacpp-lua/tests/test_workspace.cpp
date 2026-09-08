#include "workspace.hpp"
#include "test_support.hpp"
#include <filesystem>
#include <fstream>
#include <unistd.h>
int main(){using namespace g9;namespace fs=std::filesystem;auto r=fs::temp_directory_path()/("g9-ws-"+std::to_string(::getpid()));fs::remove_all(r);fs::create_directories(r/"docs");fs::create_directories(r/"src");std::ofstream(r/"docs/a")<<"x";std::ofstream(r/".env")<<"x";Workspace w(r,Access::read_write,{{"docs",Access::read},{".git",Access::none}},{".env","**/*.pem"});CHECK(w.resolve(".",Operation::execute_cwd).allowed);CHECK(w.resolve("docs/a",Operation::read).allowed);CHECK(!w.resolve("docs/a",Operation::write).allowed);CHECK(!w.resolve(".env",Operation::read).allowed);CHECK(!w.resolve("../x",Operation::read,true).allowed);CHECK(glob_match("**/*.pem","a/b.pem"));fs::remove_all(r);return test_support::finish();}
