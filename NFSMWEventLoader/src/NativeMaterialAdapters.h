#pragma once
namespace event_material_guard {
unsigned blendResume=0x6C6A35,tailResume=0x6C6A61,tailEnd=0x6C6C97;
void __cdecl Trace(){
 if(!worldLogEnabled)return;static DWORD last=0;const auto now=GetTickCount();
 if(now-last>5000){last=now;Log("EVENT_NATIVE_MATERIAL_FIXED_FUNCTION current_effect=0 shader_tail_skipped=1 geometry_preserved=1");}
}
__declspec(naked) void Blend(){__asm {
 mov eax,0982C80h
 mov eax,[eax]
 test eax,eax
 jnz valid
 cmp eax,1
 jmp dword ptr [blendResume]
 valid:
 cmp dword ptr [eax+4],edi
 jmp dword ptr [blendResume]
}}
__declspec(naked) void Tail(){__asm {
 mov ecx,0982C80h
 mov ecx,[ecx]
 test ecx,ecx
 jnz valid
 cmp byte ptr [worldLogEnabled],0
 je done
 pushfd
 pushad
 mov ebp,esp
 sub esp,528
 and esp,0FFFFFFF0h
 fxsave [esp]
 call Trace
 fxrstor [esp]
 mov esp,ebp
 popad
 popfd
 done:
 jmp dword ptr [tailEnd]
 valid:
 jmp dword ptr [tailResume]
}}
}
