#include "sls/steam_catalog_search.h"
#include "steam/backend/json.h"
#include <algorithm>
#include <cctype>
#include <limits>
#include <set>
namespace playden::sls {
namespace {
bool digit(unsigned char c) { return c>='0' && c<='9'; }
std::string EscapeUrl(std::string_view s) {
  const char* hex="0123456789ABCDEF";
  std::string out;
  for(unsigned char c:s) {
    if((c>='A'&&c<='Z') || (c>='a'&&c<='z') || digit(c) || c=='-' || c=='_' || c=='.')out.push_back(char(c));
    else if(c==' ')out.push_back('+');
    else {out.push_back('%');out.push_back(hex[c>>4]);out.push_back(hex[c&15]);}
  }
  return out;
}
void Replace(std::string* s,std::string_view from,std::string_view to){
  std::size_t at=0;
  while((at=s->find(from,at))!=std::string::npos){s->replace(at,from.size(),to);at+=to.size();}
}
std::string HtmlEntities(std::string value) {
  Replace(&value,"&amp;","&");Replace(&value,"&quot;","\"");
  Replace(&value,"&#39;","'");Replace(&value,"&lt;","<");Replace(&value,"&gt;",">");
  Replace(&value,"&trade;","™");Replace(&value,"&reg;","®");
  return value;
}
}
std::string SteamStoreSearchUrl(std::string_view q,std::uint32_t start,std::uint32_t count) {
  if(q.size()<2 || q.size()>100 || count==0 || count>100 || start>10000)return {};
  for(unsigned char c:q)if(c<32 || c==127)return {};
  return "https://store.steampowered.com/search/results/?term="+EscapeUrl(q)+
    "&start="+std::to_string(start)+"&count="+std::to_string(count)+
    "&infinite=1&cc=US&l=english";
}
bool ParseSteamStoreSearchResponse(std::string_view body,SteamCatalogPage* output) {
  if(!output || body.size()>512*1024)return false;
  auto root=steam::json::Parse(body);
  if(!root || !root->is_object())return false;
  const auto* html=root->Find("results_html");
  if(!html || !html->is_string())return false;
  SteamCatalogPage page;
  if(const auto* total=root->Find("total_count")){
    const auto n=total->AsUint64();
    if(n)page.total=static_cast<std::uint32_t>(std::min<std::uint64_t>(*n,1000000));
  }
  const std::string_view text=html->text();
  std::set<std::uint32_t> seen;
  std::size_t pos=0;
  while(page.titles.size()<100 && pos<text.size()){
    const auto a=text.find("data-ds-appid=\"",pos);
    if(a==text.npos)break;
    const auto begin=a+15;
    const auto end=text.find('"',begin);
    if(end==text.npos)break;
    std::uint64_t id=0;
    bool valid=end>begin && end-begin<=10;
    for(auto i=begin;valid && i<end;++i){
      if(!digit(static_cast<unsigned char>(text[i])))valid=false;
      else id=id*10+(text[i]-'0');
    }
    pos=end+1;
    if(!valid || id==0 || id>std::numeric_limits<std::uint32_t>::max())continue;
    const auto title_start=text.find("class=\"title\">",end);
    const auto next_id=text.find("data-ds-appid=\"",end);
    if(title_start==text.npos || (next_id!=text.npos && next_id<title_start))continue;
    const auto name_start=title_start+14;
    const auto name_end=text.find('<',name_start);
    if(name_end==text.npos || name_end-name_start>512)continue;
    std::string name=HtmlEntities(std::string(text.substr(name_start,name_end-name_start)));
    if(name.empty() || !seen.insert(static_cast<std::uint32_t>(id)).second)continue;
    page.titles.push_back({static_cast<std::uint32_t>(id),std::move(name)});
    pos=name_end+1;
  }
  *output=std::move(page);
  return true;
}
}
