#include "SceneRoadPath.h"
#include <stdexcept>
#include <cstdio>
using namespace mwel_scene_road;
static unsigned checks=0;
static void Check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
int main(){try{
 const Graph graph{{{-50,0,0},{50,0,0},{200,0,0},{-150,0,0},{-250,0,0},{-150,50,0},{-300,50,0}},{{0,1},{1,2},{0,3},{3,4},{0,5},{5,6}}};
 const Point origin{},forward{1,0,0};Path original;
 Check(original.Build(graph,origin,forward,180,50),"default connected path");
 Frame first;Check(original.Sample(-120,first)&&std::abs(first.center.y)<.01f,"straight preference");
 Path accepted;unsigned attempts=0;
 Check(ChoosePath(graph,origin,forward,180,50,accepted,[](const Path&){return true;},attempts)&&attempts==1,"valid original must remain preferred");
 Check(accepted.samples.size()==original.samples.size()&&accepted.anchor==original.anchor&&accepted.lateral==original.lateral,"valid original unchanged");
 for(unsigned i=0;i<original.samples.size();++i)Check(Length(accepted.samples[i].p-original.samples[i].p)<.0001f,"default sample coordinates changed");
 const auto drivable=[](const Path& path){Frame f;return path.Sample(-120,f)&&f.center.y>10;};
 Check(ChoosePath(graph,origin,forward,180,50,accepted,drivable,attempts)&&attempts==2,"bad straight branch must try connected alternative");
 Check(accepted.selectedBranches.size()==1&&accepted.selectedBranches[0]==1,"geometry-approved branch selected");
 Frame go;Check(accepted.Sample(0,go)&&Length(go.center-origin)<.001f&&std::abs(go.center.z)<.001f&&Dot(go.forward,forward)>.999f,"requested GO locator and direction retained");
 auto retained=accepted.samples;Check(!ChoosePath(graph,origin,forward,180,50,accepted,drivable,attempts,1)&&attempts==1,"candidate bound honored");
 Check(accepted.samples.size()==retained.size()&&accepted.samples.front().p.y==retained.front().p.y,"failed search must not publish partial path");
 Check(!ChoosePath(graph,origin,forward,180,50,accepted,[](const Path&){return false;},attempts)&&attempts==2,"all unsafe continuations stay refused");
 auto disconnected=graph;disconnected.nodes.push_back({-150,0,30});disconnected.nodes.push_back({-400,0,30});disconnected.edges.push_back({7,8});
 Check(!ChoosePath(disconnected,origin,forward,180,50,accepted,[](const Path& path){for(const auto& k:path.samples)if(k.p.z>20)return true;return false;},attempts),"search must not jump to a disconnected bridge");
 Check(!ChoosePath(graph,{0,0,30},forward,180,50,accepted,[](const Path&){return true;},attempts),"start-height bound retained");
 std::printf("SCENE_ROAD_PATH_PASS %u checks; original preference, connected alternatives, unchanged locator, atomic failure and bounded refusal\n",checks);return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
