#include "../src/NativeCollisionBuild.h"
#include <iostream>
#include <chrono>
#include <random>
using namespace native_collision_build;
int main(int argc,char** argv){try{
 bool refused=false;try{Bytes b{nullptr,0};b.Read<unsigned>(0);}catch(...){refused=true;}if(!refused)throw std::runtime_error("truncated span accepted");
 std::vector<unsigned char> malformed(128);refused=false;try{Article({malformed.data(),malformed.size()});}catch(...){refused=true;}if(!refused)throw std::runtime_error("empty article accepted");
 if(argc!=3)throw std::runtime_error("game and independent Python collision cache required");
 auto before=std::chrono::steady_clock::now();CollisionWorld actual,expected;std::vector<uint16_t> groups;Build(std::filesystem::path(argv[1]),actual,groups);
 if(!expected.Load(argv[2],0)||actual.triangles.size()!=expected.triangles.size()||groups.size()!=actual.triangles.size())throw std::runtime_error("independent decoder triangle count differs");
 auto groupFile=ReadFile(std::filesystem::path(argv[2]).parent_path()/L"groups.bin",8000012);Bytes packedGroups{groupFile.data(),groupFile.size()};
 if(packedGroups.Read<uint32_t>(0)!=0x31474D57||packedGroups.Read<uint32_t>(4)!=1||packedGroups.Read<uint32_t>(8)!=groups.size()||groupFile.size()!=12+groups.size()*2)throw std::runtime_error("independent group metadata invalid");
 std::map<std::pair<unsigned,unsigned>,unsigned> actualGroups,expectedGroups;
 for(unsigned i=0;i<groups.size();++i){++actualGroups[{groups[i],actual.triangles[i].wall}];++expectedGroups[{packedGroups.Read<uint16_t>(12+i*2),expected.triangles[i].wall}];}
 if(actualGroups!=expectedGroups)throw std::runtime_error("independent scenery activation group counts differ");
 // Compare geometry via independent ray queries, so float rounding and BVH
 // partition choices cannot turn this into a test of identical code/layout.
 std::mt19937 random(42);unsigned floorHits=0,wallHits=0;
 for(unsigned i=0;i<3000;++i){auto& t=expected.triangles[random()%expected.triangles.size()];auto n=Cross(Sub(t.p[1],t.p[0]),Sub(t.p[2],t.p[0]));float length=sqrt(Dot(n,n));if(length<1e-5f){--i;continue;}Vec3 c{(t.p[0].x+t.p[1].x+t.p[2].x)/3,(t.p[0].y+t.p[1].y+t.p[2].y)/3,(t.p[0].z+t.p[1].z+t.p[2].z)/3},a{c.x+n.x/length,c.y+n.y/length,c.z+n.z/length},b{c.x-n.x/length,c.y-n.y/length,c.z-n.z/length};RayHit x{},y{};bool ex=expected.Ray(a,b,x,!t.wall,t.wall),ac=actual.Ray(a,b,y,!t.wall,t.wall);if(ex!=ac||(ex&&fabs(x.fraction-y.fraction)>.005f))throw std::runtime_error("independent decoder ray differs");if(ex)(t.wall?wallHits:floorHits)++;}
 if(floorHits<500||wallHits<300)throw std::runtime_error("insufficient geometry coverage");
 std::cout<<"PASS bounded CARP decode triangles="<<actual.triangles.size()<<" independent_floor_rays="<<floorHits<<" independent_wall_rays="<<wallHits<<" seconds="<<std::chrono::duration<double>(std::chrono::steady_clock::now()-before).count()<<"\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
