# NV84 VP2 video decode (VDPAU) — status and bugs

The NV84 (GeForce 8600M GT) has a VP2 video engine pair — BSP (H.264
bitstream) and VP (entropy/mocomp) — exposed to userspace as VDPAU.
Getting it actually working required chasing two independent bugs.

## Firmware

Two firmware layers are needed:

* Kernel engine init: `nouveau/nv84_xuc00f` + `nouveau/nv84_xuc103`
  (symlinks to `nv84_vp` / `nv84_bsp`).
* Userspace decoder firmware loaded by mesa at decoder-create time:
  `nv84_bsp-h264`, `nv84_vp-h264-1`, `nv84_vp-h264-2`,
  `nv84_vp-mpeg12`, `nv84_vp-vc1-{1,2,3}`.

All are extracted from NVIDIA driver 325.15 with envytools'
`extract_firmware.py` (see replication.md). Proprietary — do not
commit or redistribute.

## Bug A: mesa subchan_del uses a bogus fd (fixed by shim)

`src/gallium/winsys/nouveau/drm/nouveau.c`,
`nouveau_object_subchan_del()`:

```c
drmCommandWrite(obj->parent->handle, DRM_NOUVEAU_NVIF, &args, ...);
```

It passes `obj->parent->handle` — the channel's object handle (a small
integer like 9) — as the ioctl fd. The create path correctly uses
`nouveau_drm(obj->parent)->fd`. Result: every subchannel-object DEL
ioctl lands on a dead fd, returns EBADF, and is silently dropped.

Consequences:

* Kernel objects are never destroyed for the life of the fd.
* nvkm keys nvif objects by `args->new.object = (u64)userspace_ptr`.
  The next `calloc` that reuses a freed object address fails EEXIST.
* `nv84`'s `firmware_present()` probe creates+deletes handle-0 engine
  objects back-to-back; the second probe (BSP, class 0x74b0) collides
  → `BSP_KERN` bit never set → H.264 reported unsupported →
  `VDP_STATUS_INVALID_DECODER_PROFILE` on `vdp_decoder_create`.
* MPEG1/2 worked because they only need one probe object (VP 0x7476),
  no collision.

Present in mesa main as of 2026-09. One-line fix:

```c
drmCommandWrite(nouveau_drm(obj->parent)->fd, ...);
```

### Workaround: `tools/nvif_delfix.so`

LD_PRELOAD shim that intercepts `ioctl()`: when a `DRM_NOUVEAU_NVIF`
(nrf 0x47) ioctl arrives on an fd that fails `fcntl(F_GETFD)`, it
redirects the call to the process's real `/dev/dri/card*` fd (found
via /proc/self/fd). Usage:

```sh
LD_PRELOAD=./nvif_delfix.so NVIF_DELFIX_DEBUG=1 vdpauinfo
```

With the shim, `vdpauinfo` reports H264_BASELINE/MAIN/HIGH and
`vdp_decoder_create` succeeds for all supported profiles.

## Bug B: kernel regression — decode hangs engines on ≥ ~7.1

With decoder creation fixed, actual bitstream submission wedges both
engines on kernel 7.1.8:

* `fifo: CACHE_ERROR - ch N subc 2 mthd 0000 data beef74b0` — the
  legacy subchannel-object bind can't find the engine object
* `vp: Watchdog interrupt, engine hung` / `bsp: ...` — engines run
  without valid firmware/program state (binds failed) → watchdog
* `g84_ectx_bind` unbind timeout WARNING on teardown
* Desktop locks up; requires reboot.

The **same userspace works on kernel 6.5.0** (deb12): ffmpeg
`-hwaccel vdpau` reaches `pix_fmt: vdpau`, decodes to EOF, dmesg clean.

```
# works (6.5.0-0.deb12.1-amd64):
DISPLAY=:0 LD_PRELOAD=./nvif_delfix.so ffmpeg -hwaccel vdpau -i clip.mp4 -f null -
```

Likely suspect: the nvkm uchan/cctx/vctx rework (Ben Skeggs' nvif
reorganisation, ~6.7-6.10 era) changed how engine objects created on
legacy abi16 channels populate the channel RAMHT used by `mthd 0`
binds. Not yet root-caused to a commit — needs bisection between
6.5 and 7.1 (snapshot.debian.org kernels) or an upstream report.

Note: `-f null` benchmarks misleadingly show VDPAU slower than
software (55fps vs 300fps) because every frame is read back from
VRAM; real playback paths (GL interop) skip the readback.

## Test program

`tools/vdtest.c` — creates a VDPAU device via X11 and calls
`vdp_decoder_create` for H.264-high, MPEG2, MPEG1. Before the shim:
H.264 → INVALID_DECODER_PROFILE. With shim on 6.5: all OK.

## Browser reality check

Neither Chrome nor Firefox consumes VDPAU — they speak VA-API/V4L2
only. Nouveau/NV50 has no VA-API backend; `vdpau-va-driver` was
dropped from Debian years ago. A VA-API→VDPAU bridge (resurrecting
the abandoned libva-vdpau-driver approach) is the only path to
hardware-accelerated browser video on this GPU — and only worth
building after the kernel decode regression is fixed.
