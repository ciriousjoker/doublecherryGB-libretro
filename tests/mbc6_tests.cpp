#include "gb_core/gb.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

static void check(bool valid, const char *message) {
    if (!valid) throw std::runtime_error(message);
}
class TestRenderer : public renderer {
public:
    void reset() override {}
    void refresh() override {}
    void render_screen(byte*, int, int, int) override {}
    int check_pad() override { return 0; }
    word map_color(word color) override { return color; }
    word unmap_color(word color) override { return color; }
    byte get_time(int) override { return 0; }
    void set_time(int, byte) override {}
    word get_sensor(bool) override { return 0; }
    void set_bibrate(bool) override {}
};
static std::vector<byte> image() {
    std::vector<byte> data(Mbc6::RomSize, 0);
    for (unsigned bank = 0; bank < 128; ++bank) {
        data[bank * 0x2000] = byte(bank);
        data[bank * 0x2000 + 0x1fff] = byte(bank ^ 0xa5);
    }
    data[0x143] = 0xc0; data[0x147] = 0x20; data[0x148] = 5; data[0x149] = 3;
    return data;
}
struct Fixture {
    std::vector<byte> rom = image();
    TestRenderer renderer;
    gb game{&renderer, false, false};
    cpu *bus;
    Fixture() {
        check(game.load_rom(rom.data(), int(rom.size()), nullptr, 0, false), "MBC6 load");
        bus = game.get_cpu();
    }
    void enable() {
        bus->write(0x0c00, 1); bus->write(0x2800, 8); bus->write(0x3800, 8);
    }
    void command(byte first, int second = -1) {
        bus->write(0x2000, 2); bus->write(0x3000, 1);
        bus->write(0x5555, 0xaa); bus->write(0x6aaa, 0x55); bus->write(0x5555, first);
        if (second >= 0) {
            bus->write(0x5555, 0xaa); bus->write(0x6aaa, 0x55); bus->write(0x5555, byte(second));
        }
    }
    void finish(unsigned ticks = Mbc6::EraseTicks) {
        game.get_mbc()->advance_mbc6(ticks);
        check(bus->read(0x4000) & 0x80, "Ready after elapsed emulated time");
        bus->write(0x4000, 0xf0);
    }
    void program(unsigned physical, byte value, bool hidden = false) {
        command(hidden ? 0x60 : 0xa0, hidden ? 0xe0 : -1);
        if (!(bus->read(0x4000) & 0x80)) throw std::runtime_error("Write-buffer not ready at " + std::to_string(physical) + " status " + std::to_string(bus->read(0x4000)));
        bus->write(0x2000, byte(physical / 0x2000));
        const word address = word(0x4000 + (physical & 0x1fff));
        bus->write(address, value);
        bus->write(address, 0); // Same low seven bits trigger a 128-byte block.
    }
};
static void banks() {
    Fixture f;
    check(f.bus->read(0x4000) == 2 && f.bus->read(0x6000) == 3, "Power-up ROM banks");
    check(f.bus->read(0xa000) == 0xff, "RAM disabled at power up");
    for (unsigned bank = 0; bank < 256; ++bank) {
        f.bus->write(0x27ff, byte(bank)); f.bus->write(0x37ff, byte(bank + 1));
        check(f.bus->read(0x4000) == (bank & 127), "ROM A / bank masking");
        check(f.bus->read(0x5fff) == byte((bank & 127) ^ 0xa5), "ROM A end");
        check(f.bus->read(0x6000) == ((bank + 1) & 127), "ROM B independence");
        check(f.bus->read(0x7fff) == byte(((bank + 1) & 127) ^ 0xa5), "ROM B end");
        check(f.bus->read(0x2000) == 1, "Fixed 16 KiB remains fixed");
    }
    f.bus->write(0, 0x0a);
    for (unsigned bank = 0; bank < 8; ++bank) {
        f.bus->write(0x07ff, byte(bank)); f.bus->write(0x0bff, byte(bank));
        f.bus->write(0xa000, byte(bank + 20));
        f.bus->write(0xbfff, byte(bank + 40));
        check(f.bus->read(0xb000) == bank + 20 && f.bus->read(0xafff) == bank + 40, "SRAM aliases across windows");
    }
    for (unsigned bank = 0; bank < 8; ++bank) {
        f.bus->write(0x400, byte(bank + 8)); f.bus->write(0x800, byte(7 - bank));
        check(f.bus->read(0xa000) == bank + 20 && f.bus->read(0xbfff) == 47 - bank, "SRAM masks / independent windows");
    }
    f.bus->write(0, 0); f.bus->write(0xa000, 0);
    check(f.bus->read(0xa000) == 0xff, "Disabled SRAM read");
    f.bus->write(0, 10); check(f.bus->read(0xa000) == 27, "Disabled SRAM write ignored");
    f.bus->write(0x2800, 8); check(f.bus->read(0x4000) == 0xff, "Flash CE disabled");
    f.enable(); f.bus->write(0x2000, 127);
    check(f.bus->read(0x5fff) == 0xff, "Last flash address initially erased");
    f.bus->write(0x4000, 0); check(f.bus->read(0x4000) == 0xff, "Flash is not RAM");
}
static void flash() {
    Fixture f; f.enable();
    f.command(0x90);
    check(f.bus->read(0x4000) == 0xc2 && f.bus->read(0x6001) == 0x81, "JEDEC C2/81 across both windows");
    f.bus->write(0x4000, 0xf0);
    // Incorrect unlock sequence cannot enable programming.
    f.bus->write(0x5555, 0xaa); f.bus->write(0x6aab, 0x55); f.bus->write(0x5555, 0xa0);
    f.bus->write(0x4000, 0); f.bus->write(0x4000, 0);
    check(f.bus->read(0x4000) == 0xff, "Malformed commands ignored");
    f.program(0x20000 + 17, 0xf0); // F0 is valid buffer data on the first write.
    check(!(f.bus->read(0x6000) & 0x80), "Busy is shared across flash windows");
    f.bus->write(0x4000, 0xf0);
    check(!(f.bus->read(0x4000) & 0x80), "Reset cannot abort active programming");
    f.game.get_mbc()->advance_mbc6(Mbc6::ProgramTicks - 1);
    check(!(f.bus->read(0x4000) & 0x80), "Operation has a real elapsed-time deadline");
    f.finish(1);
    check(f.bus->read(0x4011) == 0xf0 && f.bus->read(0x4010) == 0xff, "Partial buffer writes / untouched bytes");
    f.program(0x20011, 0x0f); f.finish();
    check(f.bus->read(0x4011) == 0, "Programming only clears bits");
    f.program(0x20011, 0xff); f.finish();
    check(f.bus->read(0x4011) == 0, "Programming cannot set bits");
    f.program(0x40000, 0x12); f.finish();
    f.command(0x80); // Complete six-write erase sequence, targeting sector 1 through B.
    f.bus->write(0x5555, 0xaa); f.bus->write(0x6aaa, 0x55);
    f.bus->write(0x3000, 16); f.bus->write(0x6000, 0x30); f.finish();
    f.bus->write(0x2000, 16); check(f.bus->read(0x4011) == 0xff, "Sector erase restores ones");
    f.bus->write(0x2000, 32); check(f.bus->read(0x4000) == 0x12, "Sector erase preserves next sector");
    // Buffer target comes from the repeated offset's upper address bits.
    f.command(0xa0); f.bus->write(0x4010, 0x66); f.bus->write(0x2000, 33); f.bus->write(0x4110, 0);
    f.finish(); check(f.bus->read(0x4110) == 0x66, "Trigger determines aligned block destination");
    f.command(0xa0); f.bus->write(0x4000, 0x33); f.bus->write(0x4000, 0xf0);
    check(f.bus->read(0x4000) == 0xff, "Repeated F0 cancels a pending buffer");
}
static void protection_hidden_state() {
    Fixture f; f.enable();
    f.program(0, 0x44); f.finish(); check(f.bus->read(0x4000) == 0xff, "WP protects sector zero");
    f.bus->write(0x1000, 1); f.program(0, 0x44); f.finish();
    check(f.bus->read(0x4000) == 0x44, "WP unlock allows sector zero");
    f.command(0x60, 0x20); f.finish();
    f.program(0, 0); f.finish(); check(f.bus->read(0x4000) == 0x44, "Nonvolatile protection independent of WP");
    f.command(0xa0); check(f.bus->read(0x4000) == 0x82, "Status includes protection flag");
    f.bus->write(0x4000, 0xf0); f.bus->write(0x4000, 0xf0);
    f.program(0x80, 0x25, true); f.finish(); f.command(0x77, 0x77);
    check(f.bus->read(0x4080) == 0x25 && f.bus->read(0x4000) == 0xff, "Hidden region second block");
    f.bus->write(0x4000, 0xf0);
    f.program(0x20000, 0x77); f.finish();
    f.command(0x80, 0x10); f.finish();
    f.bus->write(0x2000, 0); check(f.bus->read(0x4000) == 0x44, "Mass erase preserves protected sector zero");
    f.bus->write(0x2000, 16); check(f.bus->read(0x4000) == 0xff, "Mass erase clears unprotected sector");
    f.command(0x77, 0x77); check(f.bus->read(0x4080) == 0x25, "Mass erase preserves hidden region");
    f.bus->write(0x4000, 0xf0); f.bus->write(0x1000, 0);
    f.command(0x60, 0x04); f.command(0x77, 0x77);
    check(f.bus->read(0x4080) == 0x25, "WP prevents hidden erase");
    f.bus->write(0x4000, 0xf0); f.bus->write(0x1000, 1);
    f.command(0x60, 0x04); f.finish(); f.command(0x77, 0x77);
    check(f.bus->read(0x4080) == 0xff, "Hidden erase works when enabled");
    f.bus->write(0x4000, 0xf0); f.command(0x60, 0x40); f.finish();
    check(f.game.get_rom()->get_sram()[Mbc6::ProtectionOffset] == 0, "Unprotect persists");
    f.program(0x40000, 0x89);
    std::vector<byte> state(f.game.get_state_size()); f.game.save_state_mem(state.data());
    f.finish(); f.game.restore_state_mem(state.data());
    check(!(f.bus->read(0x4000) & 0x80), "Savestate restores busy operation");
    f.finish(); check(f.bus->read(0x4000) == 0x89, "Restored pending buffer commits correctly");
    f.program(0x40001, 0x55); f.game.reset(); f.enable(); f.bus->write(0x2000, 32);
    check(!(f.bus->read(0x4000) & 0x80), "Soft reset preserves in-progress flash");
    f.finish(); check(f.bus->read(0x4001) == 0x55, "Flash completes across soft reset");
    auto *battery = f.game.get_rom()->get_sram();
    check(f.game.get_rom()->get_sram_size() == Mbc6::RamSize && f.game.get_rom()->get_battery_size() == Mbc6::BatterySize, "Separate RAM and battery geometry");
    TestRenderer renderer; gb restarted(&renderer, false, false);
    check(restarted.load_rom(f.rom.data(), int(f.rom.size()), battery, Mbc6::BatterySize, false), "Battery restart load");
    auto *bus = restarted.get_cpu(); bus->write(0x0c00, 1); bus->write(0x2800, 8); bus->write(0x2000, 32);
    check(bus->read(0x4000) == 0x89 && bus->read(0x4001) == 0x55, "Flash survives battery restart");
}
static void dma_and_fetch() {
    Fixture f;
    f.bus->write(0x2000, 7); f.bus->write(0x3000, 9);
    f.bus->write(0xff46, 0x60);
    check(f.bus->get_oam()[0] == 9, "OAM DMA uses bank B");
    f.bus->write(0xff51, 0x60); f.bus->write(0xff52, 0);
    f.bus->write(0xff53, 0); f.bus->write(0xff54, 0); f.bus->write(0xff55, 0);
    check(f.bus->get_vram()[0] == 9, "GDMA uses bank B");
    f.bus->write(0, 10); f.bus->write(0x800, 7); f.bus->write(0xb000, 0x73);
    f.bus->write(0xff46, 0xb0); check(f.bus->get_oam()[0] == 0x73, "OAM DMA uses SRAM B");
    for (bool stepping : {false, true}) {
        Fixture hdma;
        hdma.bus->write(0x3000, 11);
        hdma.bus->write(0xff51, 0x60); hdma.bus->write(0xff52, 0);
        hdma.bus->write(0xff53, 0); hdma.bus->write(0xff54, 0x10); hdma.bus->write(0xff55, 0x80);
        if (stepping) for (unsigned i = 0; i < 300; ++i) hdma.game.run_step();
        else hdma.game.run();
        check(hdma.bus->get_vram()[0x10] == 11, "HDMA bank B in both execution paths");
    }
    // A real SM83 fetch/call from B writes its result to WRAM then loops.
    auto data = image();
    data[9 * 0x2000] = 0x3e; data[9 * 0x2000 + 1] = 0x5a;
    data[9 * 0x2000 + 2] = 0xea; data[9 * 0x2000 + 3] = 0x40; data[9 * 0x2000 + 4] = 0xc0;
    data[9 * 0x2000 + 5] = 0x18; data[9 * 0x2000 + 6] = 0xfe;
    check(f.game.load_rom(data.data(), int(data.size()), nullptr, 0, false), "Reload instruction fixture");
    f.bus->write(0x3000, 9); f.bus->get_regs()->PC = 0x6000; f.bus->exec(100);
    check(f.bus->read(0xc040) == 0x5a, "Opcode / immediate / data bus all honor MBC6");
}
static void malformed() {
    Fixture f;
    for (auto header : {0x148, 0x149}) {
        auto data = f.rom; data[header] = 0;
        check(!f.game.load_rom(data.data(), int(data.size()), nullptr, 0, false), "Unsupported MBC6 geometry rejected");
        check(f.bus->read(0x4000) == 2, "Rejected load preserves running cartridge");
    }
    check(!f.game.load_rom(f.rom.data(), int(f.rom.size()) - 1, nullptr, 0, false), "Truncated MBC6 rejected");
}
int main() {
    try { banks(); flash(); protection_hidden_state(); dma_and_fetch(); malformed(); }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
    std::cout << "Document-derived MBC6 bank, flash, protection, persistence and bus tests passed\n";
}
