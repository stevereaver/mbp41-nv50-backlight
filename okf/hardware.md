---
type: Hardware Context
title: MacBookPro4,1 target hardware
description: The machine this module was written for and verified on.
tags: [macbook, nvidia, efi, backlight]
timestamp: 2026-09-17T00:00:00Z
---

# Target hardware

| Property | Value |
|---|---|
| Model | MacBookPro4,1 (early 2008, pre-unibody) |
| GPU | NVIDIA GeForce 8600M GT, G84M (NV50 family) |
| PCI ID | `10de:0407` at `0000:01:00.0` |
| Host bridge | Intel PM965 (`8086` at `00:00.0`) |
| Panel | LVDS on SOR0, EFI framebuffer 1440x900x32 (`/dev/fb0`, "EFI VGA") |
| Boot path | Apple EFI → GRUB → Linux (no CSM/legacy BIOS) |
| Kernel cmdline | `nomodeset loglevel=0 threadirqs splash` |
| OS | Debian 13 (trixie) via SpiralLinux, kernel 7.1.8+deb13-amd64 |

## Key constraints

- `nomodeset` is mandatory: nouveau cannot obtain a VBIOS under this boot
  path, so KMS is unavailable and no DRM driver binds the GPU. See
  [dead-approaches](dead-approaches.md).
- Because the GPU has no bound driver, its PCI BAR0 is unclaimed and can
  be iomapped by this module. See [register-interface](register-interface.md).
- Xorg runs via the fbdev driver on the EFI framebuffer; display timing is
  fixed by firmware. Backlight control is orthogonal to the display
  pipeline — the PWM register works regardless of what drives the panel.

## Verifying a candidate machine

The module is conservative: it checks that `01:00.0` is an NVIDIA VGA
controller and scans SORs 0–3 for a nonzero `PWM_CTL` before registering.
A quick userspace check for the PWM without loading anything:

```sh
sudo python3 -c "
import mmap, os, struct
f = os.open('/sys/bus/pci/devices/0000:01:00.0/resource0', os.O_RDWR)
m = mmap.mmap(f, 16*1024*1024, mmap.MAP_SHARED)
for i in range(2):
    print('SOR%d CTL=0x%08x' % (i, struct.unpack_from('<I', m, 0x61c084 + i*0x800)[0]))
"
```

A nonzero CTL on some SOR means the PWM is live and the module will work.
