#pragma once
#include "cli.hpp"
#include <memory>
namespace g9 {class Application{public:explicit Application(CliOptions options):options_(std::move(options)){}int run();private:struct BaseState;CliOptions options_;std::unique_ptr<BaseState>build_base();int run_chat();int run_json_contract();int run_doctor();int run_tools();int run_version();int run_help();};}
