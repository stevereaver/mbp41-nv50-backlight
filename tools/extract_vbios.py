#!/usr/bin/env python3
"""Extract the NVIDIA VBIOS from a MacBook Pro 4,1 SPI flash dump.

The 8600M GT has no on-card ROM; its VBIOS lives inside Apple's EFI
firmware on the ICH8M SPI flash (SST25VF016B, 2 MiB). This script parses
the dump with uefi-firmware and pulls out any section that looks like a
legacy PCI option ROM (55 AA + PCIR) with the expected PCI IDs.

Usage:
    # on the MacBook (needs iomem=relaxed for flashrom):
    sudo flashrom -p internal -r mbp41_flash.bin
    pip install uefi-firmware
    python3 extract_vbios.py mbp41_flash.bin
"""

import struct
import sys

VENDOR_ID = 0x10DE   # NVIDIA
DEVICE_ID = 0x0407   # G84M / GeForce 8600M GT (MacBookPro4,1)


def find_vbios_images(blob):
    """Yield (offset, image) for every 55AA+PCIR option ROM in blob."""
    pos = blob.find(b"\x55\xaa")
    while pos >= 0:
        img = blob[pos:]
        if len(img) >= 0x20:
            pcir_off = struct.unpack_from("<H", img, 0x18)[0]
            if pcir_off + 24 <= len(img) and img[pcir_off:pcir_off+4] == b"PCIR":
                vid, did = struct.unpack_from("<HH", img, pcir_off + 4)
                size = img[2] * 512
                yield pos, img[:size], vid, did, size
        pos = blob.find(b"\x55\xaa", pos + 1)


def walk(obj, blobs):
    for attr in ("content", "data"):
        c = getattr(obj, attr, None)
        if isinstance(c, (bytes, bytearray)):
            blobs.append(bytes(c))
    for s in getattr(obj, "objects", []):
        walk(s, blobs)


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    dump = open(sys.argv[1], "rb").read()

    # Collect raw + decompressed blobs from every firmware volume.
    try:
        from uefi_firmware.uefi import FirmwareVolume
    except ImportError:
        sys.exit("pip install uefi-firmware")

    blobs = [dump]
    for off in range(len(dump) - 0x28):
        if dump[off:off+4] != b"_FVH":
            continue
        fv = FirmwareVolume(dump[off - 0x1c:], "fv%x" % off)
        try:
            fv.process()
            walk(fv, blobs)
        except Exception:
            pass

    found = 0
    for blob in blobs:
        for off, img, vid, did, size in find_vbios_images(blob):
            if vid != VENDOR_ID or did != DEVICE_ID or size == 0:
                continue
            if sum(img) % 256 != 0:
                print("candidate at %x fails checksum, skipped" % off)
                continue
            out = "vbios_%04x_%04x.rom" % (vid, did)
            open(out, "wb").write(img)
            ver_at = img.find(b"60.")
            ver = img[ver_at:ver_at+14].split(b"\x00")[0].decode("ascii", "replace")
            print("wrote %s (%d bytes, version %s)" % (out, len(img), ver))
            found += 1
    sys.exit(0 if found else 1)


if __name__ == "__main__":
    main()
