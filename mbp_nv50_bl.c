// SPDX-License-Identifier: GPL-2.0-only
/*
 * Backlight driver for EFI-booted NVIDIA-GPU MacBook Pros (MacBookPro4,1)
 * where nouveau cannot bind (no VBIOS under GRUB/EFI).
 *
 * The SMI-based mechanism used by mbp_nvidia_bl/apple_bl is dead under EFI
 * (no SMI handler installed), but the panel backlight PWM register in the
 * GPU's BAR0 still responds - this is the same register nouveau uses
 * (NV50_PDISP_SOR_PWM_CTL). With nomodeset, no driver owns the GPU, so we
 * iomap BAR0 and drive the PWM directly.
 *
 * Duty range matches nouveau's nv50 backlight ops: 0..1025.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/pci.h>

#define PWM_DIV(i)	(0x0061c080 + (i) * 0x800)
#define PWM_CTL(i)	(0x0061c084 + (i) * 0x800)
#define PWM_CTL_NEW	0x80000000
#define PWM_CTL_VAL	0x000007ff
#define PWM_MAX_DUTY	1025
#define MAX_SOR		4

static struct backlight_device *bldev;
static struct pci_dev *gpu;
static void __iomem *bar0;
static u32 ctl_reg;

static void nv50_bl_write(u32 val)
{
	writel(PWM_CTL_NEW | val, bar0 + ctl_reg);
}

static u32 nv50_bl_read(void)
{
	return readl(bar0 + ctl_reg) & PWM_CTL_VAL;
}

static int nv50_bl_send(struct backlight_device *bd)
{
	nv50_bl_write(bd->props.brightness * PWM_MAX_DUTY / 100);
	return 0;
}

static int nv50_bl_get(struct backlight_device *bd)
{
	/* brightness 0..100 <-> duty 0..1025, same as nouveau */
	return (nv50_bl_read() * 100 + PWM_MAX_DUTY / 2) / PWM_MAX_DUTY;
}

static const struct backlight_ops nv50_bl_ops = {
	.options	= BL_CORE_SUSPENDRESUME,
	.get_brightness	= nv50_bl_get,
	.update_status	= nv50_bl_send,
};

static int __init nv50_bl_init(void)
{
	struct backlight_properties props;
	int i;

	/* GPU is at fixed slot 01:00.0 on the MBP4,1; verify it's NVIDIA VGA */
	gpu = pci_get_domain_bus_and_slot(0, 1, 0);
	if (!gpu)
		return -ENODEV;

	if (gpu->vendor != PCI_VENDOR_ID_NVIDIA ||
	    (gpu->class >> 8) != PCI_CLASS_DISPLAY_VGA) {
		pci_dev_put(gpu);
		return -ENODEV;
	}

	if (pci_enable_device(gpu) < 0) {
		pci_dev_put(gpu);
		return -ENODEV;
	}

	bar0 = pci_iomap(gpu, 0, 0);
	if (!bar0) {
		pci_dev_put(gpu);
		return -ENODEV;
	}

	/* find the SOR with a live PWM (LVDS is SOR0 on the 8600M GT) */
	for (i = 0; i < MAX_SOR; i++) {
		if (readl(bar0 + PWM_CTL(i))) {
			ctl_reg = PWM_CTL(i);
			break;
		}
	}
	if (!ctl_reg) {
		pci_iounmap(gpu, bar0);
		pci_dev_put(gpu);
		return -ENODEV;
	}

	memset(&props, 0, sizeof(props));
	props.type = BACKLIGHT_RAW;
	props.max_brightness = 100;

	bldev = backlight_device_register("nvidia_backlight", NULL, NULL,
					  (void *)&nv50_bl_ops, &props);
	if (IS_ERR(bldev)) {
		pci_iounmap(gpu, bar0);
		pci_dev_put(gpu);
		return PTR_ERR(bldev);
	}

	bldev->props.brightness = nv50_bl_get(bldev);
	backlight_update_status(bldev);

	pr_info("registered (SOR%d, duty %u)\n",
		(ctl_reg - PWM_CTL(0)) / 0x800, nv50_bl_read());
	return 0;
}

static void __exit nv50_bl_exit(void)
{
	backlight_device_unregister(bldev);
	pci_iounmap(gpu, bar0);
	pci_dev_put(gpu);
}

module_init(nv50_bl_init);
module_exit(nv50_bl_exit);

MODULE_AUTHOR("Devin / derived from nouveau NV50 backlight code");
MODULE_DESCRIPTION("MacBook Pro 4,1 NVIDIA backlight driver (EFI boot)");
MODULE_LICENSE("GPL");
