#pragma once
// Private development probe. It never changes game objects or race state.
#include <windows.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <vector>
#include <fstream>
#include <string>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <map>
#include <memory>
#include <array>
#include "SceneLighting.h"

namespace carbonprobe {
struct Input {float p[3],uv[2],weight[3];unsigned bone[3];};
struct Draw {unsigned first,count,texture;};
struct Vertex {D3DXVECTOR3 p;unsigned color;float u,v;}; // Unlit effect sprites.
struct ActorVertex {D3DXVECTOR3 p,n;float u,v;};
static_assert(sizeof(Input)==44);
class Renderer {
 std::vector<Input> input;std::vector<Draw> draws;std::vector<D3DXMATRIX> matrices;
 std::vector<ActorVertex> output;std::vector<D3DXVECTOR3> normals;std::map<unsigned,IDirect3DTexture9*> textures;
 unsigned frames=0,bones=0,frameRate=15;bool texturesAttempted=false;std::string folder;
 struct Prop{std::unique_ptr<Renderer> renderer;D3DXMATRIX relative;unsigned hash=0;};std::vector<Prop> props;
#ifdef MWEL_CARBON_TORCH_RUNTIME
 struct TorchTrack{unsigned hash=0;std::vector<D3DXVECTOR3> points;};std::vector<TorchTrack> torch;
#endif
 template<class T> void Read(std::ifstream& in,T* data,size_t count){
  if(!in.read(reinterpret_cast<char*>(data),sizeof(T)*count))throw std::runtime_error("Truncated private scene");
 }
public:
 ~Renderer(){for(auto& item:textures)if(item.second)item.second->Release();}
 void Load(const std::string& directory,const std::string& filename="character.cni",bool hasActor=true){
  if(!hasActor){frames=1;frameRate=60;return;}
  folder=directory;std::ifstream in(folder+"/"+filename,std::ios::binary);
  unsigned header[6]{};Read(in,header,6);
  if(header[0]!=0x31494E43||header[1]>100000||!header[1]||header[2]>32||!header[3]||header[3]>10000||!header[4]||header[4]>256||(header[5]!=0&&header[5]!=60))throw std::runtime_error("Invalid private scene header");
  frameRate=header[5]?header[5]:15;
  input.resize(header[1]);draws.resize(header[2]);frames=header[3];bones=header[4];matrices.resize(frames*bones);
  Read(in,input.data(),input.size());Read(in,draws.data(),draws.size());Read(in,matrices.data(),matrices.size());
  char extra;if(in.read(&extra,1))throw std::runtime_error("Trailing private scene data");
  for(auto& d:draws)if(d.first>input.size()||d.count>input.size()-d.first||d.count%3)throw std::runtime_error("Invalid triangle range");
  for(auto& v:input){float sum=0;for(unsigned i=0;i<3;i++){if(v.bone[i]>=bones||!std::isfinite(v.weight[i])||v.weight[i]<0)throw std::runtime_error("Invalid bone reference");sum+=v.weight[i];}
   if(std::abs(sum-1)>.002f)throw std::runtime_error("Invalid skin weights");
   for(float p:v.p)if(!std::isfinite(p))throw std::runtime_error("Nonfinite vertex");
   for(float p:v.uv)if(!std::isfinite(p))throw std::runtime_error("Nonfinite UV");}
  for(auto& m:matrices)for(float p:*reinterpret_cast<float(*)[16]>(&m))if(!std::isfinite(p))throw std::runtime_error("Nonfinite pose");
  output.resize(input.size());
  // CNI's old vertex layout has no normals. Reconstruct smooth bind-pose
  // normals once, welding UV seams by position, then skin them with the actor.
  std::map<std::array<float,3>,D3DXVECTOR3> smooth;
  for(const auto& draw:draws)for(unsigned i=draw.first;i<draw.first+draw.count;i+=3){
   D3DXVECTOR3 a(input[i].p),b(input[i+1].p),c(input[i+2].p),n;auto ab=b-a,ac=c-a;D3DXVec3Cross(&n,&ab,&ac);
   for(unsigned j=0;j<3;++j){auto& p=input[i+j].p;auto key=std::array<float,3>{p[0],p[1],p[2]};auto inserted=smooth.emplace(key,D3DXVECTOR3(0,0,0));inserted.first->second+=n;}
  }
  normals.resize(input.size());for(size_t i=0;i<input.size();++i){auto& p=input[i].p;auto n=smooth[{p[0],p[1],p[2]}];if(D3DXVec3LengthSq(&n)<1e-12f)n=D3DXVECTOR3(0,0,1);D3DXVec3Normalize(&normals[i],&n);}
  if(filename=="character.cni"){
   std::ifstream children(folder+"/props.bin",std::ios::binary);
   if(children){unsigned header[2];Read(children,header,2);if(header[0]!=0x31525043||!header[1]||header[1]>2)throw std::runtime_error("Invalid secondary actors");
    for(unsigned i=0;i<header[1];i++){unsigned hash;Prop prop;Read(children,&hash,1);prop.hash=hash;Read(children,&prop.relative,1);
     for(float v:*reinterpret_cast<float(*)[16]>(&prop.relative))if(!std::isfinite(v))throw std::runtime_error("Invalid prop locator");
     if(std::abs(prop.relative._14)>.001f||std::abs(prop.relative._24)>.001f||std::abs(prop.relative._34)>.001f||std::abs(prop.relative._44-1)>.001f)throw std::runtime_error("Invalid prop matrix");
     char name[40];sprintf_s(name,"prop-%08X.cni",hash);prop.renderer=std::make_unique<Renderer>();prop.renderer->Load(folder,name);
     if(prop.renderer->frames!=frames||prop.renderer->frameRate!=frameRate)throw std::runtime_error("Secondary actor clock mismatch");props.push_back(std::move(prop));
    }char extra;if(children.read(&extra,1))throw std::runtime_error("Trailing props data");
   }
#ifdef MWEL_CARBON_TORCH_RUNTIME
   std::ifstream attachments(folder+"/torch-anchors.bin",std::ios::binary);
   if(attachments){unsigned h[4]{};Read(attachments,h,4);if(h[0]!=0x31415443||h[1]!=2||h[2]!=frames||h[3]!=frameRate||h[3]!=60||props.size()!=2)throw std::runtime_error("Torch attachment clock mismatch");
    for(unsigned i=0;i<2;++i){TorchTrack t;Read(attachments,&t.hash,1);if((t.hash!=0xD639336A&&t.hash!=0xD639336B)||std::none_of(props.begin(),props.end(),[&](const Prop& p){return p.hash==t.hash;}))throw std::runtime_error("Torch attachment model mismatch");
     if(!torch.empty()&&torch[0].hash==t.hash)throw std::runtime_error("Duplicate torch attachment");t.points.resize(frames);Read(attachments,t.points.data(),frames);for(auto v:t.points)if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)||std::abs(v.x)>10000||std::abs(v.y)>10000||std::abs(v.z)>10000)throw std::runtime_error("Invalid torch position");torch.push_back(std::move(t));
    }char extra;if(attachments.read(&extra,1))throw std::runtime_error("Trailing torch data");
   }
#endif

  }
 }
#ifdef MWEL_CARBON_TORCH_RUNTIME
 bool HasTorches()const{return torch.size()==2;}
 bool TorchPositions(float seconds,const D3DXMATRIX& world,float positions[2][3])const{
  if(!HasTorches()||!std::isfinite(seconds)||seconds<0)return false;
  const unsigned frame=seconds>=float(frames-1)/frameRate?frames-1:unsigned(seconds*frameRate);
  for(unsigned i=0;i<2;++i){auto prop=std::find_if(props.begin(),props.end(),[&](const Prop& p){return p.hash==torch[i].hash;});if(prop==props.end())return false;
   const auto transform=prop->relative*world;D3DXVECTOR3 point;D3DXVec3TransformCoord(&point,&torch[i].points[frame],&transform);positions[i][0]=point.x;positions[i][1]=point.y;positions[i][2]=point.z;
  }return true;
 }
#endif
 unsigned FrameCount()const{return frames;}
 unsigned DrawCount()const{unsigned n=unsigned(draws.size());for(auto& p:props)n+=p.renderer->DrawCount();return n;}
 unsigned FrameAt(float seconds)const{return std::min(frames-1,unsigned(std::max(0.f,seconds)*frameRate));}
 void Fade(IDirect3DDevice9* device,const D3DVIEWPORT9& v,float amount){
  // Old private camera packs encoded signed interpolation undershoot as an
  // unsigned scalar. Never turn these invalid percentages into a blackout.
  if(!std::isfinite(amount)||amount>=2147483648.f)return;
  unsigned alpha=unsigned(std::clamp(amount,0.f,100.f)*2.55f);if(!alpha)return;
  IDirect3DStateBlock9* state=nullptr;if(FAILED(device->CreateStateBlock(D3DSBT_ALL,&state)))return;
  struct ScreenVertex{float x,y,z,w;DWORD color;};DWORD color=alpha<<24;
  float x=float(v.X)-.5f,y=float(v.Y)-.5f,r=x+v.Width,b=y+v.Height;
  ScreenVertex q[]={{x,y,0,1,color},{r,y,0,1,color},{x,b,0,1,color},{r,b,0,1,color}};
  device->SetViewport(&v);device->SetVertexShader(nullptr);device->SetPixelShader(nullptr);device->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE);
  device->SetRenderState(D3DRS_ZENABLE,FALSE);device->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
  device->SetRenderState(D3DRS_STENCILENABLE,FALSE);device->SetRenderState(D3DRS_SCISSORTESTENABLE,FALSE);device->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);device->SetRenderState(D3DRS_COLORWRITEENABLE,15);
  device->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);device->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);device->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);device->SetRenderState(D3DRS_BLENDOP,D3DBLENDOP_ADD);
  device->SetTexture(0,nullptr);device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE);device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
  device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,q,sizeof(ScreenVertex));state->Apply();state->Release();
 }
 unsigned DrawFrame(IDirect3DDevice9* device,unsigned frame,const D3DVIEWPORT9& viewport,bool inset=true,const D3DXMATRIX* world=nullptr,const D3DXMATRIX* gameView=nullptr,const D3DXMATRIX* gameProjection=nullptr){
  if(!frames)return 0;
  if(!texturesAttempted){texturesAttempted=true;for(auto& d:draws){if(textures.count(d.texture))continue;char name[40];sprintf_s(name,"/%08X.dds",d.texture);IDirect3DTexture9* t=nullptr;
    if(FAILED(D3DXCreateTextureFromFileA(device,(folder+name).c_str(),&t)))throw std::runtime_error("Private texture unavailable");textures[d.texture]=t;}}
  frame%=frames;D3DXVECTOR3 lo(1e9f,1e9f,1e9f),hi(-1e9f,-1e9f,-1e9f);
  for(size_t i=0;i<input.size();i++){
   auto& v=input[i];D3DXVECTOR3 p(v.p[0],v.p[1],v.p[2]),out(0,0,0);
   for(unsigned j=0;j<3;j++)if(v.weight[j]>0){D3DXVECTOR3 transformed;D3DXVec3TransformCoord(&transformed,&p,&matrices[frame*bones+v.bone[j]]);out+=transformed*v.weight[j];}
   D3DXVECTOR3 normal(0,0,0);for(unsigned j=0;j<3;++j)if(v.weight[j]>0){D3DXVECTOR3 n;D3DXVec3TransformNormal(&n,&normals[i],&matrices[frame*bones+v.bone[j]]);normal+=n*v.weight[j];}
   output[i]={out,normal,v.uv[0],v.uv[1]};
   lo.x=std::min(lo.x,out.x);lo.y=std::min(lo.y,out.y);lo.z=std::min(lo.z,out.z);
   hi.x=std::max(hi.x,out.x);hi.y=std::max(hi.y,out.y);hi.z=std::max(hi.z,out.z);
  }
  IDirect3DStateBlock9* state=nullptr;if(FAILED(device->CreateStateBlock(D3DSBT_ALL,&state)))return 0;
  D3DXMATRIX identity,view,projection;D3DXMatrixIdentity(&identity);
  D3DXVECTOR3 center=(lo+hi)*.5f,eye=center+D3DXVECTOR3(0,-3.4f,.3f),up(0,0,1);
  D3DXMatrixLookAtLH(&view,&eye,&center,&up);D3DXMatrixPerspectiveFovLH(&projection,.75f,float(viewport.Width)/viewport.Height,.05f,100.f);
  device->SetViewport(&viewport);
  if(inset){D3DRECT rect={LONG(viewport.X),LONG(viewport.Y),LONG(viewport.X+viewport.Width),LONG(viewport.Y+viewport.Height)};device->Clear(1,&rect,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xFF24303D,1,0);}
  device->SetFVF(D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_TEX1);
  if(!WorldActorShader().Bind(device,world?*world:identity,gameView?*gameView:view,gameProjection?*gameProjection:projection)){state->Apply();state->Release();return 0;}
  device->SetTransform(D3DTS_WORLD,world?world:&identity);device->SetTransform(D3DTS_VIEW,gameView?gameView:&view);device->SetTransform(D3DTS_PROJECTION,gameProjection?gameProjection:&projection);
  device->SetRenderState(D3DRS_ZENABLE,TRUE);device->SetRenderState(D3DRS_ZWRITEENABLE,TRUE);device->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);
  device->SetRenderState(D3DRS_DEPTHBIAS,0);device->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS,0);
  device->SetRenderState(D3DRS_LIGHTING,FALSE);device->SetRenderState(D3DRS_FOGENABLE,FALSE);device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
  device->SetRenderState(D3DRS_STENCILENABLE,FALSE);device->SetRenderState(D3DRS_SCISSORTESTENABLE,FALSE);device->SetRenderState(D3DRS_COLORWRITEENABLE,15);
  device->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);device->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);device->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);device->SetRenderState(D3DRS_BLENDOP,D3DBLENDOP_ADD);
  device->SetRenderState(D3DRS_ALPHATESTENABLE,TRUE);device->SetRenderState(D3DRS_ALPHAREF,40);device->SetRenderState(D3DRS_ALPHAFUNC,D3DCMP_GREATER);
  device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);
  device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);
  device->SetTextureStageState(0,D3DTSS_TEXCOORDINDEX,0);device->SetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE);
  device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);device->SetTextureStageState(1,D3DTSS_ALPHAOP,D3DTOP_DISABLE);
  device->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);device->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);device->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_LINEAR);device->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_WRAP);device->SetSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_WRAP);
  unsigned done=0;for(auto& draw:draws){device->SetTexture(0,textures[draw.texture]);if(SUCCEEDED(device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,draw.count/3,output.data()+draw.first,sizeof(ActorVertex))))++done;}
  state->Apply();state->Release();
  for(auto& prop:props){auto transform=prop.relative*(world?*world:identity);done+=prop.renderer->DrawFrame(device,frame,viewport,false,&transform,gameView?gameView:&view,gameProjection?gameProjection:&projection);}
  return done;
 }
};
}
