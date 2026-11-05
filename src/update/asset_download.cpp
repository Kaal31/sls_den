#include "update/asset_download.h"
#include "update/release.h"

#if defined(_WIN32)
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <filesystem>
#include <thread>
#include <chrono>
#include <cstring>
#else
#include <fcntl.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <cerrno>
#include <cstdio>

namespace playden::update {
namespace {

constexpr int kMaxRedirects = 5;
void DownloadClose(int fd) {
#if defined(_WIN32)
  _close(fd);
#else
  close(fd);
#endif
}

#if defined(_WIN32)
int DownloadOpen(const std::string& path) {
  const auto wide=std::filesystem::u8path(path).wstring();
  return _wopen(wide.c_str(),_O_CREAT|_O_RDWR|_O_BINARY|_O_NOINHERIT,
                _S_IREAD|_S_IWRITE);
}
bool DownloadExistsSize(const std::string& path,std::uint64_t* size) {
  std::error_code ec;
  const auto native=std::filesystem::u8path(path);
  const auto st=std::filesystem::symlink_status(native,ec);
  if(ec || !std::filesystem::is_regular_file(st) || std::filesystem::is_symlink(st))
    return false;
  *size=std::filesystem::file_size(native,ec);
  return !ec;
}
bool DownloadRemove(const std::string& path) {
  std::error_code ec;
  return std::filesystem::remove(std::filesystem::u8path(path),ec)&&!ec;
}
bool DownloadRename(const std::string& from,const std::string& to) {
  std::error_code ec;
  std::filesystem::rename(std::filesystem::u8path(from),std::filesystem::u8path(to),ec);
  return !ec;
}
bool DownloadWriteAt(int fd,const std::uint8_t* data,std::size_t count,std::uint64_t offset) {
  if(_lseeki64(fd,static_cast<__int64>(offset),SEEK_SET)<0)return false;
  std::size_t done=0;
  while(done<count) {
    const unsigned amount=static_cast<unsigned>(std::min<std::size_t>(count-done,1u<<20));
    const int n=_write(fd,data+done,amount);
    if(n<=0)return false;
    done+=static_cast<std::size_t>(n);
  }
  return true;
}
#endif


std::string Header(const steam::HttpResponse& response, const char* name) {
  for (const auto& [key, value] : response.headers)
    #if defined(_WIN32)
    if (_stricmp(key.c_str(), name) == 0) return value;
#else
    if (strcasecmp(key.c_str(), name) == 0) return value;
#endif
  return {};
}

bool Redirect(long status) { return status == 301 || status == 302 || status == 303 || status == 307 || status == 308; }

std::string Host(const std::string& url) {
  const std::string scheme = "https://";
  if (url.compare(0, scheme.size(), scheme) != 0) return {};
  const auto end = url.find_first_of("/?#", scheme.size());
  std::string host = url.substr(scheme.size(), end == std::string::npos ? std::string::npos : end - scheme.size());
  // No user info or ports.
  if (host.find_first_of("@:") != std::string::npos) return {};
  for (auto& c : host) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
  return host;
}

void Log(const DownloadOptions& options, const std::string& line) {
  if (options.log) options.log(line);
}

bool Cancelled(const DownloadOptions& options) { return options.cancel && options.cancel->load(); }

steam::HttpResponse Get(const DownloadOptions& options, const std::string& url, const std::string& range,
                        std::size_t limit, const std::string& accept = "*/*") {
  steam::HttpRequest request;
  request.url = url;
  request.headers = {{"User-Agent", options.user_agent}, {"Accept", accept}};
  if (!range.empty()) request.headers.emplace_back("Range", "bytes=" + range);
  request.timeout_ms = 120000;
  request.max_response_bytes = limit;
  static const std::atomic<bool> never{false};
  return options.fetch(request, options.cancel ? *options.cancel : never);
}

std::string Describe(const steam::HttpResponse& response) {
  return "status " + std::to_string(response.status_code) + " error " + std::to_string(static_cast<int>(response.error));
}

// Follows redirects from url until a server answers the probe range. Returns the final URL.
std::optional<std::string> Resolve(const DownloadOptions& options, std::string url, std::string* error) {
  for (int hop = 0; hop <= kMaxRedirects; ++hop) {
    if (!AllowedDownloadUrl(url)) {
      *error = "download host not allowed";
      return std::nullopt;
    }
    const auto response = Get(options, url, "0-0", 64 << 10);
    if (Redirect(response.status_code)) {
      url = Header(response, "location");
      continue;
    }
    if (response.status_code == 206 || response.status_code == 200) return url;
    *error = "download link failed (" + Describe(response) + ")";
    return std::nullopt;
  }
  *error = "too many redirects";
  return std::nullopt;
}

void Wait(const DownloadOptions& options, int attempt) {
  if (options.wait_before_retry) {
    options.wait_before_retry(attempt);
    return;
  }
  for (int step = 0; step < 20 * attempt && !Cancelled(options); ++step) {
#if defined(_WIN32)
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
#else
    usleep(100000);
#endif
  }
}

}  // namespace

bool AllowedDownloadUrl(const std::string& url) {
  const std::string host = Host(url);
  if (host.empty()) return false;
  if (host == "github.com" || host == "api.github.com") return true;
  const std::string suffix = ".githubusercontent.com";
  return host.size() > suffix.size() && host.compare(host.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::optional<std::string> FetchSmall(const std::string& url, std::size_t limit, const DownloadOptions& options,
                                      const std::string& accept, std::string* error) {
  std::string target = url;
  for (int hop = 0; hop <= kMaxRedirects; ++hop) {
    if (!AllowedDownloadUrl(target)) {
      *error = "host not allowed";
      return std::nullopt;
    }
    const auto response = Get(options, target, "", limit, accept);
    if (Redirect(response.status_code)) {
      target = Header(response, "location");
      continue;
    }
    if (response.status_code != 200 || !response.ok()) {
      *error = Describe(response);
      return std::nullopt;
    }
    return std::string(response.body.begin(), response.body.end());
  }
  *error = "too many redirects";
  return std::nullopt;
}

DownloadResult DownloadAsset(const std::string& url, std::uint64_t size, const std::string& sha256,
                             const std::string& path, const DownloadOptions& options) {
  DownloadResult result;
  auto fail = [&](std::string reason) {
    result.error = std::move(reason);
    Log(options, "download " + path + " failed: " + result.error);
    return result;
  };
  if (!size) return fail("empty asset");
  // GitHub's digest is the only check on the bytes: without one, nothing is installed.
  if (!IsSha256(sha256)) return fail("no checksum for this download");
  const std::string part = path + ".part";
#if defined(_WIN32)
  std::uint64_t done=0;
  if(DownloadExistsSize(part,&done) && done>size) {
    if(!DownloadRemove(part))return fail("cannot discard oversized part");
    done=0;
  }
  if(done)Log(options,"download "+path+" resumes at "+std::to_string(done));
  const int fd=DownloadOpen(part);
  if(fd<0)return fail("cannot open Windows download part");
  if(_chsize_s(fd,done)!=0) {
    _close(fd);return fail("cannot resize Windows download part");
  }
#else
  struct stat st{};
  std::uint64_t done = stat(part.c_str(), &st) == 0 ? static_cast<std::uint64_t>(st.st_size) : 0;
  if (done > size) {
    unlink(part.c_str());
    done = 0;
  }
  if (done) Log(options, "download " + path + " resumes at " + std::to_string(done));
  const int fd = open(part.c_str(), O_CREAT | O_WRONLY, 0666);
  if (fd < 0) return fail("cannot create " + part + " errno=" + std::to_string(errno));
  fchmod(fd, 0666);
  if (ftruncate(fd, static_cast<off_t>(done)) != 0) {
    close(fd);
    return fail("cannot resize " + part);
  }
#endif
  std::string target;
  int attempts = 0;
  while (done < size) {
    if (Cancelled(options)) {
      DownloadClose(fd);
      result.cancelled = true;
      return fail("cancelled");
    }
    if (target.empty()) {
      std::string error;
      const auto resolved = Resolve(options, url, &error);
      if (!resolved) {
        if (Cancelled(options)) continue;
        if (++attempts >= options.max_attempts) { DownloadClose(fd); return fail(error); }
        Wait(options, attempts);
        continue;
      }
      target = *resolved;
    }
    const std::uint64_t end = std::min(size, done + options.chunk_bytes) - 1;
    const auto response = Get(options, target, std::to_string(done) + "-" + std::to_string(end),
                              static_cast<std::size_t>(end - done + 1 + 1024));
    // The bytes asked for, and only those: "Content-Range: bytes <done>-<end>/<size>".
    const std::string range = "bytes " + std::to_string(done) + "-" + std::to_string(end) + "/";
    if (response.status_code == 206 && response.ok() && response.body.size() == end - done + 1 &&
        Header(response, "content-range").rfind(range, 0) == 0) {
      std::size_t written = 0;
      while (written < response.body.size()) {
#if defined(_WIN32)
        if(!DownloadWriteAt(fd,response.body.data()+written,response.body.size()-written,done+written)){
          DownloadClose(fd);return fail("Windows write failed");
        }
        written=response.body.size();
#else
        const ssize_t n = pwrite(fd, response.body.data() + written, response.body.size() - written,
                                 static_cast<off_t>(done + written));
        if (n <= 0) { DownloadClose(fd); return fail("write failed errno=" + std::to_string(errno)); }
        written += static_cast<std::size_t>(n);
#endif
      }
      done += response.body.size();
      attempts = 0;
      if (options.progress) options.progress(done, size);
      continue;
    }
    if (Cancelled(options)) continue;
    Log(options, "download chunk at " + std::to_string(done) + ": " + Describe(response));
    // An expired signed link or a server that ignores Range: ask GitHub for a new link.
    if (response.status_code == 401 || response.status_code == 403 || response.status_code == 404 ||
        response.status_code == 200 || response.status_code == 416)
      target.clear();
    if (++attempts >= options.max_attempts) { DownloadClose(fd); return fail("download keeps failing (" + Describe(response) + ")"); }
    Wait(options, attempts);
  }
#if defined(_WIN32)
  const bool synced=_commit(fd)==0;
  _close(fd);
#else
  const bool synced = fsync(fd) == 0;
  close(fd);

#endif
  if(!synced)return fail("cannot sync "+part);
  const auto actual = options.hash_file ? options.hash_file(part) : std::nullopt;
  if (!actual) return fail("cannot read the download to check it");  // the bytes stay for a retry
  if (*actual != sha256) {
#if defined(_WIN32)
    DownloadRemove(part);
#else
    unlink(part.c_str());
#endif
    return fail("checksum mismatch");
  }
#if defined(_WIN32)
  if(!DownloadRename(part,path))return fail("cannot rename "+part);
#else
  if (rename(part.c_str(), path.c_str()) != 0) return fail("cannot rename " + part);
#endif
  result.ok = true;
  return result;
}

}  // namespace playden::update
