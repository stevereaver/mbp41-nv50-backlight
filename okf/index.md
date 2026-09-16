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
- [Related system quirks](related-system-quirks.md) — the xserver fbdev bug and other context on this machine

## Change log

See [log.md](log.md).
