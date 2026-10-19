# Vita port: working notes for Claude

Read this first; it replaces re-reading old chats. Detail lives in [HANDOFF.md](HANDOFF.md) (build
history, measurements, design notes), [README.md](README.md) (install, switches) and
[CHANGELOG.md](CHANGELOG.md). The repo-root CLAUDE.md covers the PC port and decomp.

## How the owner works

- Plan first, then wait for an explicit "go" before building or changing anything. Keep replies short.
- Builds run on the owner's PC, never in Claude's workspace. Claude edits source in its own clone and
  ships a source tarball; the owner double-clicks the build script.
- `C:\VitaBuild\` holds other projects (Super Metroid). Only touch `C:\VitaBuild\Vandalhearts\`.
- Don't push or publish unless asked. Git identity: DarthSully. Repo:
  `DarthSully/VandalHearts-Slopified-Vita-Port`, branch `main`.

## Build loop (owner's PC)

- Folder `C:\VitaBuild\Vandalhearts\`: `build_vh.bat` -> WSL `build_vh_wsl.sh <tag>`, which unpacks
  the newest `vh-src-*.tar.gz` into `~/vitabuilds/vandalhearts/src` (keeps `build_vita/` for
  incremental builds), extracts `SLUS_004.47` and the font from
  `C:\Games\QuiverLauncher\Apps\VandalHearts-VandalHeartsPC\` (disc .bin + `vandalhearts_pc.exe`),
  builds with VitaSDK at `/usr/local/vitasdk` (SDL2 from vdpm), and copies
  `vandal-vita-<tag>.vpk`, `vandal-<tag>.elf` and the build log to `out\`.
- To ship a build: commit, `git archive --format=tar.gz -o vh-src-<tag>.tar.gz HEAD -- . ':!downloads'`,
  write it plus `build_vh.bat` (tag bumped) into the folder.
- Claude's workspace has no VitaSDK. Syntax-check with clang
  `--target=armv7a-none-eabihf -mcpu=cortex-a9 -D__vita__` against vita-headers + VitaSDK newlib and
  pthread-embedded headers, or test logic on the host (below).

## Debugging a device problem

- Ask for `ux0:data/vandalhearts/vandalhearts.log` (empty `profile` file = timing lines), and on a hang
  `hang_report.txt` + the newest `ux0:data/psp2core-*.psp2dmp`. The log is rewritten on every launch.
- Read dumps with xyzz/vita-parse-core (needs small Python 3 fixes) against that build's
  `out\vandal-<tag>.elf`. Game text loads at 0x8100a000 vs ELF 0x81000000: subtract 0xa000. For
  threads parked in the kernel, scan their stack for return addresses in game text and addr2line them.
- Lesson (r07-r09): pthread-embedded's mutex/condvar can lose wake-ups on the Vita's 4 cores. Use
  kernel semaphores/event flags plus atomics for cross-thread handshakes.
- Host test pattern: compile the real `__vita__` code with a POSIX stand-in for the kernel calls and
  inject random delays (see HANDOFF "Pool handshake"). `tools/raster_selftest.c` checks drawing is
  pixel-identical.

## Open ideas (not started)

- Music synthesis (SPU) off the main thread: ~11 ms of each 33 ms battle frame.
- Thread the texture/draw-area overlap frames by copying the sampled region first (~25 fps today).
- Keep the previous log/hang report as `.prev` on launch; stamp the build tag in both.
- Black screen at boot (seen on r07) is unexplained; the watchdog covers boot (20 s).
