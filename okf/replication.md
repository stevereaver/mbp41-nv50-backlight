---
type: Runbook
title: Replicating on another MacBookPro4,1
description: Condensed setup for a second machine, assuming the VBIOS is already extracted.
timestamp: 2026-09-17T00:00:00Z
---

# Replicating on another MBP4,1

Everything learned on the first machine is reusable. All MBP4,1 units
share the same firmware, so the already-extracted VBIOS
(`60.84.49.03.00`, sha256
`1e0a009d9daefb145e52e762d961dee0cf7de4dac4f4e0bc45275973e9ab41b3`) can
be copied directly — no SPI dump needed. Only re-extract if the new
machine reports a different EFI firmware version in `dmidecode`.

## Steps

1. **Install Debian 13 (trixie)** or SpiralLinux on trixie. Requires
   xserver ≥ 21.1.13 (fbdevhw fix) — needed only for the fallback path.
2. **Install the VBIOS** (keep the file private — copyrighted firmware):
   ```sh
   sudo cp mbp41-8600mgt-vbios.rom /usr/lib/firmware/nvidia/mbp41-8600mgt.rom
   ```
3. **Initramfs hook** so nouveau finds the ROM during early boot:
   `/etc/initramfs-tools/hooks/nvidia-vbios`:
   ```sh
   #!/bin/sh
   PREREQ=""
   prereqs() { echo "$PREREQ"; }
   case $1 in prereqs) prereqs; exit 0;; esac
   . /usr/share/initramfs-tools/hook-functions
   copy_file firmware /usr/lib/firmware/nvidia/mbp41-8600mgt.rom
   ```
   then `sudo update-initramfs -u -k all`.
4. **GRUB default cmdline** (`/etc/default/grub`,
   `GRUB_CMDLINE_LINUX_DEFAULT`):
   ```
   threadirqs loglevel=0 splash nouveau.config=NvBios=nvidia/mbp41-8600mgt.rom module_blacklist=mbp_nv50_bl
   ```
   No `nomodeset`. Run `sudo update-grub`.
5. **Fallback entry** in `/etc/grub.d/40_custom` — clone the normal
   entry with `nomodeset` and without the nouveau params, so the machine
   still boots to a desktop if anything regresses. On that path, install
   `mbp_nv50_bl` via DKMS (see [install-and-verify](install-and-verify.md))
   for backlight.
6. Reboot. Verify: `dmesg | grep "bios: version"` shows
   `60.84.49.03.00`; `ls /sys/class/backlight/` shows `nv_backlight`;
   Xorg log shows `modeset` + `glamor` on NV84.

## What to check if it fails

- Different EFI firmware (`dmidecode -t bios`) → re-extract via
  [vbios-extraction](vbios-extraction.md); the extractor verifies
  `10de:0407` and checksum automatically.
- Black screen on boot → boot the fallback entry, check
  `dmesg | grep -i nouveau` for the BIOS image score (`scored 4` =
  accepted).
- No backlight → panel plugin only enumerates at startup; restart
  `xfce4-panel`/`xfce4-power-manager` once.

## Other models

For a different MacBook (e.g. MBP5,1 with a different GPU), the ROM
can't be reused — re-extract that machine's own VBIOS and adjust the
`DEVICE_ID` in `tools/extract_vbios.py` to the card's PCI ID.
