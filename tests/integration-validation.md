Validation on 2026-10-11 used macOS 27.0.1 on Apple Silicon and upstream emulator
base `03f58ca3dfb4b716f7e66a0e0467f9e85ef82abb`. The game reference was the local
Polished Crystal `mobileadapter` commit
`6d719149dbbe7deb974e7683b7c3390748e768b8`. Its worktree was clean before and
after validation. No ROM, save, snapshot or copyrighted game data is committed
to this emulator repository.

Completed checks:

- The loader rejects truncated ROM data and unsupported size codes before
  touching a loaded cartridge. Both copied and persistent buffers are covered.
- Release and AddressSanitizer builds pass all six CTest cases. Generated
  fixtures execute nested upper-bank calls with 1/2/3/4/16 instances and exchange
  serial bytes with two linked instances. SRAM restoration compares every byte
  of each player's complete 64 KiB battery buffer.
- A macOS 11+ universal library builds with `arm64` and `x86_64` slices.
  Local execution is native ARM64. GitHub Actions also passed native Linux,
  macOS ARM64, macOS Intel and Linux AddressSanitizer execution for candidate
  `8c46585cec5dbd4b4b9dfd0f2aae584781f812ed` in
  [run 38176128563](https://github.com/ciriousjoker/doublecherryGB-libretro/actions/runs/38176128563).
- The game's existing `probe_mbc30.py --require-supported` reads `0x31`, `0xa5`
  and `0x5a` from banks 1, 128 and 255. Its generated ROM SHA-256 is
  `2f8fb4eda3a017fdde6a5b8ee5fcca8e5e1ce2dc4bcad366107168dfb986b0de`.
- Existing `probe_emulator.py` passes normal in-game Save, fresh Continue and
  one-tile movement, with valid save checksums and the unchanged baseline ROM
  SHA-256 `314001ec5a2dde0211d65211b7c3311c3f9d0e430a3d7de3a1b567b6db754b1e`.
- An isolated archive of the game reference was built with RGBDS 1.0.4, then its
  `probe_mbc30_layout.py` placed all six banked mobile sections in bank 128. The
  resulting 4 MiB/32 KiB ROM SHA-256 is
  `f36b806eadce5abb5ebe233ffa800c63b77b73b295abb62e3d3a37ad05cc8fbd`.
- That upper-bank ROM passes the existing four-client `probe_lobby.py` with
  `--scenario start-menu --native-actor-probe`. Four real core processes exchange
  adapter traffic through the local Bridge, render three peers per client and
  pass the native graphics/OAM checks. The game's read-only palette observer was
  applied only to a separate emulator source copy for this check. Emulation
  behavior is the same as the release core.
- The same upper-bank ROM passes all five four-client `probe_save_safety.py`
  scenarios: connected, peer disconnect, server close, malformed response and
  emergency disconnect. Each completes the in-game Save, validates primary and
  backup checksums, reloads through a fresh offline Continue and accepts movement.
  The resulting battery also loads in the ordinary disabled counterpart ROM.
  Native palette save equality and the disabled-ROM palette control pass. The
  optional instruction-level same-state profiling core was not supplied, so
  those additional copy assertions are not included in this result.

Full logs, game binaries, generated fixtures and machine-readable reports stay
under the local ignored `build/integration/` directory. The generated mapper and
libretro tests are reproducible directly from this repository without the game.
Game-specific integration checks require the separate reference and its prepared
tools, fixtures and server; they are not part of public CI.

Adoption still belongs in the game repository. Its launcher pins the old core,
its production mapper/layout switch is not yet implemented, and its native
`tools/bankends` rejects bank 128. The isolated layout diagnostic reports that
known post-link failure and verifies the expanded map separately. It is not a
passing production game build. Adopt the new emulator revision, add a guarded
game-side MBC30 layout, update the bank budget checks and regenerate snapshots
before shipping an upper-bank multiplayer ROM.

The selected runtime scenario does not resolve the game's existing protected
NPC/scanline-pressure failures. Those documented game-side acceptance gates remain
required for a multiplayer game release. RTC regression coverage preserves the
existing renderer interface; complete hardware halt/carry behavior, cartridge
hardware and WAN/mobile-device gameplay were not validated by this change.
