#include "command_tool.hpp"
#include "test_support.hpp"
#include "tools.hpp"
#include <filesystem>
#include <fstream>
#include <unistd.h>

using namespace g9;
namespace fs = std::filesystem;

static std::string read_all(const fs::path& p){std::ifstream f(p);return{std::istreambuf_iterator<char>(f),{}};}
static std::string field_str(const JsonValue& v,std::string_view k){if(auto* x=object_get(v.as_object(),k))return x->as_string();return{};}

int main(){
    auto base=fs::temp_directory_path()/("g9-exec-"+std::to_string(::getpid()));
    fs::remove_all(base);
    fs::create_directories(base/"ws"/"src");
    fs::create_directories(base/"agent");
    fs::create_directories(base/"tmp");
    std::ofstream(base/"ws"/"src"/"a.cpp")<<"alpha\nbeta\nalpha\n";
    std::ofstream(base/"ws"/"README.md")<<"hello\n";
    std::ofstream(base/"sibling.txt")<<"outside\n";

    Workspace w(base/"ws",Access::read_write,{},{});
    Guardrails g(GuardrailProfile::balanced);
    ToolRegistry reg(g);
    CodingTools ft(w);
    register_file_tools(reg,ft);
    auto sb=ProcessSandbox::detect(SandboxMode::off,w,base/"agent",base/"tmp");
    CommandTool ct(w,std::move(sb),base/"agent",base/"tmp");
    register_command_tool(reg,ct);

    // list_files
    {auto r=reg.execute({"list_files",JsonValue::object{{"path","."},{"depth",2}}});CHECK(r.ok);CHECK_EQ(r.disposition,ToolDisposition::completed);auto* items=object_get(r.payload.as_object(),"items");CHECK(items&&items->is_array());bool a=false,m=false;if(items&&items->is_array())for(auto&it:items->as_array()){auto p=field_str(it,"path");a=a||p=="src/a.cpp";m=m||p=="README.md";}CHECK(a);CHECK(m);}

    // find_files
    {auto r=reg.execute({"find_files",JsonValue::object{{"pattern","**/*.cpp"}}});CHECK(r.ok);auto* mm=object_get(r.payload.as_object(),"matches");CHECK(mm&&mm->is_array()&&mm->as_array().size()==1);if(mm&&mm->is_array()&&!mm->as_array().empty())CHECK_EQ(mm->as_array()[0].as_string(),std::string("src/a.cpp"));}

    // search_text
    {auto r=reg.execute({"search_text",JsonValue::object{{"query","beta"}}});CHECK(r.ok);auto* res=object_get(r.payload.as_object(),"results");CHECK(res&&res->is_array()&&res->as_array().size()==1);if(res&&res->is_array()&&!res->as_array().empty())CHECK_EQ(field_str(res->as_array()[0],"path"),std::string("src/a.cpp"));}

    // read_file
    {auto r=reg.execute({"read_file",JsonValue::object{{"path","src/a.cpp"}}});CHECK(r.ok);CHECK(field_str(r.payload,"content").find("alpha")!=std::string::npos);}

    // write_file (create)
    {auto r=reg.execute({"write_file",JsonValue::object{{"path","new.txt"},{"content","hello"}}});CHECK(r.ok);CHECK_EQ(r.disposition,ToolDisposition::completed);CHECK(fs::exists(base/"ws"/"new.txt"));CHECK_EQ(read_all(base/"ws"/"new.txt"),std::string("hello"));}

    // write_file overwrite -> approval required; then approved
    {auto r=reg.execute({"write_file",JsonValue::object{{"path","new.txt"},{"content","bye"},{"overwrite",true}}});CHECK(!r.ok);CHECK_EQ(r.disposition,ToolDisposition::approval_required);auto r2=reg.execute({"write_file",JsonValue::object{{"path","new.txt"},{"content","bye"},{"overwrite",true}}},true);CHECK(r2.ok);CHECK_EQ(read_all(base/"ws"/"new.txt"),std::string("bye"));}

    // replace_text
    {auto r=reg.execute({"replace_text",JsonValue::object{{"path","src/a.cpp"},{"old_text","alpha"},{"new_text","A"},{"expected_occurrences",2}}});CHECK(r.ok);CHECK_EQ(read_all(base/"ws"/"src"/"a.cpp"),std::string("A\nbeta\nA\n"));}

    // apply_patch
    {auto r=reg.execute({"apply_patch",JsonValue::object{{"patch","*** Begin Patch\n*** Add File: p.txt\n+patched\n*** End Patch\n"}}});CHECK(r.ok);CHECK_EQ(read_all(base/"ws"/"p.txt"),std::string("patched\n"));}

    // delete_file -> approval required; then approved
    {auto r=reg.execute({"delete_file",JsonValue::object{{"path","p.txt"}}});CHECK(!r.ok);CHECK_EQ(r.disposition,ToolDisposition::approval_required);auto r2=reg.execute({"delete_file",JsonValue::object{{"path","p.txt"}}},true);CHECK(r2.ok);CHECK(!fs::exists(base/"ws"/"p.txt"));}

    // workspace_status reflects the session changes
    {auto r=reg.execute({"workspace_status",JsonValue::object{}});CHECK(r.ok);auto* ch=object_get(r.payload.as_object(),"changes");CHECK(ch&&ch->is_array()&&!ch->as_array().empty());}

    // run_command -> approval required (unsandboxed in sandbox=off); then approved
    {auto args=JsonValue::object{{"command",JsonValue::array{JsonValue("echo"),JsonValue("hi")}}};auto r=reg.execute({"run_command",args});CHECK(!r.ok);CHECK_EQ(r.disposition,ToolDisposition::approval_required);auto r2=reg.execute({"run_command",args},true);CHECK(r2.ok);CHECK_EQ(r2.disposition,ToolDisposition::completed);auto* ec=object_get(r2.payload.as_object(),"exit_code");CHECK(ec&&ec->is_number()&&ec->as_number()==0);CHECK(field_str(r2.payload,"stdout").find("hi")!=std::string::npos);}

    // workspace boundary: reads/writes outside the root are rejected
    {auto r=reg.execute({"read_file",JsonValue::object{{"path","../sibling.txt"}}});CHECK(!r.ok);auto w2=reg.execute({"write_file",JsonValue::object{{"path","../sibling.txt"},{"content","x"}}});CHECK(!w2.ok);}

    // guardrail policy matrix (balanced)
    {ActionDescriptor d;d.risk=RiskClass::forbidden;CHECK_EQ(g.decide(d).action,PolicyAction::deny);d.risk=RiskClass::vcs_destructive;CHECK_EQ(g.decide(d).action,PolicyAction::deny);d.risk=RiskClass::filesystem_destructive;CHECK_EQ(g.decide(d).action,PolicyAction::ask);d.risk=RiskClass::read;CHECK_EQ(g.decide(d).action,PolicyAction::allow);d.risk=RiskClass::write;CHECK_EQ(g.decide(d).action,PolicyAction::allow);d.risk=RiskClass::execute_approval;CHECK_EQ(g.decide(d).action,PolicyAction::ask);}

    fs::remove_all(base);
    return test_support::finish();
}
