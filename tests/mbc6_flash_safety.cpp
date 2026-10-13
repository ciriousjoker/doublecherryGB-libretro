#include "gb_core/mbc6.h"
#include <algorithm>
#include <iostream>
#include <random>
#include <vector>

int main() {
    std::vector<uint8_t> bytes(Mbc6::BatterySize + 64, 0xa5);
    auto *battery = bytes.data() + 32;
    std::fill(battery, battery + Mbc6::BatterySize, 0xff);
    battery[Mbc6::ProtectionOffset] = 0;
    std::vector<uint8_t> rom(Mbc6::RomSize, 0);
    Mbc6 mapper;
    mapper.reset();
    size_t size = 0;
    serializer count(&size, serializer::COUNT);
    mapper.serialize(count);
    std::vector<uint8_t> state(size);
    std::mt19937 random(0x6bc);
    for (unsigned i = 0; i < 100000; ++i) {
        mapper.write(uint16_t(random()), uint8_t(random()), battery);
        mapper.advance(random() & 0x7fff, battery);
        (void)mapper.read(uint16_t(random()), rom.data(), battery);
        if (!(i & 127)) {
            serializer save(state.data(), serializer::SAVE_BUF);
            mapper.serialize(save);
            for (unsigned j = 0; j < 8; ++j) state[random() % state.size()] = uint8_t(random());
            serializer load(state.data(), serializer::LOAD_BUF);
            mapper.serialize(load);
            mapper.advance(Mbc6::EraseTicks, battery);
        }
    }
    if (!std::all_of(bytes.begin(), bytes.begin() + 32, [](uint8_t v) { return v == 0xa5; }) ||
        !std::all_of(bytes.end() - 32, bytes.end(), [](uint8_t v) { return v == 0xa5; })) return 1;
    std::cout << "100000 deterministic mapper operations and corrupt-state mutations preserved battery bounds\n";
}
