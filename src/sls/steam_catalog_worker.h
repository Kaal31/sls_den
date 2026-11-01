#pragma once
#include "sls/steam_catalog_search.h"
#include "steam/backend/http_client.h"
#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
namespace playden::sls {
class SteamCatalogWorker {
public:
  explicit SteamCatalogWorker(std::string ca_bundle);
  ~SteamCatalogWorker();
  SteamCatalogWorker(const SteamCatalogWorker&)=delete;
  SteamCatalogWorker& operator=(const SteamCatalogWorker&)=delete;
  void Search(std::string query);
  struct Result { std::string query; bool ok=false; SteamCatalogPage page; };
  std::optional<Result> Poll();
private:
  void Run();
  std::string ca_;
  std::mutex mutex_;
  std::thread thread_;
  std::atomic<bool> stop_{false};
  std::optional<std::string> queued_;
  std::optional<Result> ready_;
};
}
