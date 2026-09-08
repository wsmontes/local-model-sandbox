#include "process_sandbox.hpp"
#include "error.hpp"
#ifdef __linux__
#include <fcntl.h>
#include <linux/landlock.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif
namespace g9 {
ProcessSandbox ProcessSandbox::detect(SandboxMode m,const Workspace&w,std::filesystem::path a,std::filesystem::path t){
 ProcessSandbox s;s.mode_=m;s.workspace_=w.root();s.agent_root_=std::move(a);s.temp_root_=std::move(t);
 if(m==SandboxMode::off){s.caps_.backend="off";s.caps_.detail="process sandbox explicitly disabled";return s;}
#ifdef __linux__
 int abi=(int)syscall(__NR_landlock_create_ruleset,nullptr,0,LANDLOCK_CREATE_RULESET_VERSION);s.landlock_abi_=abi;
 if(abi>0){s.caps_.filesystem=true;s.caps_.backend="linux-landlock";
#ifdef LANDLOCK_ACCESS_NET_CONNECT_TCP
  s.caps_.network=abi>=4;
#endif
  s.caps_.detail="Landlock ABI "+std::to_string(abi)+(s.caps_.network?" with TCP network restrictions":" without TCP network restrictions");return s;}
#endif
#ifdef __APPLE__
 s.caps_.backend="macos-unavailable-v0.1";s.caps_.detail="strict Seatbelt wrapper is not implemented in v0.1";return s;
#endif
 s.caps_.backend="unavailable";s.caps_.detail="no supported OS sandbox backend";return s;
}
void ProcessSandbox::apply_in_child()const{
 if(mode_==SandboxMode::off)return;
#ifdef __linux__
 if(!caps_.filesystem)throw Error(ExitCode::process_failure,"Landlock unavailable");
 __u64 ro=LANDLOCK_ACCESS_FS_EXECUTE|LANDLOCK_ACCESS_FS_READ_FILE|LANDLOCK_ACCESS_FS_READ_DIR;
 __u64 rw=ro|LANDLOCK_ACCESS_FS_WRITE_FILE|LANDLOCK_ACCESS_FS_REMOVE_DIR|LANDLOCK_ACCESS_FS_REMOVE_FILE|LANDLOCK_ACCESS_FS_MAKE_CHAR|LANDLOCK_ACCESS_FS_MAKE_DIR|LANDLOCK_ACCESS_FS_MAKE_REG|LANDLOCK_ACCESS_FS_MAKE_SOCK|LANDLOCK_ACCESS_FS_MAKE_FIFO|LANDLOCK_ACCESS_FS_MAKE_BLOCK|LANDLOCK_ACCESS_FS_MAKE_SYM;
#ifdef LANDLOCK_ACCESS_FS_REFER
 rw|=LANDLOCK_ACCESS_FS_REFER;
#endif
#ifdef LANDLOCK_ACCESS_FS_TRUNCATE
 rw|=LANDLOCK_ACCESS_FS_TRUNCATE;
#endif
 landlock_ruleset_attr rs{};rs.handled_access_fs=rw;
#ifdef LANDLOCK_ACCESS_NET_CONNECT_TCP
 if(caps_.network)rs.handled_access_net=LANDLOCK_ACCESS_NET_CONNECT_TCP|LANDLOCK_ACCESS_NET_BIND_TCP;
#endif
 int fd=(int)syscall(__NR_landlock_create_ruleset,&rs,sizeof(rs),0);if(fd<0)throw Error(ExitCode::process_failure,"landlock create failed");
 auto add=[&](const std::filesystem::path&p,__u64 rights){if(p.empty()||!std::filesystem::exists(p))return;int parent=open(p.c_str(),O_PATH|O_CLOEXEC);if(parent<0)return;landlock_path_beneath_attr r{};r.allowed_access=rights;r.parent_fd=parent;int rc=(int)syscall(__NR_landlock_add_rule,fd,LANDLOCK_RULE_PATH_BENEATH,&r,0);close(parent);if(rc<0){close(fd);throw Error(ExitCode::process_failure,"landlock rule failed");}};
 for(auto p:{std::filesystem::path("/usr"),std::filesystem::path("/bin"),std::filesystem::path("/lib"),std::filesystem::path("/lib64"),std::filesystem::path("/etc"),agent_root_})add(p,ro);
 add(workspace_,rw);add(temp_root_,rw);
 if(prctl(PR_SET_NO_NEW_PRIVS,1,0,0,0)!=0||syscall(__NR_landlock_restrict_self,fd,0)<0){close(fd);throw Error(ExitCode::process_failure,"landlock restrict failed");}close(fd);return;
#else
 throw Error(ExitCode::process_failure,"sandbox backend unavailable");
#endif
}
}
