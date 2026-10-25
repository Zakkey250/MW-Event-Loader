// A native full-physics racer can outlive the player's streamed collision
// region. Reuse the verified installation's real triangles only when the
// native ground query failed near an owned NeoAI body/checked recovery pose.
// Successful native contacts and the W2C loader retain their own surfaces.
namespace neo_native_ground {
using namespace native_event_geometry;
struct Face {Vec3 a;unsigned padA;Vec3 b;unsigned flags;Vec3 c;unsigned padC;};
static_assert(sizeof(Face)==48);
using GroundFn=bool(__thiscall*)(void*,void*,const Vec3*);
using ContactFn=bool(__thiscall*)(void*,const Vec3*,float*,unsigned,void*,unsigned);
GroundFn originalGround=nullptr;ContactFn originalContact=nullptr;
bool installed=false;unsigned supplied=0;
void* Surface(unsigned index){
 if(index>=world.surfaces.size())return nullptr;
 static std::vector<void*> materials;if(materials.size()!=world.surfaces.size())materials.assign(world.surfaces.size(),nullptr);
 if(materials[index])return materials[index];
 // Rockport caches preserve the original Collection key as eight hex digits;
 // named surfaces are used by the W2C converter. Never hash a hex key again.
 const auto* name=world.surfaces[index].data();unsigned key=0;bool encoded=strnlen_s(name,80)==8;
 for(unsigned i=0;encoded&&i<8;++i){char c=name[i];unsigned digit=c>='0'&&c<='9'?c-'0':c>='A'&&c<='F'?c-'A'+10:c>='a'&&c<='f'?c-'a'+10:16;if(digit==16)encoded=false;else key=(key<<4)|digit;}
 auto hash=reinterpret_cast<unsigned(__cdecl*)(const char*)>(0x5CC240);
 auto lookup=reinterpret_cast<void*(__cdecl*)(unsigned,unsigned)>(0x455FD0);
 auto material=lookup(hash("simsurface"),encoded?key:hash(name));
 return materials[index]=material?material:lookup(hash("simsurface"),hash("asphalt_no_leaves"));
}
bool Fill(void* self,const Vec3* point){
 if(!self||!point||!loaded||W2CActive()||!MWELNeoGroundQueryAllowed(&point->x))return false;
 auto previous=Begin();RayHit hit;bool found=world.Ray({point->x,point->y+1.25f,point->z},{point->x,point->y-4.f,point->z},hit,true);
 if(!found){End(previous);return false;}
 const auto& t=world.triangles[hit.triangle];
 auto material=Surface(t.surface);
 if(!material){End(previous);return false;}
 Face face{t.p[0],0,t.p[1],2,t.p[2],0};
 if(Cross(Sub(face.b,face.a),Sub(face.c,face.a)).y<0)std::swap(face.b,face.c);
 memcpy(self,&face,sizeof(face));auto bytes=static_cast<BYTE*>(self);
 *reinterpret_cast<unsigned*>(bytes+48)|=1;*reinterpret_cast<void**>(bytes+56)=material;
 End(previous);
 if(MWELNeoLoggingEnabled()&&supplied++%600==0){char text[256]{};sprintf_s(text,"EVENT_NEOAI_NATIVE_GROUND_FALLBACK at=%.3f,%.3f,%.3f floor=%.3f original_query_failed=1 native_triangle=1 native_group_status=1",point->x,point->y,point->z,hit.point.y);MWELNeoLog(text);}
 return true;
}
bool __fastcall Ground(void* self,void*,void* cache,const Vec3* point){
 if(originalGround(self,cache,point))return true;
 return Fill(self,point);
}
bool __fastcall Contact(void* self,void*,const Vec3* point,float* normal,unsigned reuse,void* cache,unsigned fallback){
 if(originalContact(self,point,normal,reuse,cache,fallback))return true;
 // A second native Contact call performs another stream query and discards
 // this face. Use its verified face-normal helper and identical signed-plane
 // depth calculation; suspension/contact impulses remain entirely native.
 if(!normal||!Fill(self,point))return false;
 reinterpret_cast<void(__thiscall*)(void*,float*)>(0x402690)(self,normal);
 auto face=static_cast<Face*>(self);
 normal[3]=(face->a.x-point->x)*normal[0]+(face->a.y-point->y)*normal[1]+(face->a.z-point->z)*normal[2];
 return std::isfinite(normal[0])&&std::isfinite(normal[1])&&std::isfinite(normal[2])&&std::isfinite(normal[3]);
}
}
bool MWELInstallNeoGroundFallback(){
 using namespace neo_native_ground;if(installed)return true;
 // W2C owns this same native surface, including its contact/cache hooks.
 // Do not reserve those addresses while that loader is still initializing.
 if(GetModuleHandleW(L"World2MWNativeLoader.asi"))return false;
 const BYTE normalGuard[]={0x83,0xEC,0x18,0xD9,0x41,0x10,0x56};
 if(memcmp(reinterpret_cast<void*>(0x402690),normalGuard,sizeof(normalGuard)))return false;
 struct Hook{unsigned address;void* detour;void** original;BYTE bytes[10];unsigned length;};
 Hook hooks[]={
 {0x778F80,Ground,reinterpret_cast<void**>(&originalGround),{0x81,0xEC,0xA8,0,0,0,0x55,0x8B,0xE9},9},
 {0x789CC0,Contact,reinterpret_cast<void**>(&originalContact),{0x8B,0x44,0x24,0x10,0x53,0x55,0x8B,0x6C,0x24,0x0C},10}};
 for(const auto& h:hooks)if(memcmp(reinterpret_cast<void*>(h.address),h.bytes,h.length))return false;
 unsigned made=0;bool ok=true;
 for(const auto& h:hooks){if(MH_CreateHook(reinterpret_cast<void*>(h.address),h.detour,h.original)!=MH_OK){ok=false;break;}++made;}
 if(ok)for(const auto& h:hooks)if(MH_QueueEnableHook(reinterpret_cast<void*>(h.address))!=MH_OK)ok=false;
 if(ok)ok=MH_ApplyQueued()==MH_OK;
 if(!ok){for(unsigned i=0;i<made;++i){MH_DisableHook(reinterpret_cast<void*>(hooks[i].address));MH_RemoveHook(reinterpret_cast<void*>(hooks[i].address));}originalGround=nullptr;originalContact=nullptr;return false;}
 installed=true;return true;
}
void MWELNeoGroundFrame(){
 // Install optional suspension support on the simulation thread only after
 // world loading is complete. ASI initialization must not intercept the
 // native loading allocator's ground/stream queries or race its setup.
 static bool attempted=false;
 if(attempted||!MWELNeoDriverInstalled()||*reinterpret_cast<unsigned*>(0x925E90)!=6||!MWELNeoCourseEnabled())return;
 attempted=true;auto ok=MWELInstallNeoGroundFallback();
 if(MWELNeoLoggingEnabled())MWELNeoLog(ok?"EVENT_NEOAI_NATIVE_GROUND_ACTIVE game_frame=1 loading_complete=1":"EVENT_NEOAI_NATIVE_GROUND_FALLBACK_DISABLED original_native_queries=1");
}
void MWELNeoCompleteGroundLists(void* manager,unsigned flags){
 using namespace neo_native_ground;
 if(!(flags&8)||!installed||!loaded||W2CActive())return;
 auto data=static_cast<BYTE*>(manager);auto list=data+0x5C;
 bool owned=MWELNeoGroundManagerOwned(manager);
 if(!owned)return;
 // This is a batched list, not a flat triangle vector. A nonempty list of
 // batches can still have no floor at this remote vehicle's position.
 // Ask the native decoded-list ray before supplementing it. Successful
 // native support and all existing entries remain untouched.
 Vec3 center=*reinterpret_cast<Vec3*>(data+32);float radius=*reinterpret_cast<float*>(data+44);
 alignas(16) BYTE face[64]{};float normal[4]{};
 if(originalContact(face,&center,normal,0,manager,0))return;
 const BYTE appendGuard[]={0x6A,0xFF,0x68,0xEF,0x22,0x88,0x00};
 if(memcmp(reinterpret_cast<void*>(0x787350),appendGuard,sizeof(appendGuard)))return;
 if(!std::isfinite(radius)||radius<0)return;radius=std::clamp(radius+2.f,8.f,40.f);
 auto previous=Begin();std::vector<unsigned> candidates;
 world.QueryBox({center.x-radius,center.y-6,center.z-radius},{center.x+radius,center.y+2,center.z+radius},candidates,false);
 struct Triangle {Vec3 a;void* material;Vec3 b;unsigned padB;Vec3 c;unsigned padC;};static_assert(sizeof(Triangle)==48);
 unsigned added=0;
 for(auto id:candidates){const auto& t=world.triangles[id];if(t.wall)continue;
  auto material=Surface(t.surface);if(!material)continue;
  Triangle triangle{t.p[0],material,t.p[1],0,t.p[2],0};if(Cross(Sub(triangle.b,triangle.a),Sub(triangle.c,triangle.a)).y<0)std::swap(triangle.b,triangle.c);
  reinterpret_cast<void(__thiscall*)(void*,const Triangle*)>(0x787350)(list,&triangle);++added;
 }
 End(previous);
 if(added&&MWELNeoLoggingEnabled()){static unsigned report=0;if(report++%120==0){char text[240]{};sprintf_s(text,"EVENT_NEOAI_NATIVE_FLOOR_CACHE center=%.3f,%.3f,%.3f triangles=%u native_support_missing=1 native_allocator=1 exact_world_owner=1",center.x,center.y,center.z,added);MWELNeoLog(text);}}
}
