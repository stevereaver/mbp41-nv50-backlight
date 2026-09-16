# AGENTS.md

## What this repo is

`mbp_nv50_bl` — a small out-of-tree Linux kernel module providing
`/sys/class/backlight/nvidia_backlight` on EFI-booted NVIDIA MacBook Pros
(verified: MacBookPro4,1 / GeForce 8600M GT / kernel 7.1.8) where no
in-tree driver works. It iomaps the GPU's BAR0 and drives the panel
backlight PWM register directly — the same register nouveau uses.

## Read first

- [`okf/index.md`](okf/index.md) — OKF knowledge bundle: hardware context,
  why stock drivers fail, the register interface, install/verify runbook.
  Treat it as the source of truth for the "why" behind every design
  decision here.
- [`README.md`](README.md) — user-facing overview and install steps.

## Repo layout

- `mbp_nv50_bl.c` — the entire driver (~135 lines)
- `Makefile` — kbuild entry (`obj-m`)
- `dkms.conf` — DKMS packaging (`AUTOINSTALL=yes`)
- `okf/` — OKF knowledge bundle; **update it when you change the code**

## Conventions

- Kernel C style (tabs, kerneldoc-lite comments). Keep the driver minimal —
  it should stay understandable in one screen.
- The register/scale contract (0–100 brightness ↔ 0–1025 duty,
  `PWM_CTL_NEW` commit bit) must match nouveau's nv50 backlight ops —
  don't "improve" the semantics without a hardware-verified reason.
- Keep the probe conservative: verify NVIDIA VGA at `01:00.0` and require
  a nonzero `PWM_CTL` before registering. The module must fail harmlessly
  on non-target hardware.

## Build / test / verify

On the target machine (SSH: `alexw@192.168.10.176`, passwordless sudo):

```sh
make -C /lib/modules/$(uname -r)/build M=$PWD
sudo insmod ./mbp_nv50_bl.ko
ls /sys/class/backlight/                       # expect nvidia_backlight
echo 50 | sudo tee /sys/class/backlight/nvidia_backlight/brightness
dmesg | tail                                   # "registered (SOR0, duty N)"
```

There is no CI and no automated test suite — verification is empirical on
the target laptop. The user has local console access if a bad change
wedges the display; the register write only affects backlight PWM and
cannot hang the GPU, but keep BAR0 writes limited to `PWM_CTL` exactly.

## Known constraints / don'ts

- **Never load alongside a bound nouveau** — two owners of the PWM
  conflict. The target machine can never bind nouveau (no VBIOS), but
  guard docs/comments accordingly.
- The SMI-port approach (`apple_bl`/`mbp_nvidia_bl` style, ports
  `0x52e/0x52f` or `0xb2/0xb3`) is verified dead under EFI — don't
  reintroduce it.
- Not upstreamable as-is (touches another driver's device without
  coordination). The upstream-acceptable path would be a nouveau patch
  that registers the PWM backlight even when VBIOS init fails — see
  `okf/log.md` and `okf/dead-approaches.md`.
- If you change behavior or install steps, update both `README.md` and
  the relevant `okf/*.md` files (and append to `okf/log.md`).
