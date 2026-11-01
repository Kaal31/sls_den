#include "sls/config_fix.h"
#include <fstream>
#include <iterator>
#include <system_error>
namespace playden::sls {
namespace {
namespace fs=std::filesystem;
constexpr std::uintmax_t kMaxBytes=1024*1024;
bool SafePath(const fs::path& root,const fs::path& relative,fs::path* target) {
  if(root.empty()||relative.empty()||relative.is_absolute()||relative.has_root_name())return false;
  for(const auto& piece:relative)if(piece==".."||piece=="."||piece.empty())return false;
  std::error_code ec;
  const auto base=fs::weakly_canonical(root,ec);
  if(ec||!fs::is_directory(base,ec)||ec)return false;
  const auto candidate=(base/relative).lexically_normal();
  const auto parent=fs::weakly_canonical(candidate.parent_path(),ec);
  if(ec)return false;
  auto a=parent.begin(),b=base.begin();
  for(;b!=base.end();++a,++b)if(a==parent.end()||*a!=*b)return false;
  ec.clear();
  if(fs::is_symlink(candidate,ec))return false;
  // Missing targets are valid for file-creation fixes.
  if(ec && ec!=std::errc::no_such_file_or_directory)return false;
  *target=candidate;
  return true;
}
}
FixResult ApplyConfigFix(const fs::path& root,const fs::path& relative,std::string_view text){
  if(text.size()>kMaxBytes)return FixResult::TooLarge;
  fs::path target;
  if(!SafePath(root,relative,&target))return FixResult::UnsafePath;
  std::error_code ec;
  const bool target_exists=fs::exists(target,ec);
  if(ec)return FixResult::IoError;
  if(target_exists && fs::is_directory(target,ec))return FixResult::UnsafePath;
  if(ec)return FixResult::IoError;
  const auto backup=fs::path(target.string()+".lua-den-original");
  const auto created=fs::path(target.string()+".lua-den-created");
  if(fs::exists(backup,ec)||fs::exists(created,ec))return FixResult::AlreadyApplied;
  if(ec)return FixResult::IoError;
  const bool existed=fs::exists(target,ec);
  if(ec)return FixResult::IoError;
  if(existed) {
    if(!fs::is_regular_file(target,ec)||ec||fs::file_size(target,ec)>kMaxBytes||ec)return FixResult::UnsafePath;
    fs::copy_file(target,backup,fs::copy_options::none,ec);
    if(ec)return FixResult::IoError;
  } else {
    fs::create_directories(target.parent_path(),ec);
    if(ec||!SafePath(root,relative,&target))return FixResult::UnsafePath;
    std::ofstream marker(created,std::ios::binary);
    if(!marker)return FixResult::IoError;
  }
  const auto temp=fs::path(target.string()+".lua-den-temp");
  {std::ofstream out(temp,std::ios::binary|std::ios::trunc);
   if(!out)return FixResult::IoError;
   out.write(text.data(),static_cast<std::streamsize>(text.size()));
   out.flush();if(!out)return FixResult::IoError;}
  // Existing file remains protected in the backup until restore.
  fs::rename(temp,target,ec);
  if(ec) {
    fs::remove(target,ec);ec.clear();fs::rename(temp,target,ec);
    if(ec)return FixResult::IoError;
  }
  return FixResult::Applied;
}
FixResult RestoreConfigFix(const fs::path& root,const fs::path& relative){
  fs::path target;
  if(!SafePath(root,relative,&target))return FixResult::UnsafePath;
  const auto backup=fs::path(target.string()+".lua-den-original");
  const auto created=fs::path(target.string()+".lua-den-created");
  std::error_code ec;
  if(fs::exists(backup,ec)) {
    if(fs::is_symlink(backup,ec)||ec||!fs::is_regular_file(backup,ec)||ec)return FixResult::UnsafePath;
    fs::remove(target,ec);
    if(ec)return FixResult::IoError;
    fs::rename(backup,target,ec);
    return ec?FixResult::IoError:FixResult::Restored;
  }
  if(fs::exists(created,ec)) {
    fs::remove(target,ec);if(ec)return FixResult::IoError;
    fs::remove(created,ec);
    return ec?FixResult::IoError:FixResult::Restored;
  }
  return FixResult::NotApplied;
}
}
