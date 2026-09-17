/*
 * nvif_delfix - LD_PRELOAD workaround for mesa nouveau winsys bug
 *
 * nouveau_object_subchan_del() in src/gallium/winsys/nouveau/drm/nouveau.c
 * calls drmCommandWrite(obj->parent->handle, DRM_NOUVEAU_NVIF, ...) passing
 * the channel's object *handle* as the ioctl fd. The DEL ioctl lands on a
 * dead fd (EBADF), is silently dropped, and the kernel nvif object leaks.
 * Since the object is keyed by its userspace malloc address, the next
 * allocation that reuses the address fails with EEXIST.
 *
 * Visible symptom: vdpauinfo/nv84 firmware probes create+delete engine
 * objects with handle 0 back-to-back; the second probe's create collides
 * with the leaked first -> H.264 (BSP) reported unsupported.
 *
 * This shim redirects nouveau NVIF ioctls issued on a dead fd to the
 * process's real DRM fd.
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define DRM_IOCTL_BASE     'd'   /* 0x64 */
#define DRM_NOUVEAU_NVIF   0x47

static int drm_fd = -2; /* -2 = unprobed, -1 = not found */

static int
find_drm_fd(void)
{
    if (drm_fd != -2)
        return drm_fd;
    drm_fd = -1;
    DIR *d = opendir("/proc/self/fd");
    if (!d)
        return drm_fd;
    struct dirent *e;
    char path[64], link[256];
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.')
            continue;
        int f = atoi(e->d_name);
        if (f == dirfd(d))
            continue;
        snprintf(path, sizeof(path), "/proc/self/fd/%d", f);
        ssize_t n = readlink(path, link, sizeof(link) - 1);
        if (n > 0) {
            link[n] = '\0';
            if (!strncmp(link, "/dev/dri/card", 13)) {
                drm_fd = f;
                break;
            }
        }
    }
    closedir(d);
    return drm_fd;
}

int
ioctl(int fd, unsigned long request, ...)
{
    static int (*real_ioctl)(int, unsigned long, void *) = NULL;
    va_list ap;
    void *argp;

    if (!real_ioctl)
        real_ioctl = dlsym(RTLD_NEXT, "ioctl");
    va_start(ap, request);
    argp = va_arg(ap, void *);
    va_end(ap);

    if (_IOC_TYPE(request) == DRM_IOCTL_BASE &&
        _IOC_NR(request) == DRM_NOUVEAU_NVIF &&
        fcntl(fd, F_GETFD) == -1 && errno == EBADF) {
        int real = find_drm_fd();
        if (real >= 0) {
            if (getenv("NVIF_DELFIX_DEBUG"))
                fprintf(stderr, "nvif_delfix: nvif ioctl on dead fd %d -> fd %d\n",
                        fd, real);
            fd = real;
        }
    }
    return real_ioctl(fd, request, argp);
}
