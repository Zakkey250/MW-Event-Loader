#pragma once

namespace native_patrol_scan {
void* countLoop = nullptr;
void* updateLoop = nullptr;

// AICopManager owns this vector. Vehicle callbacks in the two native passes
// can change its size or backing storage. The native loops only test equality
// with the current end, so an invalidated cursor can run past the array.
bool __cdecl ReadableSlot(const BYTE* manager, const unsigned* cursor) {
 struct Vector {
  const unsigned* data;
  unsigned capacity;
  unsigned count;
 } live{};
 const auto address = reinterpret_cast<std::uintptr_t>(cursor);
 unsigned vehicle = 0;
 if (Copy(manager + 0xC8, &live, sizeof(live)) && live.data && live.count <= live.capacity) {
  const auto base = reinterpret_cast<std::uintptr_t>(live.data);
  const auto offset = address - base;
  if (address >= base && offset % sizeof(unsigned) == 0 && offset / sizeof(unsigned) < live.count &&
      Copy(cursor, &vehicle, sizeof(vehicle)) && vehicle) return true;
 }
 if (worldLogEnabled) {
  static ULONGLONG next = 0;
  const auto now = GetTickCount64();
  if (now >= next) {
   next = now + 5000;
   Log("NATIVE_PATROL_SCAN_DEFERRED manager=%08X data=%08X cursor=%08X count=%u capacity=%u",
       unsigned(manager), unsigned(live.data), unsigned(cursor), live.count, live.capacity);
  }
 }
 return false;
}

// Both sites run inside the same native frame. An invalid slot returns via
// its own epilogue; the next simulation update starts with a fresh cursor.
void __declspec(naked) CountSlot() {
 __asm {
  pushfd
  pushad
  push esi
  push ebp
  call ReadableSlot
  add esp, 8
  test al, al
  jz deferred
  popad
  popfd
  jmp dword ptr [countLoop]
 deferred:
  popad
  popfd
  push 0x431927
  ret
 }
}

void __declspec(naked) UpdateSlot() {
 __asm {
  pushfd
  pushad
  push eax
  push ebp
  call ReadableSlot
  add esp, 8
  test al, al
  jz deferred
  popad
  popfd
  jmp dword ptr [updateLoop]
 deferred:
  popad
  popfd
  push 0x431927
  ret
 }
}

bool Install() {
 auto count = reinterpret_cast<void*>(0x431590);
 auto update = reinterpret_cast<void*>(0x4315F1);
 const BYTE countBytes[] = {0x8B, 0x0E, 0x8B, 0x01, 0xFF, 0x90, 0xAC, 0, 0, 0};
 const BYTE updateBytes[] = {0x8B, 0x30, 0x8B, 0x16, 0x8B, 0xCE, 0xFF, 0x92, 0x88, 0, 0, 0};
 const BYTE exitBytes[] = {0x8B, 0x4C, 0x24, 0x6C, 0x5F, 0x5E, 0x5D, 0x64, 0x89, 0x0D, 0, 0, 0, 0, 0x5B, 0x83, 0xC4, 0x68, 0xC3};
 if (memcmp(count, countBytes, sizeof(countBytes)) || memcmp(update, updateBytes, sizeof(updateBytes)) ||
     memcmp(reinterpret_cast<void*>(0x431927), exitBytes, sizeof(exitBytes))) return false;
 if (MH_CreateHook(count, CountSlot, &countLoop) != MH_OK) return false;
 if (MH_CreateHook(update, UpdateSlot, &updateLoop) != MH_OK) {
  MH_RemoveHook(count);
  countLoop = nullptr;
  return false;
 }
 MH_QueueEnableHook(count);
 MH_QueueEnableHook(update);
 if (MH_ApplyQueued() == MH_OK) return true;
 for (auto site : {count, update}) {
  MH_DisableHook(site);
  MH_RemoveHook(site);
 }
 countLoop = updateLoop = nullptr;
 return false;
}
}
