#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

namespace mwel_barrier_bank {
inline constexpr std::array<uint32_t,2> Keys{0xA711C606,0x2C505700};
inline int Slot(uint32_t hash) {
    for (int i=0;i<2;++i)
        if (hash==Keys[i] || hash==(Keys[i]^0x6E15A923u)) return i;
    return -1;
}

struct Image {
    unsigned width=0,height=0,mips=0,compression=0;
    unsigned tile=0,usage=0,blend=0,flags=0,scroll=0;
    int step=0,s=0,t=0;
    std::string pixels;
    unsigned RowBytes(unsigned width) const {
        return compression==0x20?width*4:std::max(1u,(width+3)/4)*(compression==0x22?8:16);
    }
    unsigned Rows(unsigned height) const {
        return compression==0x20?height:std::max(1u,(height+3)/4);
    }
};

template<class T> inline T Read(std::string_view bytes,size_t offset) {
    T value{};
    std::memcpy(&value,bytes.data()+offset,sizeof(value));
    return value;
}

template<class F> bool Chunks(std::string_view bank,size_t at,size_t end,F visit) {
    while (at<end) {
        if (end-at<8) return false;
        const auto tag=Read<uint32_t>(bank,at),len=Read<uint32_t>(bank,at+4);
        at+=8;
        if (len>end-at || !visit(tag,at,at+len)) return false;
        at+=len;
    }
    return true;
}

// MW's uncompressed TPK container. Images inside it can use block compression.
// A complete pair is required: changing only one layer leaves mismatched walls.
inline bool Parse(std::string_view bank,std::array<Image,2>& result) {
    std::array<Image,2> found{};
    const bool valid=Chunks(bank,0,bank.size(),[&](uint32_t tag,size_t begin,size_t end) {
        if (tag!=0xB3300000) return true;
        size_t descriptors=0,descEnd=0,data=0,dataEnd=0;
        bool compressed=false;
        if (!Chunks(bank,begin,end,[&](uint32_t sub,size_t first,size_t last) {
            if (sub!=0xB3310000 && sub!=0xB3320000) return true;
            return Chunks(bank,first,last,[&](uint32_t type,size_t a,size_t b) {
                if (sub==0xB3310000 && type==0x33310004) {descriptors=a;descEnd=b;}
                if (sub==0xB3310000 && type==0x33310003 && a!=b) compressed=true;
                if (sub==0xB3320000 && type==0x33320002) {data=(a+127)&~size_t(127);dataEnd=b;}
                return true;
            });
        })) return false;
        if (compressed || !data || data>dataEnd || !descriptors || (descEnd-descriptors)%124) return true;
        for (size_t at=descriptors;at<descEnd;at+=124) {
            const auto key=Read<uint32_t>(bank,at+36);
            if (key!=Keys[0] && key!=Keys[1]) continue;
            auto& image=found[Slot(key)];
            if (!image.pixels.empty()) return false; // ambiguous source
            const auto offset=Read<uint32_t>(bank,at+48),length=Read<uint32_t>(bank,at+56);
            image.width=Read<uint16_t>(bank,at+68);
            image.height=Read<uint16_t>(bank,at+70);
            const auto byte=[&](size_t i){return static_cast<unsigned char>(bank[at+i]);};
            image.compression=byte(74);image.mips=byte(78);image.tile=byte(79);image.scroll=byte(82);
            image.usage=byte(85);image.blend=byte(86);image.flags=byte(87);
            image.step=Read<int16_t>(bank,at+88);image.s=Read<int16_t>(bank,at+90);image.t=Read<int16_t>(bank,at+92);
            if (!image.width||!image.height||image.width>4096||image.height>4096||!image.mips||
                Read<uint32_t>(bank,at+60)||image.scroll>2||image.blend>6||
                (image.compression!=0x20&&image.compression!=0x22&&image.compression!=0x24&&image.compression!=0x26))
                return false;
            size_t total=0;
            for (unsigned mip=0,w=image.width,h=image.height;mip<image.mips;++mip) {
                total+=size_t(image.RowBytes(w))*image.Rows(h);
                if (w==1 && h==1 && mip+1<image.mips) return false;
                w=std::max(1u,w/2);h=std::max(1u,h/2);
            }
            if (length!=total||offset>dataEnd-data||length>dataEnd-data-offset) return false;
            image.pixels.assign(bank.data()+data+offset,length);
        }
        return true;
    });
    if (!valid||found[0].pixels.empty()||found[1].pixels.empty()) return false;
    result=std::move(found);
    return true;
}
}
