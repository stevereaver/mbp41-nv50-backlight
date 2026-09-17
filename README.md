# mbp41-nv50-backlight

Two pieces for the MacBookPro4,1 (GeForce 8600M GT) booted under GRUB/EFI:

1. **`tools/extract_vbios.py`** — recovers the GPU's real VBIOS from the
   machine's own SPI flash. Feed it to nouveau via
   `nouveau.config=NvBios=nvidia/mbp41-8600mgt.rom` and you get full KMS,
   glamor-accelerated Xorg, and nouveau's native `nv_backlight`. This is
   the **preferred** configuration — verified working on kernel 7.1.8.
2. **`mbp_nv50_bl`** — a small out-of-tree kernel module that provides
   backlight control when booted with `nomodeset` (the fallback path, or
   machines where the VBIOS route isn't used). Registers
   `/sys/class/backlight/nvidia_backlight` so xfce4-power-manager, Fn
   keys, systemd-backlight, `brightnessctl`, etc. all work.

## Getting nouveau working (preferred)

The 8600M GT has no on-card ROM; the VBIOS is buried inside Apple's EFI
firmware in compressed FFS sections on the SPI flash:

```sh
# read the flash — either via the tiny read-only MTD driver (no reboot):
cd tools && make -C /lib/modules/$(uname -r)/build M=$PWD
sudo modprobe mtd map_rom && sudo insmod int0800.ko
sudo cat /dev/mtd0 > firmware_window.bin        # flash is at the top of the window
# ...or with flashrom (needs one boot with iomem=relaxed):
#   sudo flashrom -p internal -r firmware_window.bin

python3 -m venv venv && ./venv/bin/pip install uefi-firmware
./venv/bin/python tools/extract_vbios.py firmware_window.bin   # writes vbios_10de_0407.rom

sudo cp vbios_10de_0407.rom /usr/lib/firmware/nvidia/mbp41-8600mgt.rom
# include it in the initramfs (nouveau loads early), then:
#   kernel cmdline: drop nomodeset, add
#   nouveau.config=NvBios=nvidia/mbp41-8600mgt.rom module_blacklist=mbp_nv50_bl
```

Verified result: `bios: version 60.84.49.03.00`, nouveau DRM init,
`nouveaudrmfb` console, modesetting Xorg with glamor on NV84, LVDS-1 at
native 1440x900, `nv_backlight` functional. See
[`okf/vbios-extraction.md`](okf/vbios-extraction.md) for the full
procedure including the initramfs hook and GRUB setup. **Do not commit or
redistribute the extracted ROM** — it's NVIDIA/Apple copyrighted firmware.

## Why the kernel module exists

When running `nomodeset` (no VBIOS supplied), nouveau cannot bind and
**none** of the in-tree backlight drivers work:

| Approach | Why it fails |
|---|---|
| `nouveau` (`nvidia_backlight`) | Cannot probe the GPU — no VBIOS is readable from any source (PRAMIN/PROM/ACPI/PCIROM/PLATFORM all empty), `probe fails -22`. Works only if a VBIOS file is supplied — see above. |
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
