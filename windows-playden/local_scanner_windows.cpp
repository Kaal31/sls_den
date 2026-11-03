// Native Windows scan adapter. Does not traverse symlinks or junctions.
// This does not emulate the PS5 descriptor-walk and is limited to ordinary
// non-reparse Windows filesystem entries for local import preview.
#include "stores/local/scanner.h"
#include <algorithm>
#include <cerrno>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>
namespace playden::stores::local {
namespace fs=std::filesystem;
namespace {
std::string Lower(std::string text) {
  std::transform(text.begin(),text.end(),text.begin(),[](unsigned char c) {
    return c>='A'&&c<='Z' ? static_cast<char>(c+32) : static_cast<char>(c);
  });
  return text;
}
bool SafeRoot(const fs::path& path) {
  if(!path.is_absolute())return false;
  fs::path partpath;
  std::error_code ec;
  for(const auto& part:path) {
    partpath/=part;
    const auto st=fs::symlink_status(partpath,ec);
    if(ec || fs::is_symlink(st))return false;
  }
  return true;
}
ScanStatus ErrorStatus(const fs::path& path) {
  std::error_code ec;
  if(!fs::exists(path,ec))return ScanStatus::kMissing;
  if(!fs::is_directory(path,ec))return ScanStatus::kNotFolder;
  return ScanStatus::kFailed;
}
bool EndsExe(const std::string& value) {
  const auto lower=Lower(value);
  return lower.size()>4 && lower.substr(lower.size()-4)==".exe";
}
bool AllowedRoot(const std::string& path) {
  return !path.empty() && SafeRoot(fs::u8path(path));
}
}
bool IsPrintableAscii(const std::string& text) {
  return std::all_of(text.begin(),text.end(),[](unsigned char c){return c>=32&&c<127;});
}
bool IsSafeRelativePath(const std::string& value) {
  if(value.empty()||value.size()>4096||value.find('\\')!=std::string::npos||
     value.find('\0')!=std::string::npos)return false;
  fs::path path=fs::u8path(value);
  if(path.is_absolute()||path.has_root_name())return false;
  for(const auto& part:path)if(part.empty()||part=="."||part=="..")return false;
  return true;
}
std::string TitleForFolder(const std::string& folder) {
  return fs::u8path(folder).filename().u8string();
}
bool IsLikelyHelper(const std::string& path) {
  const auto name=Lower(fs::u8path(path).filename().u8string());
  for(const char* prefix:{"unins","setup","install","vcredist","crash","redist","update","patch","config","settings"})
    if(name.rfind(prefix,0)==0)return true;
  return false;
}
void RankPrograms(std::vector<Program>& programs,const std::string& title) {
  const auto wanted=Lower(title);
  std::stable_sort(programs.begin(),programs.end(),[&](const Program& a,const Program& b){
    if(a.helper!=b.helper)return !a.helper;
    const bool am=Lower(a.path).find(wanted)!=std::string::npos;
    const bool bm=Lower(b.path).find(wanted)!=std::string::npos;
    if(am!=bm)return am;
    const auto ad=std::count(a.path.begin(),a.path.end(),'/');
    const auto bd=std::count(b.path.begin(),b.path.end(),'/');
    if(ad!=bd)return ad<bd;
    if(a.bytes!=b.bytes)return a.bytes>b.bytes;
    return a.path<b.path;
  });
}
FolderScan ScanGameFolder(const std::string& folder) {
  FolderScan result;
  result.title=TitleForFolder(folder);
  result.printable_path=IsPrintableAscii(folder);
  const fs::path root=fs::u8path(folder);
  if(!AllowedRoot(folder)) { result.status=ScanStatus::kUnsupported;result.error=EINVAL;return result; }
  std::error_code ec;
  if(!fs::is_directory(root,ec)||ec) {
    result.status=ErrorStatus(root);result.error=ec.value();return result;
  }
  struct Frame {fs::path path;int depth;};
  std::vector<Frame> pending{{root,0}};
  std::size_t entries=0;
  while(!pending.empty()&&!result.limit_reached) {
    const auto frame=pending.back();pending.pop_back();
    fs::directory_iterator it(frame.path,fs::directory_options::none,ec),end;
    if(ec) {result.status=ScanStatus::kFailed;result.error=ec.value();result.programs.clear();return result;}
    for(;it!=end;it.increment(ec)) {
      if(ec)break;
      if(++entries>kMaxScanEntries) {result.limit_reached=true;break;}
      const auto st=it->symlink_status(ec);
      if(ec){++result.unreadable;ec.clear();continue;}
      if(fs::is_symlink(st))continue;
      auto relative=it->path().lexically_relative(root).generic_u8string();
      if(relative.empty()||!IsSafeRelativePath(relative))continue;
      if(fs::is_directory(st)) {
        if(frame.depth+1>=kMaxProgramDepth)result.deeper_folders=true;
        else pending.push_back({it->path(),frame.depth+1});
      } else if(fs::is_regular_file(st)) {
        const auto bytes=fs::file_size(it->path(),ec);
        if(ec){++result.unreadable;ec.clear();continue;}
        result.total_bytes+=bytes;++result.file_count;
        if(EndsExe(relative)) {
          if(result.programs.size()>=kMaxScanPrograms){result.limit_reached=true;break;}
          result.programs.push_back({relative,bytes,IsLikelyHelper(relative),IsPrintableAscii(relative)});
        }
      }
    }
    if(ec) {result.status=ScanStatus::kFailed;result.error=ec.value();result.programs.clear();return result;}
  }
  RankPrograms(result.programs,result.title);
  result.status=ScanStatus::kOk;return result;
}
ProgramCheck CheckProgram(const std::string& folder,const std::string& program) {
  if(!IsSafeRelativePath(program))return ProgramCheck::kInvalidPath;
  if(!AllowedRoot(folder))return ProgramCheck::kUnsupported;
  fs::path current=fs::u8path(folder);
  const fs::path relative=fs::u8path(program);
  std::error_code ec;
  for(const auto& part:relative) {
    current/=part;
    const auto st=fs::symlink_status(current,ec);
    if(ec||!fs::exists(st))return ProgramCheck::kMissing;
    if(fs::is_symlink(st))return ProgramCheck::kNotRegularFile;
  }
  return fs::is_regular_file(current,ec)&&!ec?ProgramCheck::kOk:ProgramCheck::kNotRegularFile;
}
const std::vector<ImportRoot>& ImportRoots() {
  static const std::vector<ImportRoot> roots; // PS5 mount constants do not apply on Windows.
  return roots;
}
CandidateScan ScanImportRoot(const std::string& folder) {
  CandidateScan result;
  if(!AllowedRoot(folder)){result.status=ScanStatus::kUnsupported;return result;}
  const fs::path root=fs::u8path(folder);
  std::error_code ec;
  if(!fs::exists(root,ec)) {result.status=ScanStatus::kOk;return result;}
  fs::directory_iterator it(root,fs::directory_options::none,ec),end;
  if(ec){result.status=ScanStatus::kFailed;result.error=ec.value();return result;}
  std::size_t entries=0;
  for(;it!=end;it.increment(ec)) {
    if(ec)break;
    if(++entries>kMaxScanEntries){result.limit_reached=true;break;}
    const auto st=it->symlink_status(ec);
    if(ec)break;
    if(!fs::is_symlink(st)&&fs::is_directory(st)){
      if(result.folders.size()>=kMaxImportCandidates){result.limit_reached=true;break;}
      result.folders.push_back(it->path().filename().u8string());
    }
  }
  if(ec){result.status=ScanStatus::kFailed;result.error=ec.value();result.folders.clear();return result;}
  std::sort(result.folders.begin(),result.folders.end());
  result.status=ScanStatus::kOk;return result;
}
} // namespace playden::stores::local
