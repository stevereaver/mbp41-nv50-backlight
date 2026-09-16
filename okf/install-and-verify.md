---
type: Runbook
title: Install, verify, and integrate
description: Build/install via DKMS, autoload at boot, and desktop environment integration.
tags: [dkms, install, xfce, verification]
timestamp: 2026-09-17T00:00:00Z
---

# Install

Requires: `dkms`, `linux-headers-$(uname -r)`, a compiler toolchain.

```sh
sudo cp -r . /usr/src/mbp-nvidia-bl-1.0        # Makefile, dkms.conf, mbp_nv50_bl.c
sudo dkms add -m mbp-nvidia-bl -v 1.0
sudo dkms build -m mbp-nvidia-bl -v 1.0
sudo dkms install -m mbp-nvidia-bl -v 1.0       # lands in updates/dkms/, depmod runs
echo mbp_nv50_bl | sudo tee /etc/modules-load.d/mbp-nvidia-bl.conf
sudo modprobe mbp_nv50_bl
```

DKMS keeps the module rebuilt across kernel upgrades (`AUTOINSTALL=yes`).
Debian's dkms will also MOK-sign the module if configured — irrelevant on
this hardware (no Secure Boot), harmless elsewhere.

## Verify

```sh
ls /sys/class/backlight/          # expect: nvidia_backlight
cat /sys/class/backlight/nvidia_backlight/{brightness,actual_brightness,max_brightness}
echo 50 | sudo tee /sys/class/backlight/nvidia_backlight/brightness   # screen dims visibly
dmesg | tail                      # expect: "mbp_nv50_bl: registered (SOR0, duty N)"
```

Expected dmesg line on MBP4,1: `registered (SOR0, duty <n>)`.

## Desktop integration (XFCE verified)

- `xfce4-power-manager` auto-discovers `/sys/class/backlight` at daemon
  start; `handle-brightness-keys=true` makes Fn brightness keys work.
- The **panel plugin** (`xfce4-power-manager-plugins`) enumerates
  backlights when it starts — if it was already running before the device
  appeared, restart it:

  ```sh
  xfce4-power-manager --restart
  xfce4-panel -r
  ```

- Clicking the battery icon in the panel then shows the brightness
  slider. Equivalent for other DEs: GNOME/KDE pick up the device
  automatically on next power-daemon probe.

## Persistence across reboots

`systemd-backlight` automatically saves/restores
`/sys/class/backlight/nvidia_backlight` levels — no extra setup needed.

## Failure modes

| Symptom | Meaning |
|---|---|
| `insmod: No such device` | GPU not at `01:00.0`, not NVIDIA VGA, or no SOR has a live PWM — see the userspace probe in [hardware](hardware.md). |
| Device exists but writes do nothing | Wrong SOR or non-NV50 chip — check which SOR index the dmesg line reports. |
| Module loads but nouveau also bound | Shouldn't happen on the target machine; if it does, `rmmod` this module — two owners of the PWM will conflict. |
