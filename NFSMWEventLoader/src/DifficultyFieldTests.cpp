#include "DifficultyPolicy.h"
#include <windows.h>
#include "DifficultySettings.h"
#include "GridPolicy.h"
#include "GridLaunchPolicy.h"
#include "EventAudioPolicy.h"
#include "OpponentRosterPolicy.h"
#include "RaceOpponentPolicy.h"
#include "CareerTuningPolicy.h"
#include <cstdio>
#include <stdexcept>
#include <limits>
static void Require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){try{
 {using namespace mwel_career_tuning;Levels maximum{3,4,3,4,4,3,3},ceiling{};
  Require(Ceilings(maximum,ceiling,[](unsigned,unsigned){return false;})&&ceiling==Levels{},"locked career parts admitted");
  Require(Limit(maximum,ceiling)==Levels{},"BL15 locked NOS retained");
  Require(Ceilings(maximum,ceiling,[](unsigned type,unsigned shop){return type!=6&&shop<=1;})&&ceiling[6]==0&&ceiling[0]==1,"per-category unlock boundary lost");
  Require(Ceilings(maximum,ceiling,[](unsigned,unsigned){return true;})&&ceiling==maximum,"late-career parts lost");
  Levels stock{};Require(Limit(stock,ceiling)==stock,"stock AI upgraded automatically");
  for(unsigned max=0;max<=6;++max)for(unsigned unlocked=0;unlocked<=4;++unlocked){
   Levels m{};m.fill(max);Require(Ceilings(m,ceiling,[&](unsigned,unsigned shop){return shop<=unlocked;}),"valid local maximum rejected");
   for(unsigned type=0;type<7;++type)for(unsigned generated=0;generated<=6;++generated){Levels a{};a.fill(generated);auto out=Limit(a,ceiling);
    Require(out[type]<=generated&&out[type]<=max,"career cap increased native difficulty or exceeded car maximum");
    if(out[type])Require(int(ShopCounts[type])+int(out[type])-int(max)<=int(unlocked),"car-local/shop tier conversion exceeds unlock");
   }
  }
  maximum[0]=7;Require(!Ceilings(maximum,ceiling,[](unsigned,unsigned){return true;}),"unknown package layout accepted");
  Require(Applies(true,true,false,true,true,1,16)&&Applies(true,false,true,true,true,15,16),"eligible custom opponent excluded");
  Require(!Applies(false,true,true,true,true,1,8)&&!Applies(true,false,false,true,true,1,8)&&!Applies(true,true,false,false,true,1,8)&&!Applies(true,true,false,true,false,1,8)&&!Applies(true,true,false,true,true,0,8)&&!Applies(true,true,false,true,true,8,8),"player, unloaded profile, stock event or quick race changed");
 }
 Require(mwel_opponents::OrdinaryCandidate(true,true,1),"installed eligible racer excluded from ordinary extra grid");
 Require(!mwel_opponents::OrdinaryCandidate(false,true,1),"traffic with RandomOpponent enabled admitted");
 Require(!mwel_opponents::OrdinaryCandidate(true,false,1),"uninstalled racer admitted");
 Require(!mwel_opponents::OrdinaryCandidate(true,true,0)&&!mwel_opponents::OrdinaryCandidate(true,true,2),"native excluded/prototype racer admitted");
 using namespace mwel_difficulty;Settings settings;
 Require(Multiplier(settings,{1,1000,true})==1.75f,"leader gained catchup");
 Require(Multiplier(settings,{2,-100,true})==1.75f,"AI ahead of player gained catchup");
 Require(Multiplier(settings,{2,50,true})==1.75f,"dead zone changed base output");
 Require(Multiplier(settings,{2,600,true})==3.5f,"maximum deficit not boosted");
 Require(Multiplier(settings,{16,600,true})==3.5f,"sixteenth racer lost catchup");
 Require(Multiplier(settings,{2,1000,false})==1.75f,"invalid sample gained catchup");
 Require(Multiplier(settings,{2,std::numeric_limits<float>::quiet_NaN(),true})==1.75f,"NaN gap gained catchup");
 float last=1.75f;for(int gap=0;gap<5000;++gap){float scale=Multiplier(settings,{2,float(gap),true});Require(scale>=last&&scale<=3.5f,"catchup not monotonic/bounded");last=scale;}
 settings.catchUp=false;Require(Multiplier(settings,{2,600,true})==1.75f,"catchup off ignored");
 settings.enabled=false;Require(Multiplier(settings,{2,600,true})==1,"difficulty off ignored");
 {Settings curve;curve.blacklistScaling=curve.farCatchUp=true;
  auto closeEnough=[](float a,float b){return std::abs(a-b)<.00001f;};
  for(unsigned bl=1;bl<=15;++bl){float base=1.4f+.025f*(15-bl);
   Require(closeEnough(Multiplier(curve,{2,0,true,bl}),base),"BL ladder wrong");
   Require(closeEnough(Multiplier(curve,{2,301,true,bl}),base*2.5f)&&closeEnough(Multiplier(curve,{16,302,true,bl}),base*2.5f),"301/302m far catchup is not base times 2.5");
   Settings legacy;legacy.engineScale=base;
   for(float gap:{-10.f,0.f,50.f,100.f,300.f,300.999f})Require(closeEnough(Multiplier(curve,{2,gap,true,bl}),Multiplier(legacy,{2,gap,true})),"existing smooth catchup below 301 changed");
  }
  Require(closeEnough(Multiplier(curve,{2,301,true,15}),3.5f)&&closeEnough(Multiplier(curve,{2,301,true,1}),4.375f),"catchup mistaken for final output scale");
  for(unsigned rank:{0u,1u,17u})Require(closeEnough(Multiplier(curve,{rank,400,true,15}),1.4f),"ineligible rank gained catchup");
  Require(Multiplier(curve,{2,400,false,15})==1.75f&&Multiplier(curve,{0,0,false,15})==1.75f,"invalid/player sample gained BL curve or catchup");
  Require(BaseScale(curve,{2,0,true,0})==1.75f&&BaseScale(curve,{2,0,true,16})==1.75f,"unknown BL guessed a curve level");
  Require(closeEnough(Multiplier(curve,{2,std::numeric_limits<float>::quiet_NaN(),true,15}),1.4f),"NaN gained catchup");
  curve.catchUp=false;Require(closeEnough(Multiplier(curve,{2,1000,true,15}),1.4f),"catchup OFF left far multiplier active");
  curve.enabled=false;Require(Multiplier(curve,{2,1000,true,1})==1,"master OFF ignored");
  curve.enabled=curve.catchUp=true;curve.blacklistScaling=false;curve.engineScale=2.75f;Require(BaseScale(curve,{2,0,true,15})==2.75f,"explicit legacy base ignored when curve OFF");
 }
 Require(Output(-5,4)==-5&&Output(100,2)==200&&Output(100,4)==400,"engine drag or positive output changed");
 Require(Number("2.75",2,1,4)==2.75f&&Number("nan",2,1,4)==2&&Number("2x",2,1,4)==2&&Number("999",2,1,4)==4,"INI parsing/bounds invalid");
 {char directory[MAX_PATH]{},file[MAX_PATH]{};Require(GetTempPathA(MAX_PATH,directory)&&GetTempFileNameA(directory,"MWD",0,file),"temporary INI unavailable");
  struct Cleanup {const char* file;~Cleanup(){DeleteFileA(file);}} cleanup{file};
  Require(WritePrivateProfileStringA("Difficulty","EnginePowerScale","2.75",file)!=0,"temporary INI write failed");
  WritePrivateProfileStringA("Difficulty","CatchUpMaxScale","1.25",file);
  WritePrivateProfileStringA("Difficulty","CatchUpStartMetres","5000",file);
  WritePrivateProfileStringA("Difficulty","NitrousEnabled","0",file);
  auto ini=Load(file);Require(ini.engineScale==2.75f&&ini.catchUpMaxScale==1.25f&&!ini.nitrous&&ini.fullMetres==5001,"Windows INI fractional settings/default interval failed");
  Require(!ini.blacklistScaling&&!ini.farCatchUp,"old/missing INI silently opted into new boost settings");
  WritePrivateProfileStringA("Difficulty","BlacklistScalingEnabled","1",file);WritePrivateProfileStringA("Difficulty","FarCatchUpEnabled","1",file);
  ini=Load(file);Require(ini.blacklistScaling&&ini.farCatchUp&&std::abs(BaseScale(ini,{2,0,true,15})-1.4f)<.00001f,"explicit BL curve did not override legacy EnginePowerScale");
  WritePrivateProfileStringA("Difficulty","BlacklistScalingEnabled","1bad",file);WritePrivateProfileStringA("Difficulty","FarCatchUpEnabled","bad",file);
  ini=Load(file);Require(!ini.blacklistScaling&&!ini.farCatchUp,"malformed opt-in enabled a new multiplier");
  WritePrivateProfileStringA("Difficulty","FarCatchUpMetres","nan",file);WritePrivateProfileStringA("Difficulty","FarCatchUpScale","inf",file);
  ini=Load(file);Require(ini.farMetres==301&&ini.farScale==2.5f,"invalid far values lost safe defaults");
  WritePrivateProfileStringA("Difficulty","EnginePowerScale","nan",file);
  Require(Load(file).engineScale==1.75f,"invalid Windows INI float lost safe default");
 }
 for(unsigned count:{1u,4u,5u,8u,12u,16u})Require(FieldMask(count)==(1u<<count)-1,"full field mask truncated");
 Require(FieldMask(0)==0&&FieldMask(17)==0,"bad count accepted");
 {std::vector<unsigned> roster;std::vector<unsigned> original{10,20,30},pool{0,10,20,30};
  for(unsigned i=1;i<=15;++i)pool.push_back(i);
  mwel_roster::AppendUnique(roster,original,15);mwel_roster::AppendUnique(roster,pool,15);
  Require(roster.size()==15&&roster[0]==10&&roster[1]==20&&roster[2]==30,"sixteen-car roster lost preferred identities or unique count");
  for(unsigned a=0;a<roster.size();++a)for(unsigned b=a+1;b<roster.size();++b)Require(roster[a]!=roster[b],"native GCharacter identity reused");
 }
 for(unsigned count:{1u,4u,5u,8u,12u,16u}){
  auto grid=mwel_grid::Plan(count,{0,0,0},0,[](auto&,float,float){return true;});
  Require(grid.valid&&grid.count==count&&grid.columns==2,"wide grid wrong count");
  for(unsigned a=0;a<count;++a)for(unsigned b=a+1;b<count;++b){auto& x=grid.positions[a];auto& y=grid.positions[b];Require(std::hypot(x[0]-y[0],x[2]-y[2])>=2.99f,"overlapping grid positions");}
 }
 {using mwel_grid::ApproachCandidate;
  bool considered=false;
  Require(mwel_grid::BeginApproach(considered,8,1,11,0,70)&&considered,"previous CP lost its own physical approach");
  considered=false; // The provider resets this state when native CP changes.
  Require(mwel_grid::BeginApproach(considered,8,1,0,0,70)&&considered,"subsequent CP0 did not initialize the approach");
  Require(!mwel_grid::BeginApproach(considered,8,1,0,0,70),"an accepted boundary must not rebuild each frame");
  considered=false;
  Require(!mwel_grid::BeginApproach(considered,8,0,0,0,70)&&!considered,"CP-first native update consumed the moving first-lap attempt");
  Require(mwel_grid::BeginApproach(considered,8,1,0,0,70),"lap update after CP0 lost its approach");
  struct Knot{float p[3];float station;};
  std::vector<Knot> prefix={{{0,0,0},0},{{0,0,62},62}},canonical={{{0,0,62},0},{{0,0,100},38}};
  mwel_grid::AppendCanonical(canonical,62,prefix);
  Require(prefix.size()==3&&prefix.back().station==100&&prefix[1].station==62,"driving/recovery approach coordinates diverged at canonical zero");
  for(unsigned count:{1u,4u,6u,8u,16u})Require(ApproachCandidate(count,1,0,0,70),"moving next-lap vehicle lost finish-to-grid approach");
  Require(!ApproachCandidate(6,0,0,0,70)&&ApproachCandidate(6,0,0,0,0)&&!ApproachCandidate(4,0,0,0,0),"initial grid restrictions changed");
  Require(ApproachCandidate(6,1,1,0,70)&&!ApproachCandidate(6,1,0,20,70)&&!ApproachCandidate(17,1,0,0,70),"CP-boundary approach or racer bounds changed");
  mwel_grid::Launch approach;
  mwneo::Point finish{-2862.7717f,197.15872f,-142.96684f},grid{-2840.2666f,196.17461f,-63.86494f},next{-2839.7815f,196.14714f,-62.10670f};
  Require(approach.Initialize(finish,grid,next)&&approach.length>80&&approach.length<85&&approach.Active(finish),"native LKO separated finish/grid not bridged");
  Require(!approach.Active({grid.x+approach.direction.x*2,grid.y,grid.z+approach.direction.z*2}),"repeat-lap approach retained after grid");
  Require(!mwel_grid::Launch{}.Initialize({finish.x+30,finish.y,finish.z},grid,next),"parallel road incorrectly bridged after lap");
  mwneo::Point edge{-2870.929f,197.856f,-141.781f};
  Require(!mwel_grid::Launch{}.Initialize(edge,grid,next),"regression pose no longer exercises narrow initial-grid rejection");
  mwel_grid::Launch checked;
  Require(checked.InitializeChecked(edge,grid,next,42,[](auto,auto){return true;})&&checked.Active(edge),"supported wide-lane lap crossing rejected");
  mwel_grid::Launch blocked;
  Require(!blocked.InitializeChecked(edge,grid,next,42,[](auto,auto){return false;})&&!blocked.length,"blocked or unsupported lap join accepted");
  Require(!blocked.InitializeChecked(edge,grid,next,43,[](auto,auto){return true;}),"unchecked lane extent accepted");
 }
 auto narrow=mwel_grid::Plan(16,{0,0,0},0,[](auto& p,float,float){return std::abs(p[0])<.5f;});
 Require(narrow.valid&&narrow.columns==1&&narrow.positions[15][2]<-100,"narrow grid fallback failed");
 auto unsupported=mwel_grid::Plan(16,{0,0,0},0,[](auto&,float,float){return false;});
 Require(!unsupported.valid&&!unsupported.count,"partial unsupported grid committed");
 auto barrier=mwel_grid::Plan(16,{0,0,0},0,[](auto& p,float,float){return p[2]<-15;});
 Require(barrier.valid&&barrier.positions[0][2]<-15,"grid did not move behind obstacle");
 auto rotated=mwel_grid::Plan(16,{10,5,20},90,[](auto& p,float,float){p[1]=6;return true;});
 Require(rotated.valid&&std::abs(rotated.positions[0][0]-12)<.001f&&rotated.positions[0][1]==6,"native/editor rotation or floor fitting wrong");
 Require(!mwel_grid::Plan(17,{0,0,0},0,[](auto&,float,float){return true;}).valid,"native racer array overflow accepted");
 {mwel_grid::Launch launch;
  Require(launch.Initialize({1.5f,0,-51},{0,0,0},{0,0,10}),"rear grid cannot initialize approach");
  Require(launch.Active({1,0,-30})&&launch.Cursor({1,0,-30})>20&&launch.Cursor({1,0,-30})<22,"rear grid approach cursor wrong");
  Require(!launch.Active({0,0,1}),"approach remained active after start plane");
  Require(!launch.Initialize({0,0,-300},{0,0,0},{0,0,10}),"distant off-course pose accepted as grid");
  Require(!launch.Initialize({30,0,-30},{0,0,0},{0,0,10}),"parallel road accepted as grid");
  Require(!launch.Initialize({0,0,5},{0,0,0},{0,0,10}),"ordinary course progress reset as grid");
 }
 for(unsigned total:{0u,1u,4u,5u,6u,16u,64u,129u}){
  Require(!mwel_audio::ExpandedCount(total,6,false,true),"stock event audio changed");
  Require(!mwel_audio::ExpandedCount(total,6,true,false),"unsupported mixer expanded");
  Require(!mwel_audio::ExpandedCount(total,4,true,true),"default audio expanded");
 }
 Require(mwel_audio::ExpandedCount(16,6,true,true)==6&&mwel_audio::ExpandedCount(5,6,true,true)==5,"expanded native audio ceiling wrong");
 Require(!mwel_audio::ExpandedCount(16,16,true,true),"untested sixteen-voice audio ceiling accepted");
 std::puts("PASS difficulty, gap/rank ownership boundaries, finite output, INI parsing, full-field masks, 5/8/12/16 grids, narrow/floor/obstacle fallback, native audio capability and event gates");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
