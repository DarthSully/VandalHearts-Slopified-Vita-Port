# Vita port changelog

Builds are tagged `rNN`. Every build runs the same game code as the PC port; only the Vita layer
changes. Measurements are from a PS Vita with a core-unlock plugin (4 cores). Technical detail for each
build is in [HANDOFF.md](HANDOFF.md).

## r09 (current release)

- **Fixed: the game could freeze completely during battles** (picture and sound stopped, only closing
  the game helped). The threads that share the drawing work occasionally lost a "finished" signal, so
  the game waited forever. They now coordinate through the Vita's own kernel semaphores, which cannot
  lose a signal.
- If drawing ever runs more than a quarter second late, the log says so (`pool: workers late`) instead
  of the game hanging silently.
- The hang watchdog now adds its report to the end of the log instead of overwriting it.
- Tested: chapter 1 played through with no freezes or slowdowns.

## r08 (test build)

- Added a hang watchdog: if no frame finishes for 6 seconds (20 at boot), the game writes
  `hang_report.txt` and closes with error C2-12828-1 on purpose, leaving a crash dump that shows where
  it was stuck. This is what found the r09 bug. Turn it off with an empty `no_watchdog` file.
- Added the `no_serial` test switch.

## r07

- Movies play at full speed: removed a busy-wait the PC port inherited from the PS1 code.
- Movie decoding about 2x faster (identical output).
- Music is mixed while the other cores draw; the main core also helps finish drawing.
- Battles hold the game's own 30 fps.

## r06

- The game no longer waits for drawing to finish before starting the next frame.
- Movie decoding spread across all cores.

## r05

- Reverb no longer does divisions (the Vita's CPU has no hardware divide); identical sound.
- Faster colour conversion in movies.

## r04

- Faster drawing of common shapes; drawing overlaps the game's own work.

## r03

- Touch controls: double-tap the left half of the screen for battle 2x speed, the right half to
  stretch the picture to 16:9.

## r02

- Drawing split across all cores; faster screen output and movie decoding.

## r01

- First boot on the Vita (17 fps movies, 8 fps battles).
