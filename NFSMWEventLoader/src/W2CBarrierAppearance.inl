// Included inside event_barriers after the active course lookup.
#include "../../World2MW-v2/runtime/game-mw13/ported/RenderFrustum.h"

static bool installedAppearanceHook=false;
static bool preferInstalledNativeTextures=false;
static std::array<mwel_barrier_bank::Image,2> barrierImages;
static std::array<IDirect3DTexture9*,2> uploadedBarriers{};
static IDirect3DDevice9* barrierDevice=nullptr;
static IDirect3DDevice9** w2cDevice=nullptr;
static const float* w2cTime=nullptr;
static float w2cTimeRate=1.f;
static void(__cdecl* originalBarrierDraw)(const D3DMATRIX&,Vec3,bool)=nullptr;
static IDirect3DTexture9*(__cdecl* originalBarrierTexture)(unsigned)=nullptr;
static void(__cdecl* originalBarrierMaterial)(unsigned)=nullptr;

struct BarrierPass {
 float time;
 int slot=-1;
 unsigned stock=0;
};
static thread_local BarrierPass* barrierPass=nullptr;

static IDirect3DTexture9* UploadBarrier(IDirect3DDevice9* device,const mwel_barrier_bank::Image& image) {
 const D3DFORMAT format=image.compression==0x22?D3DFMT_DXT1:
  image.compression==0x24?D3DFMT_DXT3:image.compression==0x26?D3DFMT_DXT5:D3DFMT_A8R8G8B8;
 IDirect3DTexture9* texture=nullptr;
 if(FAILED(device->CreateTexture(image.width,image.height,image.mips,0,format,D3DPOOL_MANAGED,&texture,nullptr)))return nullptr;
 size_t offset=0;
 for(unsigned level=0,w=image.width,h=image.height;level<image.mips;++level,w=std::max(w/2,1u),h=std::max(h/2,1u)){
  D3DLOCKED_RECT locked{};
  if(FAILED(texture->LockRect(level,&locked,nullptr,0))){texture->Release();return nullptr;}
  const auto bytes=image.RowBytes(w);
  if(locked.Pitch<int(bytes)){texture->UnlockRect(level);texture->Release();return nullptr;}
  for(unsigned row=0;row<image.Rows(h);++row,offset+=bytes)
   memcpy(static_cast<BYTE*>(locked.pBits)+size_t(row)*locked.Pitch,image.pixels.data()+offset,bytes);
  if(FAILED(texture->UnlockRect(level))){texture->Release();return nullptr;}
 }
 return texture;
}

static bool EnsureBarrierTextures(IDirect3DDevice9* device) {
 if(!device)return false;
 if(device==barrierDevice)return uploadedBarriers[0]&&uploadedBarriers[1];
 for(auto*& texture:uploadedBarriers){if(texture)texture->Release();texture=nullptr;}
 barrierDevice=device;
 for(size_t i=0;i<uploadedBarriers.size();++i)uploadedBarriers[i]=UploadBarrier(device,barrierImages[i]);
 if(!uploadedBarriers[0]||!uploadedBarriers[1]){
  for(auto*& texture:uploadedBarriers){if(texture)texture->Release();texture=nullptr;}
  MWELNeoLog("EVENT_INGAMEA_WALL_UPLOAD_REFUSED fallback=1");return false;
 }
 return true;
}

static D3DMATRIX BarrierIdentity(){
 D3DMATRIX result{};result._11=result._22=result._33=result._44=1.f;return result;
}

static void ApplyBarrierAppearance(IDirect3DDevice9* device,int slot,float time){
 const auto& image=barrierImages[slot];
 device->SetTexture(0,uploadedBarriers[slot]);
 device->SetRenderState(D3DRS_DEPTHBIAS,0);
 device->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS,0);
 device->SetRenderState(D3DRS_ALPHATESTENABLE,image.usage==1);
 device->SetRenderState(D3DRS_ALPHAREF,11);
 device->SetRenderState(D3DRS_ALPHAFUNC,D3DCMP_GREATER);
 device->SetRenderState(D3DRS_ALPHABLENDENABLE,image.blend!=0);
 device->SetRenderState(D3DRS_ZWRITEENABLE,image.blend==0);
 if(image.flags&2)device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
 constexpr D3DBLEND src[]={D3DBLEND_ONE,D3DBLEND_SRCALPHA,D3DBLEND_SRCALPHA,D3DBLEND_ZERO,D3DBLEND_SRCALPHA,D3DBLEND_DESTALPHA,D3DBLEND_DESTALPHA};
 constexpr D3DBLEND dst[]={D3DBLEND_ZERO,D3DBLEND_INVSRCALPHA,D3DBLEND_ONE,D3DBLEND_INVSRCCOLOR,D3DBLEND_INVSRCALPHA,D3DBLEND_INVDESTALPHA,D3DBLEND_ONE};
 device->SetRenderState(D3DRS_SRCBLEND,src[image.blend]);device->SetRenderState(D3DRS_DESTBLEND,dst[image.blend]);
 device->SetRenderState(D3DRS_BLENDOP,D3DBLENDOP_ADD);
 device->SetSamplerState(0,D3DSAMP_ADDRESSU,image.tile&4?D3DTADDRESS_MIRROR:image.tile&1?D3DTADDRESS_WRAP:D3DTADDRESS_CLAMP);
 device->SetSamplerState(0,D3DSAMP_ADDRESSV,image.tile&8?D3DTADDRESS_MIRROR:image.tile&2?D3DTADDRESS_WRAP:D3DTADDRESS_CLAMP);
 device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_MODULATE);
 device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);
 device->SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_DIFFUSE);
 device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_MODULATE);
 device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);
 device->SetTextureStageState(0,D3DTSS_ALPHAARG2,D3DTA_DIFFUSE);
 auto uv=BarrierIdentity();
 if(image.scroll==2&&image.step>0)time=std::floor(time/(image.step/256.f));
 if(image.scroll){uv._31=std::fmod(time*image.s/4096.f,1.f);uv._32=std::fmod(time*image.t/4096.f,1.f);}
 device->SetTransform(D3DTS_TEXTURE0,&uv);
 device->SetTextureStageState(0,D3DTSS_TEXCOORDINDEX,0);
 device->SetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_COUNT2);
}

static IDirect3DTexture9* __cdecl CheckBarrierTexture(unsigned key){
 const int slot=barrierPass?mwel_barrier_bank::Slot(key):-1;
 return slot>=0?uploadedBarriers[slot]:originalBarrierTexture(key);
}
static void __cdecl SelectBarrierMaterial(unsigned key){
 const int slot=barrierPass?mwel_barrier_bank::Slot(key):-1;
 if(barrierPass)barrierPass->slot=slot;
 if(slot<0)originalBarrierMaterial(key);
}
static HRESULT __stdcall SubmitStockBarrier(IDirect3DDevice9* device,D3DPRIMITIVETYPE type,UINT first,UINT vertices,
 UINT primitives,const void* indices,D3DFORMAT format,const void* data,UINT stride){
 const int slot=barrierPass?barrierPass->slot:-1;
 if(slot>=0){
  ApplyBarrierAppearance(device,slot,barrierPass->time);
  barrierPass->slot=-1;
  ++barrierPass->stock;
 }
 return device->DrawIndexedPrimitiveUP(type,first,vertices,primitives,indices,format,data,stride);
}

static unsigned DrawAuthoredBarriers(IDirect3DDevice9* device,const Course& course,const D3DMATRIX& clip,Vec3 eye,float time){
 static_assert(sizeof(Vertex)==24);
 const RenderFrustum frustum(clip);
 unsigned submitted=0;
 for(const auto& wall:course.walls){
  const auto low=wall.header.minimum,high=wall.header.maximum;
  const float dx=(low.x+high.x)*.5f-eye.x,dy=(low.y+high.y)*.5f-eye.y,dz=(low.z+high.z)*.5f-eye.z;
  if(dx*dx+dy*dy+dz*dz>650.f*650.f||!frustum.Visible(low,high))continue;
  for(const auto& mesh:wall.meshes){
   const int slot=mwel_barrier_bank::Slot(mesh.texture);
   if(slot<0||!mesh.indexCount)continue;
   if(!submitted){
    auto identity=BarrierIdentity();
    device->SetVertexShader(nullptr);device->SetPixelShader(nullptr);
    device->SetFVF(D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1);
    device->SetTransform(D3DTS_WORLD,&identity);
    device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
   }
   ApplyBarrierAppearance(device,slot,time);
   if(SUCCEEDED(device->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST,0,mesh.vertexCount,mesh.indexCount/3,
     wall.indices.data()+mesh.firstIndex,D3DFMT_INDEX32,wall.vertices.data()+mesh.firstVertex,sizeof(Vertex))))++submitted;
  }
 }
 return submitted;
}

static void __cdecl DrawInstalledBarriers(const D3DMATRIX& clip,Vec3 eye,bool mainView){
 auto* device=*w2cDevice;
 if(!device||!EnsureBarrierTextures(device)){originalBarrierDraw(clip,eye,mainView);return;}
 const float time=*w2cTime*w2cTimeRate;
 BarrierPass pass{std::isfinite(time)?time:0.f};
 auto* previous=barrierPass;
 barrierPass=&pass;
 __try{originalBarrierDraw(clip,eye,mainView);}
 __finally{barrierPass=previous;}
 const auto* course=Find();
 const unsigned authored=course&&course->ingameMeshes?DrawAuthoredBarriers(device,*course,clip,eye,pass.time):0;
 if(mainView&&MWELNeoLoggingEnabled()){
  static DWORD last=0;const auto now=GetTickCount();
  if((pass.stock||authored)&&now-last>=2000){
   last=now;char line[220];
   sprintf_s(line,"EVENT_INGAMEA_WALL_DRAW key=%08X stock_meshes=%u authored_meshes=%u time=%.3f source=GLOBAL/InGameA.bun",Current(),pass.stock,authored,pass.time);
   MWELNeoLog(line);
  }
 }
}

static bool UseInstalledBarrierPass(){
 if(!installedAppearanceHook||(barrierDevice&&!uploadedBarriers[0]))return false;
 const auto* course=Find();
 return course&&course->ingameMeshes;
}

// Keep the substitutions scoped to the validated race-barrier mesh loop.
static bool HookInstalledAppearance(const mwel_barrier_layout::DrawPass& layout){
 struct Call {BYTE* site;BYTE before[6];BYTE after[6];unsigned length;};
 Call calls[]={
  {layout.textureCall,{0xE8},{0xE8},5},
  {layout.materialCall,{0xE8},{0xE8},5},
  {layout.primitiveCall,{0xFF,layout.primitiveCall[1],0x50,0x01,0,0},{0xE8,0,0,0,0,0x90},6},
 };
 auto rel=[](BYTE* code,const void* site,const void* target){
  const uint32_t displacement=uint32_t(uintptr_t(target)-uintptr_t(site)-5);
  memcpy(code+1,&displacement,4);
 };
 rel(calls[0].before,calls[0].site,layout.texture);
 rel(calls[1].before,calls[1].site,layout.material);
 rel(calls[0].after,calls[0].site,reinterpret_cast<void*>(CheckBarrierTexture));
 rel(calls[1].after,calls[1].site,reinterpret_cast<void*>(SelectBarrierMaterial));
 rel(calls[2].after,calls[2].site,reinterpret_cast<void*>(SubmitStockBarrier));
 auto* entry=layout.entry;
 if(!mwel_barrier_layout::DrawPrologue(entry))return false;
 for(const auto& call:calls)if(memcmp(call.site,call.before,call.length))return false;
 if(MH_CreateHook(entry,DrawInstalledBarriers,reinterpret_cast<void**>(&originalBarrierDraw))!=MH_OK)return false;
 // All three instructions are within one function; preserve page permissions
 // and keep the original calls in place if the entry detour cannot be enabled.
 const size_t region=layout.primitiveCall-layout.textureCall+6;
 DWORD protection=0;
 if(!VirtualProtect(calls[0].site,region,PAGE_EXECUTE_READWRITE,&protection)){MH_RemoveHook(entry);return false;}
 w2cDevice=reinterpret_cast<IDirect3DDevice9**>(layout.device);
 w2cTime=layout.clock;
 w2cTimeRate=layout.clockRate;
 originalBarrierTexture=reinterpret_cast<decltype(originalBarrierTexture)>(layout.texture);
 originalBarrierMaterial=reinterpret_cast<decltype(originalBarrierMaterial)>(layout.material);
 for(const auto& call:calls)memcpy(call.site,call.after,call.length);
 FlushInstructionCache(GetCurrentProcess(),calls[0].site,region);
 const bool installed=MH_EnableHook(entry)==MH_OK;
 if(!installed){
  for(const auto& call:calls)memcpy(call.site,call.before,call.length);
  FlushInstructionCache(GetCurrentProcess(),calls[0].site,region);
  MH_RemoveHook(entry);
 }
 DWORD ignored=0;VirtualProtect(calls[0].site,region,protection,&ignored);
 return installed;
}

static void PrepareInstalledBarrierBank(){
 try{
  wchar_t path[32768]{};
  const auto exeLength=GetModuleFileNameW(nullptr,path,_countof(path));
  if(!exeLength||exeLength>=_countof(path))return;
  const auto bank=mwel::fs::path(path).parent_path()/L"GLOBAL"/L"InGameA.bun";
  preferInstalledNativeTextures=mwel_barrier_bank::Parse(mwel::Read(bank,128*1024*1024),barrierImages);
 }catch(const std::exception& error){MWELNeoLog(error.what());}
}

static void TryInstalledAppearance(HMODULE world){
 static bool attempted=false;
 if(attempted||!preferInstalledNativeTextures||!world)return;
 attempted=true;
 const auto layout=mwel_barrier_layout::Find(world);
 installedAppearanceHook=layout.entry&&HookInstalledAppearance(layout);
 if(MWELNeoLoggingEnabled()){
  char line[260];
  sprintf_s(line,"EVENT_INGAMEA_WALL_SOURCE installed=%u draw_pass_found=%u size=%ux%u,%ux%u scroll=%u,%u source=GLOBAL/InGameA.bun",
   unsigned(installedAppearanceHook),unsigned(layout.entry!=nullptr),barrierImages[0].width,barrierImages[0].height,barrierImages[1].width,barrierImages[1].height,barrierImages[0].scroll,barrierImages[1].scroll);
  MWELNeoLog(line);
 }
 if(!installedAppearanceHook)barrierImages={};
}
