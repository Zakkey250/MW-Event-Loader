#pragma once
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace mwel_barrier_layout {

struct DrawPass {
    BYTE* entry = nullptr;
    BYTE* textureCall = nullptr;
    BYTE* materialCall = nullptr;
    BYTE* primitiveCall = nullptr;
    BYTE* texture = nullptr;
    BYTE* material = nullptr;
    BYTE* device = nullptr;
    const float* clock = nullptr;
    float clockRate = 1.f;
};

struct Section {
    BYTE* begin = nullptr;
    size_t size = 0;

    bool Contains(const void* address, size_t bytes) const {
        const auto offset = uintptr_t(address) - uintptr_t(begin);
        return offset <= size && bytes <= size - offset;
    }
};

template<class T> T Read(const BYTE* address) {
    T value{};
    std::memcpy(&value, address, sizeof(value));
    return value;
}

// '?' skips operands whose values are assigned by the compiler or PE loader.
template<size_t N> bool Matches(const BYTE* address, const char (&pattern)[N]) {
    for (size_t i = 0; i < N - 1; ++i)
        if (pattern[i] != '?' && address[i] != BYTE(pattern[i])) return false;
    return true;
}

inline BYTE* RelativeTarget(BYTE* operand) {
    return reinterpret_cast<BYTE*>(uintptr_t(operand + 4) + Read<int32_t>(operand));
}

inline bool DrawPrologue(const BYTE* address) {
    return Matches(address, "\x55\x8b\xec\x6a\xff\x68????\x64\xa1\x00\x00\x00\x00") ||
           Matches(address, "\x55\x8b\xec\x81\xec????\xa1????\x33\xc5\x89\x45\xfc");
}

inline DrawPass ResolveLoop(const Section& code, const Section& writable, const Section& constants, BYTE* loop) {
    // A missing texture branches to the same mesh increment as the draw.
    const unsigned meshRegister = loop[1] & 7;
    const unsigned skip = loop[13];
    if (skip < 5 || skip > 32 || !code.Contains(loop, 14 + skip + 7)) return {};
    auto* texturePresent = loop + 14 + skip;
    if (texturePresent[-5] != 0xe9 || !Matches(texturePresent, "\xff?\xe8????") || texturePresent[1] != loop[1]) return {};
    auto* materialCall = texturePresent + 2;
    const auto advance = RelativeTarget(materialCall - 6);
    if (!code.Contains(advance, 24) || advance <= materialCall + 20 || advance - loop > 0x800 ||
        !Matches(advance, "\x83?\x2c") || advance[1] != 0xc0 + meshRegister) return {};

    auto test = advance + 3;
    const auto stackMesh = 0x85 | (meshRegister << 3);
    if (test[0] == 0x89 && test[1] == stackMesh) test += 6;
    if (!Matches(test, "\x3b?????\x0f\x85????") || test[1] != stackMesh || RelativeTarget(test + 8) != loop) return {};

    BYTE* primitive = nullptr;
    if (Matches(advance - 17, "\x6a\x00\x6a\x04\x56\xff\x97\x50\x01\x00\x00\x8b\xbd????"))
        primitive = advance - 12;
    else if (Matches(advance - 13, "\x6a\x00\x6a\x04\x50\x8b\x00\xff\x90\x50\x01\x00\x00"))
        primitive = advance - 6;
    if (!primitive) return {};

    auto* entry = loop - 20;
    const auto first = loop - std::min<size_t>(loop - code.begin, 0x800);
    while (entry > first && !DrawPrologue(entry)) --entry;
    if (!DrawPrologue(entry) || loop - entry < 40) return {};

    // IDirect3DDevice9 shader and FVF setup must reference the same device.
    BYTE* device = nullptr;
    constexpr char setup[] = "\xff\x90\x70\x01\x00\x00\x8b\x0d????\x57\x51\x8b\x01\xff\x90\xac\x01\x00\x00"
                             "\x8b\x0d????\x68\x42\x01\x00\x00\x51\x8b\x01\xff\x90\x64\x01\x00\x00";
    constexpr char setupEax[] = "\xff\x91\x70\x01\x00\x00\xa1????\x57\x50\x8b\x08\xff\x91\xac\x01\x00\x00"
                                "\xa1????\x68\x42\x01\x00\x00\x50\x8b\x08\xff\x91\x64\x01\x00\x00";
    for (auto* pos = entry; pos + sizeof(setup) - 1 <= loop; ++pos) {
        const bool eax = Matches(pos, setupEax);
        if (!eax && !Matches(pos, setup)) continue;
        const auto loaded = Read<BYTE*>(pos + (eax ? 7 : 8));
        if (device || loaded != Read<BYTE*>(pos + (eax ? 22 : 24)) || !writable.Contains(loaded, sizeof(void*))) return {};
        device = loaded;
    }

    const float* clock = nullptr;
    float rate = 1.f;
    for (auto* pos = materialCall + 5; pos + 16 <= primitive; ++pos) {
        if (!Matches(pos, "\xf3\x0f\x10?????\xf3\x0f\x59?????") ||
            (pos[3] & 0xc7) != 5 || pos[3] != pos[11]) continue;
        const auto value = Read<const float*>(pos + 4);
        const auto scale = Read<const BYTE*>(pos + 12);
        if (!writable.Contains(value, sizeof(float)) || !constants.Contains(scale, sizeof(float))) continue;
        if (clock) return {};
        clock = value;
        rate = Read<float>(scale);
    }

    auto* textureCall = loop + 2;
    auto* texture = RelativeTarget(textureCall + 1);
    auto* material = RelativeTarget(materialCall + 1);
    if (!device || !clock || !std::isfinite(rate) || rate <= 0.f ||
        !code.Contains(texture, 1) || !code.Contains(material, 1) || texture == material ||
        (texture >= entry && texture <= advance) || (material >= entry && material <= advance)) return {};
    return {entry, textureCall, materialCall, primitive, texture, material, device, clock, rate};
}

inline DrawPass Find(HMODULE module) {
    static_assert(sizeof(void*) == 4);
    auto* base = reinterpret_cast<BYTE*>(module);
    if (!base) return {};
    MEMORY_BASIC_INFORMATION header{};
    if (!VirtualQuery(base, &header, sizeof(header)) || header.RegionSize < sizeof(IMAGE_NT_HEADERS32)) return {};
    const auto dos = Read<IMAGE_DOS_HEADER>(base);
    if (dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < 0 ||
        size_t(dos.e_lfanew) > header.RegionSize - sizeof(IMAGE_NT_HEADERS32)) return {};
    const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos.e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) return {};

    const Section headers{base, header.RegionSize};
    auto* section = IMAGE_FIRST_SECTION(nt);
    if (!headers.Contains(section, nt->FileHeader.NumberOfSections * sizeof(*section))) return {};
    Section code{}, writable{}, constants{};
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section) {
        const auto start = section->VirtualAddress;
        const auto length = section->Misc.VirtualSize;
        if (!length || start >= nt->OptionalHeader.SizeOfImage || length > nt->OptionalHeader.SizeOfImage - start) continue;
        const Section range{base + start, length};
        const auto flags = section->Characteristics;
        if ((flags & IMAGE_SCN_MEM_READ) == 0) continue;
        if (flags & IMAGE_SCN_MEM_EXECUTE) {
            if (std::memcmp(section->Name, ".text\0\0", 8) == 0) code = range;
        } else if (flags & IMAGE_SCN_MEM_WRITE) {
            if (std::memcmp(section->Name, ".data\0\0", 8) == 0) writable = range;
        } else if (std::memcmp(section->Name, ".rdata\0", 8) == 0) constants = range;
    }
    if (!code.begin || !writable.begin || !constants.begin) return {};

    DrawPass result{};
    // Texture(key), null check, SetMaterial(key). Require the D3D9 mesh loop
    // and its device/clock operands as well before treating these as callsites.
    constexpr char mesh[] = "\xff?\xe8????\x83\xc4\x04\x85\xc0\x75?";
    for (size_t i = 20; i + sizeof(mesh) - 1 <= code.size; ++i) {
        auto* at = code.begin + i;
        if (!Matches(at, mesh) || (at[1] & 0xf8) != 0x30 || (at[1] & 7) == 4 || (at[1] & 7) == 5) continue;
        const auto candidate = ResolveLoop(code, writable, constants, at);
        if (!candidate.entry) continue;
        if (result.entry) return {}; // Ambiguous ownership of the render pass.
        result = candidate;
    }
    return result;
}

}
