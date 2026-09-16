---
type: Register Interface
title: NV50 panel backlight PWM registers
description: BAR0 register interface used by nouveau and this module to drive the LVDS backlight.
tags: [register, nouveau, pwm, nv50]
timestamp: 2026-09-17T00:00:00Z
---

# NV50 backlight PWM (per-SOR)

Defined in `drivers/gpu/drm/nouveau/nouveau_reg.h`:

```c
#define NV50_PDISP_SOR_PWM_DIV(i)   (0x0061c080 + (i) * 0x800)
#define NV50_PDISP_SOR_PWM_CTL(i)   (0x0061c084 + (i) * 0x800)
#define NV50_PDISP_SOR_PWM_CTL_NEW  0x80000000
#define NV50_PDISP_SOR_PWM_CTL_VAL  0x000007ff   /* NV50; NVA3+ uses 0x00ffffff */
```

## Semantics (from nouveau's nv50 backlight ops)

- `DIV` — PWM divisor programmed by firmware (read 0x3e = 62 on MBP4,1;
  nouveau ignores it and treats the duty scale as fixed).
- `CTL` — bit 31 `NEW` commits a new duty on write; bits 10:0 `VAL` are
  the duty value, range 0–1025.
- **Set:** `writel(NEW | duty, CTL)` where `duty = brightness * 1025 / 100`.
- **Get:** `readl(CTL) & 0x7ff` → `brightness = duty * 100 / 1025`.
- nouveau picks the SOR from the encoder's DCB output-resource index.
  Without a VBIOS we can't parse the DCB, so the module scans SORs 0–3 and
  uses the first one whose CTL reads nonzero (LVDS is SOR0 on the 8600M GT).

## Access path

With `nomodeset`, no driver binds `01:00.0`, so BAR0 is unclaimed:

- **Kernel:** `pci_get_domain_bus_and_slot(0, 1, 0)` →
  `pci_enable_device()` → `pci_iomap(pdev, 0, 0)` → `readl`/`writel`.
- **Userspace (testing):** `mmap` of
  `/sys/bus/pci/devices/0000:01:00.0/resource0` as root.

## Verified on hardware

| Operation | Result |
|---|---|
| Read `SOR0` CTL | `0xdc` (duty 220) — panel visibly lit |
| Write `NEW \| 60` | screen dims slightly; reads back 60 |
| Write `NEW \| 15` | screen goes nearly dark; restore works |
| Read `SOR1` CTL | `0x0` — no PWM there, correct to skip |

So a duty of ~220 corresponds to a normal-looking brightness level; 1025
is full. The mapping is treated as linear 0–1025 → 0–100, matching
nouveau.

## Safety notes

- Only write `NEW | (0..1025)` to `CTL`. Do not touch `DIV` or other
  registers — arbitrary BAR0 writes can glitch the GPU.
- The write only affects the backlight PWM; it is orthogonal to the
  framebuffer and safe while efifb/Xorg are running.
- Never load this while nouveau is bound — see
  [dead-approaches](dead-approaches.md) for why nouveau can't bind on the
  target machine anyway.
