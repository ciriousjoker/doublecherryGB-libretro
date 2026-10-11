MBC30 support extends the existing MBC3 mapper to 256 ROM banks (4 MiB) and eight
SRAM banks (64 KiB). There is no separate MBC30 cartridge type in the header.
MBC3-family cartridges with ROM size code 7 or larger, or RAM size code 5, use the
eight-bit bank register. A 4 MiB ROM can keep RAM size code 3 and its ordinary
32 KiB battery save. Ordinary MBC3 cartridges retain the seven-bit register.
See [Pan Docs](https://github.com/gbdev/pandocs/blob/master/src/MBC3.md).

Build and run the native tests on macOS or Linux:

```sh
cmake -S . -B build/native -DCMAKE_BUILD_TYPE=Release \
  -DLIBMOBILE_BUILD_SHARED=OFF -DDCGB_BUILD_TESTS=ON
cmake --build build/native --parallel 4
ctest --test-dir build/native --output-on-failure
```

The macOS output is `build/native/DoubleCherryGB_libretro.dylib`. Load it as a
libretro core in RetroArch. It is a library, not a standalone emulator app.
For a universal Apple Silicon/Intel core, also pass
`-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0`. Cross-building an Intel slice does not
test execution on Intel; CI executes native tests on both architectures.

The generated fixtures contain no commercial ROM data. Coverage includes:

- Every ROM bank register value, both register boundaries, both ends of the
  switchable window, zero-to-one remapping, the fixed bank, and smaller ROM mirrors.
- Rejection of missing/truncated ROM buffers and unsupported size codes before
  replacing a loaded cartridge, with both copied and persistent frontend data.
- MBC30 with 4 MiB/64 KiB, 4 MiB/32 KiB and 2 MiB/64 KiB, plus ordinary MBC3
  controls, through all five MBC3-family cartridge type dispatches.
- Independent SRAM banks, physical RAM mirroring and complete battery buffers.
- RTC register routing and latching against a deterministic renderer, including
  latched RTC selector restoration. This tests the existing RTC interface, not
  complete hardware RTC timing, halt/carry behavior or host clock persistence.
- Real CPU instructions making nested calls between banks 128 and 255 through
  libretro, with 1, 2, 3, 4 and 16 active Game Boys, plus a two-player serial exchange.
- Per-player upper ROM/SRAM bank and data restoration through libretro save states,
  battery file export/import, fresh content loading and reset persistence.
- Three/four-player split-screen geometry and distinct player quadrants. The lower
  row previously used absolute player indexes as columns, overflowing its buffer.

AddressSanitizer checks the same tests for invalid memory accesses:

```sh
cmake -S . -B build/asan -DCMAKE_BUILD_TYPE=Debug \
  -DLIBMOBILE_BUILD_SHARED=OFF -DDCGB_BUILD_TESTS=ON \
  '-DCMAKE_C_FLAGS=-fsanitize=address -fno-omit-frame-pointer' \
  '-DCMAKE_CXX_FLAGS=-fsanitize=address -fno-omit-frame-pointer'
cmake --build build/asan --parallel 4
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build/asan --output-on-failure
```

Leak detection is disabled because upstream peripheral ownership leaks are
outside these tests. A passing run establishes the tested memory accesses, not
the absence of every emulator defect. Save-state layout is unchanged; MBC30
states cannot be expected to work in an older core that masks the upper banks.

For Polished Crystal's `mobileadapter` tooling, select this core explicitly in
probes accepting `--core`. The existing launcher pins the old upstream revision;
adoption requires updating that pin and regenerating snapshots in the game
repository. Its read-only palette observer can be applied to a separate checkout
of this source for multiplayer tests. No game-source change is part of this PR.
See [integration validation](integration-validation.md) for the tested reference
and the remaining game-side gates.
