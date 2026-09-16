---
type: Change Log
title: Bundle history
description: Chronological record of how this module and knowledge were developed.
timestamp: 2026-09-17T00:00:00Z
---

# Log

## 2026-09-16 — fbdev/Xorg root cause and trixie upgrade

- Diagnosed MacBookPro4,1 black screen on kernel 6.12: kernel fine, Xorg's
  `fbdev_open()` rejected the now-PCI-parented EFI framebuffer (kernel
  ≥6.9, commit `9eac534db001`).
- Confirmed nouveau unusable: no VBIOS from any source under GRUB/EFI
  (PRAMIN/PROM/ACPI/PCIROM/PLATFORM all empty); `nomodeset` mandatory.
- Initially binary-patched `libfbdevhw.so`, then rebuilt xserver from
  source with upstream MR 1612 as a held local package.
- Upgraded the system bookworm→trixie; xserver 21.1.16 includes the fix
  natively. Kernel 7.1.8+deb13 verified booting to a working desktop.

## 2026-09-17 — backlight driver

- Found `/sys/class/backlight` empty; all stock drivers dead (SMI ports
  unresponsive under EFI, no gmux, no ACPI methods, nouveau unbound).
- Discovered SOR0 PWM register `0x61c084` in GPU BAR0 responds; verified
  live dimming via userspace mmap writes.
- Wrote `mbp_nv50_bl`: iomaps BAR0, scans SORs, registers
  `nvidia_backlight` (0–100 → duty 0–1025). Packaged via DKMS, autoloaded
  via `modules-load.d`. XFCE panel plugin + Fn keys confirmed working
  after restarting the panel/power-manager to re-enumerate backlights.
- Assessed upstreamability: as written it pokes another driver's PCI
  device, so not upstream-acceptable; the defensible upstream fix would be
  inside nouveau (register PWM backlight even when VBIOS init fails), or
  supplying a VBIOS via `nouveau.config=NvBios=`.
