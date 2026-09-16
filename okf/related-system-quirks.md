---
type: Context
title: Related system quirks on the MacBookPro4,1
description: Other workarounds this machine carries — the xserver fbdev bug and the X config this module coexists with.
tags: [xorg, fbdev, efifb, nomodeset, trixie]
timestamp: 2026-09-17T00:00:00Z
---

# Related quirks on the same machine

This module is one of several fixes needed to make an EFI-booted
MacBookPro4,1 usable on modern kernels. The others matter because they
constrain how this module can be used.

## Xorg fbdev bug on kernels ≥ 6.9

Kernel commit `9eac534db001` ("firmware/sysfb: Set firmware-framebuffer
parent device", v6.9) re-parents the EFI framebuffer under the PCI device
in sysfs:

- before: `/sys/class/graphics/fb0` → `devices/platform/efi-framebuffer.0/...`
- after: `... → devices/pci0000:00/0000:01:00.0/efi-framebuffer.0/graphics/fb0`

xserver ≤ 1.21.1.12 `fbdev_open()` rejects fb devices whose sysfs path
contains `devices/pci` → "Unable to find a valid framebuffer device" →
"no screens found" → black screen with a booted, reachable system.

Fixed upstream in xserver MR 1612 (≥ 21.1.13); Debian 13 trixie ships
21.1.16 so no local patch is needed there. On Debian 12 (bookworm,
xserver 21.1.7) a rebuild with the patch was required.

## The Xorg config this module coexists with

`/etc/X11/xorg.conf.d/20-fbdev.conf` pins a Device section **without a
BusID**, which forces the screen onto the non-PCI fb slot (`fbdev_open`
path). This remains necessary even with a fixed xserver, because
`fbdev_open_pci` looks for `<pci>/graphics/fb0` while the fb is actually
nested under `<pci>/efi-framebuffer.0/graphics/fb0`.

```conf
Section "Device"
	Identifier  "EFI Framebuffer"
	Driver      "fbdev"
	Option      "fbdev" "/dev/fb0"
EndSection

Section "Screen"
	Identifier  "Default Screen"
	Device      "EFI Framebuffer"
EndSection
```

## Interaction with this module

- The module only touches the GPU's backlight PWM — it does not provide a
  DRM/KMS device and does not conflict with efifb/fbdev.
- Because it calls `pci_enable_device`/`pci_iomap` without claiming the
  device, it must never coexist with a bound nouveau (two owners of the
  PWM). On the target machine nouveau can never bind — see
  [dead-approaches](dead-approaches.md) — but on machines where it can,
  prefer nouveau's own `nvidia_backlight` and do not load this module.

## One observed shutdown hang

A shutdown hang was observed once on the first poweroff after the
bookworm→trixie upgrade; it did not reproduce and left no logs (journald
stops before the final unmount/poweroff phase). Suspects if it recurs:
`firewire_ohci` (TI TSB82AA2, known poweroff blocker on this generation)
or the ACPI S5 path on the new kernel. To capture it: raise console
loglevel (`kernel.printk=7`) and watch the last message on screen.
