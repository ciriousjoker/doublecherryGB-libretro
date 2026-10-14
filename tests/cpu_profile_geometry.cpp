#include "gb_core/mbc6.h"
#include <cassert>
#include <vector>
int main() {
    Mbc6 mapper; mapper.reset();
    std::vector<uint8_t> battery(Mbc6::BatterySize,0xff);
    assert(mapper.profile_rom_offset(0)==0);
    assert(mapper.profile_rom_offset(0x3fff)==0x3fff);
    for(unsigned bank=0;bank<128;++bank) {
        mapper.write(0x2000,uint8_t(bank),battery.data());
        mapper.write(0x3000,uint8_t(127-bank),battery.data());
        assert(mapper.profile_rom_offset(0x4000)==bank*8192);
        assert(mapper.profile_rom_offset(0x5fff)==bank*8192+8191);
        assert(mapper.profile_rom_offset(0x6000)==(127-bank)*8192);
        assert(mapper.profile_rom_offset(0x7fff)==(127-bank)*8192+8191);
    }
    mapper.write(0x2800,8,battery.data());
    assert(mapper.profile_rom_offset(0x4000)==0xffffffffu);
    assert(mapper.profile_rom_offset(0x6000)!=0xffffffffu);
    assert(mapper.profile_rom_offset(0x8000)==0xffffffffu);
}
