#include "sls/protondb_worker.h"
#include "steam/backend/http_client.h"
#include "steam/backend/json.h"
#include <chrono>
#include <ctime>
#include <fstream>
#include <limits>
#include <optional>
#include <utility>
namespace playden::sls {
namespace {
constexpr std::int64_t kDay=86400;
constexpr std::int64_t kRetry=300;
std::int64_t Now(){return static_cast<std::int64_t>(std::time(nullptr));}
bool IsTier(const std::string& s){
  return s=="platinum" || s=="gold" || s=="silver" || s=="bronze" || s=="borked";
}
std::optional<std::string> ParseTier(std::string_view body){
  const auto root=steam::json::Parse(body);
  if(!root || !root->is_object())return std::nullopt;
  const auto* tier=root->Find("tier");
  if(!tier)return std::nullopt;
  if(tier->is_null())return std::string(); // Valid no-rating response.
  if(!tier->is_string())return std::nullopt;
  if(tier->text()=="pending")return std::string();
  if(!IsTier(tier->text()))return std::nullopt;
  return tier->text();
}
}
ProtonDbWorker::ProtonDbWorker(std::string ca,std::filesystem::path cache):
  ca_(std::move(ca)),cache_(std::move(cache)){
  std::ifstream input(cache_);
  std::uint64_t id=0;std::int64_t checked=0;std::string tier;
  while(input>>id>>checked>>tier)
    if(id>0 && id<=std::numeric_limits<std::uint32_t>::max() && checked>0 &&
       (IsTier(tier)||tier=="none"))
      entries_[static_cast<std::uint32_t>(id)]={tier=="none"?"":tier,checked};
  thread_=std::thread([this]{Run();});
}
ProtonDbWorker::~ProtonDbWorker(){stopping_=true;if(thread_.joinable())thread_.join();}
void ProtonDbWorker::Observe(const std::vector<std::uint32_t>& ids){
  std::lock_guard<std::mutex> lock(mutex_);
  for(auto id:ids)if(id>0)observed_.insert(id);
}
std::map<std::uint32_t,std::string> ProtonDbWorker::Ratings() const{
  std::lock_guard<std::mutex> lock(mutex_);
  std::map<std::uint32_t,std::string> out;
  for(const auto& [id,e]:entries_)if(!e.tier.empty())out[id]=e.tier;
  return out;
}
void ProtonDbWorker::Persist(){
  std::map<std::uint32_t,Entry> entries;
  {std::lock_guard<std::mutex> lock(mutex_);entries=entries_;}
  std::error_code ec;
  std::filesystem::create_directories(cache_.parent_path(),ec);
  if(ec)return;
  auto temp=cache_;temp+=".tmp";
  {std::ofstream file(temp,std::ios::trunc);if(!file)return;
   for(const auto& [id,e]:entries)file<<id<<" "<<e.checked<<" "<<(e.tier.empty()?"none":e.tier)<<"\n";
   file.flush();if(!file)return;}
  // Existing cache is never destroyed on a failed refresh.
  // On Windows rename() does not replace an existing destination. Keep the
  // previous valid cache as a rollback copy while publishing the update.
  auto backup=cache_;backup+=".bak";
  std::error_code ignored;
  std::filesystem::remove(backup,ignored);
  const bool had_cache=std::filesystem::exists(cache_,ignored);
  if(had_cache) {
    std::filesystem::rename(cache_,backup,ec);
    if(ec){std::filesystem::remove(temp,ignored);return;}
  }
  ec.clear();
  std::filesystem::rename(temp,cache_,ec);
  if(ec && had_cache) {
    std::error_code restore;
    std::filesystem::rename(backup,cache_,restore);
  }
  if(!ec)std::filesystem::remove(backup,ignored);
  else std::filesystem::remove(temp,ignored);
}
void ProtonDbWorker::Run(){
  steam::HttpClient client(ca_);
  while(!stopping_){
    std::uint32_t selected=0;const auto now=Now();
    {std::lock_guard<std::mutex> lock(mutex_);
     for(auto id:observed_){
       auto it=entries_.find(id);
       if((it==entries_.end() || now-it->second.checked>=kDay) && now>=retry_after_[id]) {selected=id;break;}
     }}
    if(!selected){std::this_thread::sleep_for(std::chrono::milliseconds(200));continue;}
    steam::HttpRequest req;
    req.url="https://www.protondb.com/api/v1/reports/summaries/"+std::to_string(selected)+".json";
    req.timeout_ms=10000;
    req.max_response_bytes=64*1024;
    req.headers={{"Accept","application/json"},{"User-Agent","LuaDen/1.0"}};
    const auto response=client.Perform(req,stopping_);
    std::optional<std::string> tier;
    if(response.ok() && response.status_code==200){
      tier=ParseTier(std::string(response.body.begin(),response.body.end()));
    }else if(response.ok() && response.status_code==404){
      tier=std::string(); // No rating is not Borked.
    }
    {std::lock_guard<std::mutex> lock(mutex_);
     if(tier)entries_[selected]={*tier,Now()};
     else retry_after_[selected]=Now()+kRetry;
    }
    if(tier)Persist();
    for(int i=0;i<15 && !stopping_;++i)std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}
}
