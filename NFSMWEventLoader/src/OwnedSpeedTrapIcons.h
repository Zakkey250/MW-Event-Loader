// Included inside registration_experiment. The extra icons have native
// GTrigger lifetimes and leave GManager's 200-entry array available to W2C.
namespace owned_speedtrap_icons {
using Icon=NFSPluginSDK::MW05::GIcon;
using Player=NFSPluginSDK::MW05::IPlayer;
using Position=decltype(Icon::mPosition);
constexpr unsigned nativeLimit=200;
constexpr unsigned iconOffset=0xC0;
thread_local bool constructing=false;
bool installed=false;
std::array<Icon*,256> extraIcons{};
unsigned extraCount=0;
Icon*(__thiscall* originalAllocate)(void*,Icon::Type,const Position&,float,bool);
void(__thiscall* originalDestroy)(void*);

Icon* CreateDetached(void* manager,Icon::Type type,const Position& position,float rotation){
 if(!installed)return nullptr;
 unsigned count=0;
 if(!Copy(static_cast<BYTE*>(manager)+0x2BC,&count,4)||count!=nativeLimit)return nullptr;
 auto entry=std::find(extraIcons.begin(),extraIcons.end(),nullptr);
 if(entry==extraIcons.end())return nullptr;
 auto storage=reinterpret_cast<void*(__thiscall*)(void*,unsigned,unsigned)>(0x5D29D0)(reinterpret_cast<void*>(0x925B30),sizeof(Icon),0);
 if(!storage)return nullptr;
 auto icon=reinterpret_cast<Icon*(__thiscall*)(void*,Icon::Type,const Position&,float)>(0x5E5970)(storage,type,position,rotation);
 *entry=icon;++extraCount;
 if(worldLogEnabled&&extraCount==1)Log("EVENT_SPEEDTRAP_EXTRA_ICON stock_slots=%u native_trigger_owns_icon=1",nativeLimit);
 return icon;
}

Icon* __fastcall Allocate(void* manager,void*,Icon::Type type,const Position& position,float rotation,bool disposable){
 if(auto icon=originalAllocate(manager,type,position,rotation,disposable))return icon;
 if(!constructing||disposable||(type!=Icon::Type::Checkpoint&&type!=Icon::Type::SpeedTrapInRace))return nullptr;
 return CreateDetached(manager,type,position,rotation);
}

Icon* RestoreRaceIcon(void* manager,BYTE* trigger,const Position& position){
 if(!installed)return nullptr;
 auto& icon=*reinterpret_cast<Icon**>(trigger+iconOffset);
 if(!icon){
  icon=originalAllocate(manager,Icon::Type::SpeedTrapInRace,position,0.f,false);
  if(!icon)icon=CreateDetached(manager,Icon::Type::SpeedTrapInRace,position,0.f);
 }
 return icon;
}

void __fastcall Destroy(void* trigger,void*){
 if(extraCount){
  auto& icon=*reinterpret_cast<Icon**>(static_cast<BYTE*>(trigger)+iconOffset);
  if(icon){
   auto entry=std::find(extraIcons.begin(),extraIcons.end(),icon);
   if(entry!=extraIcons.end()){
    auto retire=icon;icon=nullptr;*entry=nullptr;--extraCount;
    reinterpret_cast<void(__thiscall*)(Icon*,unsigned)>(0x5EBED0)(retire,1);
   }
  }
 }
 originalDestroy(trigger);
}

unsigned __fastcall Collect(void* manager,void*,Icon** output,Player* player){
 // Keep the installed W2C collector in the call chain, including its
 // roaming facility icons. Only these two callers own the 200 output slots.
 auto collect=reinterpret_cast<unsigned(__thiscall*)(void*,Icon**,Player*)>(0x5E4940);
 auto count=collect(manager,output,player);
 if(!extraCount||!output||count>nativeLimit)return count;
 BYTE* race=*reinterpret_cast<BYTE**>(0x91E000);unsigned mode=0;
 if(!race||!Copy(race+0x1960,&mode,4)||mode!=1)return count;

 // The callers have a per-type display limit. Order the merged cameras by
 // distance so native and extra cameras both compete for those same slots.
 const auto nativeCount=count;
 for(auto icon:extraIcons){
  if(!icon||!icon->GetVisibleOnMap()||!icon->GetVisibleInWorld())continue;
  if(count==nativeLimit)break;
  output[count++]=icon;
 }
 if(count!=nativeCount&&player)if(auto sim=player->GetSimable()){
  const auto& position=sim->GetPosition();
  auto distance=[&](const Icon* icon){
   const auto x=icon->mPosition.x-position.z,y=icon->mPosition.y+position.x,z=icon->mPosition.z-position.y;
   return x*x+y*y+z*z;
  };
  Icon* cameras[nativeLimit];unsigned slots[nativeLimit],cameraCount=0;
  for(unsigned i=0;i<count;++i){
   auto icon=output[i];if(!icon||(icon->mType!=Icon::Type::Checkpoint&&icon->mType!=Icon::Type::SpeedTrapInRace))continue;
   slots[cameraCount]=i;cameras[cameraCount++]=icon;
  }
  std::sort(cameras,cameras+cameraCount,[&](const auto* a,const auto* b){return distance(a)<distance(b);});
  for(unsigned i=0;i<cameraCount;++i)output[slots[i]]=cameras[i];
 }
 return count;
}

void* Construct(void* storage,unsigned constructor,unsigned& key,bool speedTrap){
 const bool previous=constructing;constructing=speedTrap;
 __try {return reinterpret_cast<void*(__thiscall*)(void*,unsigned*)>(constructor)(storage,&key);}
 __finally {constructing=previous;}
}

bool WriteCall(unsigned address,const BYTE* bytes){
 auto site=reinterpret_cast<void*>(address);DWORD protection=0;
 if(!VirtualProtect(site,5,PAGE_EXECUTE_READWRITE,&protection))return false;
 memcpy(site,bytes,5);DWORD ignored=0;VirtualProtect(site,5,protection,&ignored);
 FlushInstructionCache(GetCurrentProcess(),site,5);return true;
}

bool Install(){
 constexpr unsigned allocate=0x5E9EC0,destroy=0x5EF2D0;
 const BYTE allocateGuard[]={0x6A,0xFF,0x68,0xBF,0x54,0x87,0x00};
 const BYTE destroyGuard[]={0x6A,0xFF,0x68,0x4E,0x56,0x87,0x00};
 const BYTE constructorGuard[]={0x83,0xEC,0x0C,0x66,0x8B,0x44,0x24,0x10};
 const BYTE destructorGuard[]={0x56,0x8B,0xF1,0xE8,0x28,0x9B,0xFF,0xFF};
 const struct Call {unsigned address;BYTE original[5];} calls[]={
  {0x5486CB,{0xE8,0x70,0xC2,0x09,0}}, {0x57A33F,{0xE8,0xFC,0xA5,0x06,0}}
 };
 auto create=reinterpret_cast<void*>(allocate);auto release=reinterpret_cast<void*>(destroy);
 if(memcmp(create,allocateGuard,sizeof(allocateGuard))||memcmp(release,destroyGuard,sizeof(destroyGuard))||
    memcmp(reinterpret_cast<void*>(0x5E5970),constructorGuard,sizeof(constructorGuard))||
    memcmp(reinterpret_cast<void*>(0x5EBED0),destructorGuard,sizeof(destructorGuard)))return false;
 for(const auto& call:calls)if(memcmp(reinterpret_cast<void*>(call.address),call.original,5))return false;
 if(MH_CreateHook(create,Allocate,reinterpret_cast<void**>(&originalAllocate))!=MH_OK)return false;
 if(MH_CreateHook(release,Destroy,reinterpret_cast<void**>(&originalDestroy))!=MH_OK){MH_RemoveHook(create);return false;}
 unsigned changed=0;
 for(const auto& call:calls){
  BYTE code[5]={0xE8};const unsigned relative=unsigned(&Collect)-call.address-5;memcpy(code+1,&relative,4);
  if(!WriteCall(call.address,code))break;
  ++changed;
 }
 if(changed==std::size(calls)&&MH_QueueEnableHook(create)==MH_OK&&MH_QueueEnableHook(release)==MH_OK&&MH_ApplyQueued()==MH_OK){installed=true;return true;}
 for(unsigned i=0;i<changed;++i)WriteCall(calls[i].address,calls[i].original);
 MH_RemoveHook(create);MH_RemoveHook(release);return false;
}
}
