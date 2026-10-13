# MBC6 cartridge support

Header type `0x20` selects the physically existing Net de Get geometry: 1 MiB read-only ROM, 1 MiB Macronix flash, 32 KiB SRAM, a 256-byte hidden flash region, and one nonvolatile sector-zero protection bit. Other MBC6 geometries are rejected explicitly. A ROM file contains exactly the 1 MiB ROM; flash contents live in the battery file.

The fixed 16 KiB ROM remains visible while the two independent 8 KiB windows select ROM or flash. Independent 4 KiB SRAM windows share the same eight banks. Disabled RAM / flash reads return FF. Fetch, immediate, ordinary bus, OAM DMA, GDMA and both HBlank DMA execution paths honor the mapper. Other mappers keep their prior bus paths.

## Flash

The unlock sequence uses the physical chip address, with command-address high bits ignored. The state machine handles JEDEC C2/81, ordinary / hidden reads, ordinary / hidden programming, sector / chip / hidden erasure, and persistent sector-zero protect / unprotect. The /WP register separately protects sector zero and the hidden region. Chip erase preserves the hidden region and protected sector zero.

Programming uses a 128-byte FF-initialized buffer. A0-A6 select buffer positions. Repeating the last buffer position triggers programming to the aligned block selected by the trigger's upper address bits. Partial buffer fill is legal. Data only clears bits. F0 is valid initial buffer data; repeated F0 exits the uncommitted buffer mode. Busy operations ignore reset / writes, report bit 7 clear, and leave ROM and SRAM accessible. Completion remains in status until a command changes mode. Bit 1 reflects persistent sector-zero protection. Undefined status bits are modeled as zero; speculative timeout failures are not injected.

Busy time advances with elapsed emulated base-clock time, including CPU HALT and DMA. Program / protection operations use 4195 ticks (approximately 1 ms), erase operations 25166 ticks (approximately 6 ms). These are deterministic engineering timings, not measured per-command timings of a particular chip. Primary research reports operations up to about 6 ms, but does not give a full timing table. Completion is quantized to CPU exec batches. No host wall-clock sleep or immediate-ready shortcut is used. Drivers must poll status, not depend on the constants. A soft emulator reset resets mapper registers but preserves an in-progress flash operation; removing / reloading content discards volatile pending work. Power-loss corruption beyond a discarded pending operation is not analog-modeled.

## Battery format version 1

Libretro SAVE_RAM exports one contiguous buffer. Addresses below are byte offsets, unrelated to guest CPU addresses:

| Offset | Size | Contents |
| --- | --- | --- |
| 0x000000 | 32768 | SRAM |
| 0x008000 | 1048576 | Main flash |
| 0x108000 | 256 | Hidden flash |
| 0x108100 | 1 | Persistent protection bit, bit 0 |

Total size: 1081601 bytes (0x108101). New flash / hidden data is FF, protection bit zero. The SRAM geometry API remains 32 KiB; the separate battery API includes flash. Frontends must persist the entire exported SAVE_RAM region. A savestate includes bytes, mapper banks / enables, command stage, buffer, pending operation and remaining ticks. Short Libretro state buffers are rejected. Cartridge geometry comes from the loaded ROM, never restored cartridge-header bytes. Malformed mapper state is normalized / reset before pending work can access host memory.

## Evidence

`mbc6_mapper` exercises all 128 ROM banks, SRAM aliasing, flash enable, malformed commands, partial / relocated buffers, programming direction, each erase family, both protection layers, hidden data, pending-operation restore, soft reset, true battery reload, instruction fetch and DMA paths. `mbc6_flash_safety` runs 100000 deterministic malformed bus / state operations with battery canaries; enable `DCGB_SANITIZE_MBC6` for AddressSanitizer and UndefinedBehaviorSanitizer. Existing MBC30 tests and the independent ROM gate remain required.

The gbcOS repository additionally contains a GBDK MBC6 ROM probe. Actual guest SM83 instructions program a block, observe busy polling, export a battery file, then load it into a new core process and read the same block. This is document-derived emulator evidence, not physical verification.

Primary sources: [Pan Docs MBC6](https://gbdev.io/pandocs/MBC6.html), [endrift hardware research](https://gbdev.gg8.se/forums/viewtopic.php?id=544), and [direct Macronix / Net de Get chip research](https://iceboy.a-singer.de/doc/np_gb_memory.html). mGBA revision `143adf5aa2a60c6036810b0cd72445e13d74b0e6` independently corroborates bank / enable / flash-selection / battery layout concepts, but explicitly does not implement flash controls or programming. No hardware-verified public MBC6 test ROM or physical test cartridge was available. Unknown electrical / power-loss details remain labeled above.
