#include "sls/steam_catalog_worker.h"
#include "sls/zip_inflate.h"
#include <utility>
#include <chrono>
#include <algorithm>
namespace playden::sls {
SteamCatalogWorker::SteamCatalogWorker(std::string ca):ca_(std::move(ca)) {thread_=std::thread([this]{Run();});}
SteamCatalogWorker::~SteamCatalogWorker(){stop_=true;if(thread_.joinable())thread_.join();}
void SteamCatalogWorker::Search(std::string query) {
  if(SteamStoreSearchUrl(query).empty())return;
  std::lock_guard<std::mutex> guard(mutex_);
  queued_=std::move(query);
  ready_.reset();
}
std::optional<SteamCatalogWorker::Result> SteamCatalogWorker::Poll() {
  std::lock_guard<std::mutex> guard(mutex_);
  return std::exchange(ready_,std::nullopt);
}
void SteamCatalogWorker::Discover(std::uint32_t app_id,ProviderSelection selection) {
  if(!app_id)return;
  std::lock_guard<std::mutex> guard(mutex_);
  discovery_queued_=DiscoveryRequest{app_id,std::move(selection)};
  discovery_ready_.reset();
}
std::optional<SteamCatalogWorker::DiscoveryResult> SteamCatalogWorker::PollDiscovery() {
  std::lock_guard<std::mutex> guard(mutex_);
  return std::exchange(discovery_ready_,std::nullopt);
}
void SteamCatalogWorker::Run(){
  steam::HttpClient http(ca_);
  while(!stop_) {
    std::optional<std::string> query;
    std::optional<DiscoveryRequest> discovery;
    {
      std::lock_guard<std::mutex> guard(mutex_);
      query=std::exchange(queued_,std::nullopt);
      if(!query)discovery=std::exchange(discovery_queued_,std::nullopt);
    }
    if(discovery) {
      DiscoveryResult outcome;
      outcome.app_id=discovery->app_id;
      outcome.resolution=DiscoverManifestWithCredentials(discovery->app_id,discovery->selection,{},
        [&](const std::string&,const std::string& url_template,std::uint32_t app_id,
            std::string_view,std::string* payload,std::string* error) {
          if(stop_.load() || !payload || !error)return false;
          std::string url=url_template;
          const std::string needle="{appid}";
          const auto at=url.find(needle);
          if(at==std::string::npos)return false;
          url.replace(at,needle.size(),std::to_string(app_id));
          if(url.rfind("https://",0)!=0 || url.size()>2048)return false;
          steam::HttpRequest request;
          request.method=steam::HttpMethod::kGet;
          request.url=std::move(url);
          request.timeout_ms=10000;
          request.max_response_bytes=1024*1024;
          request.allow_plain_http=false;
          const auto response=http.Perform(request,stop_);
          if(!response.ok() || response.status_code!=200 || response.body.empty())return false;
          payload->assign(response.body.begin(),response.body.end());
          return true;
        },
        [](std::string_view zip,std::uint32_t app,std::string* manifest,std::string* error) {
          return DecodeZipManifest(zip,app,manifest,error,InflateZipRaw);
        });
      std::lock_guard<std::mutex> guard(mutex_);
      if(!stop_ && !discovery_queued_)discovery_ready_=std::move(outcome);
      continue;
    }
    if(!query){std::this_thread::sleep_for(std::chrono::milliseconds(30));continue;}
    Result result;result.query=*query;
    steam::HttpRequest request;
    request.url=SteamStoreSearchUrl(*query);
    request.timeout_ms=10000;
    request.max_response_bytes=512*1024;
    request.headers={{"Accept","application/json"},{"User-Agent","LuaDen/1.0"}};
    auto response=http.Perform(request,stop_);
    if(!stop_ && response.ok() && response.status_code==200){
      std::string payload(response.body.begin(),response.body.end());
      result.ok=ParseSteamStoreSearchResponse(payload,&result.page);
    }
    {std::lock_guard<std::mutex> guard(mutex_);if(!queued_ && !stop_)ready_=std::move(result);}
  }
}
}
