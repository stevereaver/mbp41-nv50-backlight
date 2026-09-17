---
type: Concept
title: VBIOS extraction from the SPI flash
description: How the 8600M GT VBIOS was recovered from Apple's EFI firmware and used to make nouveau work.
timestamp: 2026-09-17T00:00:00Z
---

# VBIOS extraction

On the MacBookPro4,1 the 8600M GT has no on-card option ROM — the PCI ROM
BAR reads as empty and the legacy `0xC0000` shadow is `0xff` under EFI.
The VBIOS lives inside Apple's EFI firmware in the machine's SPI flash
(ICH8M, SST25VF016B, 2 MiB), inside compressed UEFI FFS sections — which
is why raw scans find no `PCIR`/`NVIDIA` strings.

## Extraction procedure

There are two ways to read the flash:

**Method A — read-only MTD (no reboot, no flashrom).** The firmware hub
decodes the flash into physical memory below 4GB — verified
byte-identical to a flashrom dump. `tools/int0800_flash.c` is a ~77-line
ACPI platform driver binding `INT0800` that maps that window read-only
via `map_rom`:

```sh
cd tools && make -C /lib/modules/$(uname -r)/build M=$PWD
sudo modprobe mtd map_rom && sudo insmod int0800_flash.ko
sudo cat /dev/mtd0 > firmware_window.bin     # 16MB window; flash at the top
```

The INT0800 `_CRS` window (here `ff000000–ffffffff`) may be larger than
the chip — undecoded holes read `0xff`; scan for `_FVH` signatures to
locate the real flash content (on MBP4,1 it sits at window offset
`0x1e00000`).

**Method B — flashrom.** Boot once with `iomem=relaxed` (needed because
`CONFIG_IO_STRICT_DEVMEM=y` blocks flashrom's `/dev/mem` access to the
SPI controller), revert the parameter afterwards, then
`sudo flashrom -p internal -r mbp41_flash.bin` — dumps all 2 MiB.

Then, from either dump:

3. Parse with the `uefi-firmware` Python package: create a
   `FirmwareVolume` at each `_FVH` signature, `process()` it, and walk
   `objects` recursively collecting `content`/`data` of every section
   (decompression is handled by the parser).
4. The VBIOS is a section beginning `55 AA` whose `PCIR` header carries
   `VID=10de DID=0407`, 53248 bytes, checksum 0, version
   `60.84.49.03.00` (dated 02/16/08). It was found in FFS file
   `b0cd1bfc-317d-aa49-936a-a4600d9dd083`, nested inside a
   `GuidDefinedSection` in file `7f94c9a2-16be-3e40-b84b-f246ef6f6750`.
   `tools/extract_vbios.py` automates all of this.

## Deployment

- Copy the ROM to `/usr/lib/firmware/nvidia/mbp41-8600mgt.rom`.
- Include it in every bootable kernel's initramfs (nouveau loads early);
  an initramfs-tools hook using `copy_file firmware ...` works.
- Kernel parameter: `nouveau.config=NvBios=nvidia/mbp41-8600mgt.rom`,
  with `nomodeset` removed.

## Verified result (kernel 7.1.8+deb13)

Nouveau accepted the image (`bios: using image ...`, `version
60.84.49.03.00`, BIT signature found, 512 MiB GDDR3), DRM initialized,
`nouveaudrmfb` became the console framebuffer, Xorg's modesetting driver
came up with glamor acceleration on NV84, LVDS-1 ran at native 1440x900
with full EDID modes, and nouveau registered its own `nv_backlight`
device — the same PWM register this module drives.

Caveats observed: missing `nouveau/nv84_xuc*` video-decode firmware
(non-fatal) and `TRAP_PROP`/`PAGE_NOT_PRESENT` faults when
`gst-plugin-scan` probed the video engines — cosmetic, display stable.

The `mbp_nv50_bl` module remains useful for the `nomodeset` fallback
path; when booting nouveau it must not load (`module_blacklist` or the
built-in bound-driver check), and `nv_backlight` replaces it — see
[dead approaches](dead-approaches.md) and
[register interface](register-interface.md).

## Upstream considerations

The ROM itself cannot be submitted anywhere (no redistribution license —
same reason `linux-firmware` carries no VBIOS dumps). The shareable
pieces are:

- `tools/extract_vbios.py` — works on any UEFI flash dump; a candidate
  for envytools or standalone distribution.
- `tools/int0800_flash.c` — a plausible `linux-mtd` contribution:
  INT0800 is the standard firmware-hub ACPI ID on x86, so this gives
  read-only userspace flash access on many EFI machines (not just Macs).
- A nouveau kernel patch that scans the firmware flash (FV/FFS walk +
  decompress) would remove the userspace step entirely, but putting
  UEFI volume parsing inside a GPU driver is a hard sell upstream; the
  MTD + userspace split is cleaner.

## Note on redistribution

The extracted ROM is NVIDIA/Apple copyrighted firmware — keep it on the
machine, don't commit it. The extraction tooling and documentation are
the shareable artifacts.
