---
type: Analysis
title: Why every stock backlight driver fails here
description: Empirically verified dead ends on the EFI-booted MacBookPro4,1.
tags: [nouveau, apple_bl, apple_gmux, acpi, backlight, efi]
timestamp: 2026-09-17T00:00:00Z
---

# Dead ends (all verified on hardware, kernel 7.1.8)

## nouveau — no VBIOS

Under GRUB/EFI, nouveau's BIOS image scan finds nothing:

```
bios: trying PRAMIN...    → not enabled / invalid
bios: trying PROM...      → ROM signature (0000) unknown
bios: trying ACPI...      → nothing
bios: trying PCIROM...    → Invalid ROM header (got 0x0000)
bios: trying PLATFORM...  → nothing (pdev->rom only populated by the EFI stub)
bios: unable to locate usable image
probe with driver nouveau failed with error -22
```

Apple's GPU ROM contains an EFI driver, not an x86 VBIOS, and GRUB clears
the legacy shadow area. `nouveau.config=NvBios=PRAMIN` was tested and also
fails. The only remaining path is `NvBios=<file>` via `request_firmware`,
which requires an externally-sourced ROM dump — none available.

## apple_bl — dead SMI ports

`apple_bl` (renamed from `mbp_nvidia_bl` in 2010) drives the backlight by
triggering firmware SMIs:

- Intel host bridge → ports `0xb2` (cmd) / `0xb3` (data)
- NVIDIA host bridge → ports `0x52e` (cmd) / `0x52f` (data)

Verified via `/dev/port` on MBP4,1: a write of `0x03` to the data port
reads back unchanged — the SMI handler that services these traps was part
of Apple's legacy BIOS environment and does not exist under EFI boot.

Two additional blockers even if the ports worked:

1. `apple_bl_init` requires `acpi_video_get_backlight_type() ==
   acpi_backlight_vendor`, which is not selected on this machine (would
   need `acpi_backlight=vendor` on the cmdline to test).
2. Port selection keys off the **host bridge** vendor. MBP4,1's host
   bridge is Intel (PM965) even though the GPU is NVIDIA — so it would use
   the `0xb2/0xb3` ports, while `pommed`'s `nv8600mgt` backend used
   `0x52e/0x52f` for this model. Moot under EFI either way.

## apple_gmux — hardware absent

The gmux is the display-mux chip on dual-GPU unibody MacBooks
(MacBookPro5,1 and later). The MBP4,1 has a single GPU and no gmux; the
driver loads but never creates a device.

## acpi_video — no methods

The `video` module loads but registers no `acpi_video*` backlight: the
ACPI namespace's `APP0002` device exists, but no usable `_BCL`/`_BCM`
methods are implemented under the EFI boot path.

## What works

The GPU's own panel-PWM register in BAR0 — see
[register-interface](register-interface.md).
