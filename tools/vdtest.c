#include <stdio.h>
#include <X11/Xlib.h>
#include <vdpau/vdpau.h>
#include <vdpau/vdpau_x11.h>

static const char *status_str(VdpStatus s) {
    switch (s) {
    case VDP_STATUS_OK: return "OK";
    case VDP_STATUS_INVALID_DECODER_PROFILE: return "INVALID_DECODER_PROFILE";
    case VDP_STATUS_ERROR: return "ERROR";
    case VDP_STATUS_RESOURCES: return "RESOURCES";
    case VDP_STATUS_NO_IMPLEMENTATION: return "NO_IMPLEMENTATION";
    case VDP_STATUS_INVALID_HANDLE: return "INVALID_HANDLE";
    case VDP_STATUS_DISPLAY_PREEMPTED: return "DISPLAY_PREEMPTED";
    default: return "OTHER";
    }
}

int main(void) {
    setbuf(stdout, NULL);
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "no display\n"); return 1; }

    VdpDevice dev;
    VdpGetProcAddress *getpa = NULL;
    VdpStatus st = vdp_device_create_x11(dpy, 0, &dev, &getpa);
    printf("device_create: %d %s getpa=%p\n", st, status_str(st), (void*)getpa);
    if (st || !getpa) return 1;

    VdpDecoderCreate *dec_create = NULL;
    st = getpa(dev, VDP_FUNC_ID_DECODER_CREATE, (void **)&dec_create);
    printf("getproc dec_create: %d %s ptr=%p\n", st, status_str(st), (void*)dec_create);
    if (!dec_create) return 1;

    VdpDecoder dec = 0;
    st = dec_create(dev, VDP_DECODER_PROFILE_H264_HIGH, 1280, 720, 16, &dec);
    printf("dec_create h264_high: %d %s dec=%u\n", st, status_str(st), dec);

    st = dec_create(dev, VDP_DECODER_PROFILE_MPEG2_MAIN, 720, 480, 16, &dec);
    printf("dec_create mpeg2: %d %s dec=%u\n", st, status_str(st), dec);

    st = dec_create(dev, VDP_DECODER_PROFILE_MPEG1, 720, 480, 16, &dec);
    printf("dec_create mpeg1: %d %s dec=%u\n", st, status_str(st), dec);
    return 0;
}
