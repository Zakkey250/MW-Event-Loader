#pragma once
// UG2-authored sprites, cubic curves and hand attachments. This deliberately
// does not pass UG2's incompatible packed emitter structures to MW's engine.
struct Ug2Flare {
 struct Definition{unsigned texture;BYTE options[8];float p[12],curve[8][4];};
 static_assert(sizeof(Definition)==188);
 struct Anchor{D3DXVECTOR3 point,direction;};
 struct Particle{D3DXVECTOR3 point;float size,angle;unsigned color,frame;};
 Definition definitions[3]{};std::vector<Anchor> tracks[2];IDirect3DTexture9* textures[3]{};
 std::string folder;unsigned frames=0;bool attempted=false;
 void Clear(){for(auto& t:textures){if(t)t->Release();t=nullptr;}for(auto& t:tracks)t.clear();frames=0;attempted=false;folder.clear();}
 ~Ug2Flare(){Clear();}
 void Load(const std::string& directory){
  Clear();std::ifstream in(directory+"/ug2-flare.bin",std::ios::binary);if(!in)return;
  unsigned h[4];SceneAssets::Read(in,h,4);if(h[0]!=0x31464555||h[1]!=3||!h[2]||h[2]>6000||h[3]!=60)throw std::runtime_error("Invalid UG2 flare header");
  constexpr unsigned keys[]={0x3394FE62,0x0DE30D23,0xA4C28190};
  SceneAssets::Read(in,definitions,3);
  for(unsigned i=0;i<3;++i){auto& d=definitions[i];if(d.texture!=keys[i]||d.options[1]>3||d.options[2]>3||d.options[3]>60)throw std::runtime_error("Invalid UG2 flare sprite");
   for(float v:d.p)if(!std::isfinite(v)||fabs(v)>1000)throw std::runtime_error("Invalid UG2 flare parameter");
   if(d.p[0]<=0||d.p[0]>100||d.p[1]<0||d.p[1]>1||d.p[2]<=0||d.p[2]>4||d.p[3]<0||d.p[3]>1)throw std::runtime_error("UG2 flare budget exceeded");
   for(auto& c:d.curve)for(float v:c)if(!std::isfinite(v)||fabs(v)>1000000)throw std::runtime_error("Invalid UG2 flare curve");
  }
  for(auto& track:tracks){track.resize(h[2]);SceneAssets::Read(in,track.data(),track.size());for(auto a:track)for(float v:*reinterpret_cast<float(*)[6]>(&a))if(!std::isfinite(v)||fabs(v)>100)throw std::runtime_error("Invalid UG2 flare attachment");}
  char extra;if(in.read(&extra,1))throw std::runtime_error("Trailing UG2 flare data");frames=h[2];folder=directory;
 }
 Anchor Sample(unsigned hand,float seconds)const{float t=std::clamp(seconds*60,0.f,float(frames-1));unsigned i=unsigned(t),j=std::min(i+1,frames-1);auto a=tracks[hand][i],b=tracks[hand][j];return {a.point+(b.point-a.point)*(t-i),a.direction+(b.direction-a.direction)*(t-i)};}
 static float Random(unsigned& seed){seed^=seed<<17;seed^=seed>>13;seed^=seed<<5;return (seed&0x7fffffff)/2147483648.f;}
 static float Cubic(const Definition& d,unsigned component,float t,unsigned offset=0){return ((d.curve[offset][component]*t+d.curve[offset+1][component])*t+d.curve[offset+2][component])*t+d.curve[offset+3][component];}
 unsigned Draw(IDirect3DDevice9* device,float seconds,const D3DXMATRIX& world,const D3DVIEWPORT9& viewport,const D3DXMATRIX* view,const D3DXMATRIX* projection){
  if(!frames||!std::isfinite(seconds)||seconds<0||seconds>90)return 0;
  if(!attempted){attempted=true;for(unsigned i=0;i<3;++i){char name[32];sprintf_s(name,"/%08X.dds",definitions[i].texture);if(FAILED(D3DXCreateTextureFromFileA(device,(folder+name).c_str(),&textures[i])))throw std::runtime_error("UG2 flare texture unavailable");}}
  IDirect3DStateBlock9* state=nullptr;if(FAILED(device->CreateStateBlock(D3DSBT_ALL,&state)))return 0;
  D3DXMATRIX inverse;D3DXMatrixInverse(&inverse,nullptr,view);D3DXVECTOR3 right(inverse._11,inverse._12,inverse._13),up(inverse._21,inverse._22,inverse._23);D3DXMATRIX identity;D3DXMatrixIdentity(&identity);
  device->SetViewport(&viewport);device->SetVertexShader(nullptr);device->SetPixelShader(nullptr);device->SetFVF(D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1);device->SetTransform(D3DTS_WORLD,&identity);device->SetTransform(D3DTS_VIEW,view);device->SetTransform(D3DTS_PROJECTION,projection);
  device->SetRenderState(D3DRS_ZENABLE,TRUE);device->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);device->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);device->SetRenderState(D3DRS_LIGHTING,FALSE);device->SetRenderState(D3DRS_FOGENABLE,FALSE);device->SetRenderState(D3DRS_STENCILENABLE,FALSE);device->SetRenderState(D3DRS_SCISSORTESTENABLE,FALSE);device->SetRenderState(D3DRS_COLORWRITEENABLE,15);device->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);device->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);device->SetRenderState(D3DRS_BLENDOP,D3DBLENDOP_ADD);device->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
  device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_MODULATE);device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);device->SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_DIFFUSE);device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_MODULATE);device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);device->SetTextureStageState(0,D3DTSS_ALPHAARG2,D3DTA_DIFFUSE);device->SetTextureStageState(0,D3DTSS_TEXCOORDINDEX,0);device->SetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE);device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
  device->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);device->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);device->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);device->SetSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);
  unsigned done=0;
  for(unsigned effect=0;effect<3;++effect){auto& d=definitions[effect];std::vector<Particle> particles;
   // Bound live particles by source rate/lifetime; no accumulating emitter
   // objects, frame histories, game allocations or random global state.
   const unsigned last=unsigned(seconds*d.p[0]),first=unsigned(std::max(0.f,seconds-d.p[2])*d.p[0]);
   for(unsigned hand=0;hand<2;++hand)for(unsigned n=first;n<=last&&particles.size()<384;++n){
    const float born=n/d.p[0],age=seconds-born;unsigned seed=0x4e495300u+effect*757+hand*3931+n*13427;
    const float life=d.p[2]*(1-d.p[3]+Random(seed)*d.p[3]);if(age>=life)continue;auto a=Sample(hand,born);auto previous=Sample(hand,std::max(0.f,born-1.f/60));
    const float speed=d.p[4]*(1-d.p[5]+Random(seed)*d.p[5]);auto point=a.point+a.direction*(speed*age)+(a.point-previous.point)*(60*d.p[8]*age);point.z-=.5f*d.p[10]*age*age;
    D3DXVec3TransformCoord(&point,&point,&world);float t=age/life,size=std::clamp(Cubic(d,0,t,4),0.f,4.f);if(size<=0)continue;
    unsigned color[4];for(unsigned k=0;k<4;++k)color[k]=unsigned(std::clamp(Cubic(d,k,t),0.f,255.f));
    const unsigned cells=(1u<<d.options[1])*(1u<<d.options[2]);unsigned frame=(unsigned(Random(seed)*cells)+unsigned(age*d.options[3]))%cells;
    particles.push_back({point,size,Cubic(d,1,t,4)*(2*D3DX_PI/65536.f),(color[3]<<24)|(color[0]<<16)|(color[1]<<8)|color[2],frame});
   }
   // Source smoke uses alpha blend; glow and sparks use additive textures.
   device->SetRenderState(D3DRS_DESTBLEND,effect==1?D3DBLEND_INVSRCALPHA:D3DBLEND_ONE);device->SetTexture(0,textures[effect]);
   std::sort(particles.begin(),particles.end(),[&](const Particle& a,const Particle& b){D3DXVECTOR3 x,y;D3DXVec3TransformCoord(&x,&a.point,view);D3DXVec3TransformCoord(&y,&b.point,view);return x.z>y.z;});
   std::vector<carbonprobe::Vertex> vertices;vertices.reserve(particles.size()*6);
   for(auto p:particles){auto x=(right*cosf(p.angle)+up*sinf(p.angle))*p.size,y=(up*cosf(p.angle)-right*sinf(p.angle))*p.size;const unsigned cols=1u<<d.options[1],rows=1u<<d.options[2];float u=float(p.frame%cols)/cols,v=float(p.frame/cols)/rows,du=1.f/cols,dv=1.f/rows;
    carbonprobe::Vertex q[]={{p.point-x-y,p.color,u,v+dv},{p.point-x+y,p.color,u,v},{p.point+x-y,p.color,u+du,v+dv},{p.point+x+y,p.color,u+du,v}};for(unsigned k:{0u,1u,2u,2u,1u,3u})vertices.push_back(q[k]);
   }if(!vertices.empty()&&SUCCEEDED(device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,unsigned(vertices.size()/3),vertices.data(),sizeof(carbonprobe::Vertex))))++done;
  }state->Apply();state->Release();return done;
 }
} ug2Flare;
