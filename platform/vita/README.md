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

The Vita has no L2/R2, so battle fast-forward is on the **rear touchpad**: touch the right half for
2x, the left half for normal speed.

## Switch files

Empty files in `ux0:data/vandalhearts/`:

| File | Effect |
|---|---|
| `profile` | Present and raster timing lines in the log (`VH_PRESENT_TIME`, `VH_RASTER_TIME`) |

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
  stacks, rear-touch L2/R2.
