#pragma once
#include <d3d9.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include "../../integration/MWCombatContract.h"
namespace combat_hud {
std::array<mwcombat::RacerHudV1,16> cars{};unsigned count=0;DWORD sampledAt=0;bool visible=false;
struct Vertex {float x,y,z,rhw;DWORD color;};
void Rect(Vertex* v,unsigned& used,float x,float y,float w,float h,DWORD c){
 const Vertex tl{x-.5f,y-.5f,0,1,c},tr{x+w-.5f,y-.5f,0,1,c},bl{x-.5f,y+h-.5f,0,1,c},br{x+w-.5f,y+h-.5f,0,1,c};
 v[used++]=tl;v[used++]=tr;v[used++]=bl;v[used++]=tr;v[used++]=br;v[used++]=bl;
}
bool Active(){return count&&GetTickCount()-sampledAt<500;}
unsigned Glyph(char c){
 // Compact 3x5 numerals/HP label; no font files, texture uploads or workers.
 constexpr unsigned digits[]={0x7B6F,0x2492,0x73E7,0x73CF,0x5BC9,0x79CF,0x79EF,0x7249,0x7BEF,0x7BCF};
 return c>='0'&&c<='9'?digits[c-'0']:c=='H'?0x5BED:c=='P'?0x7BE4:c=='/'?0x12A4:0;
}
void Text(Vertex* vertices,unsigned& used,float x,float y,float pixel,const char* text,DWORD color){
 for(;*text;++text,x+=4*pixel){const auto bits=Glyph(*text);
  for(unsigned row=0;row<5;++row)for(unsigned col=0;col<3;++col)
   if(bits&(1u<<(14-row*3-col)))Rect(vertices,used,x+col*pixel,y+row*pixel,pixel,pixel,color);
 }
}
bool Project(const float* position,const float* matrix,const D3DVIEWPORT9& viewport,float& x,float& y,float& depth){
 // MW render coordinates: physics Z, -X, Y. Same view/projection and lift as
 // the fingerprinted healthbars renderer, without modifying its native data.
 const float p[]={position[2],-position[0],position[1]+1.15f,1};float clip[4]{};
 for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j)clip[i]+=p[j]*matrix[j*4+i];
 for(float value:clip)if(!std::isfinite(value))return false;
 if(clip[3]<.1f||clip[3]>200||clip[2]<0||clip[2]>clip[3])return false;
 x=viewport.X+(clip[0]/clip[3]+1)*viewport.Width*.5f;y=viewport.Y+(1-clip[1]/clip[3])*viewport.Height*.5f;depth=clip[2]/clip[3];
 return x>=viewport.X&&y>=viewport.Y&&x<viewport.X+viewport.Width&&y<viewport.Y+viewport.Height;
}
void StarVertices(Vertex* vertices,unsigned& used,float x,float y,float radius,float depth,DWORD color){
 constexpr float pi=3.14159265359f;
 for(unsigned i=0;i<10;++i){const unsigned next=(i+1)%10;const float a=-pi*.5f+i*pi/5,b=-pi*.5f+next*pi/5;
  const float r=i%2?radius*.43f:radius,r2=next%2?radius*.43f:radius;
  vertices[used++]={x,y,depth,1,color};vertices[used++]={x+std::cos(a)*r,y+std::sin(a)*r,depth,1,color};vertices[used++]={x+std::cos(b)*r2,y+std::sin(b)*r2,depth,1,color};
 }
}
void LeaderStar(IDirect3DDevice9* device,const float* position,const float* matrix){
 if(!device||!visible||!Active()||FAILED(device->TestCooperativeLevel()))return;
 D3DVIEWPORT9 viewport{};float x=0,y=0,depth=0;
 if(FAILED(device->GetViewport(&viewport))||!Project(position,matrix,viewport,x,y,depth))return;
 IDirect3DStateBlock9* state=nullptr;if(FAILED(device->CreateStateBlock(D3DSBT_ALL,&state))||!state)return;
 __try{
  device->SetVertexShader(nullptr);device->SetPixelShader(nullptr);device->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE);
  device->SetTexture(0,nullptr);device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);
  device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE);device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
  // This replaces healthbars' HUD annotation, not a world mesh. The world
  // depth surface is no longer reliable at this HUD pass (reflections/NIS).
  device->SetRenderState(D3DRS_ZENABLE,FALSE);device->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);
  device->SetRenderState(D3DRS_STENCILENABLE,FALSE);device->SetRenderState(D3DRS_SCISSORTESTENABLE,FALSE);device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
  device->SetRenderState(D3DRS_FOGENABLE,FALSE);device->SetRenderState(D3DRS_LIGHTING,FALSE);device->SetRenderState(D3DRS_FILLMODE,D3DFILL_SOLID);
  device->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);device->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);device->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);device->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
  device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE,FALSE);device->SetRenderState(D3DRS_BLENDOP,D3DBLENDOP_ADD);device->SetRenderState(D3DRS_COLORWRITEENABLE,15);device->SetRenderState(D3DRS_SRGBWRITEENABLE,FALSE);
  Vertex vertices[60];unsigned used=0;const float size=std::clamp(viewport.Height/1080.f*14.f,8.f,28.f);
  StarVertices(vertices,used,x,y,size+2,depth,0xEE121820);StarVertices(vertices,used,x,y,size,depth,0xFFFFD44D);
  device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,used/3,vertices,sizeof(Vertex));
 }__finally{state->Apply();state->Release();}
}
void Player(IDirect3DDevice9* device){
 if(!device||!visible||!Active()||FAILED(device->TestCooperativeLevel()))return;
 const mwcombat::HealthV1* player=nullptr;
 for(unsigned i=0;i<count;++i)if(cars[i].health.driver==0){player=&cars[i].health;break;}
 if(!player||!std::isfinite(player->remaining)||!std::isfinite(player->maximum)||player->maximum<=0||player->maximum>100000||player->remaining<0||player->remaining>player->maximum)return;
 D3DVIEWPORT9 viewport{};if(FAILED(device->GetViewport(&viewport))||!viewport.Width||!viewport.Height)return;
 IDirect3DStateBlock9* state=nullptr;if(FAILED(device->CreateStateBlock(D3DSBT_ALL,&state))||!state)return;
 __try {
  device->SetVertexShader(nullptr);device->SetPixelShader(nullptr);device->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE);
  device->SetTexture(0,nullptr);device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);
  device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE);device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
  device->SetRenderState(D3DRS_ZENABLE,FALSE);device->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);device->SetRenderState(D3DRS_STENCILENABLE,FALSE);
  device->SetRenderState(D3DRS_SCISSORTESTENABLE,FALSE);device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);device->SetRenderState(D3DRS_FOGENABLE,FALSE);
  device->SetRenderState(D3DRS_LIGHTING,FALSE);device->SetRenderState(D3DRS_FILLMODE,D3DFILL_SOLID);device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE,FALSE);device->SetRenderState(D3DRS_BLENDOP,D3DBLENDOP_ADD);
  device->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);device->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);device->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
  device->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);device->SetRenderState(D3DRS_SRGBWRITEENABLE,FALSE);device->SetRenderState(D3DRS_COLORWRITEENABLE,15);
  const float unit=viewport.Height/1080.f,w=238*unit,h=12*unit,x=viewport.X+viewport.Width-270*unit,y=viewport.Y+viewport.Height-22*unit;
  const float fraction=std::clamp(player->remaining/player->maximum,0.f,1.f);
  const DWORD color=fraction>.6f?0xFF73DF5Fu:fraction>.3f?0xFFFFCD4Du:0xFFEF5C55u;
  Vertex vertices[1536];unsigned used=0;
  Rect(vertices,used,x-5*unit,y-25*unit,w+10*unit,h+30*unit,0xDB080D12);Rect(vertices,used,x,y,w,h,0xD9424B53);Rect(vertices,used,x,y,w*fraction,h,color);
  for(unsigned i=1;i<10;++i)Rect(vertices,used,x+w*i/10.f,y,unit,h,0x75080D12);
  char label[32];sprintf_s(label,"HP %u/%u",unsigned(player->remaining+.5f),unsigned(player->maximum+.5f));
  Text(vertices,used,x,y-20*unit,3*unit,label,0xFFF4F6F8);
  device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,used/3,vertices,sizeof(Vertex));
 }__finally {state->Apply();state->Release();}
}
}
