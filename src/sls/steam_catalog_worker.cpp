#include "sls/steam_catalog_worker.h"
#include <utility>
namespace playden::sls {
SteamCatalogWorker::SteamCatalogWorker(std::string ca):ca_(std::move(ca)),thread_([this]{Run();}){}
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
void SteamCatalogWorker::Run(){
  steam::HttpClient http(ca_);
  while(!stop_) {
    std::optional<std::string> query;
    {std::lock_guard<std::mutex> guard(mutex_);query=std::exchange(queued_,std::nullopt);}
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
