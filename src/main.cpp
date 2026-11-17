#include <windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cctype>
#include <type_traits>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <vector>

// Script Hook V public ABI. Resolved at runtime: no redistributed SDK/runtime.
using U64 = std::uint64_t;
using Handle = int;
struct Vec { float x{}, y{}, z{}; };
struct NativeVec { float x; int px; float y; int py; float z; int pz; };
namespace Hook {
 using Init = void(*)(U64); using Push = void(*)(U64); using Call = U64*(*)();
 Init init{}; Push push{}; Call call{};
 void(*wait)(DWORD){}; void(*reg)(HMODULE, void(*)()){};
 void(*unreg)(HMODULE){}; int(*vehicles)(int*, int){};
 bool load() {
  auto h = GetModuleHandleW(L"ScriptHookV.dll"); if (!h) return false;
  init = reinterpret_cast<Init>(GetProcAddress(h,"nativeInit"));
  push = reinterpret_cast<Push>(GetProcAddress(h,"nativePush64"));
  call = reinterpret_cast<Call>(GetProcAddress(h,"nativeCall"));
  wait = reinterpret_cast<decltype(wait)>(GetProcAddress(h,"scriptWait"));
  reg = reinterpret_cast<decltype(reg)>(GetProcAddress(h,"scriptRegister"));
  unreg = reinterpret_cast<decltype(unreg)>(GetProcAddress(h,"scriptUnregister"));
  vehicles = reinterpret_cast<decltype(vehicles)>(GetProcAddress(h,"worldGetAllVehicles"));
  return init && push && call && wait && reg && unreg && vehicles;
 }
 template<class T> U64 arg(T v) {
  U64 n = 0;
  if constexpr(std::is_pointer_v<T>) n = reinterpret_cast<U64>(v);
  else if constexpr(std::is_floating_point_v<T>) { float f = static_cast<float>(v); std::memcpy(&n,&f,4); }
  else n = static_cast<U64>(v);
  return n;
 }
 template<class R = void, class... A> R invoke(U64 hash, A... a) {
  init(hash); (push(arg(a)), ...); auto result = call();
  if constexpr(!std::is_void_v<R>) { R v{}; std::memcpy(&v,result,sizeof(R)); return v; }
 }
}
using Hook::invoke;
namespace N {
 constexpr U64 player=0xD80958FC74E988A6, coords=0x3FEF770D40960D5A, heading=0xE83D4F9BA2A38914,
 exists=0x7239B21A38F536BA, driver=0xBB40DD2270B65366, mission=0x0A7B270912999B3C,
 isPlayer=0x12534C348C6CB68B, speed=0xD5037BA82E12416F, dead=0x5F9532F3B5CC2551,
 request=0x963D27A58DF860AC, loaded=0x98A4EB5D89A0C952, inCd=0x35B9E0803292B641,
 isVehicle=0x19AAC8F07BFEC53E, releaseModel=0xE532F5D78798DAAB,
 createVehicle=0xAF35D0D2583051B0, createDriver=0x7DD959874C1FD534,
 drive=0xE2A2AA2F659D77A7, wander=0x480142959D337D00, brake=0xC429DCEEB339E129,
 blockEvents=0x9F8AA94D6D97DBF4, keepTask=0x971D38760FBC02EF,
 setMission=0xAD738C3085FE7E11, releaseEntity=0xB736A491E64A32CF,
 deleteEntity=0xAE3CBE5BF394C9C9, visible=0xE659E47AF827484B,
 roads=0xBF1A602B5BA52FEE, restore=0x1EE7063B80FFC77C,
 angled=0x1A5AA1208AF5DB59, restoreAngled=0x0027501B9F3B407E,
 generators=0xC12321827687FE4D, line=0x6B7256074AE34680,
 rect=0x3A618A217E5154F0, textFont=0x66E0276CC5F6B9DA,
 textScale=0x07C837F9A01C34C9, textColour=0xBE6B23FFA53FB442,
 textBegin=0x25FBB336DF1804CB, textAdd=0x6C188BE134E074AA, textEnd=0xCD015E5BB0D96A57,
 disable=0xFE99B66D079CF6BC, keyboard=0x00DC833F2568DBF6,
 keyboardState=0x0CF2B696BBF945AE, keyboardResult=0x8362B09B91893647;
}
Vec pos(int h) { auto v=invoke<NativeVec>(N::coords,h,true); return {v.x,v.y,v.z}; }
bool exists(int h) { return h && invoke<bool>(N::exists,h); }
bool playerVehicle(int car) { return car==invoke<int>(0x9A9112A0FE9A4713,invoke<int>(N::player),false); }
float distance(Vec a, Vec b) { return std::sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z)); }
U64 hashModel(std::string s) { std::uint32_t h=0; for(unsigned char c:s) { h+=static_cast<unsigned char>(std::tolower(c)); h+=h<<10; h^=h>>6; } h+=h<<3;h^=h>>11;h+=h<<15;return h; }
struct Waypoint { Vec p; float radius=7, zone=20, speed=30, stopSeconds=3; bool stop=false,direct=false; };
struct Path { int id; std::string name; bool enabled=true; std::vector<Waypoint> points; };
struct Spawner { std::string name="Spawner",model="asea"; Vec p; float heading=0, interval=8; int path=0,limit=8; bool enabled=false; ULONGLONG next=0; std::vector<int> spawned; };
struct Zone { std::string name="Traffic block"; Vec p; float heading=0,length=40,width=12,height=6; bool enabled=true,cleanup=false; };
struct Driver { int ped,car,path; size_t target=1; bool issued=false,stopping=false; ULONGLONG resume=0; };
std::vector<Path> paths; std::vector<Spawner> spawners; std::vector<Zone> zones;
std::vector<Driver> drivers;
int nextId=1; bool menu=false,preview=true,collect=true; int page=0,selectedPath=-1,selectedWp=0,selectedSpawn=-1,selectedZone=-1,cursor=0;
std::filesystem::path folder; std::string message="Ready"; ULONGLONG messageUntil=0;
void notify(std::string s) { message=std::move(s); messageUntil=GetTickCount64()+6000; }
Path* getPath(int id) { for(auto& p:paths) if(p.id==id) return &p; return nullptr; }
bool controlled(int car) { for(auto& d:drivers) if(d.car==car) return true; return false; }
bool ownSpawn(int car) { for(auto& s:spawners) for(int v:s.spawned) if(v==car) return true; return false; }
Vec corner(const Zone& z,float forward,float side,float up) {
 float a=z.heading*0.01745329252f;
 return {z.p.x-std::sin(a)*forward+std::cos(a)*side,z.p.y+std::cos(a)*forward+std::sin(a)*side,z.p.z+up};
}
bool inside(const Zone& z,Vec p) {
 float a=z.heading*0.01745329252f,dx=p.x-z.p.x,dy=p.y-z.p.y;
 return std::abs(-std::sin(a)*dx+std::cos(a)*dy)<=z.length/2 && std::abs(std::cos(a)*dx+std::sin(a)*dy)<=z.width/2 && std::abs(p.z-z.p.z)<=z.height/2;
}
void applyZone(const Zone& z,bool on) {
 auto a=corner(z,-z.length/2,0,-z.height/2),b=corner(z,z.length/2,0,z.height/2);
 if(on) invoke(N::angled,a.x,a.y,a.z,b.x,b.y,b.z,z.width,false,false,true);
 else invoke(N::restoreAngled,a.x,a.y,a.z,b.x,b.y,b.z,z.width,true);
 // Static parking generators use an axis-aligned volume; road blocking stays oriented.
}
void restoreAll() { for(auto& z:zones) if(z.enabled) applyZone(z,false); }
void refreshZones() { for(auto& z:zones) if(z.enabled) applyZone(z,true); }
void releaseDriver(Driver& d) {
 if(exists(d.ped)) { invoke(N::blockEvents,d.ped,false); if(exists(d.car)&&!invoke<bool>(N::dead,d.ped,false)&&invoke<int>(N::driver,d.car,-1,false)==d.ped)invoke(N::wander,d.ped,d.car,12.0f,786603); int h=d.ped;invoke(N::releaseEntity,&h); }
 if(exists(d.car)) { int h=d.car; invoke(N::releaseEntity,&h); }
}
void resetDrivers() { for(auto& d:drivers) releaseDriver(d); drivers.clear(); }
bool assign(int car,int pathId) {
 auto p=getPath(pathId); if(!p||p->points.size()<2||controlled(car)||!exists(car)) return false;
 int ped=invoke<int>(N::driver,car,-1,false);
 if(!exists(ped)||playerVehicle(car)||invoke<bool>(N::isPlayer,ped)||invoke<bool>(N::dead,ped,false)) return false;
 invoke(N::setMission,car,true,true);invoke(N::setMission,ped,true,true);
 invoke(N::blockEvents,ped,true);invoke(N::keepTask,ped,true);
 drivers.push_back({ped,car,pathId});return true;
}
void save() {
 std::ofstream f(folder/"TrafficDirector.tmp"); f<<"TD 1\n"<<preview<<' '<<collect<<'\n';
 f<<paths.size()<<'\n'; for(auto& p:paths) { f<<p.id<<' '<<std::quoted(p.name)<<' '<<p.enabled<<' '<<p.points.size()<<'\n'; for(auto& w:p.points) f<<std::setprecision(9)<<w.p.x<<' '<<w.p.y<<' '<<w.p.z<<' '<<w.radius<<' '<<w.zone<<' '<<w.speed<<' '<<w.stopSeconds<<' '<<w.stop<<' '<<w.direct<<'\n'; }
 f<<spawners.size()<<'\n'; for(auto& s:spawners) f<<std::quoted(s.name)<<' '<<std::quoted(s.model)<<' '<<s.p.x<<' '<<s.p.y<<' '<<s.p.z<<' '<<s.heading<<' '<<s.interval<<' '<<s.path<<' '<<s.limit<<' '<<s.enabled<<'\n';
 f<<zones.size()<<'\n'; for(auto& z:zones) f<<std::quoted(z.name)<<' '<<z.p.x<<' '<<z.p.y<<' '<<z.p.z<<' '<<z.heading<<' '<<z.length<<' '<<z.width<<' '<<z.height<<' '<<z.enabled<<' '<<z.cleanup<<'\n';
 f.close(); if(!f) {notify("Save failed");return;}
 if(!MoveFileExW((folder/L"TrafficDirector.tmp").c_str(),(folder/L"TrafficDirector.dat").c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) notify("Save failed"); else notify("Scene saved");
}
bool readScene(std::istream& f,std::vector<Path>& pp,std::vector<Spawner>& ss,std::vector<Zone>& zz,bool& pv,bool& cl) {
 std::string magic;int version;size_t n;
 if(!(f>>magic>>version)||magic!="TD"||version!=1||!(f>>pv>>cl>>n)||n>500) return false;
 for(size_t i=0;i<n;i++) { Path p;size_t count;if(!(f>>p.id>>std::quoted(p.name)>>p.enabled>>count)||p.id<=0||count>5000) return false;for(auto& old:pp) if(old.id==p.id)return false;
 for(size_t j=0;j<count;j++) {Waypoint w;if(!(f>>w.p.x>>w.p.y>>w.p.z>>w.radius>>w.zone>>w.speed>>w.stopSeconds>>w.stop>>w.direct))return false;w.radius=std::clamp(w.radius,1.f,200.f);w.zone=std::clamp(w.zone,1.f,300.f);w.speed=std::clamp(w.speed,1.f,250.f);w.stopSeconds=std::clamp(w.stopSeconds,0.f,120.f);p.points.push_back(w);}pp.push_back(p); }
 if(!(f>>n)||n>500)return false; for(size_t i=0;i<n;i++){Spawner s;if(!(f>>std::quoted(s.name)>>std::quoted(s.model)>>s.p.x>>s.p.y>>s.p.z>>s.heading>>s.interval>>s.path>>s.limit>>s.enabled))return false;s.interval=std::clamp(s.interval,1.f,600.f);s.limit=std::clamp(s.limit,1,100);ss.push_back(s);}
 if(!(f>>n)||n>500)return false;for(size_t i=0;i<n;i++){Zone z;if(!(f>>std::quoted(z.name)>>z.p.x>>z.p.y>>z.p.z>>z.heading>>z.length>>z.width>>z.height>>z.enabled>>z.cleanup))return false;z.length=std::clamp(z.length,2.f,1000.f);z.width=std::clamp(z.width,2.f,300.f);z.height=std::clamp(z.height,2.f,100.f);zz.push_back(z);}return true;
}
void load() {
 std::ifstream f(folder/"TrafficDirector.dat");if(!f){notify("No saved scene yet");return;}
 std::vector<Path> pp;std::vector<Spawner> ss;std::vector<Zone> zz;bool pv,cl;
 if(!readScene(f,pp,ss,zz,pv,cl)){notify("Invalid scene file; current scene retained");return;}
 restoreAll();resetDrivers();paths=std::move(pp);spawners=std::move(ss);zones=std::move(zz);preview=pv;collect=cl;nextId=1;
 for(auto& p:paths) nextId=std::max(nextId,p.id+1);refreshZones();page=0;cursor=0;notify("Scene loaded");
}
std::array<bool,256> lastKeys{},keys{};
bool hit(int k){return keys[k]&&!lastKeys[k];}
void poll(){lastKeys=keys;for(int i=0;i<256;i++)keys[i]=(GetAsyncKeyState(i)&0x8000)!=0;}
std::string input(std::string initial) {
 invoke(N::keyboard,0,"FMMC_KEY_TIP8","",initial.c_str(),"","","",48);
 int state;while((state=invoke<int>(N::keyboardState))==0)Hook::wait(0);
 poll();lastKeys=keys;
 if(state==1){auto s=invoke<const char*>(N::keyboardResult);if(s&&*s)return s;}return initial;
}
struct Row {std::string label;std::function<void(int)> action;};std::vector<Row> rows;
void add(std::string s,std::function<void(int)> fn){rows.push_back({std::move(s),std::move(fn)});}
void go(int p){page=p;cursor=0;}
std::string yes(bool b){return b?"ON":"OFF";}
std::string num(float f){std::ostringstream s;s<<std::fixed<<std::setprecision(1)<<f;return s.str();}
void change(float& v,int d,float step,float lo,float hi){v=std::clamp(v+(d==0?1:d)*step,lo,hi);}
void zoneEdit(std::function<void(Zone&)> edit){restoreAll();edit(zones[selectedZone]);refreshZones();}
void buildMenu() {
 rows.clear();
 if(page==0){add("Manage Paths",[](int){go(1);});add("Manage Spawners",[](int){go(4);});add("Traffic Block Zones",[](int){go(6);});add("Settings",[](int){go(8);});add("Save Scene",[](int){save();});add("Load Scene",[](int){load();});}
 if(page==1){add("Create Path (collector here)",[](int){Path p{nextId++,"Path "+std::to_string(nextId-1)};p.points.push_back({pos(invoke<int>(N::player))});paths.push_back(p);selectedPath=static_cast<int>(paths.size())-1;go(2);});for(size_t i=0;i<paths.size();i++)add(paths[i].name+" ["+std::to_string(paths[i].points.size())+"]",[i](int){selectedPath=static_cast<int>(i);go(2);});}
 if(page==2&&selectedPath>=0&&selectedPath<static_cast<int>(paths.size())){auto& p=paths[selectedPath];
 add("Rename: "+p.name,[&p](int){p.name=input(p.name);});add("Enabled: "+yes(p.enabled),[&p](int){p.enabled=!p.enabled;});
 add("Add Waypoint Here",[&p](int){p.points.push_back({pos(invoke<int>(N::player))});notify("Waypoint added");});
 add("Remove Last Waypoint",[&p](int){if(p.points.size()>1)p.points.pop_back();});
 add("Assign Nearest Driver",[&p](int){std::array<int,2048> vv{};int n=Hook::vehicles(vv.data(),static_cast<int>(vv.size()));float best=40;int car=0;Vec me=pos(invoke<int>(N::player));for(int i=0;i<n;i++){float d=distance(me,pos(vv[i]));if(d<best&&!controlled(vv[i])&&!invoke<bool>(N::mission,vv[i])){int ped=invoke<int>(N::driver,vv[i],-1,false);if(exists(ped)&&!invoke<bool>(N::isPlayer,ped)){best=d;car=vv[i];}}}notify(assign(car,p.id)?"Driver assigned":"No eligible driver within 40 m / path needs 2 points");});
 for(size_t i=0;i<p.points.size();i++)add(i==0?"Collector / Waypoint 0":"Waypoint "+std::to_string(i),[i](int){selectedWp=static_cast<int>(i);go(3);});
 add("Delete Path",[&p](int){int id=p.id;for(auto it=drivers.begin();it!=drivers.end();)if(it->path==id){releaseDriver(*it);it=drivers.erase(it);}else ++it;for(auto& s:spawners)if(s.path==id){s.enabled=false;s.path=0;}paths.erase(paths.begin()+selectedPath);go(1);});}
 if(page==3){auto& w=paths[selectedPath].points[selectedWp];add("Move Here",[&w](int){w.p=pos(invoke<int>(N::player));});
 add((selectedWp==0?"Collection Radius: ":"Arrival Radius: ")+num(w.radius)+" m",[&w](int d){change(w.radius,d,1,1,200);});
 add("Speed Zone Radius: "+num(w.zone)+" m",[&w](int d){change(w.zone,d,1,1,300);});add("Waypoint Speed: "+num(w.speed)+" km/h",[&w](int d){change(w.speed,d,5,1,250);});
 add("Stop Waypoint: "+yes(w.stop),[&w](int){w.stop=!w.stop;});add("Stop Duration: "+num(w.stopSeconds)+" s",[&w](int d){change(w.stopSeconds,d,1,0,120);});add("Drive Directly: "+yes(w.direct),[&w](int){w.direct=!w.direct;});
 if(selectedWp>0)add("Delete This Waypoint",[](int){resetDrivers();auto& pp=paths[selectedPath];pp.points.erase(pp.points.begin()+selectedWp);go(2);});}
 if(page==4){add("Create Spawner Here",[](int){Spawner s;s.p=pos(invoke<int>(N::player));s.heading=invoke<float>(N::heading,invoke<int>(N::player));if(!paths.empty())s.path=paths.front().id;spawners.push_back(s);selectedSpawn=static_cast<int>(spawners.size())-1;go(5);});for(size_t i=0;i<spawners.size();i++)add(spawners[i].name+" ["+yes(spawners[i].enabled)+"]",[i](int){selectedSpawn=static_cast<int>(i);go(5);});}
 if(page==5){auto& s=spawners[selectedSpawn];add("Rename: "+s.name,[&s](int){s.name=input(s.name);});add("Enabled: "+yes(s.enabled),[&s](int){auto p=getPath(s.path);if(!s.enabled&&(!p||p->points.size()<2)){notify("Select a path with at least 2 waypoints");return;}s.enabled=!s.enabled;});
 add("Vehicle Model: "+s.model,[&s](int){auto m=input(s.model);auto h=hashModel(m);if(invoke<bool>(N::inCd,h)&&invoke<bool>(N::isVehicle,h))s.model=m;else notify("Invalid vehicle model");});
 auto p=getPath(s.path);add("Path: "+(p?p->name:"None"),[&s](int d){if(paths.empty())return;int idx=-1;for(size_t i=0;i<paths.size();i++)if(paths[i].id==s.path)idx=static_cast<int>(i);idx=(idx+(d<0?-1:1)+static_cast<int>(paths.size()))%static_cast<int>(paths.size());s.path=paths[idx].id;});
 add("Spawn Interval: "+num(s.interval)+" s",[&s](int d){change(s.interval,d,1,1,600);});add("Maximum Active Vehicles: "+std::to_string(s.limit),[&s](int d){s.limit=std::clamp(s.limit+(d<0?-1:1),1,100);});add("Heading: "+num(s.heading),[&s](int d){s.heading=std::fmod(s.heading+(d<0?-5:5)+360.f,360.f);});add("Move Here",[&s](int){s.p=pos(invoke<int>(N::player));});add("Delete Spawner",[](int){spawners.erase(spawners.begin()+selectedSpawn);go(4);});}
 if(page==6){add("Create Block Zone In Front",[](int){Zone z;int me=invoke<int>(N::player);z.p=pos(me);z.heading=invoke<float>(N::heading,me);z.p=corner(z,z.length/2+2,0,1);zones.push_back(z);applyZone(z,true);selectedZone=static_cast<int>(zones.size())-1;go(7);});for(size_t i=0;i<zones.size();i++)add(zones[i].name+" ["+yes(zones[i].enabled)+"]",[i](int){selectedZone=static_cast<int>(i);go(7);});}
 if(page==7){auto& z=zones[selectedZone];add("Rename: "+z.name,[&z](int){z.name=input(z.name);});add("Enabled: "+yes(z.enabled),[](int){zoneEdit([](Zone& q){q.enabled=!q.enabled;});});
 add("Length: "+num(z.length)+" m",[](int d){zoneEdit([d](Zone& q){change(q.length,d,2,2,1000);});});add("Width: "+num(z.width)+" m",[](int d){zoneEdit([d](Zone& q){change(q.width,d,1,2,300);});});add("Height: "+num(z.height)+" m",[](int d){zoneEdit([d](Zone& q){change(q.height,d,1,2,100);});});add("Heading: "+num(z.heading),[](int d){zoneEdit([d](Zone& q){q.heading=std::fmod(q.heading+(d<0?-5:5)+360.f,360.f);});});add("Move Center Here",[](int){zoneEdit([](Zone& q){q.p=pos(invoke<int>(N::player));});});
 add("Off-screen Ambient Cleanup: "+yes(z.cleanup),[&z](int){z.cleanup=!z.cleanup;});add("Clear Ambient Vehicles Once",[&z](int){std::array<int,2048> vv{};int n=Hook::vehicles(vv.data(),static_cast<int>(vv.size()));for(int i=0;i<n;i++){int v=vv[i];if(!exists(v)||playerVehicle(v)||controlled(v)||ownSpawn(v)||invoke<bool>(N::mission,v)||!inside(z,pos(v)))continue;int ped=invoke<int>(N::driver,v,-1,false);if(exists(ped)&&invoke<bool>(N::isPlayer,ped))continue;if(exists(ped))invoke(N::deleteEntity,&ped);invoke(N::deleteEntity,&v);}notify("Ambient vehicles cleared; managed vehicles protected");});
 add("Delete Zone",[](int){restoreAll();zones.erase(zones.begin()+selectedZone);refreshZones();go(6);});}
 if(page==8){add("Show World Preview: "+yes(preview),[](int){preview=!preview;});add("Automatic Collection: "+yes(collect),[](int){collect=!collect;});add("Release All Managed Drivers",[](int){resetDrivers();notify("Drivers released");});add("Disable All Block Zones",[](int){restoreAll();for(auto& z:zones)z.enabled=false;});add("Save Scene",[](int){save();});add("Load Scene",[](int){load();});}
}
void text(float x,float y,std::string s,float scale=0.32f){invoke(N::textFont,0);invoke(N::textScale,0.f,scale);invoke(N::textColour,255,255,255,255);invoke(N::textBegin,"STRING");invoke(N::textAdd,s.c_str());invoke(N::textEnd,x,y,0);}
void drawMenu(){
 if(hit(VK_BACK)||hit(VK_ESCAPE)){int back[]={0,0,1,2,0,4,0,6,0};if(page==0)menu=false;else go(back[page]);return;}
 buildMenu();if(rows.empty())return;cursor=std::clamp(cursor,0,static_cast<int>(rows.size())-1);
 if(hit(VK_UP))cursor=(cursor+static_cast<int>(rows.size())-1)%static_cast<int>(rows.size());if(hit(VK_DOWN))cursor=(cursor+1)%static_cast<int>(rows.size());
 int start=std::max(0,cursor-11),end=std::min(start+12,static_cast<int>(rows.size()));
 invoke(N::rect,0.20f,0.12f,0.32f,0.065f,20,100,180,245,false);text(0.05f,0.098f,"TRAFFIC DIRECTOR",0.48f);
 for(int i=start;i<end;i++){float y=0.163f+(i-start)*0.034f;invoke(N::rect,0.20f,y+0.014f,0.32f,0.034f,i==cursor?35:12,i==cursor?110:15,i==cursor?175:22,235,false);text(0.049f,y,rows[i].label);}
 text(0.049f,0.19f+(end-start)*0.034f,"Arrows: navigate / adjust | Enter: select");text(0.049f,0.218f+(end-start)*0.034f,"Backspace: back | K: close");
 if(hit(VK_RETURN)||hit(VK_LEFT)||hit(VK_RIGHT)){auto fn=rows[cursor].action;fn(hit(VK_LEFT)?-1:hit(VK_RIGHT)?1:0);}
 for(int k:{172,173,174,175,176,177,199,200}) { invoke(N::disable,0,k,true); }
}
void drawLine(Vec a,Vec b,int r,int g,int bl){invoke(N::line,a.x,a.y,a.z,b.x,b.y,b.z,r,g,bl,220);}
void worldPreview(){if(!preview||!menu)return;Vec me=pos(invoke<int>(N::player));
 for(auto& p:paths)for(size_t i=0;i<p.points.size();i++){auto w=p.points[i];if(distance(me,w.p)>500)continue;Vec top=w.p;top.z+=2;drawLine(w.p,top,40,200,255);if(i)drawLine(p.points[i-1].p,w.p,40,200,255);float r=i? w.zone:w.radius;for(int j=0;j<32;j++){float a=j*6.2831853f/32,b=(j+1)*6.2831853f/32;drawLine({w.p.x+std::cos(a)*r,w.p.y+std::sin(a)*r,w.p.z+0.2f},{w.p.x+std::cos(b)*r,w.p.y+std::sin(b)*r,w.p.z+0.2f},i?80:255,i?150:180,30);}}
 for(auto& z:zones){if(distance(me,z.p)>700)continue;Vec v[8];for(int i=0;i<8;i++)v[i]=corner(z,(i&1?1:-1)*z.length/2,(i&2?1:-1)*z.width/2,(i&4?1:-1)*z.height/2);for(int i=0;i<8;i++)for(int bit:{1,2,4})if(!(i&bit))drawLine(v[i],v[i|bit],255,z.enabled?60:150,60);}
 for(auto& s:spawners){Vec top=s.p;top.z+=3;drawLine(s.p,top,180,80,255);}
}
void tickDrivers(ULONGLONG now){for(auto it=drivers.begin();it!=drivers.end();){auto& d=*it;auto p=getPath(d.path);if(!p||!p->enabled||!exists(d.car)||!exists(d.ped)||invoke<bool>(N::dead,d.ped,false)||invoke<int>(N::driver,d.car,-1,false)!=d.ped||d.target>=p->points.size()){releaseDriver(d);it=drivers.erase(it);continue;}
 auto& w=p->points[d.target];float dist=distance(pos(d.car),w.p);
 if(d.stopping){invoke(N::brake,d.ped,d.car,27,250);if(now>=d.resume){d.stopping=false;d.target++;d.issued=false;}++it;continue;}
 if(dist<=w.radius){if(w.stop){d.stopping=true;d.resume=now+static_cast<ULONGLONG>(w.stopSeconds*1000);}else{d.target++;d.issued=false;}++it;continue;}
 // Cruise at previous waypoint speed; enter this waypoint's speed zone to adopt its speed.
 float kmh=dist<=w.zone?w.speed:p->points[d.target-1].speed;
 if(!d.issued || now%1000<110){int style=w.direct?16777216:786603;auto model=invoke<unsigned int>(0x9F47B058362C84B5,d.car);invoke(N::drive,d.ped,d.car,w.p.x,w.p.y,w.p.z,kmh/3.6f,0,model,style,std::max(1.f,w.radius*0.65f),w.direct?100000.f:1.f);d.issued=true;}
 ++it;}}
void tickSpawners(ULONGLONG now,const std::vector<int>& vv){for(auto& s:spawners){s.spawned.erase(std::remove_if(s.spawned.begin(),s.spawned.end(),[](int h){return !exists(h);}),s.spawned.end());auto p=getPath(s.path);if(!s.enabled||!p||!p->enabled||p->points.size()<2||now<s.next||static_cast<int>(s.spawned.size())>=s.limit)continue;
 bool blocked=false;for(int v:vv)if(exists(v)&&distance(pos(v),s.p)<6){blocked=true;break;}if(blocked)continue;
 auto hash=hashModel(s.model);if(!invoke<bool>(N::inCd,hash)||!invoke<bool>(N::isVehicle,hash)){s.enabled=false;notify("Spawner disabled: invalid model");continue;}invoke(N::request,hash);if(!invoke<bool>(N::loaded,hash))continue;
 auto pedModel=hashModel("a_m_y_business_01");invoke(N::request,pedModel);if(!invoke<bool>(N::loaded,pedModel))continue;
 int car=invoke<int>(N::createVehicle,hash,s.p.x,s.p.y,s.p.z,s.heading,false,false,false);if(!exists(car))continue;
 int ped=invoke<int>(N::createDriver,car,26,pedModel,-1,true,false);
 if(!exists(ped)||!assign(car,s.path)){if(exists(ped))invoke(N::deleteEntity,&ped);invoke(N::deleteEntity,&car);}else{s.spawned.push_back(car);s.next=now+static_cast<ULONGLONG>(s.interval*1000);}invoke(N::releaseModel,hash);invoke(N::releaseModel,pedModel);
}}
void scriptMain(){
 wchar_t modulePath[MAX_PATH];GetModuleFileNameW(GetModuleHandleW(L"TrafficDirector.asi"),modulePath,MAX_PATH);folder=std::filesystem::path(modulePath).parent_path()/"TrafficDirector";std::filesystem::create_directories(folder);load();
 ULONGLONG nextTick=0,nextRoads=0;while(true){poll();if(hit('K'))menu=!menu;if(menu)drawMenu();worldPreview();auto now=GetTickCount64();if(now<messageUntil)text(0.40f,0.86f,message);
 if(now>=nextRoads){refreshZones();nextRoads=now+2000;}
 if(now>=nextTick){nextTick=now+100;std::array<int,4096> buffer{};int n=Hook::vehicles(buffer.data(),static_cast<int>(buffer.size()));n=std::clamp(n,0,static_cast<int>(buffer.size()));std::vector<int> vv(buffer.begin(),buffer.begin()+n);
 for(int v:vv){if(!exists(v)||playerVehicle(v)||controlled(v)||ownSpawn(v)||invoke<bool>(N::mission,v))continue;int ped=invoke<int>(N::driver,v,-1,false);if(exists(ped)&&(invoke<bool>(N::isPlayer,ped)||invoke<bool>(N::mission,ped)))continue;Vec at=pos(v);
 bool removed=false;for(auto& z:zones)if(z.enabled&&z.cleanup&&inside(z,at)&&!invoke<bool>(N::visible,v)){if(exists(ped))invoke(N::deleteEntity,&ped);invoke(N::deleteEntity,&v);removed=true;break;}if(removed)continue;
 if(collect&&exists(ped))for(auto& p:paths)if(p.enabled&&p.points.size()>1&&distance(at,p.points[0].p)<=p.points[0].radius){assign(v,p.id);break;}}
 tickDrivers(now);tickSpawners(now,vv);}
 Hook::wait(0);
 }
}
BOOL APIENTRY DllMain(HMODULE module,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(module);if(!Hook::load())return FALSE;Hook::reg(module,scriptMain);}else if(reason==DLL_PROCESS_DETACH&&Hook::unreg)Hook::unreg(module);return TRUE;}


