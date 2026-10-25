#pragma once
// World-pass shading for imported actors. No game shader or device state is
// modified permanently; the caller restores a complete D3D state block.
namespace carbonprobe {
struct SceneLight {
 float diffuse[4]{.6f,.6f,.6f,1},ambient[4]{.35f,.35f,.35f,1},sun[4]{0,0,1,0};
 float fog[4]{},fogSun[4]{},fogValue[4]{},eye[4]{},settings[4]{};
};
inline bool LightVector(ID3DXEffect* effect,const char* name,float* out){
 auto handle=effect->GetParameterByName(nullptr,name);float value[4]{};
 if(!handle||FAILED(effect->GetVector(handle,reinterpret_cast<D3DXVECTOR4*>(value))))return false;
 for(float v:value)if(!std::isfinite(v)||std::abs(v)>100000)return false;
 memcpy(out,value,sizeof(value));return true;
}
inline bool ReadSceneLight(SceneLight& light){
 // This EN1.3 layout is shared by vanilla and SunSet. In particular, read
 // the live world effect rather than imposing W2C's daylight on Main.
 __try{
  auto wrapper=*reinterpret_cast<BYTE**>(0x93DE78);
  auto fx=wrapper&&*reinterpret_cast<unsigned*>(wrapper+4)==0?*reinterpret_cast<ID3DXEffect**>(wrapper+0x48):nullptr;
  bool native=fx&&LightVector(fx,"cvDiffuseColor",light.diffuse)&&LightVector(fx,"cvAmbientColor",light.ambient);
  if(fx){LightVector(fx,"cvFogColor",light.fog);LightVector(fx,"cvFogSunColor",light.fogSun);LightVector(fx,"cvFogValue",light.fogValue);}
  auto tod=*reinterpret_cast<BYTE**>(0x9B392C);
  if(tod){float direction[4];memcpy(direction,tod+0x180,sizeof(direction));
   float len=direction[0]*direction[0]+direction[1]*direction[1]+direction[2]*direction[2];
   if(std::isfinite(len)&&len>.0001f&&len<100000){len=std::sqrt(len);for(unsigned i=0;i<3;++i)light.sun[i]=direction[i]/len;}}
  BOOL compressed=GetModuleHandleA("SunSet.asi")!=nullptr;
  if(fx){auto param=fx->GetParameterByName(nullptr,"cbCompressColor");BOOL value;if(param&&SUCCEEDED(fx->GetBool(param,&value)))compressed=value;}
  light.settings[0]=compressed?1.f:0.f;return native;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
class SceneShader {
 IDirect3DDevice9* owner=nullptr;IDirect3DVertexShader9* vertex=nullptr;IDirect3DPixelShader9* pixel=nullptr;
 bool attempted=false;SceneLight light;bool lightReady=false;
 void Clear(){if(vertex)vertex->Release();if(pixel)pixel->Release();vertex=nullptr;pixel=nullptr;}
public:
 ~SceneShader(){Clear();}
 void SetLight(const SceneLight& value){light=value;lightReady=true;}
 void RefreshLight(){light=SceneLight{};ReadSceneLight(light);lightReady=true;}
 bool Init(IDirect3DDevice9* device){
  if(owner!=device){Clear();owner=device;attempted=false;}if(attempted)return vertex&&pixel;attempted=true;
  const char* source=R"(
row_major float4x4 Clip:register(c0);row_major float4x4 World:register(c4);row_major float4x4 NormalWorld:register(c8);
struct VI{float3 p:POSITION;float3 n:NORMAL;float2 uv:TEXCOORD0;};
struct VO{float4 p:POSITION;float3 world:TEXCOORD0;float3 n:TEXCOORD1;float2 uv:TEXCOORD2;};
VO VS(VI i){VO o;float4 p=mul(float4(i.p,1),World);o.p=mul(p,Clip);o.world=p.xyz;o.n=mul(float4(i.n,0),NormalWorld).xyz;o.uv=i.uv;return o;}
sampler2D Base:register(s0);
float4 Diffuse:register(c0),Ambient:register(c1),Sun:register(c2),Fog:register(c3),FogSun:register(c4),FogValue:register(c5),Eye:register(c6),Settings:register(c7);
float4 PS(VO i):COLOR{float4 t=tex2D(Base,i.uv);float3 n=normalize(i.n);
float3 color=t.rgb*max(0,Ambient.rgb+Diffuse.rgb*saturate(dot(n,Sun.xyz)));
float3 delta=i.world-Eye.xyz;float dist=length(delta);
float fog=pow(1-saturate((FogValue.x-dist)*FogValue.y),max(.01,FogValue.w))*FogValue.z;
color=lerp(color,lerp(Fog.rgb,FogSun.rgb,smoothstep(.3,1,dot(delta/max(dist,.001),Sun.xyz))),saturate(fog));
color=lerp(color,color/(1+max(color,0)),Settings.x);return float4(color,t.a);}
)";
  ID3DXBuffer* code=nullptr;ID3DXBuffer* error=nullptr;
  HRESULT status=D3DXCompileShader(source,UINT(strlen(source)),nullptr,nullptr,"VS","vs_2_0",D3DXSHADER_OPTIMIZATION_LEVEL3,&code,&error,nullptr);
  if(error)error->Release();if(FAILED(status))return false;
  status=device->CreateVertexShader(static_cast<DWORD*>(code->GetBufferPointer()),&vertex);code->Release();if(FAILED(status))return false;
  error=nullptr;status=D3DXCompileShader(source,UINT(strlen(source)),nullptr,nullptr,"PS","ps_2_0",D3DXSHADER_OPTIMIZATION_LEVEL3,&code,&error,nullptr);
  if(error)error->Release();if(FAILED(status)){Clear();return false;}
  status=device->CreatePixelShader(static_cast<DWORD*>(code->GetBufferPointer()),&pixel);code->Release();if(FAILED(status)){Clear();return false;}return true;
 }
 bool Bind(IDirect3DDevice9* device,const D3DXMATRIX& world,const D3DXMATRIX& view,const D3DXMATRIX& projection){
  if(!Init(device))return false;if(!lightReady)RefreshLight();
  light.eye[0]=-(view._41*view._11+view._42*view._12+view._43*view._13);
  light.eye[1]=-(view._41*view._21+view._42*view._22+view._43*view._23);
  light.eye[2]=-(view._41*view._31+view._42*view._32+view._43*view._33);
  auto clip=view*projection;device->SetVertexShader(vertex);device->SetPixelShader(pixel);
  device->SetVertexShaderConstantF(0,&clip._11,4);device->SetVertexShaderConstantF(4,&world._11,4);
  D3DXMATRIX normal;D3DXMatrixIdentity(&normal);if(D3DXMatrixInverse(&normal,nullptr,&world))D3DXMatrixTranspose(&normal,&normal);device->SetVertexShaderConstantF(8,&normal._11,4);
  device->SetPixelShaderConstantF(0,light.diffuse,1);device->SetPixelShaderConstantF(1,light.ambient,1);device->SetPixelShaderConstantF(2,light.sun,1);
  device->SetPixelShaderConstantF(3,light.fog,1);device->SetPixelShaderConstantF(4,light.fogSun,1);device->SetPixelShaderConstantF(5,light.fogValue,1);
  device->SetPixelShaderConstantF(6,light.eye,1);device->SetPixelShaderConstantF(7,light.settings,1);return true;
 }
};
inline SceneShader& WorldActorShader(){static SceneShader shader;return shader;}
}
