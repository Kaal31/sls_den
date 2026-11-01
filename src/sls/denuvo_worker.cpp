#include "sls/denuvo_worker.h"
#include "steam/backend/http_client.h"
#include "steam/backend/json.h"
#include <chrono>
#include <ctime>
#include <fstream>
#include <limits>
#include <utility>
namespace playden::sls {
namespace {
constexpr std::int64_t kDay=86400;
constexpr std::int64_t kRetry=300;
const std::uint32_t kSeed[]={368260,491540,668580,700600,801800,990080,1004640,1029690,1088850,1113000,1134570,1142710,1222690,1225560,1233570,1237320,1237980,1238840,1238880,1239520,1244460,1252330,1273400,1285190,1295660,1307710,1364780,1382330,1413480,1462570,1487210,1490890,1506830,1519350,1569040,1570010,1602010,1611910,1677350,1687950,1692250,1693980,1708520,1760250,1761390,1777620,1785650,1794960,1795190,1805480,1809700,1810820,1811260,1844380,1846380,1849250,1868170,1868180,1875830,1904540,1938010,1941540,1971870,2022670,2051010,2054970,2055290,2058180,2058190,2108330,2140330,2161700,2169200,2185060,2195250,2215200,2215260,2244210,2246340,2252570,2254740,2287220,2288350,2338770,2358720,2361770,2362060,2383760,2385530,2424110,2486820,2488620,2495100,2499860,2513280,2556990,2582560,2591280,2604480,2624870,2669320,2679460,2680010,2688950,2750080,2833580,2840770,2852190,2878980,2928600,2929170,2958130,3035570,3059520,3159330,3230400,3274580,3357650,3405690,3472040,3483510,3500390,3551340,3717070,3751950,3764200,3768760,3936610,4128400,4148530};
std::int64_t Now(){return static_cast<std::int64_t>(std::time(nullptr));}
std::optional<bool> ParseNotice(std::string_view body,std::uint32_t app) {
  auto root=steam::json::Parse(body);
  if(!root || !root->is_object())return std::nullopt;
  auto* game=root->Find(std::to_string(app));
  if(!game || !game->is_object())return std::nullopt;
  auto* success=game->Find("success");
  if(!success || !success->is_bool() || !success->boolean())return std::nullopt;
  auto* data=game->Find("data");
  if(!data || !data->is_object())return std::nullopt;
  auto* notice=data->Find("drm_notice");
  if(!notice)return false;
  if(!notice->is_string())return std::nullopt;
  auto text=notice->text();
  for(char& c:text)if(c>='A' && c<='Z')c=static_cast<char>(c+('a'-'A'));
  return text.find("denuvo")!=std::string::npos;
}
}
DenuvoWorker::DenuvoWorker(std::string ca,std::filesystem::path cache)
  :ca_(std::move(ca)),cache_(std::move(cache)),seed_(std::begin(kSeed),std::end(kSeed)){
  std::ifstream input(cache_);
  std::uint64_t id=0;std::int64_t when=0;int present=0;
  while(input>>id>>when>>present)
    if(id>0 && id<=std::numeric_limits<std::uint32_t>::max() && when>0 && (present==0 || present==1))
      entries_[static_cast<std::uint32_t>(id)]={present==1,when};
  thread_=std::thread([this]{Run();});
}
DenuvoWorker::~DenuvoWorker(){stopping_=true;if(thread_.joinable())thread_.join();}
void DenuvoWorker::Observe(const std::vector<std::uint32_t>& ids){
  std::lock_guard<std::mutex> lock(mutex_);
  for(auto id:ids)if(id>0)observed_.insert(id);
}
std::set<std::uint32_t> DenuvoWorker::Known() const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto result=seed_;
  for(const auto& item:entries_) {
    if(item.second.present)result.insert(item.first);
    else result.erase(item.first); // Fresh publisher response supersedes an old seed.
  }
  return result;
}
void DenuvoWorker::Persist(){
  std::map<std::uint32_t,Entry> entries;
  {std::lock_guard<std::mutex> lock(mutex_);entries=entries_;}
  std::error_code ec;
  std::filesystem::create_directories(cache_.parent_path(),ec);
  if(ec)return;
  auto temp=cache_;temp+=".tmp";
  {std::ofstream file(temp,std::ios::trunc);if(!file)return;
   for(const auto& [id,entry]:entries)file<<id<<" "<<entry.checked<<" "<<int(entry.present)<<"\n";
   file.flush();if(!file)return;}
  std::filesystem::rename(temp,cache_,ec);
  if(ec){std::filesystem::remove(cache_,ec);ec.clear();std::filesystem::rename(temp,cache_,ec);}
}
void DenuvoWorker::Run(){
  steam::HttpClient client(ca_);
  while(!stopping_) {
    std::uint32_t selected=0;
    const auto now=Now();
    {std::lock_guard<std::mutex> lock(mutex_);
     for(auto id:observed_) {
       auto it=entries_.find(id);
       if((it==entries_.end() || now-it->second.checked>=kDay) && now>=retry_after_[id]) {selected=id;break;}
     }}
    if(!selected){std::this_thread::sleep_for(std::chrono::milliseconds(200));continue;}
    steam::HttpRequest req;
    req.url="https://store.steampowered.com/api/appdetails?appids="+std::to_string(selected)+"&filters=basic,drm_notice&cc=US&l=english";
    req.timeout_ms=10000;
    req.max_response_bytes=128*1024;
    req.headers={{"Accept","application/json"},{"User-Agent","LuaDen/1.0"}};
    auto response=client.Perform(req,stopping_);
    std::optional<bool> status;
    if(response.ok() && response.status_code==200) {
      const std::string body(response.body.begin(),response.body.end());
      status=ParseNotice(body,selected);
    }
    {std::lock_guard<std::mutex> lock(mutex_);
      if(status)entries_[selected]={*status,Now()};
      else retry_after_[selected]=Now()+kRetry;
    }
    if(status)Persist(); // Never erase cached values due to offline or HTTP failure.
    for(int i=0;i<15 && !stopping_;++i)std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}
}
