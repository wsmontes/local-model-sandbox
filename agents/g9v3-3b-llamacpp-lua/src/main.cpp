#include "application.hpp"
#include "contract.hpp"
#include "error.hpp"
#include "json.hpp"
#include <iostream>
int main(int argc,char**argv){bool json_mode=false;try{auto options=g9::parse_cli(argc,argv);json_mode=options.mode==g9::InvocationMode::json_run;g9::Application app(std::move(options));return app.run();}catch(const g9::Error&e){if(json_mode)std::cout<<g9::serialize_json(g9::error_to_json(e.what(),g9::JsonValue::object{{"exit_code",(double)(int)e.code()}}))<<'\n';else{std::cerr<<"g9-agent: "<<e.what()<<'\n';if(e.code()==g9::ExitCode::invalid_input)std::cerr<<g9::usage_text();}return(int)e.code();}catch(const std::exception&e){if(json_mode)std::cout<<g9::serialize_json(g9::error_to_json(e.what(),g9::JsonValue::object{{"exit_code",10}}))<<'\n';else std::cerr<<"g9-agent: "<<e.what()<<'\n';return 10;}catch(...){if(json_mode)std::cout<<g9::serialize_json(g9::error_to_json("unknown internal failure",g9::JsonValue::object{{"exit_code",10}}))<<'\n';else std::cerr<<"g9-agent: unknown internal failure\n";return 10;}}
