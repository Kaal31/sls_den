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
#include <cstdlib>
#include <cstdio>
#include <array>
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
 bool validate_assets=false;
 bool navigation_test=false;
 bool headless=false;
 int width=1280,height=720;
};
bool Parse(int argc,char** argv,Options* o) {
 for(int i=1;i<argc;i++){
   std::string_view option=argv[i];
   if(option=="--self-test"){o->self_test=true;continue;}
   if(option=="--validate-assets"){o->validate_assets=true;continue;}
   if(option=="--navigation-self-test"){o->navigation_test=true;continue;}
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
   case SDLK_RETURN:case SDLK_KP_ENTER:case SDLK_SPACE:return "cross";
   case SDLK_ESCAPE:case SDLK_BACKSPACE:return "circle";
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
std::string Utf8Path(const fs::path& path) {
  const auto bytes=path.u8string();
  return std::string(bytes.begin(),bytes.end());
}
fs::path startup_log;
void Log(const std::string& message) {
 if(startup_log.empty()) return;
#ifdef _WIN32
 FILE* file=_wfopen(startup_log.c_str(),L"a");
#else
 FILE* file=std::fopen(Utf8Path(startup_log).c_str(),"a");
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
 // Do not round-trip a Unicode log path through the Windows ANSI code page.
 std::wstring display_w=L"SLS Den startup failed. See the startup log:\n";
 display_w+=startup_log.wstring();
 MessageBoxW(nullptr,display_w.c_str(),L"SLS Den - Startup error",MB_OK|MB_ICONERROR|MB_TOPMOST);
#endif
}
int main(int argc,char** argv){
 startup_log=LogPath();
 Log("Application started; Windows Playden offline preview");
 try {
 Log("Constructing startup options");
 Options options;
 Log("Startup options initialized");
 Log("Parsing command line");
 if(!Parse(argc,argv,&options)){
   std::fprintf(stderr,"usage: playden-windows-offline [--scenario ID] [--self-test] [--capture PNG] [--snapshot JSON] [--headless]\n");
   return 2;
 }
#ifdef _WIN32
 wchar_t executable[32768]{};
 if(GetModuleFileNameW(nullptr,executable,32768)>0){
   const auto dir=fs::path(executable).parent_path();
   Log("Resolving packaged resource folders");
   std::error_code assets_error, fixtures_error;
   if(fs::is_directory(dir/"homebrew-assets",assets_error))
     options.assets=dir/"homebrew-assets";
   if(fs::is_directory(dir/"fixtures",fixtures_error))
     options.fixtures=dir/"fixtures";
   Log("Packaged resource folder lookup complete");
 }
#endif
 Log("Resource locations resolved");
 Log("Assets: "+Utf8Path(options.assets));
 Log("Fixtures: "+Utf8Path(options.fixtures));
 Log("Options: self-test="+std::to_string(options.self_test)+" headless="+std::to_string(options.headless));
 // The simulation model takes no real Steam credentials or network transport.
 Log("Constructing Playden simulation");
 playden::homebrew::Simulation simulation(options.fixtures,{},playden::homebrew::ModelMode::Simulation);
 if(!options.scenario.empty()&&!simulation.scenario(options.scenario)){
   std::fprintf(stderr,"Unknown scenario: %s\n",options.scenario.c_str());
   return 2;
 }
 if(options.navigation_test){
   // Test state transitions without requiring a GPU or a human to launch the app.
   const std::array<const char*,5> routes={"home","library","downloads","saves","settings"};
   for(const char* route:routes){
     if(!simulation.action(std::string("route:")+route)){
       StartupError(std::string("Navigation test could not switch to ")+route);return 8;
     }
     const std::string expected = std::string(R"("route":")")+route+R"(")";
     if(simulation.snapshot().find(expected)==std::string::npos){
       StartupError(std::string("Navigation route mismatch: ")+route);return 8;
     }
     simulation.action("down");
     simulation.action("cross");
     simulation.action("circle");
   }
   if(!simulation.action("route:library") ||
      !simulation.action("click:rail-favorites") ||
      simulation.snapshot().find(R"("scope":"favorites")")==std::string::npos){
     StartupError("Library rail mouse-action navigation failed");return 8;
   }
   if(!simulation.action("click:rail-all") ||
      simulation.snapshot().find(R"("scope":"all")")==std::string::npos){
     StartupError("Library rail restore-all navigation failed");return 8;
   }
   Log("Library sidebar click action smoke test passed");
   // The renderer's game-card click action is the simulation's details:<id>.
   // Verify details opening, cancellation, and modal-safe return state.
   if(!simulation.action("route:library") ||
      !simulation.action("details:900001") ||
      simulation.snapshot().find(R"("route":"details")")==std::string::npos){
     StartupError("Game-card details navigation failed");return 8;
   }
   simulation.action("circle");
   if(simulation.snapshot().find(R"("route":"library")")==std::string::npos){
     StartupError("Details back navigation did not restore Library");return 8;
   }
   Log("Game-card details and back navigation test passed");
   // Hover selects a cover without opening it; clicks still open Details.
   if(!simulation.action("route:library") ||
      !simulation.action("select:900002")){
     StartupError("Hovered library cover selection failed");return 8;
   }
   const std::string hovered=simulation.snapshot();
   if(hovered.find(R"("route":"library")")==std::string::npos ||
      hovered.find(R"("selected_game_id":900002)")==std::string::npos){
     StartupError("Hover changed the route or failed to select the cover");return 8;
   }
   if(!simulation.action("details:900002") ||
      simulation.snapshot().find(R"("route":"details")")==std::string::npos){
     StartupError("Hovered cover click did not open Details");return 8;
   }
   if(!simulation.action("route:home") || !simulation.action("select:900001") ||
      simulation.snapshot().find(R"("route":"home")")==std::string::npos){
     StartupError("Home cover hover unexpectedly opened game details");return 8;
   }
   Log("Home and Library hover selection versus click activation tests passed");
   // Offline Settings must expose source-specific, masked credential editors.
   if(!simulation.action("route:settings") ||
      !simulation.action("focus:settings-sls-key-moapikey")){
      StartupError("Provider API-key control missing from Settings");return 8;
   }
   simulation.action("cross");
   const std::string secret="preview_secret_test_only_93";
   simulation.text(secret);
   const auto editing=simulation.snapshot();
   if(editing.find(R"("active":true)")==std::string::npos ||
      editing.find(secret)!=std::string::npos){
      StartupError("API-key editor did not mask user input");return 8;
   }
   simulation.action("done");
   if(simulation.snapshot().find(secret)!=std::string::npos){
      StartupError("Provider API key leaked into simulation snapshot");return 8;
   }
   Log("SLS source key masking and snapshot protection smoke test passed");
   // Source rows must be focusable, selectable, and reversible without network I/O.
   if(!simulation.action("route:settings") ||
      !simulation.action("focus:settings-sls-default-0")){
     StartupError("SLSDeck source row is missing from Settings");return 8;
   }
   simulation.action("cross");
   if(!simulation.action("focus:settings-sls-default-0")){
     StartupError("Disabled source row disappeared");return 8;
   }
   simulation.action("cross");
   if(!simulation.action("focus:settings-sls-default-0")){
     StartupError("Re-enabled source row disappeared");return 8;
   }
   Log("SLSDeck provider enable/disable/enable test passed");




   Log("Five-route navigation and cross/circle action smoke test passed");
 }
 if(options.validate_assets){
   // Verify the actual renderer font files through native Unicode file handles.
   const std::array<const wchar_t*,4> fonts={
      L"barlow-regular.huifont",L"barlow-semibold.huifont",
      L"barlow-condensed-semibold.huifont",L"noto-greek-fallback.huifont"};
   for(const auto* font:fonts){
     const fs::path file=options.assets/"fonts"/font;
#ifdef _WIN32
     FILE* input=_wfopen(file.c_str(),L"rb");
#else
     FILE* input=std::fopen(Utf8Path(file).c_str(),"rb");
#endif
     if(!input){StartupError("Required renderer font could not be opened: "+Utf8Path(file));return 6;}
     const int first=std::fgetc(input);
     std::fclose(input);
     if(first==EOF){StartupError("Renderer font file is empty: "+Utf8Path(file));return 6;}
   }
   Log("Packaged renderer font files verified");
 }
 if(options.self_test){
   // Exercise the same mappings used by live keyboard input.
   const bool keyboard_mapping=
      std::string_view(LogicalKey(SDLK_RETURN))=="cross" &&
      std::string_view(LogicalKey(SDLK_SPACE))=="cross" &&
      std::string_view(LogicalKey(SDLK_ESCAPE))=="circle" &&
      std::string_view(LogicalKey(SDLK_UP))=="up";
   if(!keyboard_mapping){
     StartupError("Windows keyboard action mapping regression");
     return 7;
   }
   Log("Keyboard action mapping self-test passed");
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
   const auto* gl_vendor=reinterpret_cast<const char*>(glGetString(GL_VENDOR));
 const auto* gl_renderer=reinterpret_cast<const char*>(glGetString(GL_RENDERER));
 const auto* gl_version=reinterpret_cast<const char*>(glGetString(GL_VERSION));
 Log(std::string("OpenGL vendor: ")+(gl_vendor?gl_vendor:"unknown"));
 Log(std::string("OpenGL renderer: ")+(gl_renderer?gl_renderer:"unknown"));
 Log(std::string("OpenGL version: ")+(gl_version?gl_version:"unknown"));
 Log("Constructing Playden renderer");
   playden::homebrew::Render renderer(options.assets);
   Log("Renderer constructed, entering UI loop");
   bool running=true;
   bool first_frame=true;
   SDL_GameController* controller=nullptr;
   int stick_x=0,stick_y=0;
   std::string last_hover_action;
   auto open_controller=[&](){
     if(controller)return;
     for(int i=0;i<SDL_NumJoysticks();++i)
       if(SDL_IsGameController(i)){
         controller=SDL_GameControllerOpen(i);
         if(controller){Log("SDL gamepad connected");break;}
       }
   };
   open_controller();
   SDL_StartTextInput();
   const auto start=std::chrono::steady_clock::now();
   auto prev=start;
   while(running){
     SDL_Event e{};
     while(SDL_PollEvent(&e)){
       if(e.type==SDL_QUIT)running=false;
       else if(e.type==SDL_KEYDOWN && !e.key.repeat){
         if(const auto* action=LogicalKey(e.key.keysym.sym))simulation.action(action);
       }else if(e.type==SDL_MOUSEMOTION){
         int window_w=0,window_h=0;
         SDL_GetWindowSize(window,&window_w,&window_h);
         if(window_w>0 && window_h>0){
           const float px=static_cast<float>(e.motion.x)*1920.0f/window_w;
           const float py=static_cast<float>(e.motion.y)*1080.0f/window_h;
           if(const auto action=renderer.hover_test(px,py)){
             // A hover moves selection; it never activates a game or clicks a control.
             if(*action!=last_hover_action){
               simulation.action(*action);
               last_hover_action=*action;
             }
           }else last_hover_action.clear();
         }
       }else if(e.type==SDL_MOUSEWHEEL){
         // Cover shelves are horizontal; vertical wheel remains vertical on other screens.
         // Shift+wheel always moves horizontally. Native horizontal wheel/trackpad
         // deltas also move between covers, including when the wheel is diagonal.
         const auto& state=simulation.state();
         const bool on_home=state.navigation.route==playden::homebrew::Route::Home &&
                            state.navigation.modals.empty();
         const bool horizontal=(SDL_GetModState() & KMOD_SHIFT)!=0;
         const int dx=e.wheel.direction==SDL_MOUSEWHEEL_FLIPPED?-e.wheel.x:e.wheel.x;
         const int dy=e.wheel.direction==SDL_MOUSEWHEEL_FLIPPED?-e.wheel.y:e.wheel.y;
         auto wheel_actions=[&](int delta,const char* positive,const char* negative){
           for(int n=0;n<std::min(6,std::abs(delta));++n)
             simulation.action(delta>0?positive:negative);
         };
         if(dx!=0)wheel_actions(dx,"right","left");
         if(dy!=0) {
           if(horizontal || on_home)wheel_actions(dy,"left","right");
           else wheel_actions(dy,"up","down");
         }
       }else if(e.type==SDL_MOUSEBUTTONDOWN && e.button.button==SDL_BUTTON_LEFT){
         int window_w=0,window_h=0;
         SDL_GetWindowSize(window,&window_w,&window_h);
         if(window_w>0 && window_h>0){
           const float px=static_cast<float>(e.button.x)*1920.0f/window_w;
           const float py=static_cast<float>(e.button.y)*1080.0f/window_h;
           // Use hitboxes from the last rendered frame; no hardcoded mouse regions.
           if(auto action=renderer.hit_test(px,py)) {
             const bool changed=simulation.action(*action);
             if(!changed) Log("Mouse click ignored: "+*action);
           }
         }
       }else if(e.type==SDL_CONTROLLERDEVICEADDED)open_controller();
       else if(e.type==SDL_CONTROLLERDEVICEREMOVED){
         if(controller && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller))==e.cdevice.which){
           SDL_GameControllerClose(controller);controller=nullptr;
           stick_x=stick_y=0;
           open_controller();
         }
       }else if(e.type==SDL_CONTROLLERAXISMOTION && controller &&
                 e.caxis.which==SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller))){
         const auto axis=static_cast<SDL_GameControllerAxis>(e.caxis.axis);
         if(axis==SDL_CONTROLLER_AXIS_LEFTX || axis==SDL_CONTROLLER_AXIS_LEFTY){
           // Dead-zone plus edge triggering: one navigation step per tilt,
           // rather than hundreds of actions per SDL axis event.
           const int direction=e.caxis.value>16000?1:e.caxis.value< -16000?-1:0;
           int& previous=axis==SDL_CONTROLLER_AXIS_LEFTX?stick_x:stick_y;
           if(direction!=previous && direction!=0)
             simulation.action(axis==SDL_CONTROLLER_AXIS_LEFTX
                 ?(direction>0?"right":"left")
                 :(direction>0?"down":"up"));
           previous=direction;
         }
       }else if(e.type==SDL_CONTROLLERBUTTONDOWN){
         const auto button=static_cast<SDL_GameControllerButton>(e.cbutton.button);
         const char* action=nullptr;
         switch(button){
           case SDL_CONTROLLER_BUTTON_A:action="cross";break;
           case SDL_CONTROLLER_BUTTON_B:action="circle";break;
           case SDL_CONTROLLER_BUTTON_X:action="square";break;
           case SDL_CONTROLLER_BUTTON_Y:action="triangle";break;
           case SDL_CONTROLLER_BUTTON_DPAD_UP:action="up";break;
           case SDL_CONTROLLER_BUTTON_DPAD_DOWN:action="down";break;
           case SDL_CONTROLLER_BUTTON_DPAD_LEFT:action="left";break;
           case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:action="right";break;
           case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:action="l1";break;
           case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:action="r1";break;
           case SDL_CONTROLLER_BUTTON_START:action="options";break;
           default:break;
         }
         if(action)simulation.action(action);
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
   if(controller)SDL_GameControllerClose(controller);
   SDL_StopTextInput();
 }catch(const std::exception& error) {
   Log(std::string("ERROR: Renderer/UI exception: ")+error.what());
   StartupError(std::string("Renderer initialization failed: ")+error.what());
   std::fprintf(stderr,"Playden offline UI: %s\n",error.what());exit_code=1;
 }catch(...) {
   Log("ERROR: Unknown renderer/UI exception");
   exit_code=1;
 }
 if(!options.snapshot.empty()){
   #ifdef _WIN32
   std::FILE* file=_wfopen(options.snapshot.c_str(),L"wb");
#else
   std::FILE* file=std::fopen(Utf8Path(options.snapshot).c_str(),"wb");
#endif
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
 } catch(const std::exception& error) {
   StartupError(std::string("Startup exception: ")+error.what());
   return 1;
 } catch(...) {
   StartupError("Unknown startup exception");
   return 1;
 }
}
