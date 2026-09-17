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

1. Boot once with `iomem=relaxed` (needed because
   `CONFIG_IO_STRICT_DEVMEM=y` blocks flashrom's `/dev/mem` access to the
   SPI controller). Revert the parameter afterwards.
2. `sudo flashrom -p internal -r mbp41_flash.bin` — dumps all 2 MiB.
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

## Note on redistribution

The extracted ROM is NVIDIA/Apple copyrighted firmware — keep it on the
machine, don't commit it. The extraction tooling and documentation are
the shareable artifacts.
