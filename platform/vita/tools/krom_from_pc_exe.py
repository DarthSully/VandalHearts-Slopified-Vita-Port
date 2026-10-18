#!/usr/bin/env python3
"""Recover the US kanji ROM glyph block (charset 2, 209 glyphs) from a released vandalhearts_pc.exe
(or the Linux binary), for use as VH_KROM_SOURCE when no PS1 BIOS or PsyQ KROMDAT.BIN is at hand.

The released port embeds exactly the block gen_kanji_font.py extracts, so this searches the binary
for the 6270-byte window whose SHA-256 matches the one gen_kanji_font.py verifies.

    krom_from_pc_exe.py vandalhearts_pc.exe charset2_us.bin
"""
import hashlib
import sys

N = 209 * 30
WANT = "19b1dd839020e62a8a0f6dfc673c320da36105d9bc9bbce876aa151b7bd927a7"


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    data = open(sys.argv[1], "rb").read()
    zero = bytes(30)
    pos = data.find(zero)
    while pos != -1 and pos + N <= len(data):
        # glyph 0 is blank; the full-width period (4) and '0' (147) are not
        if any(data[pos + 120:pos + 150]) and any(data[pos + 147 * 30:pos + 148 * 30]):
            if hashlib.sha256(data[pos:pos + N]).hexdigest() == WANT:
                open(sys.argv[2], "wb").write(data[pos:pos + N])
                print(f"wrote {sys.argv[2]} ({N} bytes, found at offset {pos:#x})")
                return
        pos = data.find(zero, pos + 1)
    raise SystemExit("glyph block not found (not a US-capable Vandal Hearts PC port build?)")


if __name__ == "__main__":
    main()
