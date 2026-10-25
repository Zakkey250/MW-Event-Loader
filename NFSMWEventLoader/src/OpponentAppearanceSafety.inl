// Called before native opponent construction, after preset/random appearance.
// Only the private FE record's geometric slots change; paint, vinyl, decals,
// tuning, performance, player records and source presets remain intact.
static void* appearanceTrampoline=nullptr;
static_assert(unsigned(NFSPluginSDK::MW05::CarSlotId::BasePaint)==mwappearance::GeometrySlots);
static void AppearanceUnsafe(void* record,void* attributes){
 auto package=Current();unsigned slot=0,count=0;
 if(!package||!record||!attributes||!OpponentSlot(tuningContext.info,slot,count)||mwappearance::RaceExterior(slot,count,MWELNeoCurrentKey()))return;
 Log("EVENT_APPEARANCE_PREPARE slot=%u racers=%u record=%08X attributes=%08X",slot,count,unsigned(record),unsigned(attributes));
 BYTE* layout=nullptr;const char* model=nullptr;char name[32]{};
 if(!Copy(static_cast<BYTE*>(attributes)+8,&layout,4)||!layout||!Copy(layout+0x1C,&model,4)||!model||!Copy(model,name,sizeof(name))||!memchr(name,0,sizeof(name))){Log("EVENT_APPEARANCE_REFUSED slot=%u reason=model_attribute",slot);return;}
 const int type=mwcartype::FindPlayable(name);
 if(type<0){Log("EVENT_APPEARANCE_REFUSED slot=%u model=%s reason=type_missing",slot,name);return;}
 using namespace NFSPluginSDK::MW05;
 RideInfo stock(static_cast<CarType>(type));stock.SetStockParts();
 // RearWheel is legitimately absent on stock MW cars; native rendering uses
 // their shared wheel setup. Do not classify that optional slot as corruption.
 if(!stock.mPartsTable[unsigned(CarSlotId::Body)]||!stock.mPartsTable[unsigned(CarSlotId::FrontWheel)]){Log("EVENT_APPEARANCE_REFUSED slot=%u model=%s reason=invalid_stock_parts",slot,name);return;}
 for(auto part:stock.mPartsTable)if(part){auto index=reinterpret_cast<int(__thiscall*)(void*,void*)>(0x747BB0)(reinterpret_cast<void*>(0x9B26A8),part);if(index<0||index>=0xFFFF)return;}
 FECustomizationRecord stockRecord;stockRecord.WriteRideIntoRecord(&stock);
 for(unsigned i=0;i<139;++i)if(stock.mPartsTable[i]&&stockRecord.mInstalledParts[i]==-1)return;
 auto destination=static_cast<FECustomizationRecord*>(record);
 mwappearance::StockGeometry(destination->mInstalledParts,stockRecord.mInstalledParts);
 Log("EVENT_APPEARANCE_SAFETY slot=%u racers=%u exterior=stock paint_preserved=1 private_record_only=1 before_render=1",slot,count);
}
static void __cdecl Appearance(void* record,void* attributes){
 __try{AppearanceUnsafe(record,attributes);}__except(EXCEPTION_EXECUTE_HANDLER){Log("EVENT_APPEARANCE_SAFETY_REFUSED native_record_retained=1");}
}
static void __declspec(naked) AppearanceBridge(){__asm{
 pushfd
 pushad
 mov ebp,esp
 sub esp,528
 and esp,0FFFFFFF0h
 fxsave [esp]
 lea eax,[ebp+07Ch]
 push eax
 lea eax,[ebp+0A0h]
 push eax
 call Appearance
 add esp,8
 fxrstor [esp]
 mov esp,ebp
 popad
 popfd
 jmp dword ptr [appearanceTrampoline]
}}
