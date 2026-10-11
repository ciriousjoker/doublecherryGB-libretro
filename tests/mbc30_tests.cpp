#include "gb_core/gb.h"
#include "libretro/DoubleCherryEngine/libretro.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

extern std::vector<gb*> v_gb;

static void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

// Each bank has its own marker at both ends. All ROMs are generated here.
static std::vector<byte> image(byte rom_size, byte ram_size, byte type = 0x10) {
    std::vector<byte> data(size_t(0x8000) << rom_size, 0);
    for (size_t bank = 0; bank < data.size() / 0x4000; ++bank) {
        data[bank * 0x4000] = byte(bank);
        data[bank * 0x4000 + 0x3fff] = byte(bank ^ 0xa5);
    }
    data[0x143] = 0x80;
    data[0x147] = type;
    data[0x148] = rom_size;
    data[0x149] = ram_size;
    return data;
}

class ClockRenderer : public renderer {
public:
    byte clock[5] = {12, 34, 5, 0xab, 1};
    void reset() override {}
    void refresh() override {}
    void render_screen(byte*, int, int, int) override {}
    int check_pad() override { return 0; }
    word map_color(word value) override { return value; }
    word unmap_color(word value) override { return value; }
    byte get_time(int reg) override { return reg >= 8 && reg <= 12 ? clock[reg - 8] : 0; }
    void set_time(int reg, byte value) override { if (reg >= 8 && reg <= 12) clock[reg - 8] = value; }
    word get_sensor(bool) override { return 0; }
    void set_bibrate(bool) override {}
};

static void banking(byte rom_size, byte ram_size, byte type) {
    auto data = image(rom_size, ram_size, type);
    ClockRenderer renderer;
    gb game(&renderer, false, false);
    require(game.load_rom(data.data(), int(data.size()), nullptr, 0, false), "Load mapper fixture");
    auto* bus = game.get_cpu();
    const bool extended = rom_size >= 7 || ram_size == 5;
    for (int value = 0; value < 256; ++value) {
        int expected = value & (extended ? 255 : 127);
        if (!expected) expected = 1;
        expected &= (data.size() / 0x4000) - 1;
        for (word address : {word(0x2000), word(0x3fff)}) {
            bus->write(address, byte(value));
            require(bus->read(0x4000) == byte(expected), "ROM start bank " + std::to_string(value));
            require(bus->read(0x7fff) == byte(expected ^ 0xa5), "ROM end bank " + std::to_string(value));
            require(bus->read(0) == 0, "Fixed ROM bank changed");
        }
    }
    bus->write(0, 0x0a);
    const int banks = ram_size == 5 ? 8 : 4;
    require(game.get_rom()->get_sram_size() == banks * 0x2000, "Battery allocation");
    for (int bank = 0; bank < banks; ++bank) {
        bus->write(0x4000, byte(bank));
        bus->write(0xa000, byte(0x30 + bank));
        bus->write(0xbfff, byte(0xc0 + bank));
    }
    for (int bank = banks - 1; bank >= 0; --bank) {
        bus->write(0x5fff, byte(bank));
        require(bus->read(0xa000) == 0x30 + bank && bus->read(0xbfff) == 0xc0 + bank, "RAM banks alias");
    }
    // Physical RAM address lines wrap when a smaller RAM chip is installed.
    bus->write(0x4000, 7);
    require(bus->read(0xa000) == 0x30 + (7 % banks), "RAM mirroring");

    if (type == 0x10 || type == 0x0f) {
        bus->write(0x6000, 0);
        bus->write(0x7fff, 1);
        for (int reg = 8; reg <= 12; ++reg) {
            const byte latched = renderer.clock[reg - 8];
            bus->write(0x4000, byte(reg));
            bus->write(0xa000, byte(latched + 1));
            require(renderer.clock[reg - 8] == latched + 1, "RTC write routing");
            require(bus->read(0xbfff) == latched, "RTC latch changed after clock write");
        }
        bus->write(0x6000, 0);
        bus->write(0x6000, 1);
        for (int reg = 8; reg <= 12; ++reg) {
            bus->write(0x4000, byte(reg));
            require(bus->read(0xa000) == renderer.clock[reg - 8], "RTC re-latch");
        }
    }

    bus->write(0x4000, byte(banks - 1));
    bus->write(0x2000, 0xff);
    std::vector<byte> state(game.get_state_size());
    game.save_state_mem(state.data());
    bus->write(0x2000, 1);
    bus->write(0xa000, 0);
    bus->write(0x4000, 0);
    game.restore_state_mem(state.data());
    const int last = (extended ? 255 : 127) & (int(data.size() / 0x4000) - 1);
    require(bus->read(0x4000) == last, "ROM bank restore");
    require(bus->read(0xa000) == 0x30 + banks - 1, "SRAM bank/data restore");
    if (type == 0x10) {
        bus->write(0x4000, 8);
        std::vector<byte> rtc_state(game.get_state_size());
        game.save_state_mem(rtc_state.data());
        const byte latched = bus->read(0xa000);
        bus->write(0x6000, 0);
        bus->write(0xa000, 59);
        bus->write(0x6000, 1);
        bus->write(0x4000, 7);
        game.restore_state_mem(rtc_state.data());
        require(bus->read(0xa000) == latched, "RTC selector/latch restore");
    }
}

static void loader_tests() {
    for (bool persistent : {false, true}) {
        auto data = image(7, 5);
        ClockRenderer renderer;
        gb game(&renderer, false, false);
        require(game.load_rom(data.data(), int(data.size()), nullptr, 0, persistent), "Load complete ROM");
        auto* bus = game.get_cpu();
        bus->write(0x2000, 128);
        const auto reject = [&](byte* buffer, int size, const std::string& reason) {
            require(!game.load_rom(buffer, size, nullptr, 0, persistent), reason);
            require(bus->read(0x4000) == 128 && game.get_rom()->get_sram_size() == 65536,
                    "Rejected load changed current cartridge");
        };
        reject(nullptr, 0, "Reject absent ROM");
        reject(data.data(), 0x149, "Reject incomplete header");
        reject(data.data(), int(data.size()) - 1, "Reject incomplete last ROM bank");
        auto truncated = image(6, 3);
        truncated[0x148] = 7;
        reject(truncated.data(), int(truncated.size()), "Reject 2 MiB buffer advertising 4 MiB");
        auto invalid = image(0, 3);
        invalid[0x148] = 9;
        reject(invalid.data(), int(invalid.size()), "Reject unsupported ROM size code");
        invalid[0x148] = 0;
        invalid[0x149] = 6;
        reject(invalid.data(), int(invalid.size()), "Reject unsupported RAM size code");
    }
}

static void mapper_tests() {
    loader_tests();
    for (byte type : {byte(0x0f), byte(0x10), byte(0x11), byte(0x12), byte(0x13)}) {
        banking(7, 5, type);
        banking(7, 3, type); // Polished Crystal can retain its 32 KiB save format.
        banking(6, 5, type); // Japanese Crystal geometry detects MBC30 via RAM.
        banking(6, 3, type); // Ordinary MBC3 must still ignore bit 7.
    }
    banking(5, 3, 0x10); // Smaller MBC3 ROM mirrors absent address lines.
}

static std::map<std::string, std::string> options;
static std::string players;
static std::string directory;
static unsigned video_frames = 0;
static unsigned video_width = 0, video_height = 0;
static std::vector<word> video_pixels;
static void quiet_log(enum retro_log_level, const char*, ...) {}
static bool environment(unsigned command, void* data) {
    switch (command) {
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
        static_cast<retro_log_callback*>(data)->log = quiet_log;
        return true;
    case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
        *static_cast<unsigned*>(data) = 2;
        return true;
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2:
        for (auto* entry = static_cast<retro_core_options_v2*>(data)->definitions; entry->key; ++entry)
            if (entry->default_value) options[entry->key] = entry->default_value;
        return true;
    case RETRO_ENVIRONMENT_GET_VARIABLE: {
        auto* var = static_cast<retro_variable*>(data);
        options["dcgb_emulated_gameboys"] = players;
        options["dcgb_mobile_adatper_enabled"] = "0";
        options["dcgb_rtc_use_system_clock"] = "0";
        const auto entry = options.find(var->key);
        var->value = entry == options.end() ? nullptr : entry->second.c_str();
        return var->value != nullptr;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
        *static_cast<bool*>(data) = false;
        return true;
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        *static_cast<const char**>(data) = directory.c_str();
        return true;
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
        return *static_cast<retro_pixel_format*>(data) == RETRO_PIXEL_FORMAT_RGB565;
    default:
        return false;
    }
}

static void emit(std::vector<byte>& data, size_t offset, std::initializer_list<byte> code) {
    std::copy(code.begin(), code.end(), data.begin() + offset);
}

static void libretro_tests(int count) {
    players = std::to_string(count);
    const auto scratch = std::filesystem::temp_directory_path() /
        ("dcgb-mbc30-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(scratch);
    directory = scratch.string();
    retro_set_environment(environment);
    retro_set_video_refresh([](const void* data, unsigned width, unsigned height, size_t pitch) {
        ++video_frames;
        video_width = width;
        video_height = height;
        video_pixels.resize(width * height);
        if (data)
            for (unsigned row = 0; row < height; ++row)
                std::memcpy(video_pixels.data() + row * width,
                    static_cast<const byte*>(data) + row * pitch, width * sizeof(word));
    });
    retro_set_audio_sample([](int16_t, int16_t) {});
    retro_set_audio_sample_batch([](const int16_t*, size_t frames) { return frames; });
    retro_set_input_poll([]() {});
    retro_set_input_state([](unsigned, unsigned, unsigned, unsigned) -> int16_t { return 0; });
    retro_init();
    auto data = image(7, 5);
    emit(data, 0x100, {0xc3, 0x50, 0x01}); // JP entry
    // ROM0 calls bank 128, whose subroutine calls a ROM0 trampoline into bank 255.
    emit(data, 0x150, {0xf3, 0x31, 0xfe, 0xff, 0x3e, 0x0a, 0xea, 0x00, 0x00,
        0x3e, 0x07, 0xea, 0x00, 0x40, 0x3e, 0x80, 0xea, 0x00, 0x20,
        0xcd, 0x00, 0x42, 0xea, 0x00, 0xa0, 0x18, 0xfe});
    emit(data, 128 * 0x4000 + 0x200, {0xcd, 0x00, 0x02, 0xfa, 0x00, 0x40, 0xc9});
    emit(data, 0x200, {0x3e, 0xff, 0xea, 0x00, 0x20, 0xcd, 0x00, 0x42,
        0xea, 0x01, 0xa0, 0x3e, 0x80, 0xea, 0x00, 0x20, 0xc9});
    emit(data, 255 * 0x4000 + 0x200, {0xfa, 0x00, 0x40, 0xc9});
    const retro_game_info info = {nullptr, data.data(), data.size(), nullptr};
    require(retro_load_game(&info), "libretro load");
    for (int i = 0; i < count; ++i)
        std::fill_n(v_gb.at(i)->get_rom()->get_sram(), 65536, byte(0x60 + i));
    for (int frame = 0; frame < 8; ++frame) retro_run();
    require(video_frames > 0, "libretro video callbacks");
    require(retro_get_memory_size(RETRO_MEMORY_SAVE_RAM) == 65536, "libretro battery size");
    for (int i = 0; i < count; ++i) {
        auto* bus = v_gb.at(i)->get_cpu();
        require(bus->read(0xa000) == 128 && bus->read(0xa001) == 255, "Nested upper-bank CPU execution");
        bus->write(0xa002, byte(i + 1));
    }
    if (count == 2) {
        v_gb[1]->get_cpu()->write(0xff01, 0x35);
        v_gb[1]->get_cpu()->write(0xff02, 0x80); // External serial clock.
        v_gb[0]->get_cpu()->write(0xff01, 0xc5);
        v_gb[0]->get_cpu()->write(0xff02, 0x81); // Internal serial clock.
        for (int frame = 0; frame < 2; ++frame) retro_run();
        require(v_gb[0]->get_cpu()->read(0xff01) == 0x35 &&
                v_gb[1]->get_cpu()->read(0xff01) == 0xc5, "Linked MBC30 serial exchange");
    }
    const size_t size = retro_serialize_size();
    std::vector<std::vector<byte>> batteries;
    for (int i = 0; i < count; ++i)
        batteries.emplace_back(v_gb[i]->get_rom()->get_sram(), v_gb[i]->get_rom()->get_sram() + 65536);
    std::vector<byte> state(size);
    require(retro_serialize(state.data(), state.size()), "libretro save state");
    for (int i = 0; i < count; ++i) {
        auto* bus = v_gb.at(i)->get_cpu();
        std::fill_n(v_gb[i]->get_rom()->get_sram(), 65536, byte(0));
        bus->write(0x2000, 1);
        bus->write(0x4000, 0);
    }
    require(retro_unserialize(state.data(), state.size()), "libretro restore state");
    for (int i = 0; i < count; ++i) {
        auto* bus = v_gb.at(i)->get_cpu();
        require(std::equal(batteries[i].begin(), batteries[i].end(), v_gb[i]->get_rom()->get_sram()),
                "Complete per-player battery restore");
        require(bus->read(0x4000) == 128, "libretro upper ROM bank restore");
        require(bus->read(0xa000) == 128 && bus->read(0xa002) == i + 1, "Independent player state restore");
    }
    // Export and reload the frontend's entire battery buffer, including bank 7.
    auto* sram = static_cast<byte*>(retro_get_memory_data(RETRO_MEMORY_SAVE_RAM));
    const auto save_path = scratch / "battery.srm";
    std::ofstream(save_path, std::ios::binary).write(reinterpret_cast<char*>(sram), 65536);
    std::fill(sram, sram + 65536, 0);
    std::ifstream(save_path, std::ios::binary).read(reinterpret_cast<char*>(sram), 65536);
    require(sram[7 * 0x2000] == 128 && sram[7 * 0x2000 + 1] == 255, "Battery export/import upper bank");
    if (count == 3 || count == 4) {
        // Every player must occupy its own quadrant, including the lower row.
        for (int i = 0; i < count; ++i) {
            std::vector<word> frame(160 * 144, word(0x1000 * (i + 1)));
            v_gb[i]->get_renderer()->render_screen(reinterpret_cast<byte*>(frame.data()), 160, 144, 16);
        }
        require(video_width == 320 && video_height == 288, "Split-screen geometry");
        for (int i = 0; i < count; ++i) {
            const size_t pixel = (i / 2 * 144 + 72) * 320 + i % 2 * 160 + 80;
            require(video_pixels[pixel] == 0x1000 * (i + 1), "Split-screen player quadrant");
        }
    }
    retro_reset();
    require(v_gb[0]->get_cpu()->read(0x4000) == data[0x4000], "Reset ROM bank");
    require(sram[7 * 0x2000] == 128, "Reset erased battery");
    retro_unload_game();
    require(retro_load_game(&info), "Fresh libretro session");
    sram = static_cast<byte*>(retro_get_memory_data(RETRO_MEMORY_SAVE_RAM));
    std::ifstream(save_path, std::ios::binary).read(reinterpret_cast<char*>(sram), 65536);
    v_gb[0]->get_cpu()->write(0x4000, 7);
    require(v_gb[0]->get_cpu()->read(0xa000) == 128, "Battery survives fresh game load");
    retro_unload_game();
    retro_deinit();
    std::filesystem::remove_all(scratch);
}

int main(int argc, char** argv) {
    try {
        require(argc >= 2, "Specify mapper or libretro");
        if (std::string(argv[1]) == "mapper") mapper_tests();
        else {
            require(argc == 3 && std::string(argv[1]) == "libretro", "Invalid test arguments");
            libretro_tests(std::stoi(argv[2]));
        }
        std::cout << "MBC30 " << argv[1] << " tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
