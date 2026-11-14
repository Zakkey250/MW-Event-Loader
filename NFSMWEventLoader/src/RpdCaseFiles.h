#pragma once
#include "RpdUiAssets.h"
// EN1.3 native FE contracts traced in diagnostics/rpd-case-files-20261101.
// No original FNG, native category, profile car or installed bank is replaced.
namespace rpd_case_files {
constexpr unsigned Category=5,Title=0xF29B49B1;
constexpr unsigned Accept=0x0C407210,Confirm=0xD05FC3A3;
constexpr char CarSelect[]="Car_Select.fng";
constexpr unsigned FirstLoanHandle=0xE2D00000;
constexpr char SafehouseOverview[]="SafeHouseReputationOverview.fng",SafehouseSheet[]="SafeHouseRaceSheet.fng";
bool installed=false,pickerInstalled=false;
unsigned pendingRank=0;
std::map<void*,unsigned> sheets;
unsigned(__thiscall* originalAddOption)(void*,void*)=nullptr;
void*(__thiscall* originalSheetConstructor)(void*,void*)=nullptr;
void*(__thiscall* originalSheetDelete)(void*,unsigned)=nullptr;
void(__thiscall* originalSheetPopulate)(void*)=nullptr;
void(__thiscall* originalSheetReact)(void*,unsigned,unsigned,unsigned,unsigned)=nullptr;
const char*(__thiscall* originalPlayerCar)(void*)=nullptr;
bool pickerUsed=false;
struct CarRecord {
 unsigned handle,frontendKey,vehicleKey,filter;
 BYTE customization,career;
 unsigned short padding;
};
static_assert(sizeof(CarRecord)==0x14);
struct CarChoice {unsigned source;CarRecord record;std::string label;};
enum class PickerAction {None,Back,ConfirmDispatch,Start};
struct Picker {
 bool opening=false,closing=false;
 void* screen=nullptr;
 void* carDb=nullptr;
 unsigned rank=0;
 std::string eventId;
 PickerAction action=PickerAction::None;
 bool awaitingConfirmation=false;
 unsigned confirmedHandle=0;
 std::vector<CarChoice> cars;
 expanded_leaderboard::Object label=nullptr;
 unsigned labelHandle=0;
 unsigned originalLabelScript=0;
 std::array<unsigned,13> labelScript{};
 bool hidManufacturer=false,hidModel=false;
 // Neutral status for the native impound widget tick. Never a profile record.
 alignas(4) BYTE blankStatus[0x38]{};
} picker;
void*(__thiscall* originalPickerConstructor)(void*,void*)=nullptr;
void*(__thiscall* originalPickerDelete)(void*,unsigned)=nullptr;
void(__thiscall* originalPickerPopulate)(void*)=nullptr;
void(__thiscall* originalPickerReact)(void*,unsigned,unsigned,unsigned,unsigned)=nullptr;
CarRecord*(__thiscall* originalFindCar)(void*,unsigned)=nullptr;
void(__thiscall* originalBuildPreview)(void*,unsigned,unsigned,void*)=nullptr;
template<class T>T Read(void* p,unsigned offset=0){T result{};if(p)Copy(static_cast<BYTE*>(p)+offset,&result,sizeof(result));return result;}
template<size_t N>bool Named(const char* name,const char (&expected)[N]){
 char value[N]{};return name&&Copy(name,value,N)&&value[N-1]=='\0'&&_stricmp(value,expected)==0;
}
unsigned ActiveRank(){return Read<unsigned>(reinterpret_cast<void*>(0x91CA3C));}
bool IsSafehouseSheet(void* self){return Named(Read<const char*>(self,0x10),SafehouseSheet)&&!Read<BYTE>(self,0x1B4);}
bool Available(){return std::any_of(packages.begin(),packages.end(),[](const auto& p){return mwel_rpd::CaseFile(p.authoring);});}
const char* __fastcall PlayerCar(void* parameters,void*){
 // Only the player factory's GetPlayerCarType call should see the choice.
 if(pickerUsed&&_ReturnAddress()==reinterpret_cast<void*>(0x6F48E0)){
  if(const auto package=event_presentation::PackageFor(parameters);package&&mwel_rpd::InGameSelected(package->authoring)){
   auto clone=registration_experiment::Find(registration_experiment::Hash(package->nativeNamespace.c_str()));
   if(clone&&clone->package==package){
    Log("RPD_PICKER_PLAYER_FACTORY id=%s preset=%s own_car=%u",package->id.c_str(),clone->rpdLaunchPreset?clone->rpdLaunchPreset:"",unsigned(clone->rpdUsesOwnCar));
    return clone->rpdLaunchPreset;
   }
  }
 }
 return originalPlayerCar(parameters);
}
void SwitchPackage(const char* name){
 if(auto frontend=Read<void*>(reinterpret_cast<void*>(0x91CADC)))
  reinterpret_cast<void(__thiscall*)(void*,const char*,unsigned,unsigned,unsigned)>(0x525940)(frontend,name,0,0,0);
}
void HidePickerDetails(void* self,bool refresh=false){
 // Career actions and status panels do not describe an RPD loan.
 using Find=unsigned*(__cdecl*)(const char*,unsigned);
 using Hide=void(__cdecl*)(void*);
 auto hide=[refresh](unsigned* object){
  if(object&&(refresh||!(object[7]&1)))reinterpret_cast<Hide>(0x514C70)(object);
 };
 const auto name=Read<const char*>(self,0x10);
 // FNG groups: Sell Car, career/impound data, both impound markers,
 // and the showcase control (dispatch screens do not enter showcase).
 // Native Refresh also shows individual children of these hidden groups.
 for(unsigned key:{0xD42F677Bu,0xFBEAE195u,0x64F3A49Cu,0x07F3EEB0u,0xD4AA6721u,0x0E9ED0A2u,0x37AABFC7u,
                   0x39DC21F9u,0xE998FE99u,0x4B890CDBu})
  hide(reinterpret_cast<Find>(0x524850)(name,key));
 // Heat dial and its numeric marker.
 hide(Read<unsigned*>(self,0xD4));
}
CarRecord* LoanCar(void* db,unsigned handle){
 if(picker.screen&&db==picker.carDb&&handle>=FirstLoanHandle){
  for(auto& car:picker.cars)if(car.source&&car.record.handle==handle)return &car.record;
 }
 return nullptr;
}
CarRecord* __fastcall FindCar(void* self,void*,unsigned handle){
 if(auto loan=LoanCar(self,handle))return loan;
 return originalFindCar(self,handle);
}
void __fastcall BuildPreview(void* self,void*,unsigned handle,unsigned slot,void* display){
 if(auto loan=LoanCar(self,handle)){
  // 005910E0 inlines the profile table lookup. Build the equivalent
  // standard uncustomized display descriptor from our private record.
  auto type=reinterpret_cast<int(__thiscall*)(CarRecord*)>(0x5816B0)(loan);
  reinterpret_cast<void(__thiscall*)(void*,int,unsigned,unsigned,unsigned)>(0x739A70)(display,type,0,0,0);
  reinterpret_cast<void(__thiscall*)(void*)>(0x759800)(display);
  reinterpret_cast<void(__thiscall*)(void*)>(0x7594A0)(display);
  return;
 }
 originalBuildPreview(self,handle,slot,display);
}
struct PickerNode {PickerNode *next,*prev;unsigned handle,locked;};
PickerNode* PickerHead(void* self){return reinterpret_cast<PickerNode*>(static_cast<BYTE*>(self)+0x2C);}
const CarChoice* SelectedCar(void* self){
 const auto handle=Read<unsigned>(Read<PickerNode*>(self,0x34),8);
 const auto car=std::find_if(picker.cars.begin(),picker.cars.end(),[handle](const auto& item){return item.record.handle==handle;});
 return car==picker.cars.end()?nullptr:&*car;
}
std::string LoanLabel(const rpd_vehicle_catalog::Entry& car){
 if(car.origin=="vanilla"){
  if(car.name=="copgto")return "Pontiac GTO";
  if(car.name=="copgtoghost")return "Unmarked Pontiac GTO";
  if(car.name=="copsportghost")return "Unmarked Corvette C6";
  if(car.name=="copsport"||car.name=="copsporthench")return "Corvette C6";
  if(car.name=="copcross")return "Cross - Corvette C6";
  if(car.name=="copghost")return "Unmarked Cruiser";
  if(car.name=="copmidsize")return "Police Cruiser";
  if(car.name=="copsuv"||car.name=="copsuvpatrol"||car.name=="copsuvl")return "Police SUV";
 }
 auto model=car.model.empty()?car.name:car.model;
 if(model.size()>3&&_strnicmp(model.c_str(),"cop",3)==0)model.erase(0,3);
 std::replace(model.begin(),model.end(),'_',' ');
 return model;
}
void UpdatePickerName(void* self,bool reset=false){
 using namespace expanded_leaderboard;
 const auto make=Read<Object>(self,0x3C),model=Read<Object>(self,0x40);
 const auto choice=reset?nullptr:SelectedCar(self);
 const bool loan=choice&&choice->source;
 const bool missingMake=loan&&Read<unsigned>(make,0x24)==Hash("GENERIC_LOGO_128");
 const bool missingModel=loan&&Read<unsigned>(model,0x24)==Hash("GENERIC_LOGO_256");
 if(missingModel&&!picker.label){
  auto engine=Read<void*>(reinterpret_cast<void*>(0x91CADC));
  auto package=engine?reinterpret_cast<Object(__thiscall*)(void*,const char*)>(0x516AF0)(engine,CarSelect):nullptr;
  auto source=package?Find(package,0x40D23023):nullptr; // white position counter FEString
  if(source&&source[6]==2){
   picker.label=Clone(package,source,Hash("MWEL_RPD_LOAN_NAME"));
   if(auto label=picker.label;label&&label[21]){
    // Keep the native package's script allocation intact. Only our clone
    // uses this track-free state; the counter keeps its native animation.
    auto& script=picker.labelScript;
    picker.originalLabelScript=label[21];
    memcpy(script.data(),reinterpret_cast<void*>(label[21]),sizeof(script));
    script[1]=script[2]=0;script[3]=0x7FFFFFFF;
    for(unsigned i=4;i<=10;++i)script[i]=0;
    label[21]=unsigned(script.data());
    reinterpret_cast<void(__cdecl*)(void*,float,float)>(0x525050)(label,180.f,-59.f);
   }
  }
 }
 const bool showLabel=missingModel&&picker.label;
 if(showLabel&&picker.labelHandle!=choice->record.handle){
  Text(picker.label,choice->label.c_str());
  picker.labelHandle=choice->record.handle;
 }
 Show(picker.label,showLabel);
 if(reset&&picker.label&&picker.originalLabelScript)picker.label[21]=picker.originalLabelScript;
 if(missingMake||picker.hidManufacturer)Show(make,!missingMake);
 if(showLabel||picker.hidModel)Show(model,!showLabel);
 picker.hidManufacturer=missingMake;picker.hidModel=showLabel;
}
void PreviewCar(void* self,PickerNode* node){
 *reinterpret_cast<PickerNode**>(static_cast<BYTE*>(self)+0x34)=node;
 if(!node)return;
 // Equivalent to the visual section of 007AE350. Its career book-keeping
 // reads violation data and changes UserProfile's active car on navigation.
 const auto slot=reinterpret_cast<unsigned(__thiscall*)(void*)>(0x7A3B80)(self);
 if(slot<6)*reinterpret_cast<unsigned*>(static_cast<BYTE*>(self)+0x50+4*slot)=node->handle;
 auto garage=reinterpret_cast<void*(__cdecl*)(const char*)>(0x591E90)("GarageMain.fng");
 if(auto scene=Read<BYTE*>(garage,0x34))*reinterpret_cast<unsigned*>(scene+0x574)=0;
 auto frontend=Read<void*>(reinterpret_cast<void*>(0x91CADC));
 using Message=void(__thiscall*)(void*,unsigned,const char*,unsigned);
 auto send=reinterpret_cast<Message>(0x516C90);
 if(frontend){send(frontend,0xA05A328E,nullptr,0);send(frontend,0x9C0A27EB,CarSelect,0);}
 *reinterpret_cast<BYTE*>(static_cast<BYTE*>(self)+0x1A4)=1;
 *reinterpret_cast<unsigned*>(static_cast<BYTE*>(self)+0x38)=Read<unsigned>(reinterpret_cast<void*>(0x925B14));
}
void __fastcall PopulatePicker(void* self,void*){
 if(self!=picker.screen){originalPickerPopulate(self);return;}
 auto head=PickerHead(self);
 while(auto node=head->next){
  if(node==head)break;
  head->next=node->next;node->next->prev=head;
  reinterpret_cast<void(__cdecl*)(void*)>(0x652B00)(node);
 }
 for(const auto& car:picker.cars){
  auto node=reinterpret_cast<PickerNode*(__cdecl*)(unsigned)>(0x652AD0)(sizeof(PickerNode));
  if(!node)break;
  *node={head,head->prev,car.record.handle,0};
  head->prev->next=node;head->prev=node;
 }
 auto first=head->next;
 PreviewCar(self,first!=head?first:nullptr);
}
void* __fastcall PickerConstructor(void* self,void*,void* data){
 const bool ours=picker.opening&&Named(Read<const char*>(data),CarSelect);
 if(ours){picker.opening=false;picker.screen=self;}
 auto result=originalPickerConstructor(self,data);
 if(ours){
  *reinterpret_cast<BYTE**>(static_cast<BYTE*>(self)+0x70)=picker.blankStatus;
  HidePickerDetails(self,true);
 }
 return result;
}
void* __fastcall PickerDelete(void* self,void*,unsigned flags){
 const bool ours=self==picker.screen;
 if(ours)UpdatePickerName(self,true);
 auto result=originalPickerDelete(self,flags);
 if(ours)picker={};
 return result;
}
void __fastcall PickerReact(void* self,void*,unsigned message,unsigned a,unsigned b,unsigned c){
 if(self!=picker.screen){originalPickerReact(self,message,a,b,c);return;}
 if(picker.closing)return;
 // Native modal replies return to the picker. Keep the selected vehicle
 // stable while the dialog owns input, and consume duplicate accept events.
 if(picker.awaitingConfirmation){
  if(message==0x6820E23E){picker.awaitingConfirmation=false;picker.action=PickerAction::Start;}
  else if(message==0xB4EDEB6D||message==0x911AB364){
   picker.awaitingConfirmation=false;picker.confirmedHandle=0;picker.action=PickerAction::None;
   Log("RPD_PICKER_CONFIRM declined=1 selected_car_retained=1");
  }
  return;
 }
 if(picker.action!=PickerAction::None)return;
 switch(message){
  case Accept:
  case 0x406415E3:picker.action=PickerAction::ConfirmDispatch;break;
  case 0x911AB364:picker.action=PickerAction::Back;break;
  case 0x9120409E: // previous
  case 0xB5971BF1:{ // next
   auto current=Read<PickerNode*>(self,0x34);
   if(!current)break;
   auto head=PickerHead(self);
   const bool forward=message==0xB5971BF1;
   auto next=forward?current->next:current->prev;
   if(next==head)next=forward?head->next:head->prev;
   if(next!=current&&next!=head){
    PreviewCar(self,next);
    reinterpret_cast<void(__thiscall*)(void*)>(0x7ADA50)(self);
   }
   break;
  }
  case 0xC98356BA: // wait for the native garage preview to become ready
   originalPickerReact(self,message,a,b,c);
   break;
  default:break;
 }
 HidePickerDetails(self,true);
}
void PickerMessage(const char* parent,unsigned message,const wchar_t* en,const wchar_t* ja){
 event_titles::titles.insert_or_assign(message,event_titles::Title{en,"rpd-vehicle-picker",{},ja});
 reinterpret_cast<void(__cdecl*)(const char*,const char*,unsigned,unsigned)>(0x598700)(parent,nullptr,1,message);
}
void OpenPicker(void* sheet,const mwel::EventPackage& package){
 if(picker.opening||picker.screen||Read<unsigned>(reinterpret_cast<void*>(0x8F5360)))return;
 auto profile=Read<void*>(Read<void*>(reinterpret_cast<void*>(0x91CF90)),0x10);
 auto garage=reinterpret_cast<void*(__cdecl*)(const char*)>(0x591E90)("GarageMain.fng");
 if(!profile||!garage)return;
 picker.carDb=static_cast<BYTE*>(profile)+0x414;
 picker.rank=ActiveRank();
 picker.eventId=package.nativeEventId;
 const unsigned heat=MWELRpdAuthoredHeat(package.authoring);
 unsigned handle=FirstLoanHandle;
 for(const auto& car:rpd_vehicle_catalog::entries){
  if(car.helicopter||!car.available||car.name=="copmidsize_nis"||car.name=="copmidsize_nis_ld")continue;
  if(car.heat!=heat&&!(car.regulatedHeats&(1u<<heat)))continue;
  if(!MWELRpdPresetAvailable(car.preset))continue;
  auto vehicle=rpd_vehicle_catalog::findCollection(rpd_vehicle_catalog::Hash("pvehicle"),car.source);
  if(!vehicle)continue;
  unsigned frontend[3]{};
  Copy(rpd_vehicle_catalog::Data(vehicle,"frontend"),frontend,sizeof(frontend));
  while(originalFindCar(picker.carDb,handle))++handle;
  picker.cars.push_back({car.source,{handle++,frontend[1],car.source,2,0xff,0xff,0},LoanLabel(car)});
 }
 if(!mwel_rpd::OfficerHandoff(package.authoring)){
  const auto active=Read<unsigned>(profile,0xA8);
  if(active&&active!=~0u)
   if(auto owned=originalFindCar(picker.carDb,active))picker.cars.push_back({0,*owned,{}});
 }
 Log("RPD_PICKER_OPEN id=%s heat=%u options=%u native=1",package.id.c_str(),heat,unsigned(picker.cars.size()));
 if(picker.cars.empty()){
  picker={};
  PickerMessage(Read<const char*>(sheet,0x10),event_presentation::Hash("MWEL_RPD_NO_ELIGIBLE_VEHICLES"),
   L"No vehicles meet this event's Heat requirement.",
   L"\u3053\u306e\u30d2\u30fc\u30c8\u3067\u51fa\u52d5\u3067\u304d\u308b\u8eca\u4e21\u304c\u3042\u308a\u307e\u305b\u3093");
  return;
 }
 picker.opening=true;
 SwitchPackage(CarSelect);
}
void Frame(){
 if(!picker.screen||picker.closing)return;
 HidePickerDetails(picker.screen);
 UpdatePickerName(picker.screen);
 if(picker.action==PickerAction::None)return;
 const auto action=picker.action;picker.action=PickerAction::None;
 auto back=[](){
  pendingRank=picker.rank;picker.closing=true;
  SwitchPackage(SafehouseSheet);
 };
 if(action==PickerAction::Back){back();return;}
 auto db=Read<void*>(reinterpret_cast<void*>(0x91E004));
 auto descriptor=db?registration_experiment::originalParameterLookup(db,registration_experiment::Hash(picker.eventId.c_str())):nullptr;
 auto package=descriptor?event_presentation::PackageFor(descriptor):nullptr;
 if(!package||!mwel_rpd::InGameSelected(package->authoring)||
    !mwel_rpd::CanLaunchFromBlacklist(package->authoring,true,picker.rank,ActiveRank())){back();return;}
 const auto choice=SelectedCar(picker.screen);
 if(!choice){back();return;}
 if(action==PickerAction::ConfirmDispatch){
  const auto prompt=event_presentation::Hash("MWEL_RPD_CONFIRM_DISPATCH");
  event_titles::titles.insert_or_assign(prompt,event_titles::Title{
   L"Deploy in this vehicle?\nRacer vehicles will be treated as undercover vehicles.","rpd-dispatch-confirmation",{},
   L"\u3053\u306e\u8eca\u4e21\u3067\u51fa\u6483\u3057\u307e\u3059\u304b\uff1f\n\u203b\u30ec\u30fc\u30b5\u30fc\u8eca\u4e21\u306e\u5834\u5408\u306f\u8986\u9762\u8eca\u4e21\u3068\u3057\u3066\u6271\u3044\u307e\u3059\u3002"});
  picker.confirmedHandle=choice->record.handle;picker.awaitingConfirmation=true;
  // COMMON_YES / COMMON_NO, native YesButton / NoButton reply messages.
  // Default to No so a held/repeated Accept cannot dispatch immediately.
  reinterpret_cast<void(__cdecl*)(const char*,const char*,int,unsigned,unsigned,unsigned,unsigned,unsigned,unsigned)>(0x598DD0)(
   CarSelect,"",3,0x70E01038,0x417B25E4,0x6820E23E,0xB4EDEB6D,1,prompt);
  Log("RPD_PICKER_CONFIRM shown=1 handle=%08X default_no=1",picker.confirmedHandle);
  return;
 }
 if(action!=PickerAction::Start||picker.confirmedHandle!=choice->record.handle){
  picker.confirmedHandle=0;return;
 }
 const auto source=choice->source;
 auto clone=registration_experiment::Find(registration_experiment::Hash(package->nativeNamespace.c_str()));
 const auto spec=source?rpd_player_vehicle::Prepare(source):nullptr;
 if(!clone||!clone->race||clone->package!=package||(source&&(!spec||!rpd_player_vehicle::StockPreset(*spec)))){back();return;}
 // Resume the same Safehouse Race Sheet launch sequence with its original descriptor.
 auto params=reinterpret_cast<void*(__thiscall*)(void*,void*)>(0x603440)(db,descriptor);
 if(!params){back();return;}
 clone->rpdLaunchPreset=spec?spec->loanPreset.c_str():nullptr;
 clone->rpdUsesOwnCar=!source;
 pickerUsed=true;picker.closing=true;
 *reinterpret_cast<void**>(0x91CA40)=descriptor;
 Log("RPD_PICKER_DISPATCH id=%s source=%08X preset=%s own_car=%u",package->id.c_str(),source,spec?spec->loanPreset.c_str():"",unsigned(!source));
 reinterpret_cast<void(__thiscall*)(void*,void*,unsigned)>(0x600AB0)(db,params,2);
 reinterpret_cast<void(__thiscall*)(void*,void*)>(0x5E39E0)(db,params);
 reinterpret_cast<void(__cdecl*)()>(0x57ECA0)();
}
void __fastcall React(void*,void*,const char* package,unsigned message,void*,unsigned,unsigned){
 if(message!=Accept||!Named(package,SafehouseOverview))return;
 const auto rank=ActiveRank();
 auto frontend=*reinterpret_cast<void**>(0x91CADC);
 if(!frontend||!mwel_rpd::ValidRank(rank))return;
 // Native IconOption::React uses this index to route the parent accept.
 // Five has no retail branch; this option owns its distinct page routing.
 *reinterpret_cast<unsigned*>(0x91CA38)=Category;
 pendingRank=rank;
 reinterpret_cast<void(__thiscall*)(void*,const char*,unsigned,unsigned,unsigned)>(0x525940)(
  frontend,SafehouseSheet,0,0,0);
 Log("RPD_CASE_FILES_OPEN in_game=0 rank=%u native_categories_preserved=4",rank);
}
unsigned optionTable[2]={0x52EDC0,unsigned(React)};
void AppendCategory(void* self){
 auto option=reinterpret_cast<BYTE*(__cdecl*)(unsigned)>(0x652AD0)(0x50);
 if(!option){Log("RPD_CATEGORY_REFUSED allocation=1");return;}
 const auto texture=reinterpret_cast<void*(__cdecl*)(unsigned,int,int)>(0x503400)(rpd_ui_assets::Category::Texture,0,0)?rpd_ui_assets::Category::Texture:0x021A4B0C;
 reinterpret_cast<void*(__thiscall*)(void*,unsigned,unsigned,unsigned)>(0x586FA0)(option,texture,Title,0);
 *reinterpret_cast<unsigned*>(option)=unsigned(optionTable);*reinterpret_cast<unsigned*>(option+0x4C)=Category;
 originalAddOption(self,option);
 Log("RPD_CATEGORY_ADDED native_options=4 additional_options=1 title=RPD_CASE_FILES");
}
unsigned __fastcall AddOption(void* self,void*,void* option){
 const bool lastOverviewOption=_ReturnAddress()==reinterpret_cast<void*>(0x551229);
 const auto result=originalAddOption(self,option);
 // Append BEFORE 00551263 initializes the scroller's range. Appending after
 // the complete 00551070 routine leaves the fifth item outside that range.
 if(lastOverviewOption){
  const auto name=Read<const char*>(self,0x10);
  if(Named(name,SafehouseOverview))AppendCategory(self);
  else if(Named(name,"InGameReputationOverview.fng"))Log("RPD_CASE_FILES_HIDDEN in_game=1");
 }
 return result;
}
void __fastcall Populate(void* self,void*){
 const auto sheet=sheets.find(self);
 if(sheet==sheets.end()||!IsSafehouseSheet(self)){originalSheetPopulate(self);return;}
 rpd_controls::Capture();
 auto grid=static_cast<BYTE*>(self)+0x2C;
 reinterpret_cast<void(__thiscall*)(void*)>(0x593880)(grid);
 auto database=*reinterpret_cast<void**>(0x91E004);unsigned added=0;
 for(const auto& package:packages)if(database&&mwel_rpd::ListedAt(package.authoring,sheet->second)){
  auto descriptor=registration_experiment::originalParameterLookup(database,registration_experiment::Hash(package.nativeEventId.c_str()));
  if(descriptor&&reinterpret_cast<bool(__thiscall*)(void*,void*)>(0x51F120)(self,descriptor))++added;
 }
 // Same finalization as UIRaceSheet::Populate 00547FA5..00547FE1.
 *reinterpret_cast<unsigned*>(static_cast<BYTE*>(self)+0x54)=0x9BA78BA2;
 auto selected=Read<BYTE*>(self,0x44);
 if(selected)reinterpret_cast<void(__thiscall*)(void*,void*,void*,unsigned,unsigned)>(0x546400)(
  static_cast<BYTE*>(self)+0xE0,Read<void*>(selected,0x18),Read<void*>(self,0x1B0),0,0);
 reinterpret_cast<void(__thiscall*)(void*,unsigned)>(0x593560)(grid,0);
 auto table=Read<void**>(grid);reinterpret_cast<void(__thiscall*)(void*)>(table[2])(grid);
 Log("RPD_CASE_FILES_LIST entries=%u rank=%u racer_events_included=0",added,sheet->second);
}
void* __fastcall SheetConstructor(void* self,void*,void* data){
 const auto rank=pendingRank;pendingRank=0;
 const bool police=mwel_rpd::ValidRank(rank)&&rank==ActiveRank()&&
  Named(Read<const char*>(data),SafehouseSheet)&&!Read<unsigned>(data,8);
 sheets.erase(self);if(police)sheets.emplace(self,rank);
 auto result=originalSheetConstructor(self,data);
 if(police&&IsSafehouseSheet(self)){
  auto name=Read<const char*>(self,0x10);
  reinterpret_cast<void(__cdecl*)(const char*,unsigned,unsigned)>(0x525220)(name,0xBDE82FCC,Title);
  Log("RPD_CASE_FILES_SCREEN owner=%08X native_grid=1",unsigned(self));
 }
 return result;
}
void __fastcall SheetReact(void* self,void*,unsigned message,unsigned a,unsigned b,unsigned c){
 if(message==Accept||message==Confirm){
  const auto sheet=sheets.find(self);
  // 00551560 dispatches Accept to the selected grid tile; 005116E1
  // publishes its descriptor only then. Confirm consumes that publication.
  const auto tile=Read<void*>(self,0x44);
  const auto current=Read<void*>(tile,0x18);
  const auto selected=message==Accept?current:Read<void*>(reinterpret_cast<void*>(0x91CA40));
  const auto package=selected?event_presentation::PackageFor(selected):nullptr;
  const bool police=package&&mwel_rpd::CaseFile(package->authoring);
  if((sheet!=sheets.end()||police)&&(!police||sheet==sheets.end()||selected!=current||
     !mwel_rpd::CanLaunchFromBlacklist(package->authoring,IsSafehouseSheet(self),sheet->second,ActiveRank()))){
   Log("RPD_CASE_FILES_LAUNCH_BLOCKED message=%08X rank=%u",message,ActiveRank());
   return;
  }
  if(police&&mwel_rpd::InGameSelected(package->authoring)){
   if(message==Accept){
    if(pickerInstalled)OpenPicker(self,*package);
    else PickerMessage(Read<const char*>(self,0x10),event_presentation::Hash("MWEL_RPD_PICKER_UNAVAILABLE"),
     L"RPD vehicle selection is unavailable in this game configuration.",
     L"\u3053\u306e\u74b0\u5883\u3067\u306fRPD\u306e\u8eca\u4e21\u9078\u629e\u3092\u5229\u7528\u3067\u304d\u307e\u305b\u3093");
   }
   return;
  }
 }
 originalSheetReact(self,message,a,b,c);
}
void* __fastcall SheetDelete(void* self,void*,unsigned flags){sheets.erase(self);return originalSheetDelete(self,flags);}
struct Hook {unsigned address;void* replacement;void** original;const char* bytes;unsigned length;};
template<size_t Count>bool InstallHooks(const Hook (&hooks)[Count]){
 for(const auto& h:hooks)if(memcmp(reinterpret_cast<void*>(h.address),h.bytes,h.length))return false;
 std::vector<void*> prepared;
 for(const auto& h:hooks){auto address=reinterpret_cast<void*>(h.address);if(MH_CreateHook(address,h.replacement,h.original)!=MH_OK){for(auto p:prepared)MH_RemoveHook(p);return false;}prepared.push_back(address);}
 for(auto address:prepared)MH_QueueEnableHook(address);
 if(MH_ApplyQueued()!=MH_OK){for(auto address:prepared){MH_DisableHook(address);MH_RemoveHook(address);}return false;}
 return true;
}
bool Install(){
 if(!Available())return true;
 const Hook hooks[]={
  {0x573960,AddOption,reinterpret_cast<void**>(&originalAddOption),"\x8B\x41\x2C\x56\x8B\x74\x24\x08",8},
  {0x55A760,SheetConstructor,reinterpret_cast<void**>(&originalSheetConstructor),"\x6A\xFF\x68\x7A\x0D\x87\x00",7},
  {0x547710,SheetDelete,reinterpret_cast<void**>(&originalSheetDelete),"\x56\x8B\xF1",3},
  {0x547E30,Populate,reinterpret_cast<void**>(&originalSheetPopulate),"\x83\xEC\x08\x53\x55\x56",6},
  {0x551530,SheetReact,reinterpret_cast<void**>(&originalSheetReact),"\x64\xA1\x00\x00\x00\x00\x6A\xFF\x68\x4F\x09\x87\x00",13}};
 const Hook pickerHooks[]={
  {0x5FC5C0,PlayerCar,reinterpret_cast<void**>(&originalPlayerCar),"\x56\x8B\xF1\x8B\x46\x0C\x85\xC0",8},
  {0x7C2BA0,PickerConstructor,reinterpret_cast<void**>(&originalPickerConstructor),"\x6A\xFF\x68\x96\x3B\x88\x00",7},
  {0x7BF790,PickerDelete,reinterpret_cast<void**>(&originalPickerDelete),"\x56\x8B\xF1",3},
  {0x7BF7B0,PopulatePicker,reinterpret_cast<void**>(&originalPickerPopulate),"\x83\xEC\x0C\x53\x8B\xD9",6},
  {0x7C1770,PickerReact,reinterpret_cast<void**>(&originalPickerReact),"\x81\xEC\x24\x03\x00\x00\x53",7},
  {0x56ECC0,FindCar,reinterpret_cast<void**>(&originalFindCar),"\x56\x8B\x74\x24\x08\x33\xC0",7},
  {0x5910E0,BuildPreview,reinterpret_cast<void**>(&originalBuildPreview),"\x8B\x54\x24\x04\x53\x57\x8B\xD9",8}};
 if(!InstallHooks(hooks))return false;
 if(std::any_of(packages.begin(),packages.end(),[](const auto& p){return mwel_rpd::InGameSelected(p.authoring);})){
  pickerInstalled=InstallHooks(pickerHooks);
  Log("RPD_PICKER_HOOKS installed=%u native_car_select=1",unsigned(pickerInstalled));
 }
 event_titles::titles.emplace(Title,event_titles::Title{L"RPD CASE FILES","rpd-case-files",{}});
 installed=true;Log("RPD_CASE_FILES_ACTIVE standalone_category=1 original_fng_modified=0");return true;
}
}
