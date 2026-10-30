#include "DriftScorePolicy.h"
#include "DriftDrivePolicy.h"
#include "DriftSplitPolicy.h"
#include "DriftLabels.h"
#include "DriftProximityPolicy.h"
#include "DriftForecastPolicy.h"
#include "DriftContactPolicy.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
unsigned checks=0;void check(bool v){++checks;if(!v){printf("FAIL %u\n",checks);exit(1);}}
int main(){
 using namespace mwel_drift;
 auto run=[](double step){Score s;for(double t=0;t<4.01;t+=step)s.update({t,float(t*20),0,20,35,float(t),true,false});return s;};
 auto s=run(.01);check(s.chain()>120&&s.total==0);auto pending=s.chain();
 // Repeated frame and reversed clocks cannot duplicate the award.
 auto duplicate=s;s.update({s.lastTime,100,0,20,35,10,true,false});check(s.chain()==duplicate.chain());
 s.finish(5);check(s.total==pending&&s.finished&&s.chain()==0);s.finish(6);check(s.total==pending);
 check(!s.update({7,140,0,20,35,14,true,false}));
 auto a=run(.02),b=run(.01);check(std::abs(int(a.chain())-int(b.chain()))<6);
 auto impact=run(.01);impact.update({impact.lastTime+.01,impact.x+.2f,0,20,35,10,true,true});check(!impact.chain()&&!impact.total&&impact.notice==Notice::Lost);
 check(mwel_drift_contact::Wall(500,1000,0,false,false));check(!mwel_drift_contact::Wall(500,1000,1,false,false));
 check(!mwel_drift_contact::Wall(500,1000,0,true,false));check(!mwel_drift_contact::Wall(500,1000,0,false,true));check(!mwel_drift_contact::Wall(1,1000,0,false,false));
 auto scrape=run(.01);scrape.total=500;auto lost=scrape.chain();scrape.update({scrape.lastTime+.01,scrape.x+.2f,0,20,-35,10,true,true});
 check(scrape.total==500&&scrape.chain()==0&&scrape.multiplier()==1&&scrape.noticeAmount==lost);
 // A rejected crash projection may jump to another nearby section of a
 // folded road, then return behind the old high water. It must not poison
 // all subsequent drift awards, or erase already banked points.
 for(unsigned scenario=0;scenario<3;++scenario){
  auto resumed=run(.01);resumed.total=500;double t=resumed.lastTime;float x=resumed.x;
  resumed.update({t+.01,x+.2f,0,20,35,99,true,true});
  for(unsigned i=1;i<=250;++i){
   const bool grounded=scenario!=2||i>50;
   const float p=scenario==0?2.f+i*.002f:scenario==1?2.f-i*.002f:2.f+i*.002f;
   resumed.update({t+.01+i*.02,x+.2f+i*.4f,0,20,35,p,grounded,false});
   if(scenario==1)check(resumed.chain()==0);
  }
  check(resumed.total==500&&!resumed.finished);
  check(scenario==1?resumed.chain()==0:resumed.chain()>0);
  if(scenario!=1)check(!resumed.progressRearm);
 }
 {auto resumed=run(.01);auto t=resumed.lastTime;auto x=resumed.x;resumed.update({t+.01,x+.2f,0,20,35,99,true,true});
  for(unsigned i=1;i<=200;++i)resumed.update({t+.01+i*.02,x+.2f+i*.4f,0,20,35,2,true,false});
  check(resumed.chain()==0&&resumed.progressRearm); // motion without course progress
  for(unsigned i=201;i<=400;++i)resumed.update({t+.01+i*.02,x+.2f+i*.4f,0,20,35,2+i*.002f,true,i%10==0});
  check(resumed.chain()==0&&resumed.multiplier()==1); // contact never releases
 }
 // Continuity bonus is independent of direction changes and drive layout.
 for(double step:{.01,.02,1./60.,1./30.}){
  Score hold,zigzag;for(unsigned i=0;i*step<=22;++i){auto t=i*step;Sample v{t,float(t*25),0,25,35,float(t),true,false};hold.update(v);v.angle=(i/10)%2?-35.f:35.f;zigzag.update(v);check(hold.multiplier()==zigzag.multiplier());}
  check(hold.multiplier()==10&&hold.chain()==zigzag.chain());
 }
 {Score timed;timed.chainSeconds=1.999;check(timed.multiplier()==1);timed.chainSeconds=2;check(timed.multiplier()==2);timed.chainSeconds=4;check(timed.multiplier()==3);timed.chainSeconds=18;check(timed.multiplier()==10);
  timed.bank(20);check(timed.multiplier()==1);timed.chainSeconds=20;timed.lose(21);check(timed.multiplier()==1);
  auto drift=run(.01);auto m=drift.multiplier();auto before=drift.chainSeconds;auto t=drift.lastTime;auto x=drift.x;
  for(unsigned i=1;i<=100;++i)drift.update({t+i*.01,x+i*.2f,0,20,0,10+i*.01f,true,false});
  check(drift.multiplier()==m&&drift.chainSeconds==before);
 }
 auto tele=run(.01);tele.update({tele.lastTime+.01,1000,0,20,35,10,true,false});check(!tele.chain());
 // Short recovery snaps must not look like a legitimate fast body sample.
 auto snap=run(.01);snap.update({snap.lastTime+.01,snap.x+3,0,20,35,10,true,false});check(!snap.chain()&&!snap.total);
 // A stopped simulation repeats its clock, even after many wall-clock frames.
 auto frozen=run(.01);auto frozenPoints=frozen.chain();for(int i=0;i<1000;++i)frozen.update({frozen.lastTime,frozen.x,0,20,35,10,true,false});check(frozen.chain()==frozenPoints&&!frozen.total);
 for(float angle:{0.f,8.f,81.f,180.f}){Score v;for(int i=0;i<500;++i)v.update({i*.01f,i*.2f,0,20,angle,i*.01f,true,false});check(v.standing()==0);}
 for(bool ground:{false,true}){Score v;for(int i=0;i<500;++i)v.update({i*.01f,0,0,20,35,i*.01f,ground,false});check(v.standing()==0);}
 auto grace=run(.01);auto award=grace.chain();auto time=grace.lastTime;auto x=grace.x;
 for(int i=1;i<=151;++i)grace.update({time+i*.01,float(x+i*.2),0,20,0,10,true,false});check(grace.total==award&&grace.chain()==0);
 Score reverse;for(int i=0;i<500;++i)reverse.update({i*.01f,i*.2f,0,20,35,-i*.02f,true,false});check(reverse.chain()<10);
 Score invalid;check(!invalid.update({std::numeric_limits<double>::quiet_NaN()}));check(!invalid.primed);
 auto pause=run(.01);pause.update({pause.lastTime+5,pause.x,0,20,35,10,true,false});check(!pause.chain()&&!pause.total);
 std::array<Score,16> field{};std::array<float,16> progress{};field[3].total=200;field[1].total=100;field[2].pending=150;
 auto order=Order(field,progress,4);check(order[0]==3&&order[1]==2&&order[2]==1&&order[3]==0);
 for(unsigned i=0;i<16;++i){field[i].total=Maximum;progress[i]=float(i);}order=Order(field,progress,16);check(order[0]==15&&order[15]==0);
 auto huge=run(.01);huge.pending=Maximum;huge.total=Maximum;huge.finish(10);check(huge.total==Maximum);
 Score linked;for(unsigned i=0;i<4000;++i)linked.update({i*.01f,i*.3f,0,30,(i/300)%2?35.f:-35.f,i*.01f,true,false});check(linked.multiplier()==10);
 Score slow;for(unsigned i=0;i<4000;++i)slow.update({i*.01f,i*.2f,0,20,(i/300)%2?35.f:-35.f,i*.01f,true,false});check(slow.multiplier()==10);
 Score small;for(unsigned i=0;i<500;++i)small.update({i*.01f,i*.21f,0,21,(i/50)%2?35.f:-35.f,i*.01f,true,false});check(small.multiplier()==3);
 // Whole-combo multiplication includes points earned before the link.
 Score whole;whole.pending=250;whole.chainSeconds=2;check(whole.chain()==500);whole.finish(1);check(whole.total==500);
 {using namespace mwel_drift_proximity;Hull a{},b{};b.x=3;check(std::abs(Clearance(a,b)-1)<.001f);b.y=6;check(Clearance(a,b)>100);b.y=0;b.x=0;check(Clearance(a,b)==0);b.fx=1;b.fz=0;check(Clearance(a,b)==0);b.x=4;check(std::abs(Clearance(a,b)-1)<.001f);check(NearWall(.15f)&&NearWall(1.5f)&&!NearWall(0)&&!NearWall(2));check(NearCar(2)&&!NearCar(0)&&!NearCar(3));}
 Score plain,wall,car,both;for(int i=0;i<300;++i){Sample x{i*.01f,i*.3f,0,30,35,i*.01f,true,false};plain.update(x);x.nearWall=true;wall.update(x);x.nearWall=false;x.nearCar=true;car.update(x);x.nearWall=true;both.update(x);}check(std::abs(double(wall.chain())/plain.chain()-1.25)<.01);check(car.chain()==wall.chain());check(std::abs(double(both.chain())/plain.chain()-1.5)<.01);
 Score bonusStill;for(int i=0;i<300;++i)bonusStill.update({i*.01f,0,0,30,35,i*.01f,true,false,true,true});check(bonusStill.chain()==0);
 Score stationaryProgress;for(unsigned i=0;i<1000;++i)stationaryProgress.update({i*.01f,i*.2f,0,20,35,0,true,false});auto prior=stationaryProgress.standing();for(unsigned i=1000;i<2000;++i)stationaryProgress.update({i*.01f,i*.2f,0,20,35,0,true,false});check(stationaryProgress.standing()==prior);
 order=Order(field,progress,16,1u<<15);check(order[15]==15&&order[0]==14);
 check(Labels("LANGUAGES/JAPANESE.BIN")[0]==L"\u5408\u8a08");check(Labels("LANGUAGES/FRENCH.BIN")[1]==L"D\u00c9RAPAGE");check(Labels("unknown")[0]==L"TOTAL");
 {using namespace mwel_drift_forecast;Route r;r.length=1000;for(unsigned i=0;i<=20;++i)r.knots.push_back({i*50.f,i%8<4?.01f:-.01f});
  Score observed;observed.total=100;observed.pending=40;observed.lastTime=10;Performance p;p.lastSpeed=30;
  auto predicted=Predict(r,observed,p);check(predicted.complete&&predicted.geometry&&predicted.added>0&&predicted.total>observed.standing());
  check(observed.total==100&&observed.pending==40&&!observed.finished);auto repeated=Predict(r,observed,p);check(predicted.total==repeated.total&&predicted.steps==repeated.steps);
  auto reversed=r;for(auto& k:reversed.knots)k.curve=-k.curve;check(Predict(reversed,{},p).total==Predict(r,{},p).total);
  auto straight=r;for(auto& k:straight.knots)k.curve=0;Score banked;banked.total=100;check(Predict(straight,banked,p).total==100);
  auto two=r;two.laps=2;check(Predict(two,{},p).total>Predict(r,{},p).total);two.lap=1;check(Predict(two,{},p).remaining==1000&&Predict(two,{},p).complete);
  auto bad=r;bad.knots[2].station=0;check(!Predict(bad,{},p).geometry);bad=r;bad.station=10000;check(!Predict(bad,{},p).geometry);bad=r;bad.knots[1].curve=std::numeric_limits<float>::quiet_NaN();check(!Predict(bad,{},p).geometry);
  Score capped;capped.total=Maximum-1;check(Predict(r,capped,p).total<=Maximum);observed.finish(10);check(Predict(r,observed,p).total==observed.total&&Predict(r,observed,p).complete);
 }
 check(SplitLabel("LANGUAGES/JAPANESE.BIN")==L"\u30b9\u30d7\u30ea\u30c3\u30c8\u30dd\u30a4\u30f3\u30c8"&&SplitLabel("LANGUAGES/ENGLISH.BIN")==L"SPLIT POINTS");
 check(mwel_drift_drive::AwardScale(0)==1&&mwel_drift_drive::AwardScale(1)==1);
 check(mwel_drift_drive::AwardScale(.5f)==.85f&&mwel_drift_drive::AwardScale(.25f)==.85f);
 check(mwel_drift_drive::AwardScale(-1)==1&&mwel_drift_drive::AwardScale(std::numeric_limits<float>::quiet_NaN())==1);
 check(mwel_drift_drive::AwardScale(.5f,true)==.5f&&mwel_drift_drive::AwardScale(.25f,true)==.5f);
 check(mwel_drift_drive::AwardScale(0,true)==1&&mwel_drift_drive::AwardScale(1,true)==1);
 check(mwel_drift_drive::AwardScale(-1,true)==1&&mwel_drift_drive::AwardScale(std::numeric_limits<float>::quiet_NaN(),true)==1);
 {Score rear,player,opponent;player.awardScale=mwel_drift_drive::AwardScale(.5f,true);opponent.awardScale=mwel_drift_drive::AwardScale(.5f,false);
  for(unsigned i=0;i<4000;++i){Sample s{i*.01,i*.3f,0,30,(i/300)%2?35.f:-35.f,i*.01f,true,false,true,true};rear.update(s);player.update(s);opponent.update(s);check(rear.multiplier()==player.multiplier()&&rear.multiplier()==opponent.multiplier());}
  check(std::abs(double(player.standing())/rear.standing()-.5)<.0001&&std::abs(double(opponent.standing())/rear.standing()-.85)<.0001);
  rear.finish(40);player.finish(40);opponent.finish(40);check(std::abs(double(player.total)/rear.total-.5)<.0001&&std::abs(double(opponent.total)/rear.total-.85)<.0001);}
 {Score rear,all;all.awardScale=.85f;for(unsigned i=0;i<4000;++i){Sample s{i*.01,i*.3f,0,30,(i/300)%2?35.f:-35.f,i*.01f,true,false,true,true};rear.update(s);all.update(s);check(rear.multiplier()==all.multiplier());}check(std::abs(double(all.standing())/rear.standing()-.85)<.0001);rear.finish(40);all.finish(40);check(std::abs(double(all.total)/rear.total-.85)<.0001);}
 {std::array<unsigned,16> p{};p[0]=500;p[1]=200;p[2]=700;auto d=mwel_drift_split::Compare(p,3,0,0);check(d.valid&&d.points==-200&&d.opponent==2);d=mwel_drift_split::Compare(p,3,2,0);check(d.valid&&d.points==200&&d.opponent==0);d=mwel_drift_split::Compare(p,3,0,4);check(d.valid&&d.points==300&&d.opponent==1);p[2]=500;check(mwel_drift_split::Compare(p,3,0,0).points==0);check(!mwel_drift_split::Compare(p,1,0,0).valid&&!mwel_drift_split::Compare(p,17,0,0).valid&&!mwel_drift_split::Compare(p,3,3,0).valid&&!mwel_drift_split::Compare(p,3,0,1).valid&&!mwel_drift_split::Compare(p,3,0,6).valid);}
 printf("PASS drift scoring %u checks\n",checks);
}
