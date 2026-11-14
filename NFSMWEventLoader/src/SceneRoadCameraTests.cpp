#include "SceneRoadCamera.h"
#include <cstdio>
#include <stdexcept>
using namespace mwel_scene_road;
unsigned checks=0;
void Check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
float Dot3(Point a,Point b){return a.x*b.x+a.y*b.y+a.z*b.z;}
int main(){try{
 Check(SubjectInView({0,-10,2},{0,0,1},{0,0,1},24),"actual participant in frame");
 Check(!SubjectInView({0,-10,2},{0,-20,2},{0,0,1},24),"camera facing wall or sea cannot certify participant framing");
 Check(!SubjectInView({0,-10,2},{0,0,1},{30,0,1},24),"missing side actor framing rejected");
 Check(!SubjectInView({0,-10,2},{0,0,1},{0,500,1},24),"distant point cannot stand in for visible vehicle");
 Check(SubjectInView({0,0,10},{0,0,0},{0,0,1},24),"vertical camera has valid view basis");
 Check(!SubjectInView({0,0,10},{0,0,0},{100,0,1},24),"vertical camera does not accept off-screen actor");
 Check(!SubjectInView({0,0,10},{0,0,0},{0,0,1},NAN),"invalid lens refused");
 Check(ContinuousRoadGrade({0,0,10},{.001f,0,10.004f}),"stationary source tolerates millimetre collision quantization");
 Check(!ContinuousRoadGrade({0,0,10},{.001f,0,13}),"stationary source cannot jump road decks");
 Check(!ContinuousRoadGrade({0,0,10},{1,0,11}),"steep discontinuity remains refused");
 {const CameraFrame f{{0,0,10},{1,0,0},{0,1,0},{0,0,1}};
  auto e=CameraCorridorEye(f,{2,3,12});Check(e.x==2&&e.y==3&&e.z==12,"safe low shot unchanged");
  e=CameraCorridorEye(f,{30,-40,100});Check(e.x==3.5f&&e.y==-8&&e.z==16,"render-only roofs and bridge side beams bounded");
  e=CameraCorridorEye(f,{-30,40,0});Check(e.x==-3.5f&&e.y==8&&e.z==10.75f,"camera stays above the selected road deck");}
 {Point eye{10,0,10},original=eye;
  Check(!ClipCameraBoom({0,0,1},eye,[](Point,Point,float&){return false;})&&eye.x==original.x&&eye.z==original.z,"clear shot retains exact source eye");
  unsigned probes=0;Check(ClipCameraBoom({0,0,1},eye,[&](Point a,Point b,float& hit){++probes;hit=(5-a.x)/(b.x-a.x);return true;}),"aerial boom crossing facade must be clipped");
  Check(probes==9&&eye.x<4.1f&&eye.z<5,"eye volume including corners stays one metre before facade");
  eye={10,0,10};Check(ClipCameraBoom({0,0,1},eye,[](Point a,Point,float& hit){hit=.1f;return a.y>.3f;}),"off-axis wall edge clips eye even when central ray is clear");
  eye={10,0,10};Check(!ClipCameraBoom({0,0,1},eye,[](Point,Point,float& hit){hit=NAN;return true;}),"invalid hit cannot move the eye");}
 Graph graph;graph.nodes={{-200,0,-20},{0,0,0},{200,0,20}};graph.edges={{0,1},{1,2}};
 Path path;Check(path.Build(graph,{0,3,0},{1,0,0},100,100),"graded road build");
 const auto retained=path.samples;
 auto ground=[](Point p,float& h){h=p.x*.1f+p.y*.02f;return true;};CameraFrame f;
 Check(FitCameraFrame(path,0,ground,f),"camera at GO");Check(std::abs(f.center.x)<.001f&&std::abs(f.center.y-3)<.001f&&std::abs(f.center.z-.06f)<.001f,"exact reference position and ground height");
 Check(std::abs(f.forward.z/f.forward.x-.1f)<.001f,"source grade retained");
 Check(std::abs(Dot3(f.forward,f.right))<.0001f&&std::abs(Dot3(f.forward,f.up))<.0001f&&std::abs(Dot3(f.right,f.up))<.0001f,"orthogonal camera basis");
 Check(std::abs(Dot3(Cross3(f.right,f.forward),f.up)-1)<.0001f,"right handed camera basis");
 for(float station:{-path.anchor,path.samples.back().station-path.anchor})Check(FitCameraFrame(path,station,ground,f)&&Finite(f.up),"camera at path endpoint");
 Check(FitCameraFrame(path,0,[](Point,float&){return false;},f)&&std::abs(f.center.z)<.001f,"missing collision retains connected graph height");
 Check(path.samples.size()==retained.size()&&path.anchor>0,"camera must not rewrite vehicle path");
 for(unsigned i=0;i<retained.size();++i)Check(path.samples[i].station==retained[i].station&&path.samples[i].p.x==retained[i].p.x&&path.samples[i].p.y==retained[i].p.y&&path.samples[i].p.z==retained[i].p.z,"vehicle choreography unchanged");
 Graph curve;curve.nodes={{-200,0,0},{0,0,0},{150,100,0},{300,200,0}};curve.edges={{0,1},{1,2},{2,3}};
 Check(path.Build(curve,{-30,0,0},{1,0,0},100,120),"connected curve");
 CameraFrame previous;Check(FitCameraFrame(path,-50,ground,previous),"curve beginning");
 for(int i=-499;i<=1000;++i){Check(FitCameraFrame(path,i*.1f,ground,f),"curve sample");Check(Dot3(previous.forward,f.forward)>.999f,"no graph vertex heading snap");previous=f;}
 Check(!FitCameraFrame(Path{},0,ground,f),"empty path refused");
 std::printf("SCENE_ROAD_CAMERA_PASS %u checks; grade, boundaries, continuous curve and unchanged vehicle path\n",checks);return 0;
 }catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
