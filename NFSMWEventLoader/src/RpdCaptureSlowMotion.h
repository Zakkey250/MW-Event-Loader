#pragma once

namespace rpd_capture_slow_motion {
// Multiply the native simulation dt after its own time strategy has run.
// Speedbreaker, the world strategy and the user's game-speed setting retain
// their ownership. Rendering, menus and the audio mixer use their own clocks.
float scale=1.f;
void* trampoline=nullptr;
bool attempted=false;

void __declspec(naked) Step(){__asm{
 fmul dword ptr [scale]
 jmp dword ptr [trampoline]
}}

bool Install(){
 if(attempted)return trampoline!=nullptr;
 attempted=true;
 const BYTE expected[]={0x8B,0x3D,0x78,0x2B,0x98,0x00,0xD9,0x56,0x1C,0x0F,0x85};
 auto site=reinterpret_cast<void*>(0x6F6DA0);
 if(memcmp(site,expected,sizeof(expected))||MH_CreateHook(site,Step,&trampoline)!=MH_OK)return false;
 if(MH_EnableHook(site)!=MH_OK){MH_RemoveHook(site);trampoline=nullptr;return false;}
 return true;
}

void Reset(){scale=1.f;}
void Apply(){if(trampoline)scale=.35f;}
}
