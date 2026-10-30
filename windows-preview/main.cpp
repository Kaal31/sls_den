// Windows-only SLS Den preview shell. No Steam accounts, network or game launches.
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <commctrl.h>
#include "sls/source_list.h"
#include <fstream>
#include <string>
#include <vector>
using playden::sls::Source;
namespace {
HWND listbox{}, url{}, stateLabel{};
HFONT uiFont{};
std::vector<Source> sources;
std::wstring W(const std::string& value) {
  if(value.empty()) return {};
  int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),(int)value.size(),nullptr,0);
  if(!n) return {};
  std::wstring out(n,L'\0');
  MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),(int)value.size(),out.data(),n);
  return out;
}
std::string U(const std::wstring& value) {
  if(value.empty()) return {};
  int n=WideCharToMultiByte(CP_UTF8,0,value.data(),(int)value.size(),nullptr,0,nullptr,nullptr);
  std::string out(n,'\0');
  WideCharToMultiByte(CP_UTF8,0,value.data(),(int)value.size(),out.data(),n,nullptr,nullptr);
  return out;
}
std::string Filename() {
  wchar_t tmp[MAX_PATH]{};
  if(!GetTempPathW(MAX_PATH,tmp)) return "sls-den-preview.conf";
  return U(std::wstring(tmp) + L"sls-den-preview.conf");
}
void Save() {
  auto serialized=playden::sls::SerializeSourceList(sources);
  std::ofstream file(Filename(),std::ios::binary|std::ios::trunc);
  file<<serialized;
}
void Refresh() {
  SendMessageW(listbox,LB_RESETCONTENT,0,0);
  for(const auto& p:sources) {
    std::wstring line=(p.enabled?L"[ON]  ":L"[OFF] ")+W(p.display_name)+L"  |  "+W(p.url_template);
    SendMessageW(listbox,LB_ADDSTRING,0,(LPARAM)line.c_str());
  }
  std::wstring message=L"SLS Den Windows preview  |  "+std::to_wstring(sources.size())+L"/16 sources  |  Offline mock mode";
  SetWindowTextW(stateLabel,message.c_str());
}
int Selection(){return (int)SendMessageW(listbox,LB_GETCURSEL,0,0);}
HWND Control(HWND window,LPCWSTR name,LPCWSTR text,DWORD style,int x,int y,int w,int h,int id) {
  HWND child = CreateWindowExW(0,name,text,WS_CHILD|WS_VISIBLE|style,x,y,w,h,window,(HMENU)(INT_PTR)id,GetModuleHandleW(nullptr),nullptr);
  if (child && uiFont) SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont), TRUE);
  return child;
}
LRESULT CALLBACK Proc(HWND window,UINT msg,WPARAM wp,LPARAM lp) {
  if(msg==WM_CREATE) {
    NONCLIENTMETRICSW metrics{};
    metrics.cbSize = sizeof(metrics);
    if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0)) {
      uiFont = CreateFontIndirectW(&metrics.lfMessageFont);
    }
    if (!uiFont) uiFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    Control(window,L"STATIC",L"SLS DEN  /  WINDOWS PREVIEW",0,20,18,680,35,0);
    stateLabel=Control(window,L"STATIC",L"Offline mock mode",0,20,58,760,25,0);
    listbox=Control(window,L"LISTBOX",L"",LBS_NOTIFY|WS_BORDER|WS_VSCROLL,20,100,750,290,10);
    Control(window,L"STATIC",L"HTTPS URL template (must include {appid}):",0,20,410,700,23,0);
    url=Control(window,L"EDIT",L"https://example.org/manifests/{appid}.lua",WS_BORDER|ES_AUTOHSCROLL,20,436,750,29,11);
    Control(window,L"BUTTON",L"Add source",0,20,486,160,34,101);
    Control(window,L"BUTTON",L"Enable / disable",0,195,486,160,34,102);
    Control(window,L"BUTTON",L"Remove",0,370,486,130,34,103);
    Control(window,L"BUTTON",L"Move up",0,515,486,120,34,104);
    Control(window,L"BUTTON",L"Move down",0,650,486,120,34,105);
    Control(window,L"STATIC",L"Simulation only: no PS5 connection, downloads, credentials or Wine.",0,20,543,750,25,0);
    std::ifstream file(Filename(),std::ios::binary);
    std::string saved((std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>());
    playden::sls::ParseSourceList(saved,&sources);
    Refresh();
    return 0;
  }
  if(msg==WM_COMMAND) {
    int id=LOWORD(wp),sel=Selection();
    if(id==101) {
      wchar_t buffer[2200]{};
      GetWindowTextW(url,buffer,2200);
      if(sources.size()>=16) {MessageBoxW(window,L"Maximum 16 sources.",L"SLS Den",MB_OK);return 0;}
      size_t num=1;
      for(;;++num) {
        std::string key="custom-"+std::to_string(num);
        bool used=false;
        for(auto& p:sources) if(p.id==key) used=true;
        if(!used) {sources.push_back({key,"Custom "+std::to_string(num),U(buffer),true,false});break;}
      }
      if(!playden::sls::ValidateSource(sources.back())) {
        sources.pop_back();MessageBoxW(window,L"Enter a valid HTTPS URL with exactly one {appid}.",L"Invalid source",MB_OK|MB_ICONWARNING);
      }
    } else if(sel>=0 && (size_t)sel<sources.size()) {
      if(id==102) sources[sel].enabled=!sources[sel].enabled;
      if(id==103) sources.erase(sources.begin()+sel);
      if(id==104 && sel>0) std::swap(sources[sel],sources[sel-1]);
      if(id==105 && (size_t)sel+1<sources.size()) std::swap(sources[sel],sources[sel+1]);
    }
    if(id>=101 && id<=105){Save();Refresh();return 0;}
  }
  if(msg==WM_DESTROY){
    if (uiFont && uiFont != GetStockObject(DEFAULT_GUI_FONT)) DeleteObject(uiFont);
    PostQuitMessage(0);return 0;
  }
  return DefWindowProcW(window,msg,wp,lp);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR args,int show) {
  if(std::wstring(args).find(L"--self-test")!=std::wstring::npos) {
    std::vector<Source> result;
    auto valid=playden::sls::SerializeSourceList({{"demo","Demo","https://example.org/{appid}",true,false}});
    return playden::sls::ParseSourceList(valid,&result)&&result.size()==1&&
      !playden::sls::ParseSourceList("bad",&result)?0:1;
  }
  WNDCLASSW wc{};wc.lpfnWndProc=Proc;wc.hInstance=instance;wc.lpszClassName=L"SLSDenPreview";
  wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);wc.hCursor=LoadCursor(nullptr,IDC_ARROW);
  RegisterClassW(&wc);
  HWND window=CreateWindowW(wc.lpszClassName,L"SLS Den - Windows Preview",
    WS_OVERLAPPEDWINDOW|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,815,640,nullptr,nullptr,instance,nullptr);
  ShowWindow(window,show);
  MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
  return (int)msg.wParam;
}
