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

## 2026-09-17 — VBIOS extracted, nouveau works

- Dumped the 2 MiB SPI flash (SST25VF016B) with flashrom under
  `iomem=relaxed`; parsed FFS with `uefi-firmware` and found the VBIOS
  inside a compressed `GuidDefinedSection` (FFS file
  `b0cd1bfc-317d-aa49-936a-a4600d9dd083`): `55AA`+`PCIR`, `10de:0407`,
  53248 B, version `60.84.49.03.00`, checksum valid.
- Installed it as `/usr/lib/firmware/nvidia/mbp41-8600mgt.rom`, added an
  initramfs hook, booted with
  `nouveau.config=NvBios=nvidia/mbp41-8600mgt.rom` and no `nomodeset`:
  nouveau bound cleanly, `nouveaudrmfb` console, Xorg modesetting +
  glamor on NV84, LVDS 1440x900 native, `nv_backlight` registered and
  verified.
- Made permanent: default GRUB cmdline carries the `NvBios=` path and
  `module_blacklist=mbp_nv50_bl`; a `nomodeset` "fbdev fallback" GRUB
  entry preserves the old working path (where `mbp_nv50_bl` still
  provides backlight). The ROM itself is not committed (copyrighted
  firmware); `tools/extract_vbios.py` reproduces it from a flash dump.
- Added a bound-driver check to `mbp_nv50_bl` (`pdev->driver` →
  `-EBUSY`) so it can never poke BAR0 while nouveau owns the GPU.
- Investigated generalizing the fix: the firmware flash is decoded into
  physical memory (`INT0800` window `ff000000–ffffffff`; the 2MiB flash
  at `0xffe00000`) and is readable via plain `ioremap` — no SPI driver
  needed (Debian doesn't build `spi_intel` anyway, and `lpc_ich` creates
  no SPI child on ICH8M). Verified with a read-only `map_rom` MTD map
  driver: dump byte-identical to flashrom (modulo live NVRAM churn in
  the variable store). `tools/int0800.c` binds the ACPI INT0800
  device generically — candidate for linux-mtd upstreaming.
  `ichxrom` exists in-tree but only covers ICH4/5-era southbridges and
  is unmaintained.
- Prepared and sent `mtd: maps: add INT0800 firmware-flash map driver`
  to linux-mtd@lists.infradead.org (Cc: MTD maintainers). The submitted
  patch lives in the `linux-upstream` worktree; `tools/int0800.c`
  matches it exactly.
- v1 got an automated review (Sashiko AI) flagging three real issues:
  globals instead of per-device state, a map/mapping leak on
  `mtd_device_register` failure, and `%lx` on `resource_size_t`. v2 sent
  with all three fixed (devm-managed per-device struct, `map_destroy`
  error path, `%pa`) plus the missing `Signed-off-by`.
  Patchwork: https://patchwork.ozlabs.org/project/linux-mtd/list/?q=int0800
- Drafted and submitted a nouveau-wiki update documenting VBIOS
  extraction on EFI-booted Apple machines:
  https://gitlab.freedesktop.org/nouveau/wiki/-/merge_requests/63
  (draft text kept at `docs/wiki-DumpingVideoBios-addition.mdwn`).
- Replicated the full setup on a second MBP4,1 (same
  `MBP41.88Z.00C1.B03` firmware): in-place Bookworm→Trixie upgrade,
  reused the extracted VBIOS (checksum-verified), nouveau bound on
  first 7.1.8 boot with glamor + `nv_backlight`. The packaged
  `broadcom-sta` DKMS driver no longer builds on trixie kernels, so
  the BCM4321 WiFi moved to in-kernel `b43` +
  `firmware-b43-installer` — seamless. See
  [replication](replication.md) "Upgrading a Bookworm install in
  place".
- Graphics improvements pass on second machine: extracted the NV84
  VP2 video-engine firmware from NVIDIA 325.15
  (`envytools/firmware` `extract_firmware.py`), installed to
  `/lib/firmware/nouveau` + initramfs. `vp:`/`bsp:` init failures
  gone; `vdpauinfo` reports VDPAU feature set A (H.264, MPEG1/2) and
  an `ffmpeg -hwaccel vdpau` decode produced real `vdpau` frames.
  Reclocking confirmed impossible on this VBIOS (`pstate` write →
  EIO; `nvbios_pll_parse` fails — no PLL limits entries), and
  `vdpau-va-driver` no longer exists in trixie. Details in
  [replication](replication.md).
