#include "terminal.hpp"
#include <cstdlib>
#include <iostream>
#include <string>
#include <unistd.h>
namespace g9 {
Terminal::Terminal(ColorMode m){bool tty=isatty(STDOUT_FILENO);bool env=std::getenv("NO_COLOR")==nullptr;auto term=std::getenv("TERM");bool capable=!term||std::string_view(term)!="dumb";color_=m==ColorMode::always||(m==ColorMode::automatic&&tty&&env&&capable);}
std::string Terminal::paint(std::string_view c,std::string_view t)const{return color_?"\x1b["+std::string(c)+"m"+std::string(t)+"\x1b[0m":std::string(t);}
void Terminal::banner(std::string_view model,const std::filesystem::path&w,GuardrailProfile p,const SandboxCapabilities&s){std::cout<<paint("1;36","G9 Local Coding Agent")<<"\n  model      "<<model<<"\n  workspace  "<<w.string()<<"\n  policy     "<<to_string(p)<<"\n  sandbox    "<<s.backend<<(s.filesystem&&s.network?" (fs+network)":" (limited)")<<"\n\n";}
void Terminal::event(const LoopEvent&e){switch(e.type){case LoopEvent::Type::generation_started:break;case LoopEvent::Type::generation_finished:break;case LoopEvent::Type::tool_requested:std::cout<<paint("1;34","┌ tool")<<"  "<<e.summary<<"\n";break;case LoopEvent::Type::approval_required:std::cout<<paint("1;33","├ approval")<<"  "<<e.summary<<"\n";break;case LoopEvent::Type::tool_finished:std::cout<<paint("2","└ result")<<"  "<<e.summary<<"\n";break;case LoopEvent::Type::warning:warning(e.summary);break;}}
ApprovalChoice Terminal::approval(const ActionDescriptor&a){std::cout<<paint("1;33","Approval required")<<"\n  risk    "<<to_string(a.risk)<<"\n  action  "<<a.summary<<"\n";for(auto&p:a.targets)std::cout<<"  target  "<<p.string()<<"\n";if(!a.command_argv.empty()){std::cout<<"  argv    ";for(auto&s:a.command_argv)std::cout<<'['<<s<<"] ";std::cout<<'\n';}std::cout<<"Allow? [y] once / [s] session / [n] deny: "<<std::flush;std::string v;if(!std::getline(std::cin,v))return ApprovalChoice::deny;if(v=="y"||v=="Y")return ApprovalChoice::once;if(v=="s"||v=="S")return ApprovalChoice::session;return ApprovalChoice::deny;}
void Terminal::answer(std::string_view t){std::cout<<"\n"<<paint("1;32","assistant")<<"\n"<<t<<"\n";}void Terminal::info(std::string_view t){std::cout<<t<<"\n";}void Terminal::warning(std::string_view t){std::cerr<<paint("1;33","warning: ")<<t<<"\n";}
}
