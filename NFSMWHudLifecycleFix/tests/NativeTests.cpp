#define HUD_FIX_TEST
#include "../src/Plugin.cpp"
#include <cstdlib>
void Check(bool ok,const char* name){if(!ok){std::printf("FAIL %s\n",name);std::exit(1);}}
struct Object {unsigned char raw[0x30]{};float data[16]{};float x=0,y=0;};
Object groups[2];float offset=256;unsigned calls=0;
void* selfExpected=reinterpret_cast<void*>(0x12345678);
char pkg[]="HUD_SingleRace.fng";
void* __cdecl Find(const char* p,uint32_t hash){Check(p==pkg,"package ABI");return &groups[hash==0x1603009e?0:1];}
void __cdecl Center(void* p,float* x,float* y){auto o=static_cast<Object*>(p);*x=o->x;*y=o->y;}
void __cdecl Move(void* p,float x,float y){auto o=static_cast<Object*>(p);o->x=x;o->y=y;}
int __declspec(noinline) __fastcall Fake(void*,void*,uint32_t,const char*,uint32_t){Check(false,"old additive adapter must not execute");return 1;}
void __fastcall Queue(void* self,void* edx,uint32_t event,const char* p,uint32_t arg){
    Check(self==selfExpected&&edx==nullptr&&event==0x62ed04ec&&p==pkg&&arg==0xaabbccdd,"WSF event ABI");
    // Model verified WSF's cached original centers, including its pre-event pass.
    groups[0].x=-offset;groups[1].x=209.651825f+offset;++calls;
}
int main(){
    Check(mwcombat::HudRacer(0)&&mwcombat::HudRacer(3)&&!mwcombat::HudRacer(2),"police combat owner excluded from racer drawing only");
    combat_hud::Vertex labelVertices[1536];unsigned used=0;
    combat_hud::Text(labelVertices,used,0,0,3,"HP 100000/100000",0xffffffff);
    Check(used+72<=1536&&used%6==0,"maximum accepted health label fits single draw buffer");
    Check(combat_hud::Glyph('H')&&combat_hud::Glyph('P')&&combat_hud::Glyph('/')&&!combat_hud::Glyph(' '),"HP glyph coverage");
    D3DVIEWPORT9 viewport{0,0,1920,1080,0,1};float matrix[16]{},point[3]={0,-.65f,0},x=0,y=0,z=0;
    for(unsigned i=0;i<4;++i)matrix[i*4+i]=1;
    Check(combat_hud::Project(point,matrix,viewport,x,y,z)&&std::abs(x-960)<.01f&&std::abs(y-540)<.01f,"leader marker uses MW coordinate conversion and native healthbar lift");
    matrix[15]=-1;Check(!combat_hud::Project(point,matrix,viewport,x,y,z),"leader marker rejects behind-camera point");matrix[15]=1;
    point[0]=100;Check(!combat_hud::Project(point,matrix,viewport,x,y,z),"leader marker rejects offscreen point");point[0]=0;
    point[1]=3;Check(!combat_hud::Project(point,matrix,viewport,x,y,z),"leader marker rejects outside depth range");
    combat_hud::Vertex star[60];unsigned starCount=0;combat_hud::StarVertices(star,starCount,960,540,16,.5f,0xffffd44d);
    Check(starCount==30,"five-point leader star has bounded triangle fan");
    for(unsigned i=0;i<starCount;++i)Check(std::isfinite(star[i].x)&&std::isfinite(star[i].y)&&star[i].z==.5f,"leader marker preserves finite screen coordinates and world depth");
    combat_hud::count=1;combat_hud::sampledAt=GetTickCount()-501;Check(!combat_hud::Active(),"stale health samples disappear");
    combat_hud::count=0;combat_hud::sampledAt=GetTickCount();Check(!combat_hud::Active(),"empty health event disappears");
    widescreenQueue=&Queue;findObject=&Find;getCenter=&Center;setCenter=&Move;
    groups[0].x=0;groups[0].y=7;groups[1].x=209.651825f;groups[1].y=49.772728f;
    for(auto& g:groups)*reinterpret_cast<uintptr_t*>(g.raw+0x2c)=reinterpret_cast<uintptr_t>(g.data);
    Check(MH_Initialize()==MH_OK,"MinHook init");
    Check(MH_CreateHook(reinterpret_cast<void*>(&Fake),&Adapter,reinterpret_cast<void**>(&originalAdapter))==MH_OK,"create adapter detour");
    Check(MH_EnableHook(reinterpret_cast<void*>(&Fake))==MH_OK,"enable adapter detour");
    volatile AdapterFn entry=reinterpret_cast<AdapterFn>(&Fake);
    auto run=[&](){Check(entry(selfExpected,0x62ed04ec,pkg,0xaabbccdd)==1,"adapter return retained");};
    Queue(selfExpected,nullptr,0x62ed04ec,pkg,0xaabbccdd);calls=0;
    run();Check(groups[0].x==-256&&std::fabs(groups[1].x-465.651825f)<0.001,"first correction");
    for(int i=0;i<10000;++i)run();
    Check(groups[0].x==-256&&std::fabs(groups[1].x-465.651825f)<0.001,"10000 repeated calls do not accumulate");
    offset=80;run();Check(groups[0].x==-80&&std::fabs(groups[1].x-289.651825f)<0.001,"resolution changed");
    offset=0;run();Check(groups[0].x==0&&std::fabs(groups[1].x-209.651825f)<0.001,"4:3 restores base");
    offset=256;run();Check(groups[0].x==-256,"wide again");
    groups[0].x=0;groups[1].x=209.651825f;run();
    Check(groups[0].x==-256&&std::fabs(groups[1].x-465.651825f)<0.001,"package recycled at identical address");
    groups[0].x=12;run();Check(groups[0].x==-256,"WSF remains sole layout owner");
    Check(groups[0].y==7&&groups[1].y==49.772728f,"vertical coordinates unchanged");
    Check(calls==10006,"WSF queue called exactly once per event");
    Check(MH_DisableHook(MH_ALL_HOOKS)==MH_OK,"disable detour");MH_Uninitialize();
    std::puts("PASS x86 adapter detour ABI, return, repeated calls, resolution changes, recycled objects, WSF pre-applied layout, vertical preservation");
}
