#pragma once
// Private, paired NFRR/MWEL prototype contract. Values are not INI settings.
// A scope query takes a live ISimable handle; zero queries only the event mode.
namespace rpd_ai_contract {
constexpr unsigned Version=2;
enum Mode : unsigned {None=0,CustomFreeRun=1,NeoSprint=2};
// Target() v2: a valid hold target must suppress the race-pace raise. Merely
// aiming at the present position can otherwise leave native reverse/throttle
// trying to reach an already-blocked point at the full chase speed.
enum TargetResult : unsigned {TargetUnavailable=0,TargetReady=1,TargetHold=3};
using Scope=unsigned(__cdecl*)(unsigned);
using Target=unsigned(__cdecl*)(unsigned,const float*,const float*,float,float*);
using Ready=unsigned(__cdecl*)(unsigned);
}
