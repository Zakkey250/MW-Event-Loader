#pragma once
// Original camera composition around live MW bodies. No imported curves,
// animation resources, NIS vehicle binding, pose or velocity writes.
struct RpdLiveIntro {
 unsigned race=0;float started=0,go=-1;bool playing=false,lens=false;
 unsigned short savedFov=0;
 void Cancel(){if(lens)*reinterpret_cast<unsigned short*>(0x986870+0xC4)=savedFov;lens=false;playing=false;race=0;go=-1;}
 void Begin(){
  Cancel();float p[6]{},f[6]{};unsigned owner=0;
  if(!MWELRpdLiveIntroSample(p,f,&owner))return;
  race=owner;started=*reinterpret_cast<float*>(0x9885D8);playing=true;
  Log("RPD_LIVE_INTRO_BEGIN original_camera=1 imported_assets=0 vehicle_writes=0");
 }
 void Go(){if(playing)go=*reinterpret_cast<float*>(0x9885D8);}
 bool View(D3DXMATRIX& result,const D3DXMATRIX& native){
  if(!playing)return false;
  float p[6]{},f[6]{};unsigned owner=0;const float now=*reinterpret_cast<float*>(0x9885D8);
  if(!MWELRpdLiveIntroSample(p,f,&owner)||owner!=race||!std::isfinite(now)||now<started||now-started>12){Cancel();return false;}
  const float after=go<0?0:now-go;if(go>=0&&after>=1.2f){Cancel();return false;}
  // Two independently authored live-vehicle shots, then blend into the native
  // chase camera at GO. The native countdown and all player inputs continue.
  const unsigned slot=go<0&&now-started<1.4f?1:0;
  D3DXVECTOR3 car(p+slot*3),forward(f+slot*3),up(0,0,1);
  forward.z=0;if(D3DXVec3LengthSq(&forward)<.5f){Cancel();return false;}D3DXVec3Normalize(&forward,&forward);
  const D3DXVECTOR3 left(-forward.y,forward.x,0);
  auto eye=car+forward*(slot?5.8f:4.8f)+left*(slot?-3.6f:3.2f)+up*(slot?1.25f:1.5f);
  auto target=car+up*.9f;
  reinterpret_cast<void(__cdecl*)(D3DXMATRIX*,D3DXVECTOR3*,D3DXVECTOR3*,D3DXVECTOR3*)>(0x6CF0A0)(&result,&eye,&target,&up);
  if(!lens){savedFov=*reinterpret_cast<unsigned short*>(0x986870+0xC4);lens=true;}
  const float blend=go<0?0:std::clamp(after/1.2f,0.f,1.f);
  *reinterpret_cast<unsigned short*>(0x986870+0xC4)=static_cast<unsigned short>(10000+(savedFov-10000)*blend);
  if(blend>0){
   D3DXMATRIX a,b;D3DXMatrixInverse(&a,nullptr,&result);D3DXMatrixInverse(&b,nullptr,&native);
   D3DXVECTOR3 sa,sb,pa,pb;D3DXQUATERNION qa,qb,q;D3DXMatrixDecompose(&sa,&qa,&pa,&a);D3DXMatrixDecompose(&sb,&qb,&pb,&b);
   const float t=blend*blend*(3-2*blend);D3DXQuaternionSlerp(&q,&qa,&qb,t);D3DXMatrixRotationQuaternion(&a,&q);
   const auto position=pa+(pb-pa)*t;a._41=position.x;a._42=position.y;a._43=position.z;D3DXMatrixInverse(&result,nullptr,&a);
  }
  return true;
 }
} rpdLiveIntro;
