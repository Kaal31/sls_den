#include "update/file_ops.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <vector>
#include <limits>

namespace playden::update {
namespace {
namespace fs = std::filesystem;
fs::path Path(const std::string& p) { return fs::u8path(p); }
bool NotLink(const fs::path& path) {
  std::error_code ec;
  const auto st = fs::symlink_status(path,ec);
  return !ec && !fs::is_symlink(st) && (st.permissions() != fs::perms::unknown);
}
bool CheckChain(const fs::path& path) {
  if (!path.is_absolute()) return false;
  fs::path current=path.root_path();
  std::error_code ec;
  for (const auto& part:path.relative_path()) {
    if(part=="." || part=="..")return false;
    current/=part;
    const auto st=fs::symlink_status(current,ec);
    if(ec==std::errc::no_such_file_or_directory) {ec.clear();continue;}
    if(ec || fs::is_symlink(st))return false;
  }
  return true;
}
std::uint64_t DeleteContents(const fs::path& path) {
  std::error_code ec;
  const auto st=fs::symlink_status(path,ec);
  if(ec)return 0;
  std::uint64_t removed=0;
  if(fs::is_directory(st) && !fs::is_symlink(st)) {
    fs::directory_iterator it(path,fs::directory_options::none,ec),end;
    if(ec)return 0;
    for(;it!=end;it.increment(ec)) {
      if(ec)return removed;
      removed+=DeleteContents(it->path());
    }
  }
  if(fs::remove(path,ec)&&!ec)++removed;
  return removed;
}
}
bool Exists(const std::string& name) {
  std::error_code ec;
  return fs::exists(fs::symlink_status(Path(name),ec))&&!ec;
}
bool IsFolder(const std::string& name) {
  std::error_code ec;
  const auto st=fs::symlink_status(Path(name),ec);
  return !ec&&fs::is_directory(st)&&!fs::is_symlink(st);
}
bool MakeFolders(const std::string& name) {
  const auto path=Path(name);
  if(!CheckChain(path))return false;
  std::error_code ec;
  fs::create_directories(path,ec);
  return !ec && IsFolder(name);
}
bool MakeParents(const std::string& name) {
  const auto parent=Path(name).parent_path();
  return parent.empty() || MakeFolders(parent.u8string());
}
std::vector<std::string> ListFolder(const std::string& name) {
  std::vector<std::string> names;
  const auto path=Path(name);
  if(!CheckChain(path)||!IsFolder(name))return names;
  std::error_code ec;
  fs::directory_iterator it(path,fs::directory_options::none,ec),end;
  if(ec)return {};
  for(;it!=end;it.increment(ec)){
    if(ec)return {};
    names.push_back(it->path().filename().u8string());
  }
  if(ec)return {};
  std::sort(names.begin(),names.end());
  return names;
}
std::uint64_t RemoveTree(const std::string& name) {
  const auto path=Path(name);
  if(!CheckChain(path) || path==path.root_path())return 0;
  return DeleteContents(path);
}
bool WriteFileAtomically(const std::string& name,const std::string& text) {
  const auto path=Path(name);
  if(!CheckChain(path) || !MakeParents(name))return false;
  const auto temporary=fs::path(path.wstring()+L".tmp");
  if(!CheckChain(temporary))return false;
  const auto tmp=temporary.wstring(), destination=path.wstring();
  HANDLE file=CreateFileW(tmp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,
                          FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
  if(file==INVALID_HANDLE_VALUE)return false;
  bool ok=true;
  std::size_t offset=0;
  while(offset<text.size()){
    DWORD written=0;
    const DWORD amount=static_cast<DWORD>(
      std::min<std::size_t>(text.size()-offset,1024*1024));
    if(!WriteFile(file,text.data()+offset,amount,&written,nullptr)||written==0){
      ok=false;break;
    }
    offset+=written;
  }
  if(ok)ok=FlushFileBuffers(file)!=0;
  if(!CloseHandle(file))ok=false;
  if(ok && !CheckChain(path))ok=false;
  if(ok)ok=MoveFileExW(tmp.c_str(),destination.c_str(),
                       MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
  if(!ok)DeleteFileW(tmp.c_str());
  return ok;
}
bool ReadSmallFile(const std::string& name,std::string* text,std::size_t limit) {
  if(!text || !CheckChain(Path(name)) || !NotLink(Path(name)))return false;
  std::error_code ec;
  if(!fs::is_regular_file(Path(name),ec)||ec)return false;
  const auto size=fs::file_size(Path(name),ec);
  if(ec || size>limit)return false;
  std::ifstream in(Path(name),std::ios::binary);
  if(!in)return false;
  std::string bytes(static_cast<std::size_t>(size),'\0');
  if(!bytes.empty()&&!in.read(&bytes[0],static_cast<std::streamsize>(bytes.size())))
    return false;
  *text=std::move(bytes);
  return true;
}
} // namespace playden::update

#else
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>

namespace playden::update {

bool MakeFolders(const std::string& path) {
  for (std::size_t i = 1; i <= path.size(); ++i) {
    if (i != path.size() && path[i] != '/') continue;
    const std::string part = path.substr(0, i);
    if (mkdir(part.c_str(), 0777) != 0 && errno != EEXIST) return false;
  }
  return IsFolder(path);
}

bool MakeParents(const std::string& path) {
  const auto slash = path.rfind('/');
  return slash == std::string::npos || slash == 0 || MakeFolders(path.substr(0, slash));
}

bool Exists(const std::string& path) {
  struct stat st{};
  return lstat(path.c_str(), &st) == 0;
}

bool IsFolder(const std::string& path) {
  struct stat st{};
  return lstat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

std::vector<std::string> ListFolder(const std::string& path) {
  std::vector<std::string> names;
  DIR* folder = opendir(path.c_str());
  if (!folder) return names;
  while (const dirent* entry = readdir(folder)) {
    if (!std::strcmp(entry->d_name, ".") || !std::strcmp(entry->d_name, "..")) continue;
    names.emplace_back(entry->d_name);
  }
  closedir(folder);
  std::sort(names.begin(), names.end());
  return names;
}

std::uint64_t RemoveTree(const std::string& path) {
  struct stat st{};
  if (lstat(path.c_str(), &st) != 0) return 0;
  std::uint64_t removed = 0;
  if (S_ISDIR(st.st_mode)) {
    for (const auto& name : ListFolder(path)) removed += RemoveTree(path + "/" + name);
    if (rmdir(path.c_str()) == 0) ++removed;
  } else if (unlink(path.c_str()) == 0) {
    ++removed;
  }
  return removed;
}

bool WriteFileAtomically(const std::string& path, const std::string& text) {
  const std::string temporary = path + ".tmp";
  const int fd = open(temporary.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0666);
  if (fd < 0) return false;
  std::size_t written = 0;
  while (written < text.size()) {
    const ssize_t n = write(fd, text.data() + written, text.size() - written);
    if (n <= 0) { close(fd); unlink(temporary.c_str()); return false; }
    written += static_cast<std::size_t>(n);
  }
  fchmod(fd, 0666);
  const bool synced = fsync(fd) == 0;
  close(fd);
  if (!synced || rename(temporary.c_str(), path.c_str()) != 0) { unlink(temporary.c_str()); return false; }
  return true;
}

bool ReadSmallFile(const std::string& path, std::string* text, std::size_t limit) {
  const int fd = open(path.c_str(), O_RDONLY);
  if (fd < 0) return false;
  text->clear();
  char buffer[4096];
  ssize_t n;
  while ((n = read(fd, buffer, sizeof buffer)) > 0) {
    if (text->size() + static_cast<std::size_t>(n) > limit) { close(fd); return false; }
    text->append(buffer, static_cast<std::size_t>(n));
  }
  close(fd);
  return n == 0;
}

}  // namespace playden::update

#endif // _WIN32
