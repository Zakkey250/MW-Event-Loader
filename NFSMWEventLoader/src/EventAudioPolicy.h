#pragma once
#include <algorithm>
namespace mwel_audio {
inline unsigned ExpandedCount(unsigned nativeCount,unsigned requested,bool owned,bool expandedMixer){
 // Six is the existing cooperating-mod ceiling. Race count is not an audio
 // ceiling; an unsupported mixer must retain the game's exact four voices.
 return owned&&expandedMixer&&nativeCount>4&&nativeCount<=128&&requested>=5&&requested<=6?std::min(nativeCount,requested):0;
}
}
