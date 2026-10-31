// SLS Den Windows: direct offline SDL/OpenGL host for Playden's real model/renderer.
// This binary never authenticates with Steam, downloads, or installs software.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "homebrew/render.hpp"
#include "homebrew/simulation.hpp"
#include "gfx/gl.hpp"
#include "gfx/gl_program.hpp"
#define SDL_MAIN_HANDLED
#include <SDL.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <exception>
#include <filesystem>
#include <string>
#include <string_view>
#include <thread>

namespace {
namespace fs=std::filesystem;
struct Options {
 fs::path assets=PLAYDEN_HOMEBREW_ASSETS_DIR;
 fs::path fixtures=PLAYDEN_HOMEBREW_FIXTURES_DIR;
 fs::path capture;
 fs::path snapshot;
 std::string scenario;
 bool self_test=false;
 bool headless=false;
 int width=1280,height=720;
};
bool Parse(int argc,char** argv,Options* o) {
 for(int i=1;i<argc;i++){
   std::string_view option=argv[i];
   if(option=="--self-test"){o->self_test=true;continue;}
   if(option=="--headless"){o->headless=true;continue;}
   if(i+1>=argc)return false;
   std::string value=argv[++i];
   if(option=="--assets")o->assets=fs::u8path(value);
   else if(option=="--fixtures")o->fixtures=fs::u8path(value);
   else if(option=="--scenario")o->scenario=value;
   else if(option=="--capture")o->capture=fs::u8path(value);
   else if(option=="--snapshot")o->snapshot=fs::u8path(value);
   else return false;
 }
 if(!o->capture.empty())o->headless=true;
 return true;
}
const char* LogicalKey(SDL_Keycode key){
 switch(key){
   case SDLK_UP:case SDLK_w:return "up";
   case SDLK_DOWN:case SDLK_s:return "down";
   case SDLK_LEFT:case SDLK_a:return "left";
   case SDLK_RIGHT:case SDLK_d:return "right";
   case SDLK_RETURN:case SDLK_KP_ENTER:case SDLK_SPACE:return "confirm";
   case SDLK_ESCAPE:case SDLK_BACKSPACE:return "back";
   case SDLK_TAB:return "options";
   default:return nullptr;
 }
}
}
fs::path LogPath() {
#ifdef _WIN32
 wchar_t executable[32768]{};
 if(GetModuleFileNameW(nullptr,executable,32768)>0) {
   const auto location=fs::path(executable).parent_path()/"sls-den-startup.log";
   if(FILE* test=_wfopen(location.c_str(),L"a")) {std::fclose(test); return location;}
 }
 wchar_t temp[MAX_PATH]{};
 if(GetTempPathW(MAX_PATH,temp)>0) return fs::path(temp)/"sls-den-startup.log";
#endif
 return "sls-den-startup.log";
}
fs::path startup_log;
void Log(const std::string& message) {
 if(startup_log.empty()) return;
#ifdef _WIN32
 FILE* file=_wfopen(startup_log.c_str(),L"a");
#else
 FILE* file=std::fopen(startup_log.string().c_str(),"a");
#endif
 if(!file) return;
 const std::time_t now=std::time(nullptr);
 std::fprintf(file,"[%lld] %s\\n",static_cast<long long>(now),message.c_str());
 std::fclose(file);
}
void StartupError(const std::string& message) {
 Log("ERROR: "+message);
 std::fprintf(stderr,"SLS Den Windows: %s\\n",message.c_str());
#ifdef _WIN32
 std::string display=message+"\\n\\nLog: "+startup_log.string();
 MessageBoxA(nullptr,display.c_str(),"SLS Den - Startup error",MB_OK|MB_ICONERROR|MB_TOPMOST);
#endif
}
int main(int argc,char** argv){
 startup_log=LogPath();
 Log("Application started; Windows Playden offline preview");
 Options options;
 if(!Parse(argc,argv,&options)){
   std::fprintf(stderr,"usage: playden-windows-offline [--scenario ID] [--self-test] [--capture PNG] [--snapshot JSON] [--headless]\n");
   return 2;
 }
#ifdef _WIN32
 wchar_t executable[32768]{};
 if(GetModuleFileNameW(nullptr,executable,32768)>0){
   const auto dir=fs::path(executable).parent_path();
   if(!fs::exists(options.assets)&&fs::exists(dir/"homebrew-assets"))
     options.assets=dir/"homebrew-assets";
   if(!fs::exists(options.fixtures)&&fs::exists(dir/"fixtures"))
     options.fixtures=dir/"fixtures";
 }
#endif
 Log("Assets: "+options.assets.string());
 Log("Fixtures: "+options.fixtures.string());
 Log("Options: self-test="+std::to_string(options.self_test)+" headless="+std::to_string(options.headless));
 // The simulation model takes no real Steam credentials or network transport.
 Log("Constructing Playden simulation");
 playden::homebrew::Simulation simulation(options.fixtures,{},playden::homebrew::ModelMode::Simulation);
 if(!options.scenario.empty()&&!simulation.scenario(options.scenario)){
   std::fprintf(stderr,"Unknown scenario: %s\n",options.scenario.c_str());
   return 2;
 }
 if(options.self_test){
   auto data=simulation.snapshot();
   if(data.empty()){StartupError("Simulation snapshot was empty");return 3;}
   Log("Simulation self-test passed");
   std::puts("PASS: native Windows Playden offline simulation initialized");
   return 0;
 }
 Log("Initializing SDL video and controllers");
 SDL_SetMainReady();
 if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER)!=0){
   std::fprintf(stderr,"SDL init failed: %s\n",SDL_GetError());return 1;
 }
 SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,4);
 SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
 SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
 SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS,SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
 SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
 Log("Creating OpenGL 4.1 window and context");
 SDL_Window* window=SDL_CreateWindow("SLS Den - Playden Offline Preview",
   SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,options.width,options.height,
   SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE|(options.headless?SDL_WINDOW_HIDDEN:SDL_WINDOW_SHOWN));
 SDL_GLContext context=window?SDL_GL_CreateContext(window):nullptr;
 if(!window||!context){
   std::fprintf(stderr,"OpenGL 4.1 unavailable: %s\n",SDL_GetError());
   if(context)SDL_GL_DeleteContext(context);
   if(window)SDL_DestroyWindow(window);
   SDL_Quit();return 1;
 }
#ifdef _WIN32
 Log("Initializing GLEW OpenGL entry points");
 glewExperimental = GL_TRUE;
 const GLenum glew_result = glewInit();
 if (glew_result != GLEW_OK) {
   std::fprintf(stderr,"OpenGL entry-point loading failed: %s\n",glewGetErrorString(glew_result));
   SDL_GL_DeleteContext(context); SDL_DestroyWindow(window); SDL_Quit(); return 1;
 }
 glGetError(); // GLEW may leave GL_INVALID_ENUM on core-profile contexts.
#endif
 SDL_GL_SetSwapInterval(options.headless?0:1);
 hui::gfx::set_glsl_prefix("#version 410 core\n");
 int exit_code=0;
 try{
   Log("Constructing Playden renderer");
   playden::homebrew::Render renderer(options.assets);
   Log("Renderer constructed, entering UI loop");
   bool running=true;
   bool first_frame=true;
   SDL_StartTextInput();
   const auto start=std::chrono::steady_clock::now();
   auto prev=start;
   while(running){
     SDL_Event e{};
     while(SDL_PollEvent(&e)){
       if(e.type==SDL_QUIT)running=false;
       else if(e.type==SDL_KEYDOWN && !e.key.repeat){
         if(const auto* action=LogicalKey(e.key.keysym.sym))simulation.action(action);
       }else if(e.type==SDL_TEXTINPUT)simulation.text(e.text.text);
     }
     const auto now=std::chrono::steady_clock::now();
     const float dt=std::chrono::duration<float>(now-prev).count();
     prev=now;
     SDL_GL_GetDrawableSize(window,&options.width,&options.height);
     simulation.tick();
     renderer.draw(simulation,options.width,options.height,
         std::clamp(dt,0.001f,0.1f));
     SDL_GL_SwapWindow(window);
     if(first_frame){ Log("First UI frame presented"); first_frame=false; }
     if(options.headless){
       if(!options.capture.empty()&&!renderer.capture_png(options.capture,options.width,options.height))
         exit_code=4;
       running=false;
     }
     if(!options.headless)SDL_Delay(8);
   }
   SDL_StopTextInput();
 }catch(const std::exception& error) {
   std::fprintf(stderr,"Playden offline UI: %s\n",error.what());exit_code=1;
 }
 if(!options.snapshot.empty()){
   std::FILE* file=std::fopen(options.snapshot.string().c_str(),"wb");
   if(!file)exit_code=5;
   else{
     const auto data=simulation.snapshot();
     if(std::fwrite(data.data(),1,data.size(),file)!=data.size())exit_code=5;
     std::fclose(file);
   }
 }
 SDL_GL_DeleteContext(context);
 SDL_DestroyWindow(window);
 SDL_Quit();
 Log("Exiting with status "+std::to_string(exit_code));
 return exit_code;
}
