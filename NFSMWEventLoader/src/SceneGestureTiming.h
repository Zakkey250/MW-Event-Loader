#pragma once
#include <string_view>
namespace scene_gesture_timing {
enum class Clip { None, Paired, Radio };
inline Clip Identify(unsigned count,float tail,float go,float end){
 if(count!=4)return Clip::None;
 if(tail==6.f&&go==288.f/30.f&&end==415.f/30.f)return Clip::Paired;
 if(tail==200.f/30.f&&go==289.f/30.f&&end==415.f/30.f)return Clip::Radio;
 return Clip::None;
}
// These are identities of verified converted motion/clock data, not folder
// names. Unknown models, custom clocks and already adjusted timing stay owned
// by their authors. No game-derived payload is required in the MOD package.
inline float Correct(Clip clip,float original,std::string_view character,std::string_view clock,std::string_view second){
 if(clock!="5277FE07B2416346FC609D837158B637FC69E01DB602D0BD44ECD70B2B0313E4")return original;
 if(clip==Clip::Paired&&character=="3E692EE7A71FFB74D6D839FD2FCACD25EDC5F1CA4CC14DA0EA6CC163C3E2655A"&&second=="157AB57A5D97EAD09E5D2DF9E687E01E2421BF0375DFAEA613FCAD357564858E")return 523.f/60.f;
 if(clip==Clip::Radio&&character=="563090B39EA93ED25F66503A37F8B443DFE41D6170DEBAA4E7CACAAF4B8EEF07")return 289.f/30.f+(289.f/30.f-200.f/30.f)/3.f;
 return original;
}
}
