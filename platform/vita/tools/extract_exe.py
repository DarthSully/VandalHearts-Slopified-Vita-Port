#!/usr/bin/env python3
"""Extract a file (default SLUS_004.47) from a raw PS1 disc image (.bin, 2352-byte sectors).

The Vita build needs the original PS-X executable at build time (the data generators slice game
data out of it); this pulls it straight from the user's own disc dump.

    extract_exe.py "Vandal Hearts (USA).bin" out/SLUS_004.47 [NAME]
"""
import struct
import sys

RAW = 2352
DATA_OFS = 24  # Mode 2 Form 1: 12 sync + 4 header + 8 subheader


def read_sector(f, lba):
    f.seek(lba * RAW)
    raw = f.read(RAW)
    if len(raw) != RAW:
        raise SystemExit(f"short read at LBA {lba}")
    return raw[DATA_OFS:DATA_OFS + 2048]


def read_extent(f, lba, size):
    out = bytearray()
    while len(out) < size:
        out += read_sector(f, lba)
        lba += 1
    return bytes(out[:size])


def find_entry(f, dir_lba, dir_size, name):
    data = read_extent(f, dir_lba, dir_size)
    i = 0
    while i < len(data):
        n = data[i]
        if n == 0:  # records do not cross sectors: skip to the next one
            i = (i // 2048 + 1) * 2048
            continue
        lba = struct.unpack_from('<I', data, i + 2)[0]
        size = struct.unpack_from('<I', data, i + 10)[0]
        flen = data[i + 32]
        fname = data[i + 33:i + 33 + flen].decode('ascii', 'replace')
        if fname.split(';')[0].upper() == name.upper():
            return lba, size
        i += n
    return None


def main():
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)
    image, out = sys.argv[1], sys.argv[2]
    name = sys.argv[3] if len(sys.argv) > 3 else 'SLUS_004.47'
    with open(image, 'rb') as f:
        pvd = read_sector(f, 16)
        if pvd[1:6] != b'CD001':
            raise SystemExit("not an ISO 9660 data track (expected a raw 2352-byte-sector .bin)")
        root = pvd[156:156 + 34]
        root_lba = struct.unpack_from('<I', root, 2)[0]
        root_size = struct.unpack_from('<I', root, 10)[0]
        hit = find_entry(f, root_lba, root_size, name)
        if not hit:
            raise SystemExit(f"{name} not found in the disc's root directory")
        data = read_extent(f, *hit)
    with open(out, 'wb') as o:
        o.write(data)
    print(f"extracted {name}: {len(data)} bytes -> {out}")


if __name__ == '__main__':
    main()
