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
class ProtonDbWorker {
public:
  ProtonDbWorker(std::string ca,std::filesystem::path cache);
  ~ProtonDbWorker();
  ProtonDbWorker(const ProtonDbWorker&)=delete;
  ProtonDbWorker& operator=(const ProtonDbWorker&)=delete;
  void Observe(const std::vector<std::uint32_t>& appids);
  std::map<std::uint32_t,std::string> Ratings() const;
private:
  struct Entry { std::string tier; std::int64_t checked=0; };
  void Run();
  void Persist();
  std::string ca_;
  std::filesystem::path cache_;
  mutable std::mutex mutex_;
  std::map<std::uint32_t,Entry> entries_;
  std::map<std::uint32_t,std::int64_t> retry_after_;
  std::set<std::uint32_t> observed_;
  std::atomic<bool> stopping_{false};
  std::thread thread_;
};
}
