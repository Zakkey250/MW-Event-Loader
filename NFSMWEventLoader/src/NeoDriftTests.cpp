#include "../../World2MW/src/NeoAI/Driver.h"
#include "NeoDriftIntent.h"
#include "NeoDriftRoad.h"
#include "NeoDriftSweep.h"
#include "DriftLinePolicy.h"
#include "NeoPostGoalCoast.h"
#include "NeoNativePowerTrim.h"
#include "NeoNativeSlipLead.h"
#include "../../World2MW/src/NeoAI/JamWatch.h"
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <limits>
static unsigned checks=0;
static void CheckAt(bool good,unsigned line){++checks;if(!good){printf("FAIL %u line=%u\n",checks,line);std::exit(1);}}
#define Check(value) CheckAt((value),__LINE__)
int main(){
 // The input lead must equal the accepted native wheel feedback evaluated
 // with a small predicted outward slip; it may not invent a pose correction.
 for(float side:{-1.f,1.f}){
  const float request=.4f*side,beta=.6f*side,rate=.4f*side;
  const float adjusted=NeoNativeSlipLead(true,request,beta,rate);
  Check(std::abs(mcd::nativeDriftSteering(adjusted,.05f*side,beta)-
      mcd::nativeDriftSteering(request,.05f*side,beta+.05f*side))<.00001f);
  Check(adjusted*side<request*side);
  Check(NeoNativeSlipLead(true,request,beta,0)==request);
  Check(NeoNativeSlipLead(true,request,beta,-rate)==request);
  Check(NeoNativeSlipLead(true,request,.4f*side,rate)==request);
  Check(NeoNativeSlipLead(false,request,beta,rate)==request);
  Check(NeoNativeSlipLead(true,request,beta,std::numeric_limits<float>::quiet_NaN())==request);
  Check(std::abs(NeoNativeSlipLead(true,request,beta,100.f*side))<=mcd::NativeDriftTurnLimit);
  Check(std::abs(NeoNativeSlipLead(true,request,.5f*side+side*.00001f,rate)-request)<.00001f);
 }
 // Observed launch mismatch: projected station=0, a nearby moving racer,
 // body speed ~3m/s. Only measured net chassis travel may clear the jam.
 {mwneo::Vehicle car;car.heading={0,0,1};car.velocity={0,0,3};car.speed=3;car.dt=.05f;
  mwneo::Guidance guide;guide.status=1;guide.confidence=1;guide.forward={0,0,1};guide.progress=0;
  guide.leadVehicle=mwneo::RaceTrafficPush;guide.obstacleDistance=6;guide.leadSpeed=12;
  mwneo::JamWatch original,native;
  Check(!original.Update(car,guide,mwneo::Recovery::Normal));
  Check(!native.Update(car,guide,mwneo::Recovery::Normal,true));
  car.position.z=2.1f;
  Check(!native.Update(car,guide,mwneo::Recovery::Normal,true)&&!native.active);
  Check(!original.Update(car,guide,mwneo::Recovery::Normal)&&original.active);
  for(unsigned scenario=0;scenario<8;++scenario){
   auto v=car;v.position={};auto g=guide;mwneo::JamWatch jam;
   Check(!jam.Update(v,g,mwneo::Recovery::Normal,true));v.position.z=2.1f;
   auto recovery=mwneo::Recovery::Normal;
   if(scenario==0)v.position.z=.1f; // spinning wheels / no body progress
   if(scenario==1)v.velocity={};
   if(scenario==2)g.leadSpeed=0;
   if(scenario==3)g.status=2;
   if(scenario==4)g.confidence=.5f;
   if(scenario==5)recovery=mwneo::Recovery::Reverse;
   if(scenario==6)v.heading={0,0,-1};
   if(scenario==7)g.predictiveAvoidance|=mwneo::PendingGateReacquire;
   for(unsigned i=0;i<70;++i)jam.Update(v,g,recovery,true);
   Check(jam.active&&jam.age>=3);
  }
 }
 for(float side:{-1.f,1.f}){NeoNativePowerTrim trim;
  Check(trim.Step(true,.02f,1,side*.6f,0)==1); // sustained natural angle
  Check(trim.Step(true,.02f,1,side*.2f,side*1.5f)==1); // ordinary entry
  Check(trim.Step(true,.02f,1,side*.5f,side*.9f)==.5f);
  Check(trim.Step(true,.02f,.2f,side*.5f,0)==.2f); // cannot add power
  Check(trim.Step(true,.05f,1,side*.5f,0)==.5f); // short pedal hold
  for(unsigned i=0;i<5;++i)trim.Step(true,.05f,1,side*.5f,0);
  Check(trim.Step(true,.02f,1,side*.6f,0)==1);
  trim.Step(true,.02f,1,side*.5f,side*.9f);
  Check(trim.Step(false,.02f,1,side*.5f,side*.9f)==1&&trim.held==0);
 }
 {NeoPostGoalCoast coast;
  Check(coast.Step({0,0,0},25,1000,true));
  Check(coast.Step({20,0,0},20,2000,true));
  Check(!coast.Step({30,0,0},18,2500,true));
  Check(!coast.Step({31,0,0},18,2600,true));
  coast={};Check(!coast.Step({0,0,0},25,1000,false));
  Check(!coast.Step({1,0,0},25,1100,true));
  coast={};Check(coast.Step({0,0,0},25,1000,true));
  Check(!coast.Step({10,0,0},25,5000,true));
  coast={};Check(!coast.Step({0,0,0},1,1000,true));
 }
 {mwneo::Vehicle car;car.position={0,0,0};car.velocity={0,0,27};car.speed=27;
  const float close=mwel_drift_line::NativeAimRate(car,{4,0,14}),far=mwel_drift_line::NativeAimRate(car,{4,0,25});
  Check(std::abs(close)>mcd::NativeDriftTurnLimit&&std::abs(far)<mcd::NativeDriftTurnLimit);
  Check(std::abs(far+mwel_drift_line::NativeAimRate(car,{-4,0,25}))<.00001f);
  Check(!std::isfinite(mwel_drift_line::NativeAimRate(car,{0,0,0})));
  car.velocity={};Check(!std::isfinite(mwel_drift_line::NativeAimRate(car,{4,0,25})));
 }
 {std::vector<mwel_drift_line::Bend> bends{{100,340,220,1,2.4f,true}};
  auto local=bends;local[0].begin-=130;local[0].end-=130;local[0].apex-=130;
  auto mirror=bends;mirror[0].side=-1;
  auto ordinary=bends;ordinary[0].linked=false;float variation=0;
  for(float station=100;station<=340;station+=.5f){
   const float value=mwel_drift_line::Offset(bends,station,28);
   Check(std::isfinite(value)&&std::abs(value)<=2.2001f);
   Check(std::abs(value-mwel_drift_line::Offset(local,station-130,28))<.0001f);
   Check(std::abs(value+mwel_drift_line::Offset(mirror,station,28))<.0001f);
   variation=std::max(variation,std::abs(value-mwel_drift_line::Offset(ordinary,station,28)));
  }
  Check(variation>1);
  for(float edge:{100.f,340.f})Check(std::abs(mwel_drift_line::Offset(bends,edge+.01f,28)-mwel_drift_line::Offset(bends,edge-.01f,28))<.001f);
  auto tight=bends;tight[0].turn=10;auto tightOff=tight;tightOff[0].linked=false;
  Check(mwel_drift_line::Offset(tight,180,28)==mwel_drift_line::Offset(tightOff,180,28));
 }
 {
  NeoNativeTurnRecovery recovery;
  Check(!recovery.Update(true,.2f,.2f,.016f));
  Check(recovery.Update(true,.73f,.6f,.016f));
  // Crossing zero slip does not release ownership while rotation remains
  // above the native steering envelope, even after the settling interval.
  for(unsigned i=0;i<20;++i)Check(recovery.Update(true,0,.9f,.016f));
  for(unsigned i=0;i<5;++i)Check(recovery.Update(true,.1f,.1f,.02f));
  Check(recovery.Update(true,.55f,.1f,.02f)); // settling must be continuous
  for(unsigned i=0;i<5;++i)Check(recovery.Update(true,.1f,.1f,.02f));
  Check(recovery.Update(true,.1f,std::numeric_limits<float>::quiet_NaN(),.02f));
  for(unsigned i=0;i<5;++i)Check(recovery.Update(true,.1f,.1f,.02f));
  for(unsigned i=0;i<10;++i)recovery.Update(true,.1f,.1f,.02f);
  Check(!recovery.active);
  Check(recovery.Update(true,-.73f,-.6f,.016f));
  Check(!recovery.Update(false,-.73f,-.6f,.016f)&&!recovery.active);
 }

 // Forecast the physical chassis, independent of a route centre line.
 {mwneo::Vehicle car;car.heading={1,0,0};car.velocity={20,0,0};car.speed=20;
  car.halfWidth=1;car.halfLength=2;unsigned queries=0;
  auto open=[&](auto a,auto b,auto){++queries;Check(mwneo::Finite(a)&&mwneo::Finite(b));return true;};
  Check(NeoDriftSweep(car,0,0,open)==.8f&&queries>0);
  // A wall twelve metres ahead must be seen by the leading chassis corner.
  auto wall=[](auto,auto b,auto){return b.x<12;};
  const float hit=NeoDriftSweep(car,0,0,wall);Check(hit>=.4f&&hit<=.51f);
  // Rotation can strike a side wall even when the centre trajectory clears.
  auto side=[](auto,auto b,auto){return std::abs(b.z)<1.4f;};
  Check(NeoDriftSweep(car,0,0,side)==.8f);
  Check(NeoDriftSweep(car,1,0,side)<.4f);
  queries=0;car.speed=std::numeric_limits<float>::quiet_NaN();
  Check(NeoDriftSweep(car,0,0,open)==-1&&queries==0);
  car.speed=20;car.halfWidth=std::numeric_limits<float>::infinity();
  Check(NeoDriftSweep(car,0,0,open)==-1&&queries==0);
 }
 struct Knot{float p[3];float station;};std::vector<Knot> arc;
 for(unsigned i=0;i<8;++i){const float t=float(i)*.35f;arc.push_back({{100*std::cos(t),0,100*std::sin(t)},float(i)*35});}
 for(unsigned i=0;i<200;++i){const float k=NeoDriftRoadCurvature(arc,float(i));Check(std::abs(k-.01f)<.00001f);}
 auto bends=mwel_drift_line::Bends(arc);Check(bends.size()==1&&bends[0].side==1&&bends[0].apex>90&&bends[0].apex<150);
 auto broad=arc;for(auto& p:broad){p.p[0]*=3;p.p[2]*=3;p.station*=3;}
 auto broadBends=mwel_drift_line::Bends(broad);Check(broadBends.size()==1&&broadBends[0].end-broadBends[0].begin>600);
 std::vector<mwel_drift_line::Bend> corner={{100,220,160,1}};
 Check(mwel_drift_line::Offset(corner,100,28)<-2);
 Check(mwel_drift_line::Offset(corner,160,28)>.2f);
 Check(mwel_drift_line::Offset(corner,220,28)<-2);
 Check(mwel_drift_line::EntryPrepared(corner,100,-1));
 Check(mwel_drift_line::EntryPrepared(corner,130,0)&&mwel_drift_line::EntryPrepared(corner,160,0));
 Check(!mwel_drift_line::EntryPrepared(corner,100,1)&&!mwel_drift_line::EntryPrepared(corner,200,-1));
 Check(mwel_drift_line::SteeringLook(40)==28&&mwel_drift_line::SteeringLook(0)==18);
 Check(mwel_drift_line::SteeringLook(0,true)==14&&mwel_drift_line::SteeringLook(60,true)==22);
 Check(std::abs(mwel_drift_line::SteeringLook(28,true)-15.2f)<.0001f);
 auto localCorner=corner;for(auto& b:localCorner){b.begin-=130;b.apex-=130;b.end-=130;}
 for(float s=0;s<350;s+=2)Check(std::abs(mwel_drift_line::Offset(corner,s,28)-mwel_drift_line::Offset(localCorner,s-130,28))<.0001f);
 for(float s=0;s<350;s+=.25f)Check(std::abs(mwel_drift_line::Offset(corner,s+.25f,28)-mwel_drift_line::Offset(corner,s,28))<.05f);
 for(unsigned i=0;i<arc.size();++i)arc[i]={{float(i)*70,0,0},float(i)*70};
 for(unsigned i=0;i<200;++i)Check(NeoDriftRoadCurvature(arc,float(i))==0);
 Check(NeoDriftRoadPeak(arc,0,200)==0);
 // Peaks belong to authored knots, including a turn beyond a long chord.
 std::vector<Knot> sparse={{{0,0,0},0},{{100,0,0},100},{{120,0,20},128.28427f},{{120,0,100},208.28427f}};
 Check(NeoDriftRoadPeak(sparse,0,110)>.01f);
 Check(NeoDriftRoadPeak(sparse,0,-1)==0);
 // Distance-aware braking keeps the approach faster than the actual bend,
 // while retaining the same apex tyre/steering limit and sign symmetry.
 {
  std::vector<Knot> road;for(unsigned i=0;i<=20;++i)road.push_back({{float(i)*5,0,0},float(i)*5});
  for(unsigned i=1;i<=30;++i){const float a=float(i)*.05f;road.push_back({{100+50*std::sin(a),0,50*(1-std::cos(a))},100+50*a});}
  const float far=NeoDriftRoadSpeed(road,0,150,30,12),near=NeoDriftRoadSpeed(road,80,70,30,12),apex=NeoDriftRoadSpeed(road,120,30,30,12);
  Check(far>near&&near>apex&&far>apex+5&&apex<25);
  auto mirror=road,shift=road;for(auto& q:mirror)q.p[2]=-q.p[2];for(auto& q:shift)q.station+=70;
  for(float s=0;s<120;s+=3){
   const auto expected=NeoDriftRoadSpeed(road,s,50,30,12);
   Check(std::abs(expected-NeoDriftRoadSpeed(mirror,s,50,30,12))<.0001f);
   Check(std::abs(expected-NeoDriftRoadSpeed(shift,s+70,50,30,12))<.0001f);
  }
  Check(!std::isfinite(NeoDriftRoadSpeed(road,0,50,30,-1)));
  Check(!std::isfinite(NeoDriftRoadSpeed(road,0,500,30,12)));
 }
 mwneo::Vehicle v;v.speed=22;v.dt=1.f/60;v.heading={0,0,1};v.velocity={0,0,22};
 mwneo::Guidance g;g.status=1;g.aim={10,0,25};g.forward={0,0,1};g.confidence=1;g.obstacleDistance=1000;
 mwneo::Profile p;mwneo::Controls c;c.valid=true;c.targetSpeed=26;c.curvature=.02f;c.steering=.1f;
 // Projected extents must contain every actual chassis corner at drift angle.
 for(unsigned i=0;i<=80;++i){
  const float angle=float(i)*.01f;auto car=v;car.heading={std::sin(angle),0,std::cos(angle)};
  const auto planned=NeoDriftPlanningVehicle(car,true);
  const auto entry=NeoDriftPlanningVehicle(v,true,true);
  for(int x:{-1,1})for(int z:{-1,1}){
   const float cornerX=float(x)*car.halfWidth*std::cos(angle)+float(z)*car.halfLength*std::sin(angle);
   const float cornerZ=-float(x)*car.halfWidth*std::sin(angle)+float(z)*car.halfLength*std::cos(angle);
   Check(std::abs(cornerX)<=planned.halfWidth+.001f&&std::abs(cornerZ)<=planned.halfLength+.001f);
   if(angle<=.65f)Check(std::abs(cornerX)<=entry.halfWidth+.001f&&std::abs(cornerZ)<=entry.halfLength+.001f);
  }
  Check(NeoDriftPlanningVehicle(car,false).heading.x==car.heading.x);
 }
 NeoDriftIntent intent;float hb=0;auto original=c;
 // Existing physical sliding can safely resume within the swept corridor;
 // this is not permission to initiate another HB kick away from the line.
 for(unsigned bad=0;bad<15;++bad){
  auto car=v;auto guide=g;float clear=.8f,beta=-.3f,curve=.01f,yaw=.3f,speed=30,line=1.1f,room=2;bool safe=true;unsigned mode=2;
  if(bad==1)mode=1;if(bad==2)safe=false;if(bad==3)clear=.79f;
  if(bad==4)beta=-.1f;if(bad==5)beta=-.6f;if(bad==6)curve=-.01f;
  if(bad==7)yaw=.8f;if(bad==8)speed=15;if(bad==9)line=2.1f;
  if(bad==10)room=std::numeric_limits<float>::quiet_NaN();
  if(bad==11)car.speed=8;if(bad==12)guide.status=0;
  if(bad==13)guide.forward={0,0,-1};if(bad==14)beta=std::numeric_limits<float>::quiet_NaN();
  Check(NeoDriftNativeContinuation(mode,car,guide,safe,clear,beta,curve,yaw,speed,line,room)==(bad==0));
 }
 {NeoDriftIntent resume;resume.attempts=2;resume.StopAttempt();resume.sectionEdge=g.edge;resume.sectionLap=g.lap;
  auto q=g;q.count=2;q.samples[0].position={1.1f,0,0};q.samples[1].position={1.1f,0,10};
  for(unsigned n=0;n<2;++n){q.samples[n].widthLeft=q.samples[n].widthRight=6;}
  bool regained=false;
  for(unsigned n=0;n<18;++n){auto input=original;resume.Apply(2,v,q,p,true,true,-.3f,input,hb,.01f,true,0,false,.8f,.3f,30);
   Check(hb==0&&resume.attempts==2);regained|=resume.nativeRetryResumed;}
  Check(regained&&!resume.awaitingRetry&&resume.turning);
 }
 // Native entry releases the rear brake as soon as physical slip is
 // observed, instead of keeping the wheels locked for the entire timer.
 for(float frame:{1.f/30,1.f/60,1.f/120}){
  NeoDriftIntent native;auto car=v;car.dt=frame;
  for(unsigned n=0;n<8;++n){c=original;native.Apply(2,car,g,p,true,false,0,c,hb,.02f,true,0,true,-1,0);if(hb>.25f)break;}
  Check(native.entry>0&&hb>.25f);
   c=original;native.Apply(2,car,g,p,true,true,-.35f,c,hb,.02f);
  Check(native.nativeSlide&&native.entry==0&&hb==0&&c.throttle>.9f);
  for(unsigned n=0;n<12;++n){c=original;native.Apply(2,car,g,p,true,false,-.12f,c,hb,.02f);Check(hb==0&&native.attempts==1);}
 }
 // Recorded AWD spin: low beta during a switch hid an already large yaw.
 // Do not start or continue an HB kick beyond native turn authority. A
 // valid settled entry still works, at all three physics tick intervals.
 for(float frame:{1.f/30,1.f/60,1.f/120})for(float side:{-1.f,1.f}){
  auto car=v;car.dt=frame;
  for(float yaw:{side*.66f,side*1.08f,std::numeric_limits<float>::quiet_NaN()}){
   NeoDriftIntent native;
   for(unsigned n=0;n<8;++n){c=original;native.Apply(2,car,g,p,true,false,0,c,hb,.02f,true,0,true,-1,yaw);Check(hb==0);}
  }
  NeoDriftIntent native;
  for(unsigned n=0;n<8;++n){c=original;native.Apply(2,car,g,p,true,false,0,c,hb,.02f,true,0,true,-1,side*.2f);if(hb>.25f)break;}
  Check(hb>.25f&&native.entry>0);
  c=original;native.Apply(2,car,g,p,true,false,side*.03f,c,hb,.02f,true,0,true,-1,side*.9f);
  Check(hb==0&&native.entry==0);
  c=original;native.Apply(2,car,g,p,true,false,side*.05f,c,hb,.02f,true,0,true,-1,side*.2f);
  Check(hb==0); // cancelled pulse does not resume from its old timer
 }
 // A wide, physically clear slide is not cancelled merely because it is
 // displaced from an authored centre line. A real impending wall causes
 // a controlled exit without a fresh handbrake pulse or steering jump.
 {auto q=g;q.lateralError=9;NeoDriftIntent native;
  c=original;Check(native.Apply(2,v,q,p,true,true,-.5f,c,hb,.02f,true,0,true,.8f));
  const float before=c.steering;c=original;c.throttle=1;
  Check(!native.Apply(2,v,q,p,true,true,-.5f,c,hb,.02f,true,0,true,.2f));
  Check(native.rejection==18&&hb==0&&c.throttle==0&&c.brake>=.25f);
  Check(std::abs(c.steering-before)<=2.5f*v.dt+.0001f);
  const auto nativeSteering=c.steering;
  Check(!NeoDriftGripHandoff(true,false,true,18,v,-.5f,p,c)&&c.steering==nativeSteering);
  // Crossing zero is not a settled exit: retain the original side and
  // damping while the chassis is still swinging through the travel axis.
  for(float beta:{-.1f,-.03f,.02f,.04f}){
   const float last=native.steering;c=original;
   Check(!native.Apply(2,v,q,p,true,true,beta,c,hb,.02f,true,0,true,.8f));
   Check(native.nativeExiting&&native.exitSide==1&&native.rejection==18&&hb==0);
   Check(std::abs(c.steering-last)<=2.5f*v.dt+.0001f);
  }
  for(unsigned i=0;i<90&&native.nativeExiting;++i){c=original;native.Apply(2,v,q,p,true,false,0,c,hb,0,true,0,true,.8f);}
  Check(!native.nativeExiting);
  // Invalid/absent geometry restores the conservative old corridor guard.
  native.Reset();c=original;Check(!native.Apply(2,v,q,p,true,false,0,c,hb,.02f,true,0,true,-1));
  Check(native.rejection==10);
 }
 // A fresh, clear same-side native slide may leave the old exit state. A
 // shorter sweep, excess speed/yaw, opposite swing or lost support may not.
 for(unsigned condition=0;condition<7;++condition){
  NeoDriftIntent resume;resume.nativeExiting=true;resume.exitSide=1;resume.exitSlip=.4f;resume.attempts=2;
  resume.sectionEdge=g.edge;resume.sectionLap=g.lap;
  auto input=original;
  const float clear=condition==1?.79f:.8f,yaw=condition==2?.8f:.3f,roadSpeed=condition==3?15.f:30.f;
  const float beta=condition==4?.3f:-.3f,curve=condition==5?-.01f:.01f;
  resume.Apply(2,v,g,p,true,true,beta,input,hb,curve,condition!=6,0,true,clear,yaw,roadSpeed);
  Check(resume.nativeExitResumed==(condition==0));
  if(condition==0)Check(!resume.nativeExiting&&hb==0&&resume.attempts==2&&input.throttle>.9f);
 }
 // Native slide continuity survives threshold noise but not a genuine
 // straightening, obstacle or lost physical control. No extra HB pulse.
 for(float frame:{1.f/30,1.f/60,1.f/120}){
  auto car=v;car.dt=frame;NeoDriftIntent hold;
  for(unsigned n=0;n<12;++n){auto input=original;hold.Apply(2,car,g,p,true,false,0,input,hb,.02f);}
  {auto input=original;hold.Apply(2,car,g,p,true,true,.2f,input,hb,.02f);}
  for(unsigned n=0;n<unsigned(1/frame);++n){auto input=original;hold.Apply(2,car,g,p,true,false,.12f,input,hb,.02f,true,0,false);
   Check(hold.nativeSlide&&!hold.awaitingRetry);}
  for(unsigned n=0;n<unsigned(.4f/frame)+1;++n){auto input=original;hold.Apply(2,car,g,p,true,false,.04f,input,hb,.02f);}
  Check(!hold.nativeSlide);
  {auto input=original;hold.Apply(2,car,g,p,true,true,.2f,input,hb,.02f);}
  auto blocked=g;blocked.obstacleDistance=1;
  {auto input=original;Check(!hold.Apply(2,car,blocked,p,true,true,.2f,input,hb,.02f));}
  Check(!hold.nativeSlide&&hb==0);
 }
 // Native tyres can keep sliding after a temporary veto. Safe continuation
 // regains wheel control without repeatedly locking the rear wheels.
 {NeoDriftIntent resume;resume.attempts=1;resume.StopAttempt();
  for(unsigned i=0;i<29;++i){c=original;Check(!resume.Apply(2,v,g,p,true,true,-.4f,c,hb,.02f));Check(hb==0);}
  for(unsigned i=0;i<10;++i){c=original;resume.Apply(2,v,g,p,true,true,-.4f,c,hb,.02f);Check(hb==0);}
  Check(!resume.awaitingRetry&&resume.turning&&resume.attempts==1);
  resume.StopAttempt();auto obstacle=g;obstacle.obstacleDistance=1;
  for(unsigned i=0;i<40;++i){c=original;Check(!resume.Apply(2,v,obstacle,p,true,true,-.4f,c,hb,.02f));Check(hb==0);}
  Check(resume.awaitingRetry);
 }
 // Leaving drift intent cannot drop the conversion from the driver's travel
 // frame to the actual chassis. Existing braking and recovery still win.
 for(float side:{-1.f,1.f}){
  auto grip=original;grip.steering=.05f;grip.brake=.8f;grip.throttle=.1f;
  Check(NeoDriftGripHandoff(true,false,true,6,v,side*.5f,p,grip));
  Check(grip.steering*side>.8f&&grip.brake==.8f&&grip.throttle==.1f);
  auto before=grip;Check(!NeoDriftGripHandoff(true,true,true,0,v,side*.5f,p,grip));Check(grip.steering==before.steering);
  for(unsigned guard=0;guard<5;++guard){grip=original;auto car=v;
   if(guard==3)grip.reverse=true;if(guard==4)car.speed=10;
   Check(!NeoDriftGripHandoff(guard!=0,false,guard!=1,guard==2?16u:6u,car,side*.5f,p,grip));Check(grip.steering==original.steering);
  }
 }
 // Adjacent or same-pace racers must not be treated like stationary walls.
 for(unsigned reason=0;reason<=mcdai::Fault;++reason){
  const bool expected=reason==mcdai::Applied||reason==mcdai::Grip||reason==mcdai::Paused||reason==mcdai::StaleCommand;
  Check(NeoDriftSteeringFeedback(true,reason)==expected);Check(!NeoDriftSteeringFeedback(false,reason));
 }
 {auto car=v;car.speed=30;auto tandem=g;tandem.leadVehicle=mwneo::RaceTrafficPush;tandem.leadSpeed=30;tandem.obstacleDistance=1;
  Check(!NeoDriftTrafficHazard(car,tandem));tandem.leadSpeed=25;Check(!NeoDriftTrafficHazard(car,tandem));
  tandem.leadSpeed=0;Check(NeoDriftTrafficHazard(car,tandem));tandem.leadSpeed=10;tandem.obstacleDistance=30;Check(!NeoDriftTrafficHazard(car,tandem));
  tandem.obstacleDistance=10;Check(NeoDriftTrafficHazard(car,tandem));tandem.leadVehicle=0;Check(NeoDriftTrafficHazard(car,tandem));
  tandem.leadVehicle=mwneo::RaceTrafficPush;tandem.leadSpeed=30;
  tandem.aim={5,0,35}; // Broad physical turn; this test isolates tandem traffic.
  for(unsigned i=0;i<60;++i){c=original;Check(intent.Apply(2,car,tandem,p,true,true,-.6f,c,hb,.01f));Check(c.brake==0&&c.throttle>.5f);}
  intent.Reset();
 }
 // An outside rejoin may need a much tighter arc than the authored road.
 // It must not inherit the old full-throttle permission from that road.
 {auto car=v;car.speed=28;car.velocity={0,0,28};auto q=g;q.aim={15,0,15};
  NeoDriftIntent tight;c=original;c.throttle=1;
  tight.Apply(2,car,q,p,true,true,.3f,c,hb,.01f,true,0,true,.8f);
  Check(c.throttle==0&&c.brake>.25f);
 }
 for(unsigned i=0;i<8;++i){c=original;Check(!intent.Apply(2,v,g,p,true,false,0,c,hb,.02f,true,0,false));}
 Check(intent.rejection==17&&!intent.turning&&hb==0);
 Check(!intent.Apply(0,v,g,p,true,false,0,c,hb));Check(c.steering==original.steering&&c.throttle==original.throttle);
 for(unsigned i=0;i<60;++i){c=original;const bool active=intent.Apply(2,v,g,p,true,true,-.2f,c,hb);if(i>8)Check(active);Check(std::isfinite(c.steering)&&std::abs(c.steering)<=1.f&&c.brake==0&&hb==0);if(active)Check(c.throttle==1);}
 // Obstacle, airborne and recovery always retain the ordinary driver's brake.
 for(unsigned scenario=0;scenario<5;++scenario){
  auto q=g;auto car=v;c=original;c.brake=.7f;
  if(scenario==0)q.obstacleDistance=1;
  if(scenario==1)q.predictiveAvoidance=mwneo::SupportedAirborne;
  if(scenario==2)c.recovery=mwneo::Recovery::Reverse;
  if(scenario==3)c.reverse=true;
  if(scenario==4)q.status=2;
  Check(!intent.Apply(1,car,q,p,true,false,0,c,hb));Check(c.brake==.7f&&hb==0);
 }
 // One Arcade brake pulse per bend; straight travel rearms it.
 intent.Reset();unsigned pulses=0;
 for(unsigned i=0;i<120;++i){c=original;intent.Apply(1,v,g,p,true,i>12,0,c,hb);if(hb>.25f)++pulses;}
 Check(pulses>=7&&pulses<=8);
 for(unsigned i=0;i<60;++i){c=original;c.curvature=0;intent.Apply(1,v,g,p,true,false,0,c,hb);}
 for(unsigned i=0;i<8;++i){c=original;intent.Apply(1,v,g,p,true,false,0,c,hb);}
 Check(hb>.25f);
 // A safety reset cannot leave the planner permanently latched in a bend.
 // Retry requires half a second of safe aligned grip, and is allowed once.
 intent.Reset();
 for(unsigned i=0;i<12;++i){c=original;intent.Apply(1,v,g,p,true,false,0,c,hb,.02f);}
 for(unsigned i=0;i<30;++i){c=original;intent.Apply(1,v,g,p,true,true,.5f,c,hb,.02f);}
 // A crest suspends physical correction, then resumes through a short safe
 // re-arming gap. It must not poison the rest of the bend's retry budget.
 c=original;Check(!intent.Apply(1,v,g,p,true,false,.5f,c,hb,.02f,false));
 Check(intent.turning&&!intent.awaitingRetry&&hb==0);
 for(unsigned i=0;i<8;++i){c=original;Check(intent.Apply(1,v,g,p,true,false,.5f,c,hb,.02f,true));}
 Check(intent.turning&&!intent.awaitingRetry);
 for(unsigned i=0;i<20;++i){c=original;intent.Apply(1,v,g,p,true,false,.5f,c,hb,.02f,false);Check(hb==0);}
 Check(intent.awaitingRetry&&!intent.turning);
 // Existing sustained safety reset still requires its complete grip settle.
 intent.Reset();for(unsigned i=0;i<12;++i){c=original;intent.Apply(1,v,g,p,true,false,0,c,hb,.02f);}
 for(unsigned i=0;i<30;++i){c=original;intent.Apply(1,v,g,p,true,true,.5f,c,hb,.02f);}
 for(unsigned i=0;i<16;++i){c=original;intent.Apply(1,v,g,p,true,false,0,c,hb,.02f,false);}
 c=original;Check(!intent.Apply(1,v,g,p,true,false,0,c,hb,.02f,false));
 Check(intent.awaitingRetry&&intent.attempts==1&&hb==0);
 for(unsigned i=0;i<29;++i){c=original;Check(!intent.Apply(1,v,g,p,true,false,0,c,hb,.02f));Check(hb==0);}
 unsigned retries=0;
 for(unsigned i=0;i<12;++i){c=original;intent.Apply(1,v,g,p,true,false,0,c,hb,.02f);if(hb>.25f)++retries;}
 Check(retries>=7&&retries<=8&&intent.attempts==2);
 for(unsigned i=0;i<20;++i){c=original;intent.Apply(1,v,g,p,true,true,.5f,c,hb,.02f);}
 for(unsigned i=0;i<90;++i){c=original;Check(!intent.Apply(1,v,g,p,true,false,0,c,hb,.02f));Check(hb==0);}
 // The cap belongs to the progress section, not the entire remaining race.
 // Advancing a real checkpoint releases the exhausted cap but never skips
 // the half-second safety settle after the previous interrupted attempt.
 auto nextSection=g;++nextSection.edge;
 for(unsigned i=0;i<29;++i){c=original;Check(!intent.Apply(1,v,nextSection,p,true,false,0,c,hb,.02f));Check(hb==0);}
 unsigned newSectionPulses=0;
 for(unsigned i=0;i<12;++i){c=original;intent.Apply(1,v,nextSection,p,true,false,0,c,hb,.02f);if(hb>.25f)++newSectionPulses;}
 Check(newSectionPulses>=7&&newSectionPulses<=8);
 // No retry into a narrow wall corridor or an unaligned travel direction.
 for(unsigned scenario=0;scenario<2;++scenario){
  auto retry=intent;retry.StopAttempt();retry.attempts=1;auto q=g;
  if(scenario==0){q.count=2;q.samples[0].widthLeft=q.samples[0].widthRight=1.5f;q.samples[1].position={0,0,10};}
  else q.forward={1,0,0};
  for(unsigned i=0;i<90;++i){c=original;Check(!retry.Apply(1,v,q,p,true,false,0,c,hb,.02f));Check(hb==0);}
 }
 // Corner-speed braking must not prevent power-on entry. A real obstacle
 // still vetoes it in the safety cases above.
 intent.Reset();c=original;c.brake=.8f;c.throttle=0;
 for(unsigned i=0;i<8;++i){c=original;c.brake=.8f;c.throttle=0;intent.Apply(1,v,g,p,true,false,0,c,hb);}
 Check(c.throttle==1&&c.brake==0&&hb>.25f);
 // Intent ramps out instead of immediately reversing its angle on a straight.
 intent.Reset();for(unsigned i=0;i<90;++i){c=original;intent.Apply(2,v,g,p,true,true,-.6f,c,hb);}
 Check(intent.targetSlip<-.55f&&intent.targetSlip>=-.65f);const auto beforeExit=intent.targetSlip;
 for(unsigned i=0;i<5;++i){c=original;c.curvature=0;intent.Apply(2,v,g,p,true,true,-.6f,c,hb);}
 Check(intent.targetSlip<=0&&std::abs(intent.targetSlip-beforeExit)<.07f);
 // Travel rotation follows the provider's checked current arc. A grip-mode
 // predictive steering correction cannot override it or reverse its side.
 c=original;c.curvature=-.01f;intent.Apply(2,v,g,p,true,true,-.6f,c,hb);
 Check(std::abs(intent.pathYaw-440.f/725.f)<.001f);
 auto right=g;right.aim.x=-10;c=original;intent.Apply(2,v,right,p,true,true,-.6f,c,hb);
 Check(std::abs(intent.pathYaw+440.f/725.f)<.001f);
 // Tight future geometry delays entry and brakes before the power phase.
 intent.Reset();auto fast=v;fast.speed=40;auto tight=g;tight.count=1;tight.samples[0].roadCurvature=.04f;
 for(unsigned i=0;i<10;++i){c=original;Check(!intent.Apply(1,fast,tight,p,true,false,0,c,hb,.03f));}
 Check(intent.rejection==12&&c.throttle==0&&c.brake>0&&c.brake<=.6f&&hb==0&&c.targetSpeed<=22.01f);
 intent.Reset();for(unsigned i=0;i<40;++i){c=original;Check(!intent.Apply(1,fast,g,p,true,false,0,c,hb,.004f,true,.04f));}
 Check(intent.rejection==12&&c.throttle==0&&hb==0&&c.targetSpeed<=22.01f);
 // A grip planner's straight chord cannot remove the real corner-speed cap
 // after entry. Meter both modes against the same authored curvature preview.
 for(unsigned mode:{1u,2u}){
  intent.Reset();auto car=v;car.speed=21;auto broadTurn=g;broadTurn.aim={5,0,35};
  for(unsigned i=0;i<90;++i){c=original;c.targetSpeed=42;intent.Apply(mode,car,broadTurn,p,true,true,-.6f,c,hb,.01f,true,.025f);}
  Check(c.targetSpeed>=21.f&&c.targetSpeed<=23.5f&&c.throttle>.8f&&c.throttle<=1&&c.brake==0&&hb==0);
 }
 // A grip target below drift speed cannot bleed off the entry momentum;
 // the checked native-tyre corner ceiling can still trim excessive entry.
 intent.Reset();auto hold=v;hold.speed=22;auto broadHold=g;broadHold.aim={5,0,35};
 for(unsigned i=0;i<90;++i){c=original;c.targetSpeed=10;intent.Apply(2,hold,broadHold,p,true,i>2,-.6f,c,hb,.025f,true,.025f);}
 Check(c.targetSpeed>=21&&c.targetSpeed<=23.5f&&c.throttle>.75f&&c.brake==0);
 // A fast broad entry cannot force that same speed into a tighter next bend.
 intent.Reset();intent.entrySpeed=42;intent.turning=true;
 for(unsigned i=0;i<60;++i){c=original;intent.Apply(2,fast,g,p,true,true,-.6f,c,hb,.03f,true,.04f);}
 Check(c.targetSpeed<=28.51f&&c.throttle==0&&c.brake>0&&c.brake<=.6f);
 // The same physical speed preview must work before the current path bends.
 intent.Reset();for(unsigned i=0;i<20;++i){c=original;Check(!intent.Apply(2,fast,g,p,true,false,0,c,hb,0,true,.04f));Check(c.throttle==0&&c.brake>0&&c.targetSpeed<=22.01f);}
 // A distant bend permits only a bounded approach allowance. A near bend
 // and the checked local steering arc can still require a lower speed.
 for(float roadLimit:{40.f,14.f}){
  intent.Reset();auto straight=g;straight.aim={0,0,50};
  c=original;c.targetSpeed=50;
  Check(!intent.Apply(2,fast,straight,p,true,false,0,c,hb,0,true,.04f,true,-1,
    std::numeric_limits<float>::quiet_NaN(),roadLimit));
  const float expected=std::min(roadLimit,mcd::NativeDriftTurnLimit/.04f+5.f);
  Check(std::abs(c.targetSpeed-expected)<.001f&&c.throttle==0&&c.brake>0);
 }
 // Native power entry never spends another handbrake attempt. Unsafe,
 // overspeed, wrong-way, opposite/straight arcs and unmeasured geometry
 // still require the original preparation/retry path.
 {
  auto q=g;q.aim={-5,0,35};
  Check(NeoDriftPowerEntry(2,v,q,.02f,.05f,28,true,.8f,0));
  for(unsigned scenario=0;scenario<9;++scenario){
   auto car=v;auto guide=q;float clear=.8f,line=0,curve=.02f,cap=28;bool safe=true;unsigned mode=2;
   if(scenario==0)clear=.7f;if(scenario==1)line=1.1f;if(scenario==2)safe=false;
   if(scenario==3)curve=-.02f;if(scenario==4)guide.aim={0,0,50};
   if(scenario==5)guide.forward={1,0,0};if(scenario==6)cap=15;
   if(scenario==7)mode=1;if(scenario==8)guide.aim={-15,0,15};
   Check(!NeoDriftPowerEntry(mode,car,guide,curve,.05f,cap,safe,clear,line));
  }
  intent.Reset();intent.roadCurve=.02f;c=original;
  Check(intent.Apply(2,v,q,p,true,false,0,c,hb,.02f,true,0,false,.8f));
  Check(intent.powerEntry&&hb==0&&intent.attempts==0&&c.throttle==1&&c.brake==0);
  intent.Reset();intent.roadCurve=.02f;intent.sectionEdge=q.edge;intent.sectionLap=q.lap;
  intent.attempts=2;intent.awaitingRetry=true;unsigned active=0;
  for(unsigned i=0;i<40;++i){c=original;active+=intent.Apply(2,v,q,p,true,false,0,c,hb,.02f,true,0,false,.8f);Check(hb==0&&intent.attempts==2);}
  Check(active>0&&intent.powerEntries==1);
  for(unsigned i=0;i<120;++i){c=original;intent.Apply(2,v,q,p,true,false,0,c,hb,.02f,true,0,false,.8f);Check(hb==0&&intent.attempts==2);}
  Check(intent.powerEntries>1);
  const auto entries=intent.powerEntries;
  for(unsigned i=0;i<80;++i){c=original;intent.Apply(2,v,q,p,true,false,0,c,hb,.02f,true,0,false,.7f);Check(!intent.powerEntry&&hb==0&&intent.powerEntries==entries);}
 }
 // Inertia may carry a centred slide; path authority rises smoothly with
 // lateral departure, corner load or an actual approaching obstruction.
 {NeoDriftIntent centre,edge;auto q=g;q.count=2;q.samples[0].position={0,0,0};q.samples[1].position={0,0,10};q.samples[0].widthLeft=q.samples[0].widthRight=8;
  auto car=v;car.position={0,0,0};c=original;centre.Apply(2,car,q,p,true,true,-.5f,c,hb,.004f);
  Check(centre.pathStrength>=.6f&&centre.pathStrength<.7f);
  car.position.x=4;c=original;edge.Apply(2,car,q,p,true,true,-.5f,c,hb,.004f);Check(edge.pathStrength==1);
  car.position.x=0;q.obstacleDistance=30;c=original;centre.Apply(2,car,q,p,true,true,-.5f,c,hb,.004f);Check(centre.pathStrength==1);
 }
 // Excess slip must strengthen countersteer, never reduce it toward beta*0.8.
 for(float side:{-1.f,1.f}){
  intent.Reset();
  for(unsigned i=0;i<90;++i){c=original;intent.Apply(2,v,g,p,true,true,side*.75f,c,hb,side*-.02f);}
  Check(c.steering*side>.99f&&c.throttle<=.55f&&hb==0);
  c=original;Check(!intent.Apply(2,v,g,p,true,true,side*.8f,c,hb,side*-.02f));
  Check(intent.rejection==16&&c.steering==side&&c.throttle==0&&c.brake==0&&hb==0);
  c=original;c.brake=.7f;auto hazard=g;hazard.obstacleDistance=1;
  Check(!intent.Apply(2,v,hazard,p,true,true,side*.8f,c,hb,side*-.02f));
  Check(c.brake==.7f&&c.steering==original.steering);
  c=original;c.brake=.7f;Check(!intent.Apply(2,v,g,p,true,true,side*.8f,c,hb,side*-.02f,false));
  Check(c.brake==.7f&&c.steering==original.steering);
 }
 // Learning stays frozen during intentional slip, without disabling grip guard.
 mwneo::Driver driver;driver.gripGuard=true;g.count=0;
 for(unsigned i=0;i<80;++i){driver.Update(v,g,p,true);Check(driver.response.windows==0&&driver.brakes.windows==0&&driver.gripGuard);}
 printf("PASS %u drift intent, safety, pulse, and learning checks\n",checks);
}
