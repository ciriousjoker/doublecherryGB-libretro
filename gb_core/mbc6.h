#pragma once
#include <cstdint>
#include "serializer.h"

// Net de Get board geometry. Persistent bytes are owned by rom, not the mapper.
class Mbc6 {
public:
    static constexpr unsigned RomSize = 0x100000;
    static constexpr unsigned RamSize = 0x8000;
    static constexpr unsigned FlashSize = 0x100000;
    static constexpr unsigned HiddenOffset = RamSize + FlashSize;
    static constexpr unsigned ProtectionOffset = HiddenOffset + 256;
    static constexpr unsigned BatterySize = ProtectionOffset + 1;
    static constexpr unsigned ProgramTicks = 4195;  // Deterministic model: 1 ms.
    static constexpr unsigned EraseTicks = 25166;   // Deterministic model: 6 ms.

    void reset();
    uint8_t read(uint16_t address, const uint8_t *rom, const uint8_t *battery) const;
    void write(uint16_t address, uint8_t value, uint8_t *battery);
    void advance(unsigned base_clock_ticks, uint8_t *battery);
    void serialize(serializer &s);

private:
    enum Mode : uint8_t { Array, Id, Hidden, Program, ProgramHidden, Status };
    enum Operation : uint8_t { None, ProgramBlock, HiddenBlock, SectorErase, ChipErase, HiddenErase, Protect, Unprotect };
    uint8_t rom_bank[2] = {2, 3}, ram_bank[2] = {0, 1};
    uint8_t flash_select[2] = {0, 0};
    uint8_t ram_enabled = 0, flash_enabled = 0, write_enabled = 0;
    Mode mode = Array;
    Operation operation = None;
    uint8_t sequence = 0, prefix = 0;
    uint8_t buffer[128] = {};
    int16_t last_offset = -1;
    uint32_t pending_address = 0, ticks_left = 0;
    uint8_t pending_allowed = 0, chip_sector_zero = 0;

    unsigned flash_address(uint16_t address) const;
    void flash_write(unsigned address, uint8_t value, uint8_t *battery);
    void start(Operation op, unsigned address, bool allowed, unsigned ticks);
};
