obj-m += int0800.o

# Userspace helpers (not part of the kernel module):
#   make vdtest        - minimal VDPAU decoder-create probe
#   make nvif_delfix   - LD_PRELOAD shim for the mesa subchan_del fd bug
vdtest: vdtest.c
	$(CC) -o $@ $< -lvdpau -lX11

nvif_delfix.so: nvif_delfix.c
	$(CC) -shared -fPIC -O2 -o $@ $< -ldl

nvif_delfix: nvif_delfix.so

.PHONY: nvif_delfix
