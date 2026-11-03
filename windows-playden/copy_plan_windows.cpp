// Windows host read-only local import planner.
// Keeps PS5 descriptor-walk and durable copy journals in the PS5 implementation.
// No file installation is authorized until a Windows copy executor is added.
#include "stores/local/copy_plan.h"
#include <algorithm>
#include <cerrno>
#include <filesystem>
#include <limits>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace playden::stores::local {
namespace fs=std::filesystem;
namespace {
bool Reserved(const std::string& name) {
  return name==kCopyJournalDir || name==".playden-files" ||
    name==".playden-files.tmp" || name==".playden-install.json" ||
    name==".playden-install.json.tmp" || name==".playden-download";
}
CopyPlanBuild Error(const std::string& message,int code=0) {
  CopyPlanBuild result;
  result.failure=message;
  result.error=code;
  return result;
}
bool IsSafeRoot(const fs::path& root) {
  if(!root.is_absolute())return false;
  std::error_code ec;
  fs::path current;
  for(const auto& part:root) {
    current/=part;
    const auto state=fs::symlink_status(current,ec);
    if(ec || fs::is_symlink(state))return false;
  }
  return true;
}
}
std::string CopyPlanPath(const std::string& root) {
  return (fs::u8path(root)/kCopyJournalDir/kCopyPlanFile).u8string();
}
bool PlanHasFile(const CopyPlan& plan,const std::string& path) {
  return std::any_of(plan.files.begin(),plan.files.end(),
    [&](const CopyFile& file){return file.path==path;});
}
CopyPlanBuild BuildCopyPlan(const std::string& source,std::size_t max_files,
                            std::uint64_t max_bytes) {
  const fs::path root=fs::u8path(source);
  if(!IsSafeRoot(root))
    return Error("Windows copy source must be an existing ordinary absolute directory.",EINVAL);
  std::error_code ec;
  if(!fs::is_directory(root,ec)||ec)
    return Error("The source game folder is not available.",ec.value());
  CopyPlanBuild result;
  result.plan.source=source;
  struct Frame {fs::path path; std::string relative;};
  std::vector<Frame> pending{{root,{}}};
  while(!pending.empty()) {
    Frame frame=std::move(pending.back());
    pending.pop_back();
    std::vector<Frame> child_dirs;
    fs::directory_iterator iterator(frame.path,fs::directory_options::none,ec),end;
    if(ec)return Error("Cannot enumerate the source game folder.",ec.value());
    for(;iterator!=end;iterator.increment(ec)) {
      if(ec)return Error("Cannot enumerate the source game folder.",ec.value());
      const auto name=iterator->path().filename().u8string();
      if(frame.relative.empty()&&Reserved(name)){result.skipped.push_back(name);continue;}
      if(name.empty() || name=="." || name==".." || name.find('/')!=std::string::npos ||
          name.find('\\')!=std::string::npos)return Error("Unsafe source name.",EINVAL);
      const auto relative=frame.relative.empty()?name:frame.relative+"/"+name;
      const auto state=iterator->symlink_status(ec);
      if(ec)return Error("Unable to inspect a source entry.",ec.value());
      if(fs::is_symlink(state))return Error("The source contains a link: "+relative,ELOOP);
      if(fs::is_directory(state)) {
        if(result.plan.dirs.size()>=max_files)return Error("Too many source folders.");
        result.plan.dirs.push_back(relative);
        child_dirs.push_back({iterator->path(),relative});
      } else if(fs::is_regular_file(state)) {
        if(result.plan.files.size()>=max_files)return Error("Too many source files.");
        const auto bytes=fs::file_size(iterator->path(),ec);
        if(ec)return Error("Cannot inspect source file size.",ec.value());
        if(bytes>max_bytes || result.plan.total>max_bytes-bytes)
          return Error("Source game exceeds the copy size limit.");
        result.plan.files.push_back({relative,bytes,0,0});
        result.plan.total+=bytes;
      }
    }
    // Reverse stack order for stable depth-first traversal.
    std::sort(child_dirs.begin(),child_dirs.end(),
      [](const Frame& a,const Frame& b){return a.relative>b.relative;});
    for(auto& child:child_dirs)pending.push_back(std::move(child));
  }
  result.ok=true;
  return result;
}
std::string SaveCopyPlan(const CopyPlan&,const std::string&) {
  return "Windows durable copy-plan writing is not implemented.";
}
std::string SaveFileList(const CopyPlan&,const std::string&) {
  return "Windows durable copy-file inventory writing is not implemented.";
}
std::optional<CopyPlan> LoadCopyPlan(const std::string&) {
  return std::nullopt;
}
} // namespace playden::stores::local
