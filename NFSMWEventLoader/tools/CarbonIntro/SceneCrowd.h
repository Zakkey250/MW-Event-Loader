#pragma once
// Selected-scene-only visual actors. No persistent game objects or physics.
struct SceneCrowd {
 struct Actor{unsigned id=0,resource=0,flags=0;D3DXMATRIX source{},placed{};float speed=1,delay=0;unsigned loopStart=0,loopEnd=0;bool visible=false;std::vector<float> heights;};
 std::vector<Actor> actors;std::vector<std::unique_ptr<carbonprobe::Renderer>> resources;
 std::vector<std::vector<D3DXVECTOR3>> roots;
 bool prepared=false;unsigned placed=0,moved=0,rejected=0;size_t cpuBytes=0;
 void Clear(){actors.clear();resources.clear();roots.clear();prepared=false;placed=moved=rejected=0;cpuBytes=0;}
 void Load(const std::string& folder){
  Clear();std::ifstream in(folder+"/crowd.bin",std::ios::binary);if(!in)return;
  unsigned h[3];SceneAssets::Read(in,h,3);if(h[0]!=0x31524343||h[1]>24||!h[1]||!h[2]||h[2]>h[1])throw std::runtime_error("Invalid source crowd directory");
  for(unsigned i=0;i<h[1];++i){Actor a;SceneAssets::Read(in,&a.id);SceneAssets::Read(in,&a.resource);SceneAssets::Read(in,&a.flags);SceneAssets::Read(in,&a.source);SceneAssets::Read(in,&a.speed);SceneAssets::Read(in,&a.delay);SceneAssets::Read(in,&a.loopStart);SceneAssets::Read(in,&a.loopEnd);
   if(a.resource>=h[2]||!std::isfinite(a.speed)||a.speed<=0||a.speed>4||!std::isfinite(a.delay)||a.delay<0||a.delay>90||a.loopStart>a.loopEnd||a.loopEnd>10000)throw std::runtime_error("Invalid crowd clock");
   for(float v:*reinterpret_cast<float(*)[16]>(&a.source))if(!std::isfinite(v))throw std::runtime_error("Invalid crowd transform");
   if(std::abs(a.source._14)>.001f||std::abs(a.source._24)>.001f||std::abs(a.source._34)>.001f||std::abs(a.source._44-1)>.001f||fabs(a.source._41)>150||fabs(a.source._42)>150)throw std::runtime_error("Crowd locator outside scene");
   actors.push_back(a);
  }char extra;if(in.read(&extra,1))throw std::runtime_error("Trailing crowd directory");
  for(unsigned i=0;i<h[2];++i){char name[32];sprintf_s(name,"crowd-%02u.cni",i);std::ifstream file(folder+"/"+name,std::ios::binary|std::ios::ate);auto bytes=file.tellg();
   if(bytes<=0||bytes>16*1024*1024||cpuBytes+size_t(bytes)>96*1024*1024)throw std::runtime_error("Crowd resource budget exceeded");cpuBytes+=size_t(bytes);
   auto renderer=std::make_unique<carbonprobe::Renderer>();renderer->Load(folder,name);resources.push_back(std::move(renderer));
  }
  std::ifstream paths(folder+"/crowd-roots.bin",std::ios::binary);SceneAssets::Read(paths,h,3);if(h[0]!=0x31505243||h[1]!=resources.size()||h[2]!=15)throw std::runtime_error("Crowd root directory mismatch");
  for(unsigned i=0;i<h[1];++i){unsigned count;SceneAssets::Read(paths,&count);if(!count||count>1500||resources[i]->FrameCount()!=(count-1)*4+1)throw std::runtime_error("Crowd root clock mismatch");std::vector<D3DXVECTOR3> track(count);SceneAssets::Read(paths,track.data(),count);
   for(auto p:track)if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)||fabs(p.x)>20||fabs(p.y)>20||fabs(p.z)>5)throw std::runtime_error("Invalid crowd root path");roots.push_back(std::move(track));
  }if(paths.read(&extra,1))throw std::runtime_error("Trailing crowd root paths");
 }
 static float Clock(const Actor& a,float elapsed,float duration){
  float t=std::max(0.f,elapsed-a.delay)*a.speed;
  const float begin=(a.flags&0x40)?std::min(a.loopStart/15.f,duration):0,end=(a.flags&0x40)?std::min(a.loopEnd/15.f,duration):duration;
  if((a.flags&0x20)&&end>begin&&t>end)t=begin+std::fmod(t-begin,end-begin);
  return std::min(t,duration);
 }
 // Keep the whole source vehicle sweep clear, rather than only the first grid.
 static bool ClearCars(const SceneAssets& assets,const D3DXVECTOR3& local){
  for(unsigned i=0;i<assets.participants;++i)for(size_t k=0;k<assets.cars[i].size();k+=8){
   auto& m=assets.cars[i][k];float x=local.x-m._41,y=local.y-m._42;
   if(x*x+y*y<10.24f)return false;
  }return true;
 }
 static bool GroundPosition(D3DXMATRIX& m){
  float h;D3DXVECTOR3 n;D3DXVECTOR3 center(m._41,m._42,m._43);
  if(!sceneGround.Query(center,h,&n)||n.z<.85f)return false;
  // Footprint and headroom: no ledges, floating feet, walls or ceiling clipping.
  for(auto offset:{D3DXVECTOR3(.4f,0,0),D3DXVECTOR3(-.4f,0,0),D3DXVECTOR3(0,.4f,0),D3DXVECTOR3(0,-.4f,0)}){
   float edge;auto p=center+offset;p.z=h;
   if(!sceneGround.Query(p,edge)||fabs(edge-h)>.28f)return false;
   if(!sceneGround.geometry.nodes.empty()){
    event_geometry::RayHit hit;event_geometry::WorldQueries::RayQueryScope scope(sceneGround.geometry);
    if(sceneGround.geometry.Ray({-p.y,h+.35f,p.x},{-center.y,h+1.9f,center.x},hit,false,false))return false;
   }
  }
  m._43=h+.05f;return true;
 }
 D3DXVECTOR3 Root(unsigned resource,float seconds)const{auto& r=roots[resource];float t=std::clamp(seconds*15,0.f,float(r.size()-1));unsigned i=unsigned(t),j=std::min(i+1,unsigned(r.size()-1));return r[i]+(r[j]-r[i])*(t-i);}
 void Place(const D3DXMATRIX& stage,const SceneAssets& assets){
  if(prepared)return;prepared=true;
  for(auto& a:actors){
   for(unsigned attempt=0;attempt<17;++attempt){
    auto local=a.source;
    if(attempt){const float angle=float((attempt-1)%8)*D3DX_PI/4;const float radius=attempt<=8?2.f:4.f;local._41+=cosf(angle)*radius;local._42+=sinf(angle)*radius;}
    auto candidate=local*stage;std::vector<float> heights;bool safe=true;
    // Sample the whole animated walk at half-second intervals before showing
    // an actor. Render interpolates these heights without per-frame world rays.
    for(float t=0;t<=assets.duration+.5f;t+=.5f){
     float time=Clock(a,t,(resources[a.resource]->FrameCount()-1)/60.f);auto root=Root(a.resource,time);root.z=0;
     D3DXVECTOR3 sourcePoint;D3DXVec3TransformCoord(&sourcePoint,&root,&local);
     if(!ClearCars(assets,sourcePoint)){safe=false;break;}
     D3DXVECTOR3 point;D3DXVec3TransformCoord(&point,&root,&candidate);auto feet=candidate;feet._41=point.x;feet._42=point.y;
     if(!GroundPosition(feet)||(!heights.empty()&&fabs(feet._43-heights.back())>.5f)){safe=false;break;}heights.push_back(feet._43);
    }
    if(!safe)continue;
    bool overlap=false;for(auto& b:actors)if(b.visible){auto x=b.placed._41-candidate._41,y=b.placed._42-candidate._42;if(x*x+y*y<.16f)overlap=true;}
    if(overlap)continue;a.placed=candidate;a.heights=std::move(heights);a.visible=true;++placed;if(attempt)++moved;break;
   }if(!a.visible)++rejected;
  }
 }
 unsigned Draw(IDirect3DDevice9* device,float elapsed,const D3DVIEWPORT9& viewport,const D3DXMATRIX* view,const D3DXMATRIX* projection){
  if(!prepared)return 0;unsigned draws=0;
  for(auto& a:actors)if(a.visible&&elapsed>=a.delay){auto& r=*resources[a.resource];const float duration=(r.FrameCount()-1)/60.f;auto matrix=a.placed;
   float t=std::clamp(elapsed*2,0.f,float(a.heights.size()-1));unsigned i=unsigned(t),j=std::min(i+1,unsigned(a.heights.size()-1));matrix._43=a.heights[i]+(a.heights[j]-a.heights[i])*(t-i);
   draws+=r.DrawFrame(device,r.FrameAt(Clock(a,elapsed,duration)),viewport,false,&matrix,view,projection);}return draws;
 }
} sceneCrowd;
