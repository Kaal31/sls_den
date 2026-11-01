#pragma once
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>
namespace playden::sls {
// Persistent publisher-declared DRM metadata. Unknown/failed lookups retain old evidence.
class DenuvoWorker {
public:
  DenuvoWorker(std::string ca_bundle,std::filesystem::path cache);
  ~DenuvoWorker();
  DenuvoWorker(const DenuvoWorker&)=delete;
  DenuvoWorker& operator=(const DenuvoWorker&)=delete;
  void Observe(const std::vector<std::uint32_t>& ids);
  std::set<std::uint32_t> Known() const;
private:
  struct Entry { bool present=false; std::int64_t checked=0; };
  void Run();
  void Persist();
  std::string ca_;
  std::filesystem::path cache_;
  mutable std::mutex mutex_;
  std::map<std::uint32_t,Entry> entries_;
  std::map<std::uint32_t,std::int64_t> retry_after_;
  std::set<std::uint32_t> observed_;
  std::set<std::uint32_t> seed_;
  std::atomic<bool> stopping_{false};
  std::thread thread_;
};
}
