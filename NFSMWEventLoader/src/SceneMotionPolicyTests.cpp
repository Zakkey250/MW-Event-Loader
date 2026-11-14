#include "SceneMotionPolicy.h"
#include <cstdio>
#include <stdexcept>
static unsigned checks=0;
static void Check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
int main(){try{
 using namespace mwel_scene_motion;
 auto still=Measure({0,0,0},{0,0,0},{0,1,0},1.f/30);
 Check(still.valid&&still.speed==0&&still.longitudinal==0&&Skid(still,0)==0,"stationary intros must not roll/squeal");
 auto forward=Measure({0,0,0},{0,1,0},{0,1,0},1.f/30);
 Check(forward.valid&&std::abs(forward.longitudinal-30)<.001f&&std::abs(forward.lateral)<.001f,"m/s and forward wheel sign");
 auto reverse=Measure({0,0,0},{0,-1,0},{0,1,0},1.f/30);
 Check(reverse.valid&&std::abs(reverse.longitudinal+30)<.001f,"reverse rolls backwards");
 auto slide=Measure({0,0,0},{1,1,0},{0,1,0},1.f/30);
 Check(slide.valid&&Skid(slide,0)==8&&slide.rev>still.rev,"moving drift supplies bounded native skid");
 Check(Skid(slide,100)==0,"artist burnout keeps its native tyre sound");
 Check(!Measure({0,0,0},{100,0,0},{1,0,0},1.f/30).valid,"cut cannot create extreme wheelspin");
 Check(!Measure({0,0,0},{1,0,0},{1,0,0},0).valid,"paused clock has no motion command");
 Check(!Measure({0,0,0},{1,0,0},{1,0,0},1).valid,"stale frame rejected");
 Check(!Measure({0,0,0},{1,0,0},{0,0,0},.1f).valid,"invalid heading rejected");
 Check(!Measure({0,0,0},{NAN,0,0},{1,0,0},.1f).valid,"nonfinite source rejected");
 Check(!Measure({0,0,0},{1,0,0},{1,0,0},.1f,NAN).valid,"nonfinite cue rejected");
 Check(Skid(Motion{},0)==0,"unbound/invalid cache silent");
 auto hill=Measure({0,0,0},{0,1,.4f},{0,1,.4f},1.f/30);
 Check(hill.valid&&std::abs(hill.longitudinal-30)<.001f,"grade does not become lateral skid");
 auto slow=Measure({0,0,0},{.05f,0,0},{0,1,0},1.f/30);
 Check(Skid(slow,0)==0,"low-speed placement does not squeal");
 std::printf("SCENE_MOTION_PASS checks=%u\n",checks);return 0;
 }catch(const std::exception& e){std::printf("SCENE_MOTION_FAIL %s\n",e.what());return 1;}}
