#define NOMINMAX
#include <windows.h>
#include "NeoSnapshotGeometryApi.h"
#include <iostream>
#include <stdexcept>
#include <thread>
static void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static bool Enabled(const void*,unsigned id){return id!=2&&id!=3;}
static w2mw::CollisionWorld Fixture(){
 w2mw::CollisionWorld world;
 world.triangles={{{{-20,0,-20},{20,0,-20},{20,0,20}},0,0},{{{-20,0,-20},{20,0,20},{-20,0,20}},0,0},{{{4,0,-20},{4,10,-20},{4,10,20}},1,1},{{{4,0,-20},{4,10,20},{4,0,20}},1,1},{{{-10,5,-10},{10,7,-10},{10,7,10}},0,0},{{{-10,5,-10},{10,7,10},{-10,5,10}},0,0}};
 for(unsigned i=0;i<20;++i){float x=-18+float(i);world.triangles.push_back({{{x,0,-19},{x+.2f,1,-19},{x+.2f,0,-18}},1,1});}
 world.nodes={{{-20,-2,-20},{20,12,20},0,0x80000000u|unsigned(world.triangles.size())}};
 Check(world.PrepareQueries(),"fixture invalid");return world;
}
static void Compare(const w2mw::CollisionWorld& source,const event_geometry::WorldQueries& local){
 for(unsigned i=0;i<1200;++i){float x=float(int(i%19)-9),z=float(int((i/19)%19)-9),y=2+float(i%10);
  w2mw::Vec3 from{x,y,z},to{i%2?x+6:x,i%2?y:-1.f,z};
  for(unsigned flags=0;flags<4;++flags){w2mw::RayHit a;event_geometry::RayHit b;
   bool first=source.Ray(from,to,a,(flags&1)!=0,(flags&2)!=0),second=local.Ray(from,to,b,(flags&1)!=0,(flags&2)!=0);
   Check(first==second,"snapshot hit differs");if(first){Check(std::abs(a.fraction-b.fraction)<.0001f,"snapshot nearest hit differs");Check(std::abs(a.point.y-b.point.y)<.0001f,"snapshot floor deck differs");Check(std::abs(a.normal.y-b.normal.y)<.0001f,"snapshot surface normal differs");}
  }
 }
}
int main(){try{
 auto source=Fixture();mwel_async_geometry::Snapshot capture;
 Check(!mwel_async_geometry::Capture(source,{{-20,-2,-20},{20,20,20}},capture,2),"triangle cap truncated silently");
 Check(mwel_async_geometry::Capture(source,{{-20,-2,-20},{20,20,20}},capture),"capture failed");
 mwel_async_geometry::World local(std::move(capture));
 {mwel_async_geometry::Binding binding(local);event_geometry::WorldQueries adapter(mwel_async_geometry::Api());event_geometry::WorldQueries::RayQueryScope query(adapter);Compare(source,adapter);
  std::vector<unsigned> walls;adapter.QueryBox({3,0,-1},{5,8,1},walls,true);Check(walls.size()==2,"wall box filter differs");event_geometry::RayHit hit;Check(adapter.RayCandidates({0,2,0},{8,2,0},hit,walls,true),"candidate wall ray missing");
  bool otherThreadBound=true;std::thread other([&]{otherThreadBound=mwel_async_geometry::Available()!=0;});other.join();Check(!otherThreadBound,"binding leaked across threads");Check(local.Complete(),"in-region queries marked incomplete");
 }
 Check(!mwel_async_geometry::Available(),"binding not restored");
 // Freeze current activation into owned data, and never retain its callback.
 source.triangleEnabled=Enabled;
 Check(mwel_async_geometry::Capture(source,{{-20,-2,-20},{20,20,20}},capture),"filtered capture failed");
 mwel_async_geometry::World filtered(std::move(capture));source.triangleEnabled=nullptr;
 {mwel_async_geometry::Binding binding(filtered);event_geometry::WorldQueries adapter(mwel_async_geometry::Api());event_geometry::RayHit hit;
  Check(!adapter.Ray({0,2,0},{8,2,0},hit,false,true),"disabled scenery leaked into snapshot");
  Check(adapter.Ray({0,2,0},{0,-1,0},hit,true),"floor disappeared");
  Check(!adapter.Ray({0,2,0},{30,2,0},hit,false,true)&&!filtered.Complete(),"uncaptured space accepted as clear");
 }
 // Source lifetime and later scenery changes have no effect on local ownership.
 source.triangles.clear();source.nodes.clear();
 {mwel_async_geometry::Binding binding(local);event_geometry::WorldQueries adapter(mwel_async_geometry::Api());event_geometry::RayHit hit;Check(adapter.Ray({0,2,0},{0,-1,0},hit,true),"snapshot depends on source storage");}
 std::cout<<"PASS 4800 ray/filter comparisons, layered floors, native normals, wall candidates, activation freeze, snapshot budget, TLS isolation, source retirement, incomplete region rejection\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
