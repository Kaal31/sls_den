#include "sls/config_fix.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <cstdlib>
namespace fs=std::filesystem;
using playden::sls::ApplyConfigFix;
using playden::sls::RestoreConfigFix;
using playden::sls::FixResult;
static void check(bool ok,const char* message){
  if(!ok){std::cerr<<"FAIL: "<<message<<"\n";std::exit(1);}
}
static std::string read(const fs::path& p){
  std::ifstream f(p,std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>());
}
int main(){
 const auto root=fs::temp_directory_path()/"lua-den-config-fix-test";
 std::error_code ec;
 fs::remove_all(root,ec);
 fs::create_directories(root/"drive_c"/"game",ec);
 check(!ec,"fixture directory created");
 const auto file=root/"drive_c"/"game"/"settings.ini";
 {std::ofstream o(file,std::ios::binary);o<<"original=1\n";}
 check(ApplyConfigFix(root,"drive_c/game/settings.ini","original=2\n")==FixResult::Applied,"apply replacement");
 check(read(file)=="original=2\n","replacement written");
 check(ApplyConfigFix(root,"drive_c/game/settings.ini","bad")==FixResult::AlreadyApplied,"no double backup");
 check(RestoreConfigFix(root,"drive_c/game/settings.ini")==FixResult::Restored,"unfix succeeds");
 check(read(file)=="original=1\n","original restored byte-for-byte");
 check(ApplyConfigFix(root,"drive_c/game/new.ini","created=true\n")==FixResult::Applied,"create file");
 check(RestoreConfigFix(root,"drive_c/game/new.ini")==FixResult::Restored && !fs::exists(root/"drive_c"/"game"/"new.ini"),"remove created file");
 check(ApplyConfigFix(root,"drive_c/game/settings.ini","patched=1\n")==FixResult::Applied,"apply conflict fixture");
 {std::ofstream o(file,std::ios::binary|std::ios::trunc);o<<"user-change=1\n";}
 check(RestoreConfigFix(root,"drive_c/game/settings.ini")==FixResult::Conflict,"preserve user modifications");
 check(read(file)=="user-change=1\n","modified content retained");
 {std::ofstream o(file,std::ios::binary|std::ios::trunc);o<<"patched=1\n";}
 check(RestoreConfigFix(root,"drive_c/game/settings.ini")==FixResult::Restored,"restore after resolving conflict");
 check(ApplyConfigFix(root,"../escape.ini","bad")==FixResult::UnsafePath,"reject traversal");
 check(ApplyConfigFix(root,fs::absolute(root/"escape.ini"),"bad")==FixResult::UnsafePath,"reject absolute path");
 check(ApplyConfigFix(root,"drive_c/game/settings.ini",std::string(1024*1024+1,'x'))==FixResult::TooLarge,"reject oversized payload");
 fs::remove_all(root,ec);
 std::cout<<"PASS: reversible config fixes and path safety\n";
}
