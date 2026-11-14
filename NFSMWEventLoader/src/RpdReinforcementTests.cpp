#include "RpdFreeRunRoute.h"
#include "RpdRoadCorridor.h"
#include "RpdRecoveryPose.h"
#include "RpdFleetPolicy.h"
#include "NativeCsisHandlePolicy.h"
#include "HotPursuitArrestPolicy.h"
#include <map>
#include <cstring>
#include <cstdio>
#include <stdexcept>
static unsigned checks=0;
static void Check(bool ok,const char* text){++checks;if(!ok)throw std::runtime_error(text);}
int main(){try{
 using M=mwel_csis_handle::Membership;
 std::map<unsigned,unsigned> bank{{0x9C29E4,0x20020},{0x2000C,3},{0x20018,0x30000},{0x20020,0}};
 bool readCandidate=false;
 auto bankRead=[&](unsigned a,void* out,size_t bytes){if(a==0xEA777C06)readCandidate=true;auto i=bank.find(a);if(i==bank.end()||bytes>4)return false;memcpy(out,&i->second,bytes);return true;};
 Check(mwel_csis_handle::ClassDefinition(0x30000,bankRead)==M::Live,"first live Csis definition");
 Check(mwel_csis_handle::ClassDefinition(0x30018,bankRead)==M::Live,"last live Csis definition");
 Check(mwel_csis_handle::ClassDefinition(0x30024,bankRead)==M::Retired,"one-past definition must be retired");
 Check(mwel_csis_handle::ClassDefinition(0x30004,bankRead)==M::Retired,"interior misaligned pointer cannot be a definition");
 Check(mwel_csis_handle::ClassDefinition(0xEA777C06,bankRead)==M::Retired&&!readCandidate,"reported fault pointer rejected without dereferencing it");
 bank[0x20018]=0x40000;
 Check(mwel_csis_handle::ClassDefinition(0x30000,bankRead)==M::Retired&&mwel_csis_handle::ClassDefinition(0x40000,bankRead)==M::Live,"bank replacement invalidates old membership without retained pointers");
 bank[0x20020]=0x20020;
 Check(mwel_csis_handle::ClassDefinition(0xEA777C06,bankRead)==M::Unknown,"malformed bank list cannot authorize recovery");
 bank.erase(0x20018);
 Check(mwel_csis_handle::ClassDefinition(0xEA777C06,bankRead)==M::Unknown,"incomplete registry cannot authorize recovery");
 rpd_fleet_policy::Inventory inventory;
 Check(inventory.Add(1,"COPA"),"first police model");
 Check(!inventory.Add(2,"copa")&&inventory.sources.size()==2,"collection aliases remain eligible but do not enable variety");
 Check(!rpd_fleet_policy::Enabled(true,5,unsigned(inventory.models.size())),"one installed visual model preserves legacy dispatch");
 Check(inventory.Add(3,"SUVB"),"SUV counts as distinct ground model");
 Check(rpd_fleet_policy::Enabled(true,5,unsigned(inventory.models.size()))&&!rpd_fleet_policy::Enabled(false,5,unsigned(inventory.models.size())),"same environment requires LB for expanded dispatch");
 Check(rpd_fleet_policy::AtHeat(6,6,0)&&!rpd_fleet_policy::AtHeat(5,6,0),"installed SUV cannot leak across heat levels");
 Check(rpd_fleet_policy::AtHeat(10,6,1u<<10)&&!rpd_fleet_policy::AtHeat(11,11,0),"installed native pursuit regulations support extra heat tiers with bounds");
 using namespace mwel_scene_road;
 Graph graph;graph.nodes={{-100,0,0},{0,0,0},{200,0,0},{-100,0,12},{0,0,12},{200,0,12}};
 graph.edges={{0,1},{1,2},{3,4},{4,5}};Frame frame;
 Check(rpd_free_run::ReinforcementAhead(graph,{0,0,0},{1,0,0},frame),"lower road query");
 Check(std::abs(frame.center.x-100)<.01f&&std::abs(frame.center.z)<.01f&&frame.forward.x>.99f,"connected same direction road, 100m ahead");
 Check(rpd_free_run::ReinforcementAhead(graph,{0,0,12},{1,0,0},frame)&&std::abs(frame.center.z-12)<.01f,"bridge level retained");
 Check(!rpd_free_run::ReinforcementAhead(graph,{190,0,0},{1,0,0},frame),"road end must refuse invented 100m point");
 Check(!rpd_free_run::ReinforcementAhead(graph,{0,0,0},{0,0,0},frame),"invalid direction refused");
 Check(!rpd_free_run::ReinforcementAhead(Graph{},{0,0,0},{1,0,0},frame),"empty graph refused");
 rpd_free_run::Route route;Point first,second;
 Check(route.Aim(graph,{0,0,0},{1,0,0},20,first),"live suspect route initialized");
 Check(rpd_free_run::ReinforcementAhead(graph,{0,0,12},{1,0,0},frame),"independent bridge query");
 Check(route.Aim(graph,{0,0,0},{1,0,0},20,second)&&Length(first-second)<.001f,"reinforcement cannot advance or redirect suspect route");
 route.Reset();
 Check(route.Aim(graph,{0,-3,0},{1,0,0},40,first)&&std::abs(first.y+3)<.01f,"driving lane must not collapse onto median graph reference");
 auto corridor=[](Point a,Point b){
  if(std::abs(a.y)>4.6f||std::abs(b.y)>4.6f||std::abs(a.z)>.1f||std::abs(b.z)>.1f)return false;
  const Point pillar{25,0,0};auto d=b-a;float n=Dot(d,d);
  const float t=n>.001f?std::clamp(Dot(pillar-a,d)/n,0.f,1.f):0;
  return Length(a+d*t-pillar)>2.2f; // column radius + car half-width
 };
 route.Reset();
 Check(route.AimClear(graph,{0,0,0},{1,0,0},50,first,corridor),"central tunnel pillar must produce bounded guidance");
 Check(first.x>20&&std::abs(first.y)>2.2f&&corridor({},first),"issued native steering chord clears full-width pillar");
 Check(std::abs(first.z)<.01f,"pillar avoidance cannot substitute nearby elevated bridge");
 route.Reset();
 Check(route.AimClear(graph,{0,-3,0},{1,0,0},40,first,corridor)&&std::abs(first.y+3)<.01f,"clear current lane is preferred without weaving");
 Check(route.AimClear(graph,{1,-3,0},{1,0,0},40,second,corridor)&&std::abs(second.y-first.y)<.01f,"successive aim retains selected lane");
 route.Reset();
 Check(route.AimClear(graph,{0,0,0},{1,0,0},40,first,[](Point,Point){return false;})&&Length(first)<.001f,"all blocked targets hold safely instead of unchecked centre-line fallback");
 Check(rpd_free_run::RoadCorridor({0,0,0},{30,0,3},[](Point p,float& height){height=p.x*.1f;return true;},[](Point,Point){return false;}),"uphill floor is not a chassis wall");
 Check(!rpd_free_run::RoadCorridor({0,0,0},{30,0,0},[](Point p,float& height){height=p.x>10?12.f:0.f;return true;},[](Point,Point){return false;}),"footprint cannot jump to an elevated deck");
 Frame biased{{0,0,2.8f},Unit({1,0,.1f})};
 auto realFloor=[&](Point p,float& h){float expected=rpd_free_run::ProjectedFloorHeight(biased,p,0);h=p.x*.1f;return std::isfinite(expected)&&std::abs(expected-h)<.01f;};
 Check(rpd_free_run::RoadCorridor(biased.center-biased.forward*2,biased.center+biased.forward*2,realFloor,[](Point,Point){return false;}),"tyre floor queries must not subtract graph-height bias twice on a slope");
 Check(std::abs(rpd_free_run::ProjectedFloorHeight(biased,{10,1.1f,1},0)-1)<.001f,"actual-height tyre samples retain the same floor projection");
 Check(!std::isfinite(rpd_free_run::ProjectedFloorHeight({{},{}},{},0)),"invalid vertical road frame cannot supply floor hints");
 float nativePosition[]={5,12,7},heading[]={0,.1f,1},up[]={0,1,-.1f},matrix[16]{};
 Check(rpd_free_run::RecoveryMatrix(nativePosition,heading,up,matrix),"recovery pose accepts a verified sloped road");
 Check(std::abs(matrix[0]-1)<.001f&&std::abs(matrix[4]*matrix[8]+matrix[5]*matrix[9]+matrix[6]*matrix[10])<.001f&&matrix[12]==5&&matrix[13]==12&&matrix[14]==7&&matrix[15]==1,"native orientation is orthonormal, upright and retains the exact requested position");
 float vertical[]={0,1,-.1f};Check(!rpd_free_run::RecoveryMatrix(nativePosition,vertical,up,matrix),"parallel heading and normal cannot produce a recovery pose");
 auto column=[](Point a,Point b){const Point center{15,1.2f,0};auto d=b-a;float n=Dot(d,d);float t=n>.001f?std::clamp(Dot(center-a,d)/n,0.f,1.f):0;return Length(a+d*t-center)<.3f;};
 Check(!rpd_free_run::RoadCorridor({0,0,0},{30,0,0},[](Point,float& height){height=0;return true;},column),"width sweep catches a pillar missed by the centre ray");
 Graph fork{{{-100,0,0},{50,0,0},{150,0,0},{300,0,0},{80,80,0},{80,250,0}},{{0,1},{1,2},{2,3},{1,4},{4,5}}};
 route.Reset();auto supported=[](Point a,Point b){return !(a.x>100&&std::abs(a.y)<20)&&!(b.x>100&&std::abs(b.y)<20);};
 const bool forkValid=route.AimClear(fork,{0,0,0},{1,0,0},50,first,supported);
 std::printf("RPD_FORK_AIM valid=%u xyz=%.3f,%.3f,%.3f\n",unsigned(forkValid),first.x,first.y,first.z);
 Check(forkValid&&first.y>.05f&&first.x<100,"unsupported straight graph branch must choose a connected supported alternative before entry");
 Check(route.AimClear(fork,{49,0,0},{1,0,0},30,second,supported)&&second.y>10&&second.x<100,"approaching fork continues along the selected supported branch");
 route.Reset();
 Check(!route.RebuildRecovery(graph,{0,0,2.8f},{1,0,0}),"graph-only recovery remains strict about decks");
 Check(route.RebuildRecoverySupported(graph,{0,0,2.8f},{1,0,0},[](Point p,float actual){return p.z<1&&actual<3;}),"verified real surface can correct a 2.8m graph-height bias");
 Check(!route.RebuildRecoverySupported(graph,{0,0,2.8f},{1,0,0},[](Point,float){return false;}),"no real same-deck support must still refuse recovery");
 Check(route.RebuildRecoverySupported(graph,{0,-4.5f,2.8f},{1,0,0},[](Point p,float){return p.y<-3&&p.z<1;}),"supported lane must survive an unsupported median graph reference");
 Check(route.Recovery(4,frame)&&std::abs(frame.center.y+4.5f)<.01f,"recovery samples retain the real lane instead of returning to the median");
 Check(route.Recovery(4,frame,-2)&&std::abs(frame.center.y+2.5f)<.01f&&std::abs(frame.center.z)<.01f,"a checked lane alternative can clear the wall without changing road or deck");
 Check(route.Recovery(4,frame,100)&&std::abs(frame.center.y+6)<.01f,"lane alternative stays inside the six metre road envelope");
 Check(route.Recovery(4,frame)&&std::abs(frame.center.y+4.5f)<.01f,"candidate lane queries cannot mutate the live recovery anchor");
 Check(!route.Recovery(4,frame,NAN),"invalid lane offsets cannot reach placement");
 Check(route.RebuildRecoveryClear(graph,{0,0,0},{1,0,0},[](Point,float){return true;},[](Point p,Point){return p.y<-3;}),"a centre-supported blocked median cannot hide a clear full-width lane");
 Check(route.Recovery(4,frame)&&frame.center.y<-3&&std::abs(frame.center.z)<.01f,"clear recovery anchor retains its checked lane and deck");
 Check(!route.RebuildRecoveryClear(graph,{0,0,0},{1,0,0},[](Point,float){return true;},[](Point,Point){return false;}),"no full-width anchor must refuse placement");
 Graph stacked{{{-100,0,0},{0,0,0},{200,0,0},{-100,0,4},{0,0,4},{200,0,4}},{{0,1},{1,2},{3,4},{4,5}}};
 route.Reset();
 Check(route.RebuildRecoverySupported(stacked,{0,0,2.8f},{1,0,0},[](Point p,float){return p.z<1;}),"surface-verified lower deck can be selected beneath an overlapping graph edge");
 Check(route.Recovery(12,frame)&&std::abs(frame.center.z)<.01f,"rebuilding recovery cannot reselect the closer but unverified upper deck");
 route.Reset();
 Check(route.RebuildRecoverySupported(stacked,{0,0,2.8f},{1,0,0},[](Point p,float){return p.z>3;}),"surface verification may identify the upper deck independently of graph bias");
 Check(route.Recovery(12,frame)&&std::abs(frame.center.z-4)<.01f,"upper deck recovery remains on its verified connected graph edge");
 {
  using namespace hot_pursuit_arrest;
  Countdown rpd,normal;
  for(unsigned i=0;i<79;++i)Check(!rpd.Advance(.1f,true,true,true,false,true),"RPD AI arrest cannot complete before eight seconds");
  Check(rpd.RecoveryHeld(),"target recovery held throughout eight-second police countdown");
  Check(rpd.Advance(.11f,true,true,true,false,true),"RPD AI arrest completes at eight seconds");
  for(unsigned i=0;i<49;++i)Check(!normal.Advance(.1f,true,true,true,false,false),"normal AI arrest cannot complete before five seconds");
  Check(normal.Advance(.11f,true,true,true,false,false),"normal AI arrest completes at five seconds");
  Countdown handoff;
  Check(!handoff.Advance(1.f,true,true,true,false,true),"AI timer starts on genuine low-speed contact");
  for(unsigned i=0;i<80;++i)Check(!handoff.Advance(.1f,true,true,true,true,true),"nearby human officer has priority over an eight-second AI completion");
  Check(handoff.contact.elapsed==1.f&&handoff.RecoveryHeld(),"player priority preserves AI progress and recovery suppression");
  Check(!handoff.Advance(.3f,false,false,true,true,true)&&!handoff.RecoveryHeld(),"leaving the arrest zone releases recovery even during player priority");
  Check(!PlayerPriority(10,4)&&PlayerPriority(24,1.5f)&&!PlayerPriority(24.1f,0),"player priority respects road deck and proximity");
  Countdown other;
  Check(!other.RecoveryHeld(),"unrelated target recovery remains available");
 }
 std::printf("RPD_REINFORCEMENT_PASS %u checks; connected placement, bridge isolation, arrest timing, priority and recovery suppression\n",checks);return 0;
 }catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
