# Vandal Hearts — Slopified Vita Port

*Vandal Hearts* (PS1) running natively on the PS Vita: an **AI-made port** (built with Claude) of the
[Vandal Hearts PC Port](https://github.com/HalmyLyseas/VandalHearts-PcPort). It runs at **full speed**
(menus 60 fps, battles at the game's own 30 fps, smooth FMVs), but it has **not been played all the way
through**, so expect the odd bug.

**[Download the latest VPK (r09)](https://github.com/DarthSully/VandalHearts-Slopified-Vita-Port/releases/latest)**
([direct link](https://github.com/DarthSully/VandalHearts-Slopified-Vita-Port/raw/main/downloads/VandalHearts-Vita-r09.vpk))
· [Changelog](platform/vita/CHANGELOG.md)

r09 fixes a freeze that could hit during battles on r07 and earlier.

## Install

You need a Vita (or PS TV) with HENkaku/Ensō and VitaShell, and your own copy of the USA disc
(SLUS-00447). No disc image is included.

1. Install the VPK with VitaShell.
2. Copy your disc image (`.bin`, raw 2352-byte sectors) into `ux0:data/vandalhearts/game/`
   (launch once to create the folder).
3. Launch **Vandal Hearts**.

Recommended: a core-unlock plugin (e.g. CapUnlocker); the port uses the 4th core when it's available.

## Controls

| Vita | Game |
|---|---|
| Circle / Cross | Confirm / Cancel |
| Triangle / Square | Map view / Threat overlay |
| L / R | Cycle units |
| Right stick | Camera |
| Select + Start | Options |
| Double-tap left half of the screen | Battle 2x speed on/off |
| Double-tap right half of the screen | Stretch to 16:9 on/off |

## Credits

All game code and the PC port are the work of **Halmyrach and the
[Vandal Hearts PC Port](https://github.com/HalmyLyseas/VandalHearts-PcPort) contributors**, built on
**shao113's [Vandal Hearts decompilation](https://github.com/shao113/vh)**. This fork only adds the Vita
layer (`platform/vita/`, see its [README](platform/vita/README.md)). Not affiliated with or endorsed by
the original authors. GPL-2.0, same as upstream. Like the PC port's releases, the program embeds a
small amount of data reconstructed from the game; the game itself comes from your own disc.
