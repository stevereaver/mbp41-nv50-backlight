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

## Upgrading a Bookworm install in place

Verified on a second unit (2026-09-17). A stock SpiralLinux/Debian 12
install upgrades cleanly to trixie:

1. Snapper `pre`/`post` snapshot around the whole operation (Btrfs
   `@`/`@snapshots` layout already has `grub-btrfs` integration).
2. Retarget `/etc/apt/sources.list` to `trixie` (incl. `-security`,
   `-updates`, `-backports`); disable bookworm-only repos
   (SpiralLinux fasttrack etc.).
3. `apt full-upgrade` inside `tmux` with logging — ~1700 packages,
   a few hours on this hardware. Watch for debconf service-restart
   prompts.
4. **`broadcom-sta-dkms` fails to build on kernel ≥ 7.1** — the
   packaged 6.30.223.271 source doesn't compile (`typedefs.h` include
   failure). Purge it and switch the BCM4321 (`14e4:4328`) to the
   in-kernel `b43` driver:
   ```sh
   sudo apt-get install firmware-b43-installer
   sudo apt-get purge broadcom-sta-dkms broadcom-sta-common broadcom-sta-source
   ```
   `b43` bound on first boot and NetworkManager reassociated with the
   same DHCP lease — no config needed. This also unblocks the
   `linux-headers-*` packages whose postinst was failing on the DKMS
   error.
5. `apt autoremove` then drops ~100 bookworm leftovers (incl. the
   6.1 kernel; the 6.5 backports kernel is kept as a fallback).

Then continue from step 2 above (VBIOS → initramfs → GRUB → reboot).
No SPI dump or `iomem=relaxed` was needed — the shared ROM worked
as-is on identical `MBP41.88Z.00C1.B03` firmware.

## Video decode firmware (optional, recommended)

Nouveau on NV84 logs `vp:`/`bsp:` init failures until the VP2 video
engine firmware is installed. It's extracted from NVIDIA's own driver
(the license forbids redistribution, so everyone extracts their own):

```sh
mkdir /tmp/nouveau && cd /tmp/nouveau
wget https://raw.github.com/envytools/firmware/master/extract_firmware.py
wget http://us.download.nvidia.com/XFree86/Linux-x86/325.15/NVIDIA-Linux-x86-325.15.run
sh NVIDIA-Linux-x86-325.15.run --extract-only
python3 extract_firmware.py
sudo cp -d nv* vuc-* /lib/firmware/nouveau/
sudo apt-get install mesa-vdpau-drivers vdpauinfo
```

Add the `nv84_*` blobs to the initramfs hook (`/lib/firmware/nouveau`
directory), rebuild initramfs, reboot. Result: VP2 engine live →
VDPAU feature set A (full H.264, MPEG1/2; no VC-1). Verified with
`vdpauinfo` plus an ffmpeg `-hwaccel vdpau` decode producing
`vdpau(progressive)` frames. VLC/mpv use it natively.

Note: `vdpau-va-driver` (VA-API bridge) no longer exists in trixie —
VDPAU-only apps only.

## Known dead ends on this hardware

- **GPU reclocking**: `pstate` lists 4 levels (169→520 MHz) but any
  write returns EIO. `nv50_clk_calc` fails at `nvbios_pll_parse` —
  Apple's VBIOS lacks the PLL limits entries nouveau needs to program
  clocks safely. The GPU stays at its 275 MHz boot clock. Same code
  path means `nouveau.config=NvClkMode=` would fail identically.
- **`gr: TRAP_PROP` / `DATA_ERROR INVALID_BITFIELD` noise** from
  gst-plugin-scan/ffmpeg probing — cosmetic nouveau userspace quirk,
  unchanged by firmware presence. Harmless.
- **Runtime PM**: `power/control=auto` is settable but the GPU can't
  suspend while driving the internal panel. Not worth wiring up.

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
