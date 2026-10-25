#pragma once
#include <windows.h>
namespace mw_checkpoint {
struct Runtime {unsigned size,segmentsOffset,capacity,raceBytes;void* calculator;};
static_assert(sizeof(Runtime)==20,"Win32 checkpoint ABI");
using Query=unsigned(__cdecl*)(Runtime*,unsigned);
}
