#include "mbc6.h"
#include <algorithm>
#include <cstring>

void Mbc6::reset() {
    rom_bank[0] = 2; rom_bank[1] = 3;
    ram_bank[0] = 0; ram_bank[1] = 1;
    flash_select[0] = flash_select[1] = 0;
    ram_enabled = flash_enabled = write_enabled = false;
    mode = ticks_left ? Status : Array;
    if (!ticks_left) operation = None;
    sequence = prefix = 0;
    last_offset = -1;
    if (!ticks_left) {
        pending_address = 0;
        pending_allowed = chip_sector_zero = false;
        std::memset(buffer, 0xff, sizeof(buffer));
    }
}

unsigned Mbc6::flash_address(uint16_t address) const {
    return unsigned(rom_bank[address >= 0x6000]) * 0x2000 + (address & 0x1fff);
}

uint8_t Mbc6::read(uint16_t address, const uint8_t *rom, const uint8_t *battery) const {
    if (address < 0x4000) return rom[address];
    if (address >= 0xa000 && address < 0xc000) {
        if (!ram_enabled) return 0xff;
        return battery[unsigned(ram_bank[address >= 0xb000]) * 0x1000 + (address & 0xfff)];
    }
    if (address >= 0x8000) return 0xff;
    const unsigned physical = flash_address(address);
    if (!flash_select[address >= 0x6000]) return rom[physical];
    if (!flash_enabled) return 0xff;
    if (mode == Program || mode == ProgramHidden || mode == Status)
        return uint8_t((ticks_left ? 0 : 0x80) | ((battery[ProtectionOffset] & 1) ? 2 : 0));
    if (mode == Id) {
        switch (physical & 0xff) {
        case 0: return 0xc2;
        case 1: return 0x81;
        default: return 0xff;
        }
    }
    if (mode == Hidden) return battery[HiddenOffset + (physical & 255)];
    return battery[RamSize + physical];
}

void Mbc6::write(uint16_t address, uint8_t value, uint8_t *battery) {
    if (address < 0x4000) {
        switch (address >> 10) {
        case 0: ram_enabled = (value & 15) == 10; break;
        case 1: ram_bank[0] = value & 7; break;
        case 2: ram_bank[1] = value & 7; break;
        case 3: flash_enabled = value & 1; break;
        case 4: if (address == 0x1000) write_enabled = value & 1; break;
        case 8: case 9: rom_bank[0] = value & 127; break;
        case 10: case 11: flash_select[0] = value & 8; break;
        case 12: case 13: rom_bank[1] = value & 127; break;
        case 14: case 15: flash_select[1] = value & 8; break;
        default: break;
        }
    } else if (address < 0x8000) {
        if (flash_select[address >= 0x6000] && flash_enabled)
            flash_write(flash_address(address), value, battery);
    } else if (address >= 0xa000 && address < 0xc000 && ram_enabled) {
        battery[unsigned(ram_bank[address >= 0xb000]) * 0x1000 + (address & 0xfff)] = value;
    }
}

void Mbc6::start(Operation op, unsigned address, bool allowed, unsigned ticks) {
    operation = op;
    pending_address = address;
    pending_allowed = allowed;
    ticks_left = ticks;
    mode = Status;
    sequence = prefix = 0;
    last_offset = -1;
}

void Mbc6::flash_write(unsigned address, uint8_t value, uint8_t *battery) {
    if (ticks_left) return; // F0 also has no effect during a hardware operation.
    if (mode == Program || mode == ProgramHidden) {
        const unsigned offset = address & 127;
        if (last_offset == int(offset)) {
            if (value == 0xf0) { mode = Array; sequence = prefix = 0; last_offset = -1; return; }
            const bool hidden = mode == ProgramHidden;
            const bool allowed = hidden ? write_enabled : (address >= 0x20000 || (write_enabled && !(battery[ProtectionOffset] & 1)));
            start(hidden ? HiddenBlock : ProgramBlock, hidden ? (address & 128) : (address & ~127u), allowed, ProgramTicks);
        } else {
            buffer[offset] = value;
            last_offset = int16_t(offset);
        }
        return;
    }
    if (value == 0xf0) { mode = Array; sequence = prefix = 0; return; }
    const unsigned command_address = address & 0x7fff;
    if (sequence == 0) {
        if (command_address == 0x5555 && value == 0xaa) sequence = 1;
        return;
    }
    if (sequence == 1 || sequence == 4) {
        if (command_address == 0x2aaa && value == 0x55) ++sequence;
        else sequence = prefix = 0;
        return;
    }
    if (sequence == 3) {
        if (command_address == 0x5555 && value == 0xaa) sequence = 4;
        else sequence = prefix = 0;
        return;
    }
    if (sequence == 2) {
        sequence = 0;
        if (command_address != 0x5555) return;
        switch (value) {
        case 0x90: mode = Id; break;
        case 0xa0:
            mode = Program; last_offset = -1;
            std::memset(buffer, 0xff, sizeof(buffer));
            break;
        case 0x80: case 0x60: case 0x77: prefix = value; sequence = 3; break;
        default: break;
        }
        return;
    }
    // The final sector-erase address selects the target sector; other commands
    // use the unlock address. High chip address bits are don't-care for unlock.
    sequence = 0;
    const uint8_t saved_prefix = prefix;
    prefix = 0;
    if (saved_prefix == 0x80 && value == 0x30) {
        const unsigned sector = address & ~0x1ffffu;
        start(SectorErase, sector, sector != 0 || (write_enabled && !(battery[ProtectionOffset] & 1)), EraseTicks);
        return;
    }
    if (command_address != 0x5555) return;
    if (saved_prefix == 0x80 && value == 0x10) {
        chip_sector_zero = write_enabled && !(battery[ProtectionOffset] & 1);
        start(ChipErase, 0, true, EraseTicks);
    } else if (saved_prefix == 0x77 && value == 0x77) {
        mode = Hidden;
    } else if (saved_prefix == 0x60 && write_enabled) {
        switch (value) {
        case 0x04: start(HiddenErase, 0, true, EraseTicks); break;
        case 0x20: start(Protect, 0, true, ProgramTicks); break;
        case 0x40: start(Unprotect, 0, true, ProgramTicks); break;
        case 0xe0:
            mode = ProgramHidden; last_offset = -1;
            std::memset(buffer, 0xff, sizeof(buffer));
            break;
        default: break;
        }
    }
}

void Mbc6::advance(unsigned ticks, uint8_t *battery) {
    if (!ticks_left) return;
    if (ticks < ticks_left) { ticks_left -= ticks; return; }
    ticks_left = 0;
    if (pending_allowed) {
        auto *flash = battery + RamSize;
        switch (operation) {
        case ProgramBlock:
            for (unsigned i = 0; i < 128; ++i) flash[pending_address + i] &= buffer[i];
            break;
        case HiddenBlock:
            for (unsigned i = 0; i < 128; ++i) battery[HiddenOffset + pending_address + i] &= buffer[i];
            break;
        case SectorErase: std::memset(flash + pending_address, 0xff, 0x20000); break;
        case ChipErase:
            std::memset(flash + (chip_sector_zero ? 0 : 0x20000), 0xff, FlashSize - (chip_sector_zero ? 0 : 0x20000));
            break;
        case HiddenErase: std::memset(battery + HiddenOffset, 0xff, 256); break;
        case Protect: battery[ProtectionOffset] = 1; break;
        case Unprotect: battery[ProtectionOffset] = 0; break;
        default: break;
        }
    }
    operation = None;
}

void Mbc6::serialize(serializer &s) {
    s_ARRAY(rom_bank); s_ARRAY(ram_bank); s_ARRAY(flash_select);
    s_VAR(ram_enabled); s_VAR(flash_enabled); s_VAR(write_enabled);
    s_VAR(mode); s_VAR(operation); s_VAR(sequence); s_VAR(prefix);
    s_ARRAY(buffer); s_VAR(last_offset); s_VAR(pending_address); s_VAR(ticks_left);
    s_VAR(pending_allowed); s_VAR(chip_sector_zero);
    ram_enabled &= 1; flash_enabled &= 1; write_enabled &= 1;
    pending_allowed &= 1; chip_sector_zero &= 1;
    // Keep corrupt savestates from turning pending flash work into host OOB writes.
    for (unsigned i = 0; i < 2; ++i) {
        rom_bank[i] &= 127; ram_bank[i] &= 7; flash_select[i] &= 8;
    }
    if (mode > Status || operation > Unprotect || sequence > 5 ||
        ticks_left > EraseTicks || last_offset < -1 || last_offset > 127 ||
        (operation == ProgramBlock && (pending_address >= FlashSize || (pending_address & 127))) ||
        (operation == HiddenBlock && pending_address != 0 && pending_address != 128) ||
        (operation == SectorErase && (pending_address >= FlashSize || (pending_address & 0x1ffff)))) {
        ticks_left = 0;
        reset();
    }
}
