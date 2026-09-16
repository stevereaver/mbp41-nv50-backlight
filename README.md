# mbp41-nv50-backlight

A small out-of-tree Linux kernel module that provides display backlight
control on NVIDIA-GPU MacBook Pros (verified on MacBookPro4,1, GeForce
8600M GT) booted under EFI via GRUB — where **none** of the in-tree
drivers work.

Registers `/sys/class/backlight/nvidia_backlight`, so standard desktop
tools work: xfce4-power-manager (panel slider + Fn keys), systemd-backlight
(save/restore across boots), GNOME/KDE sliders, `brightnessctl`, etc.

## Why this module exists

On this hardware under a GRUB/EFI boot:

| Approach | Why it fails |
|---|---|
| `nouveau` (`nvidia_backlight`) | Cannot probe the GPU — no VBIOS is readable from any source (PRAMIN/PROM/ACPI/PCIROM/PLATFORM all empty), `probe fails -22`. Machine must boot with `nomodeset`. |
| `apple_bl` (formerly `mbp_nvidia_bl`) | Drives backlight via SMI ports (`0x52e/0x52f` for NVIDIA-chipset models, `0xb2/0xb3` for Intel-chipset models). The SMI handler only exists under Apple's legacy BIOS/CSM boot — under EFI the ports are dead. Also gated behind `acpi_video_get_backlight_type() == acpi_backlight_vendor`, which is not selected on this machine. |
| `apple_gmux` | Requires the gmux display-mux chip found only on unibody MacBooks (MacBookPro5,1+, 2008 late and later). |
| `acpi_video` | The ACPI video device exposes no `_BCL`/`_BCM` under EFI. |

The panel backlight is actually driven by a PWM register in the GPU's
BAR0 — the same `NV50_PDISP_SOR_PWM_CTL` register nouveau uses. When
`nomodeset` keeps nouveau from binding, no driver owns the GPU, so this
module iomaps BAR0 and drives the PWM directly.

## Register interface (from nouveau)

- `PWM_CTL(i) = 0x61c084 + i * 0x800` per SOR (scan for the nonzero one;
  LVDS is SOR0 on the 8600M GT)
- Write `0x80000000 | duty` to set (bit 31 = "NEW" commit bit)
- Read back `& 0x7ff` = current duty
- Duty range 0–1025, mapped to brightness 0–100 (same as nouveau)

## Hardware compatibility

Written for and tested on **MacBookPro4,1** (2008, GeForce 8600M GT /
G84M, PCI `10de:0407`, GPU at `01:00.0`). Should in principle work on any
NV50-family MacBook that:

- boots under EFI (not BIOS/CSM),
- runs `nomodeset` because nouveau can't get a VBIOS, and
- has the panel LVDS on a SOR with a live PWM register.

The module verifies the device at `01:00.0` is an NVIDIA VGA controller
before touching it, and scans SORs 0–3 for a live PWM — so it fails
harmlessly (`-ENODEV`) on mismatched hardware.

## Install

```sh
sudo apt install dkms linux-headers-$(uname -r)
sudo cp -r . /usr/src/mbp-nvidia-bl-1.0
sudo dkms add -m mbp-nvidia-bl -v 1.0
sudo dkms build -m mbp-nvidia-bl -v 1.0
sudo dkms install -m mbp-nvidia-bl -v 1.0
echo mbp_nv50_bl | sudo tee /etc/modules-load.d/mbp-nvidia-bl.conf
sudo modprobe mbp_nv50_bl
```

Then check `/sys/class/backlight/nvidia_backlight` and restart your
desktop's power manager (e.g. `xfce4-power-manager --restart`) and panel
(`xfce4-panel -r`) if they were already running — they only enumerate
backlight devices at startup.

## Warnings

- **Do not use this alongside a bound nouveau.** The module pokes GPU
  registers unconditionally once loaded; if nouveau is bound to the GPU
  the two will fight over the PWM. Only use where nouveau cannot/will not
  bind (the whole point of the module).
- Brightness range is 0–100 mapped onto duty 0–1025; the relationship may
  not be perfectly linear and values near 0 can take the backlight nearly
  to off.
- Tested on kernel 7.1.8 (Debian trixie-backports). The API surface used
  (PCI iomap, backlight framework) is very stable, but this is out-of-tree
  code — kernel updates could break it.

## Documentation

- [`okf/`](okf/index.md) — Open Knowledge Format bundle documenting the
  hardware, register interface, dead ends, and install/verify steps.
- [`AGENTS.md`](AGENTS.md) — context and guidance for agentic/AI work on
  this repo.

## License

GPL-2.0 — same as the `mbp_nvidia_bl` code it is derived from.
