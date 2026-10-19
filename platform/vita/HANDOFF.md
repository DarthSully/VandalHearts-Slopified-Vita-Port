# PS Vita port: handoff notes

Read this first when resuming work on the Vita port. Player-facing install/controls are in
[README.md](README.md); this file is for whoever continues the work.

## State (build r09)

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
| r07 | no movie spin-wait; table MDEC parse; audio pumped during drawing; main thread helps draw | battle 30 fps, FMV ~55-60; intermittent silent hard hang in battles (the timing near overlap frames was coincidental, see r08); intermittent black screen at boot |
| r08 | hang watchdog (breadcrumbs + deliberate fault for a core dump), `no_serial` test switch | hang caught: main thread waiting on the pool with done=2/3 while all 3 workers were idle (a lost done signal) |
| r09 | pool handshake on kernel semaphores + atomics; "workers late" log line; watchdog appends to the log | chapter 1 played through with no hangs or hiccups (released) |

## Hang watchdog (r08)

`platform/vita/src/vita_watchdog.c` runs a thread on core 2 that polls `g_vhWdFrames` (bumped at the
end of VSync) every 250 ms. Counting polls instead of reading the clock keeps a suspended Vita from
tripping it. On a stall it writes `hang_report.txt` with sceIo only (the hung thread may hold
stderr's lock), then writes to address 8 so the kernel produces a core dump with every thread's
registers. Breadcrumb macros live in `platform/pc/src/pc_watchdog.h` and compile to nothing off-Vita.

## Pool handshake (r09)

The r08 watchdog caught the hang: all chunks drawn, all workers idle, `done` stuck at 2 of 3. The
pool's mutex/condvar come from pthread-embedded, written for the single-core PSP; on the Vita one
done signal went missing. On `__vita__` the pool now uses one kernel counting semaphore for start
tokens (one per worker per dispatch) and one for done signals (the dispatcher waits for N), with
chunk claiming and the done count as atomics. Desktop builds keep the pthread code.

A fast worker may take a second start token of the same dispatch; it finds no chunks, signals done,
and the slow worker simply picks up the next dispatch's token. N tokens always yield N done signals,
and a token holder signals done only after its own chunks, so the wait still means "all drawn".

Verification (host, `__vita__` path compiled against a POSIX stand-in for the kernel semaphores,
random sleeps/yields at every semaphore operation): `raster_selftest.c` threaded vs inline identical;
a stress harness of parallel-for jobs (each item exactly once) and async raster passes polled with
NativeRasterDone (identical to a sync pass of the same list) at 2, 3 and 4 threads.

Side finding, not changed: a 15-bit texture whose page starts within 256 halfwords of VRAM's right
edge reads `s_vram[y][x >= 1024]`, i.e. the next row, where hardware wraps x. Not seen in game data.

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
