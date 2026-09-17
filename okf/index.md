---
type: Bundle Index
title: mbp41-nv50-backlight knowledge bundle
description: Knowledge about backlight control on EFI-booted NVIDIA MacBook Pro 4,1 systems.
timestamp: 2026-09-17T00:00:00Z
---

# mbp41-nv50-backlight

This bundle documents everything learned while adding display-backlight
control to a MacBookPro4,1 (GeForce 8600M GT) running Debian 13 (trixie),
booted under GRUB/EFI with `nomodeset`.

## Concepts

- [Target hardware](hardware.md) — the machine, GPU, and boot environment this applies to
- [Why every stock driver fails](dead-approaches.md) — nouveau, apple_bl, apple_gmux, acpi_video
- [The PWM register interface](register-interface.md) — the mechanism the module drives
- [Install & verify](install-and-verify.md) — DKMS packaging, autoload, desktop integration
- [VBIOS extraction](vbios-extraction.md) — recovering the 8600M GT VBIOS from the SPI flash to enable nouveau
- [Replication](replication.md) — condensed setup for a second MBP4,1
- [Related system quirks](related-system-quirks.md) — the xserver fbdev bug and other context on this machine
- [VDPAU / NV84 video decode](vdpau-nv84.md) — VP2 firmware, the mesa subchan_del fd bug + LD_PRELOAD shim, and the 7.x kernel decode regression

## Change log

See [log.md](log.md).
