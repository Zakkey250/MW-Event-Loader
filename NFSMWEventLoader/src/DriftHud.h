#pragma once
#include "DriftLabels.h"
// Carbon/UG2 presentation model: banked total, live drift chain, multiplier,
// and a short bank/loss notice. All objects use the installation's native FE
// font/render path. No additional Present/EndScene hook or copied game art.
namespace drift_hud {
using namespace expanded_leaderboard;
Object package=nullptr;void* board=nullptr;char packageName[64]{};
constexpr unsigned Rows=5;
std::array<Object,Rows*2> text{};std::array<Object,3> art{};std::array<std::string,Rows> cached{};
std::array<Object,28> digits{};std::array<char,28> glyphs{};
unsigned shownBonus=0;double bonusUntil=0;
float anchorX=0,anchorY=0;
float traxSlide=0;DWORD anchorTick=0;bool traxShown=false;
bool traxProofChecked=false,traxProofEnabled=false,traxProofIssued=false;
void TraxProof(){
 if(!traxProofChecked){traxProofChecked=true;traxProofEnabled=GetFileAttributesW((game/L"MWEventLoader.local-test").c_str())!=INVALID_FILE_ATTRIBUTES&&GetFileAttributesW((game/L"scripts/MWEventLoader.drift-trax-proof.ini").c_str())!=INVALID_FILE_ATTRIBUTES;}
 if(!traxProofEnabled||traxProofIssued||drift_race::lastTime<5)return;
 // Explicit no-save lab UI stimulus only. It displays the native banner;
 // no audio, music events, countdown state or player controls are changed.
 traxProofIssued=true;const BYTE guard[]={0xA1,0x48,0xE3,0x8E,0,0x85,0xC0};
 if(!memcmp(reinterpret_cast<void*>(0x595ED0),guard,sizeof(guard))){reinterpret_cast<void(__cdecl*)(const char*,const char*,const char*)>(0x595ED0)("HUD overlap check","Native EA TRAX banner","Isolated lab UI proof");Log("DRIFT_HUD_TRAX_UI_PROOF native_banner=1 audio_unchanged=1");}
}
bool TraxVisible(void* engine){
 if(!engine||!reinterpret_cast<bool(__thiscall*)(void*,const char*)>(0x516B50)(engine,"EA_Trax.fng"))return false;
 auto trax=reinterpret_cast<Object(__thiscall*)(void*,const char*)>(0x516AF0)(engine,"EA_Trax.fng");
 // Native 0058D695 selects this track-title object. The notification stays
 // on the FE stack after its animated parent fades to alpha zero.
 auto object=trax?Find(trax,0xF5387816):nullptr;if(!object)return false;
 for(unsigned depth=0;depth<12;++depth){
  if(object[7]&1)return false;
  if(object[11]&&object[12]>=16&&!Read<unsigned>(reinterpret_cast<void*>(object[11]),12))return false;
  auto parent=ParentList(trax+18,object);if(!parent)return false;if(parent==trax+18)return true;object=parent-24;
 }return false;
}
void Anchor(){
 // Native FE units are screen-height/480. Anchor our private root objects
 // to the actual client left edge, including ultrawide windows.
 static float aspect=4.f/3.f;static DWORD next=0;
 const auto now=GetTickCount();if(!next||now-next<0x80000000u){next=now+500;auto window=GetActiveWindow();DWORD pid=0;RECT r{};
  if(window&&GetWindowThreadProcessId(window,&pid)&&pid==GetCurrentProcessId()&&GetClientRect(window,&r)&&r.right>200&&r.bottom>200)aspect=float(r.right)/r.bottom;
 }
 auto engine=Read<void*>(reinterpret_cast<void*>(0x91CADC));
 // EN1.3 NotifyChyron 00595F06 checks this same active-package query.
 // Loaded/cached EA_Trax.fng alone is insufficient: it must be on the stack.
 const bool visible=TraxVisible(engine);
 if(visible!=traxShown){traxShown=visible;Log("DRIFT_HUD_TRAX_AVOID visible=%u active_package=1 audio_unchanged=1",unsigned(visible));}
 const float dt=anchorTick?std::min(.05f,float(now-anchorTick)*.001f):0.f;anchorTick=now;
 traxSlide+=std::clamp(float(visible)-traxSlide,-dt/.28f,dt/.28f);
 const float eased=traxSlide*traxSlide*(3.f-2.f*traxSlide);
 anchorX=-240.f*aspect+8.f+123.6f;anchorY=-339.3f+80.f*eased;
}
void Position(Object object,float x,float y){if(object)reinterpret_cast<void(__cdecl*)(void*,float,float)>(0x525050)(object,x+anchorX,y+anchorY);}
// EN1.3 005C31A0 deep-clones the script list; 005BC0C0 destroys that list,
// never +54. Keep its allocation/track ownership intact. Only our clones point
// at module-lifetime, track-free script states. 005C1837 requires a VALID state;
// 005BEC90 evaluates zero tracks/events without changing the owned render data.
// Preserve the original ID (+30) so a later native clone can select its own list.
std::array<std::array<unsigned,13>,41> staticScripts{};
void Static(Object object,unsigned index){
 if(!object||!object[21]||index>=staticScripts.size())return;
 auto& script=staticScripts[index];if(object[21]!=unsigned(script.data())){
  memcpy(script.data(),reinterpret_cast<void*>(object[21]),sizeof(script));
  script[1]=script[2]=0;script[3]=0x7FFFFFFF;script[4]=script[5]=script[6]=0;
  script[7]=script[8]=script[9]=script[10]=0;object[21]=unsigned(script.data());
 }
 object[7]|=0x400000;
}
void Colour(Object object,unsigned argb){
 if(!object||!object[11]||object[12]<16)return;
 // Same evaluated B,G,R,A DWORD layout as 005B2D50, without adding an inherited
 // animation-channel offset. This is private data allocated by native Clone.
 auto d=reinterpret_cast<unsigned*>(object[11]);d[0]=argb&255;d[1]=(argb>>8)&255;d[2]=(argb>>16)&255;d[3]=argb>>24;object[7]|=0x400000;
}
void Depth(Object object,float z){if(object&&object[11]&&object[12]>=40){memcpy(reinterpret_cast<BYTE*>(object[11])+0x24,&z,4);object[7]|=0x400000;}}
unsigned Key(unsigned i){return Hash(("MWEL_DriftHud_"+std::to_string(i)).c_str());}
unsigned ArtKey(unsigned i){return Hash(("MWEL_DriftHud_Art_"+std::to_string(i)).c_str());}
unsigned DigitKey(unsigned i){return Hash(("MWEL_DriftHud_Digit_"+std::to_string(i)).c_str());}
void Root(Object object){
 auto list=ParentList(package+18,object);auto root=package+18;if(!list||list==root)return;
 auto next=reinterpret_cast<Object>(object[1]),previous=reinterpret_cast<Object>(object[2]);
 if(previous)previous[1]=unsigned(next);else list[1]=unsigned(next);
 if(next)next[2]=unsigned(previous);else list[2]=unsigned(previous);--list[0];
 auto tail=reinterpret_cast<Object>(root[2]);object[1]=0;object[2]=unsigned(tail);
 if(tail)tail[1]=unsigned(object);else root[1]=unsigned(object);root[2]=unsigned(object);++root[0];ClearCache(object);
}
bool Live(){
 auto engine=Read<void*>(reinterpret_cast<void*>(0x91CADC));
 if(!package||!engine||!packageName[0]||reinterpret_cast<Object(__thiscall*)(void*,const char*)>(0x516AF0)(engine,packageName)!=package)return false;
 for(unsigned i=0;i<text.size();++i)if(text[i]&&Find(package,Key(i))!=text[i])return false;
 for(unsigned i=0;i<art.size();++i)if(art[i]&&Find(package,ArtKey(i))!=art[i])return false;
 for(unsigned i=0;i<digits.size();++i)if(digits[i]&&Find(package,DigitKey(i))!=digits[i])return false;
 return true;
}
void Hide(){if(Live()){for(auto o:text)Show(o,false);for(auto o:art)Show(o,false);for(auto o:digits)Show(o,false);}}
void Reset(){Hide();package=nullptr;board=nullptr;packageName[0]=0;text={};art={};digits={};glyphs={};cached={};shownBonus=0;bonusUntil=0;traxSlide=0;anchorTick=0;traxShown=false;traxProofChecked=traxProofEnabled=traxProofIssued=false;}
bool DigitArt(Object background){
 if(!event_presentation::driftDigitResource||!background||!background[11]||background[12]<84)return false;
 for(unsigned i=0;i<digits.size();++i)if(!digits[i]){
  digits[i]=Clone(package,background,DigitKey(i));if(!digits[i])return false;
  if(!digits[i][11]||digits[i][12]<84)return false;
  memcpy(reinterpret_cast<void*>(digits[i][11]),reinterpret_cast<void*>(background[11]),84);digits[i][23]=0;Root(digits[i]);Static(digits[i],13+i);Depth(digits[i],90.f);
  reinterpret_cast<void(__cdecl*)(const char*,unsigned,unsigned)>(0x527EE0)(packageName,DigitKey(i),drift_digit_asset::Texture);
 }return true;
}
void Number(unsigned& index,const std::string& value,float left,float centreY,float advance,float height,unsigned colour){
 const std::string alphabet="0123456789+xPTS-";
 for(char c:value){if(index>=digits.size())return;auto object=digits[index];if(!object)return;
  const auto glyph=alphabet.find(c);if(glyph==std::string::npos)continue;
  if(glyphs[index]!=c){const float u=float(glyph%4)*.25f,v=float(glyph/4)*.25f;const float uv[]={u,v,u+.25f,v+.25f};memcpy(reinterpret_cast<BYTE*>(object[11])+0x44,uv,16);glyphs[index]=c;object[7]|=0x400000;}
  reinterpret_cast<void(__cdecl*)(void*,float,float)>(0x5155E0)(object,advance,height);
  Position(object,left+advance*.5f,centreY);
  Colour(object,colour);Show(object,true);left+=advance;++index;
 }
}
void Features(void* self,unsigned long long mask){if(self==board&&!FeaturesVisible(self,mask))Hide();}
unsigned Score(unsigned slot){return slot<drift_race::count?drift_race::scores[slot].standing():0;}
const char* SplitLabel(){
 static std::string language,encoded="SPLIT POINTS";
 try{const auto path=event_titles::NativeLanguagePath();if(language!=path){
  language=path;encoded="SPLIT POINTS";const auto word=mwel_drift::SplitLabel(path);auto& codec=event_titles::NativeCodec(path);std::string result;bool valid=true;
  for(auto c:word){auto it=codec.find(c);if(it==codec.end()){valid=false;break;}result+=it->second;}if(valid)encoded=result;
 }}catch(...){}return encoded.c_str();
}
const char* Label(unsigned which){
 static std::string language;static std::array<std::string,6> labels={"TOTAL","DRIFT","BANKED","LOST","WALL","TRAFFIC"};
 try{auto path=event_titles::NativeLanguagePath();if(language!=path){
  language=path;labels={"TOTAL","DRIFT","BANKED","LOST","WALL","TRAFFIC"};auto lower=path;std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return char(std::tolower(c));});
  const auto main=mwel_drift::Labels(lower);const auto bonus=mwel_drift::BonusLabels(lower);auto& codec=event_titles::NativeCodec(path);
  const std::array<std::wstring,6> words={main[0],main[1],main[2],main[3],bonus[0],bonus[1]};
  for(unsigned i=0;i<words.size();++i){std::string encoded;bool valid=true;for(auto c:words[i]){auto glyph=codec.find(c);if(glyph==codec.end()){valid=false;break;}encoded+=glyph->second;}if(valid)labels[i]=encoded;}
 }}catch(...){}return labels[which].c_str();
}
void Update(void* self,void* player){
 driftActive=drift_race::Current;driftScore=Score;
 if(!drift_race::Current()||drift_race::faulted||!FeaturesVisible(self,Read<unsigned long long>(self,0x20))||reinterpret_cast<bool(__cdecl*)()>(0x64B680)()) {Hide();return;}
 auto name=Read<const char*>(self,0x10);auto engine=Read<void*>(reinterpret_cast<void*>(0x91CADC));
 auto live=name&&engine?reinterpret_cast<Object(__thiscall*)(void*,const char*)>(0x516AF0)(engine,name):nullptr;
 if(!live)return;if(live!=package||self!=board){Reset();package=live;board=self;strncpy_s(packageName,name,_TRUNCATE);}
 TraxProof();const float oldX=anchorX,oldY=anchorY;Anchor();const bool moved=oldX!=anchorX||oldY!=anchorY;
 auto source=Read<Object>(self,0x234);if(!source)return;
 auto background=Read<Object>(self,0x264);
 if(event_presentation::driftHudResource&&background&&Read<unsigned>(background,0x18)==1&&background[11]&&background[12]>=84){
  for(unsigned i=0;i<art.size();++i)if(!art[i]){
   art[i]=Clone(package,background,ArtKey(i));if(!art[i])break;
   if(!art[i][11]||art[i][12]<84)continue;
   memcpy(reinterpret_cast<void*>(art[i][11]),reinterpret_cast<void*>(background[11]),84);Root(art[i]);Static(art[i],10+i);Depth(art[i],i?190.f:200.f);
   Log("DRIFT_HUD_ART row=%u native_image_flags=%08X",i,art[i][23]);
   // Owned unmirrored, non-additive art; the leaderboard's background flags
   // belong to its original animated rank strips, not this new texture.
   art[i][23]=0;
   const float all[]={0,0,1,1},bar[]={.2f,.294f,.8f,.300f};
   memcpy(reinterpret_cast<BYTE*>(art[i][11])+0x44,i?bar:all,16);
   reinterpret_cast<void(__cdecl*)(const char*,unsigned,unsigned)>(0x527EE0)(packageName,ArtKey(i),drift_hud_asset::Texture);
   reinterpret_cast<void(__cdecl*)(void*,float,float)>(0x5155E0)(art[i],i?180.f:280.f,i?3.f:180.f);
   Position(art[i],0.f,i?174.f:150.f);
   Colour(art[i],i==1?0x66556666u:0xFFFFFFFFu);
  }
 }
 for(unsigned i=0;i<text.size();++i)if(!text[i]){
  text[i]=Clone(package,source,Key(i));if(!text[i])return;
  // A package may survive retry. Rebase its owned clones on the current
  // native font's render state rather than repeatedly scaling an old extent.
  if(source[11]&&text[i][11]&&source[12]>=68&&text[i][12]>=68)memcpy(reinterpret_cast<void*>(text[i][11]),reinterpret_cast<void*>(source[11]),68);
  Text(text[i],"");text[i][7]|=0x400000;
  Root(text[i]);Static(text[i],i);Depth(text[i],99.f);
  // Root clones use our top-left anchor; native HUD objects stay untouched.
  const unsigned row=i%Rows;const bool shadow=i>=Rows;
  // Preserve the clone's valid native script. The FE animation scheduler
  // requires it even for an apparently static string (005C1837..005C183D).
  // FEString +38/+3C are font scale (not pixel bounds). Rebase from the
  // current native source each package/retry, then scale only our own clones.
  // Numeric glyphs use owned image quads with explicit pixel dimensions.
  // Keep the installation's original font state for multilingual labels.
  // Verified native 005157EC reads ONE packed ARGB argument; the bundled
  // SDK's four-byte-channel declaration is incorrect for this executable.
  Colour(text[i],shadow?0xFF000000u:row==1?0xFFFFD246u:row==4?0xFF76E7D4u:0xFFFFFFFFu);
  float sx=0,sy=0,cx=0,cy=0;reinterpret_cast<void(__cdecl*)(void*,float*,float*)>(0x514FD0)(text[i],&sx,&sy);reinterpret_cast<void(__cdecl*)(void*,float*,float*)>(0x524EE0)(text[i],&cx,&cy);
  Log("DRIFT_HUD_OBJECT row=%u position=%.2f,%.2f centre=%.2f,%.2f",i,sx,sy,cx,cy);
 }
 auto race=reinterpret_cast<BYTE*>(drift_race::owner);const auto slot=PlayerSlot(player,race,drift_race::count);
 if(slot>=drift_race::count){Hide();return;}const auto& s=drift_race::scores[slot];
 char values[Rows][96]{};sprintf_s(values[0],"%s  %u",Label(0),s.total);
 sprintf_s(values[1],"+%u PTS",s.chain());sprintf_s(values[2],"x%u",s.multiplier());
 const bool numeric=DigitArt(background);
 if(numeric){
  sprintf_s(values[0],"%s",Label(0));values[1][0]=values[2][0]=0;
  unsigned index=0;Number(index,std::to_string(s.total),-79,160,9,14,0xFFFFFFFFu);
  auto main="+"+std::to_string(s.chain());const float advance=std::min(18.f,132.f/main.size());const float width=advance*main.size()+26;
  Number(index,main,-25-width*.5f,140,advance,30,0xFFFFD246u);
  Number(index,"PTS",-25-width*.5f+advance*main.size()+3,146,7.5f,13,0xFFFFD246u);
  auto multiplier="x"+std::to_string(s.multiplier());Number(index,multiplier,94-multiplier.size()*6.5f,140,13,24,0xFFFFFFFFu);
  while(index<digits.size())Show(digits[index++],false);
 }
 if(drift_race::lastTime<s.noticeUntil){if(s.notice==mwel_drift::Notice::Banked)sprintf_s(values[3],"%s +%u",Label(2),s.noticeAmount);else if(s.notice==mwel_drift::Notice::Lost)sprintf_s(values[3],"%s -%u",Label(3),s.noticeAmount);}
 unsigned alpha=unsigned(255*std::clamp((s.noticeUntil-drift_race::lastTime)/.4,0.,1.));
 Colour(text[3],(alpha<<24)|(s.notice==mwel_drift::Notice::Lost?0xFF6666u:0xFFFFFFu));
 Colour(text[Rows+3],alpha<<24);
 if(s.bonusFlags){shownBonus=s.bonusFlags;bonusUntil=drift_race::lastTime+.35;}
 if(drift_race::lastTime<bonusUntil){if(shownBonus==3)sprintf_s(values[4],"%s +25%%  /  %s +25%%",Label(4),Label(5));else sprintf_s(values[4],"%s +25%%",Label(shownBonus==1?4:5));}
 const float x[]={-70.f,-24.f,94.f,67.f,0.f},y[]={160.f,140.f,140.f,160.f,184.f};
 for(unsigned i=0;i<Rows;++i){if(moved||cached[i]!=values[i]){Text(text[i],values[i]);Text(text[i+Rows],values[i]);cached[i]=values[i];
   const float centreX=numeric&&i==0?-102.f:x[i];
   Position(text[i],centreX,y[i]);Position(text[i+Rows],centreX+1,y[i]+1);
  }Show(text[i],values[i][0]!=0);Show(text[i+Rows],values[i][0]!=0);}
 if(moved){Position(art[0],0,150);Position(art[1],0,174);}Show(art[0],true);Show(art[1],true);
 if(art[2]){float width=180*std::clamp(s.displayAngle/80.f,0.f,1.f);if(width>.1f){reinterpret_cast<void(__cdecl*)(void*,float,float)>(0x5155E0)(art[2],width,3.f);Position(art[2],-90+width*.5f,174.f);Colour(art[2],s.displayAngle>65?0xFFFF6666u:0xFFFFFFFFu);}Show(art[2],width>.1f);}
}
}
