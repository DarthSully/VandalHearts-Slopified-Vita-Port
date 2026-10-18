# PS Vita port: handoff notes

Read this first when resuming work on the Vita port. Player-facing install/controls are in
[README.md](README.md); this file is for whoever continues the work.

## State (build r07)

- Base: HalmyLyseas/VandalHearts-PcPort `14552ff` (v2.1.0 + packaging fix). The port is the
  `vita` branch: one commit per build, `git log 14552ff..vita`.
- Boots and plays (US disc). Menus/world map 60 fps; battles near the game's own 30 fps target (the
  game runs battles with `VSync(2)`); FMVs were the last stutter, addressed in r06 (untested on device
  at the time of writing).
- Builds are `vandal-vita-rNN.vpk`; change one thing per build, measure with the `profile` switch file.

## Building

```sh
export VITASDK=/opt/vitasdk PATH=$VITASDK/bin:$PATH     # needs SDL2 for Vita (2.32 tested)
platform/vita/build.sh "<disc>.bin" <KROMDAT.BIN | PS1 BIOS | charset2_us.bin> r07
```

- `build.sh` extracts `SLUS_004.47` from the disc (`tools/extract_exe.py`); md5 must be
  `596bb082a2de5f1fe977dd3d7e160b03`. A disc too big to copy around: the exe is 975 sectors at LBA 23
  (2048-byte payload at offset 24 of each 2352-byte sector).
- Font: no BIOS needed. `tools/krom_from_pc_exe.py vandalhearts_pc.exe charset2_us.bin` recovers the
  verified glyph block from the user's installed PC port.
- Later builds: `cmake -S platform/pc -B platform/pc/build_vita -DVH_VITA_BUILD=r07 && cmake --build ...`
  (the tag is a cached variable: pass it every time you bump it).
- Keep each build's unstripped ELF (`build_vita/vandalhearts_pc`) for core dumps.

## Measuring

`profile` switch file -> log lines every 120 frames or 1 s:

- `[FPS] VSync() calls/sec=N (mode=M)`: mode 2 = battle (30 fps target), mode 0 = 60 fps screens.
- `[frame] work=.. idle=..`: main-thread work vs pacing sleep per VSync.
- `[raster] async: raster X on the workers, main waited Y`: drawing time on cores 1-3 and how much
  of it the main thread still waited for.
- `[audio] main thread per VSync: xa=.. seq+spu=..`: software SPU (+reverb) and XA on the main thread.
- `[mdec] N ms per movie frame`, `[present] direct: N ms/frame`.

## History (device measurements)

| Build | Change | Result |
|---|---|---|
| r01 | first boot | 17 fps FMV, 8 fps battle; raster 58-70 ms, present 13.5 ms |
| r02 | banded MT raster, direct present, sparse IDCT | raster ~20-26 ms, present 2.2 ms |
| r03 | touch toggles (stretch, fast-forward) | - |
| r04 | span fast path, async raster | battle 18-25 fps; main thread ~27 ms of non-raster work |
| r05 | division-free reverb, inline lrintf | battle 25-30 fps; seq+spu 5-14 ms; MDEC 34 ms/frame |
| r06 | VSync never blocks on drawing; MDEC across the pool | battle ~27-30 fps (main: ~14 ms raster wait + ~12 ms SPU, serial); FMV 35 ticks/s, MDEC 14.5 ms |
| r07 | no movie spin-wait; table MDEC parse; audio pumped during drawing; main thread helps draw | (pending) |

## Where the time still goes / next ideas

0. r07 removed: `Movie_SyncFrame`'s `WAIT_TIME` spin (~10-19 ms per movie tick on the A9, the MDEC
   callback never fires on PC); the serial raster wait (DrawSync now runs the audio pump while the
   workers draw, then the main thread claims the remaining bands).
1. **SPU on the main thread** (`PC_SeqTick` -> `PC_SpuService`): 3-14 ms per VSync, heaviest with
   many voices + reverb. Moving rendering to its own thread needs care: libsnd mutates voice state
   from the game thread and the sequencer is clocked by rendered samples. Per-voice float math in
   `SpuRenderBlock` is the other lever.
2. **MDEC bitstream parse** stays serial, now with a 32-bit reader and an 11-bit AC lookup table
   (identical output vs r06 on 6000 random/structured v2+v3 streams).
3. Native widescreen is not available upstream; battles (3D) are the only candidate (projection
   width, off-screen culling, HUD layout). The stretch toggle is the stopgap.

## Verification tools (host)

- `tools/raster_selftest.c`: random prims through inline vs banded/threaded vs fast path,
  compares all VRAM words.
- `tools/mdec_selftest.c`: synthetic v2 bitstreams through the old vs two-phase decoder.
- The reverb rewrite was verified the same way (old vs new over 2M samples); see the r05 commit.
