# PS Vita port

An unofficial PS Vita build of the Vandal Hearts native PC port. Same game code, same backends;
this folder holds the Vita-only pieces. Everything Vita-specific in shared files is gated on
`__vita__` (C) or `VH_VITA` (CMake).

## Installing

1. Install `vandal-vita-rNN.vpk` with VitaShell.
2. Put your own Vandal Hearts **USA** disc image (`.bin`, SLUS-00447) in
   `ux0:data/vandalhearts/game/`. The folder is created on first launch. Raw 2352-byte-sector
   dumps only (a `.cue` alone or an `.iso` won't work).
3. Launch. Log: `ux0:data/vandalhearts/vandalhearts.log`. Saves and `vandalhearts.ini` live in
   `ux0:data/vandalhearts/` too.

## Controls

The PC port's gamepad layout ([docs/controls.md](../../docs/controls.md)), positional on the Vita:
Cross = cancel, Circle = confirm, Triangle = map view, Square = threat overlay, L/R = cycle units,
right stick = camera, **Select + Start** = options overlay.

The Vita has no L2/R2, so battle fast-forward is on the touch surfaces:

- **Double-tap the left half of the screen**: toggle battle fast-forward (2x / normal). Battle only,
  as on PC; two small arrows top-right show it's on.
- **Double-tap the right half of the screen**: toggle stretching the 4:3 picture to fill the 16:9
  screen (saved as `VH_VITA_STRETCH` in `vandalhearts.ini`). The game renders a fixed 320x240 frame,
  so this widens the picture rather than showing more of the map.
- Rear touchpad: right half = 2x, left half = normal speed.

## Switch files

Empty files in `ux0:data/vandalhearts/`:

| File | Effect |
|---|---|
| `profile` | Timing lines in the log: FPS, work/idle split, raster, present, audio, MDEC |
| `no_core3` | Keep the port off core 3 even when a core-unlock plugin makes it available |
| `no_serial` | Test switch: draw texture/draw-area overlap frames threaded (may glitch those effects) |
| `no_watchdog` | Turn off the hang watchdog |

**Hang watchdog.** If no frame completes for 6 s (20 s at boot; `VH_WATCHDOG_SECONDS` in
`vandalhearts.ini`), the port writes `hang_report.txt` (also appended to the log) with the main
thread's last stage and the drawing threads' state, then faults on purpose so the Vita writes a
`ux0:data/psp2core-*.psp2dmp`. Read the dump against that build's unstripped ELF.

Performance-related `vandalhearts.ini` keys: `VH_RASTER_THREADS` (default 4 with core 3 unlocked,
else 3), `VH_NATIVE_THREADS=0` (draw on one thread), `VH_RASTER_ASYNC=0` (no drawing/game overlap).

Every `VH_*` option from [OPTIONS.md](../pc/OPTIONS.md) also works through `vandalhearts.ini`.

## Building

```sh
export VITASDK=/usr/local/vitasdk
platform/vita/build.sh "Vandal Hearts (USA).bin" scph1001.bin r01
# -> platform/pc/build_vita/vandal-vita-r01.vpk
```

The second argument is PsyQ's `KROMDAT.BIN` or any 512 KB PS1 BIOS: the US game draws some text
(turn counters, item names, gold) with the BIOS kanji ROM font, which the build embeds. The
generator verifies the font bytes by hash.

Requirements: VitaSDK with SDL2 for Vita (2.32 tested), cmake, python3. Builds are tagged
`rNN` (`-DVH_VITA_BUILD=rNN`, a cached CMake variable: pass it on every configure when bumping).

## How the Vita build differs

- **No OpenGL, no OpenAL.** Presentation goes through SDL_Renderer (the GXM backend), like the macOS
  Metal path in `pc_gpu_window.c`. OpenAL is replaced by `src/vita_al.c`, a small OpenAL-subset
  over SDL2 audio (48 kHz): the port only ever streams two queued sources (software SPU, CD-XA).
- **Writable .rodata.** The game writes into some string literals. Desktop builds remap .rodata at
  startup; the Vita can't, so `vita.ld` links .rodata into the data segment.
- **No fixed PS1 addresses, no signal handlers.** The PS1 RAM/scratchpad literals already use
  host buffers under `PC_PORT` (the macOS path); crash diagnostics come from Vita core dumps.
- **ABI flags** `-fno-short-enums -fsigned-char`: the decomp assumes 4-byte enums and signed
  `char` (PS1 GCC, x86); ARM EABI defaults to neither. The data generator's sizeof probe uses the
  same flags.
- **Compile-only sizeof probe.** The generator normally compiles and *runs* a probe to read struct
  sizes. Nothing built for the Vita runs on the build host, so `VH_SIZEOF_VIA_NM=1` compiles
  `char vh_sz__<sym>[sizeof(sym)]` arrays and reads the sizes back with `nm -S`.
- **No PSX ELF needed.** Symbol addresses come from `symbol_addrs.txt`; unnamed `D_XXXXXXXX`
  symbols fall back to the address in their name.
- **No HD packs yet** (`VH_WEBP`/`VH_HDVIDEO` off): native backgrounds and MDEC movies.
- `src/vita_platform.c`: clocks (444 MHz), data folder + log, heap (192 MB, extended memory mode),
  stacks, core-3 probe and raster worker pinning, touch controls.

## Performance work (see HANDOFF.md for the measurements)

- **Banded multithreaded raster** (`pc_raster.c`): each DrawOTag's primitives are recorded and drawn
  in row chunks across up to 4 cores; per band, prims outside the rows are skipped before setup.
  Frames that sample textures from their own draw area are drawn serially. Pixel-identical
  (`tools/raster_selftest.c`).
- **Span fast path**: texture/CLUT/window/dither decisions hoisted per span; unmodulated texels
  copied directly. Pixel-identical.
- **Async raster**: DrawOTag returns while the workers draw; the frame is presented when done
  (VSync, VRAM access, DrawSync or the next DrawOTag), so game logic overlaps drawing.
- **Direct present**: BGR555 -> ABGR8888 through a LUT straight into a streaming texture.
- **Division-free SPU reverb** (`pc_spu.c`): the A9 has no divide instruction; `%` and 64-bit
  `/0x8000` replaced with exact equivalents (bit-identical over 2M samples).
- **Audio during drawing**: DrawSync runs the frame's XA/SPU pump while the workers are still
  drawing; the main thread then draws the bands left unclaimed.
- **Movies**: no spin-wait for the (never-fired) MDEC callback; table-driven AC decode.
- **MDEC**: sparse IDCT, inline `vcvtr` rounding, and a serial-parse / parallel-assemble split across
  the pool (`tools/mdec_selftest.c`: identical output).
