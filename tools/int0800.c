// SPDX-License-Identifier: GPL-2.0-only
/*
 * Read-only MTD access to the system firmware flash behind the ACPI
 * INT0800 "Intel 82802 firmware hub" device.
 *
 * On x86 systems the boot flash is decoded into the physical address
 * space below 4 GB, so a plain ioremap() is sufficient to read it -
 * no SPI or LPC controller access is required.  The declared _CRS
 * window may be larger than the real flash (the whole top-16MiB
 * decode range is commonly claimed); undecoded holes read as 0xff.
 *
 * The device is exposed read-only via the ROM chip driver; there is
 * deliberately no write or erase support.
 */

#include <linux/module.h>
#include <linux/acpi.h>
#include <linux/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/platform_device.h>

/* top-of-4GB firmware decode, used when _CRS reports no window */
#define INT0800_DEFAULT_PHYS	0xffe00000UL
#define INT0800_DEFAULT_SIZE	SZ_2M

static struct map_info int0800_map = {
	.name		= "int0800",
	.bankwidth	= 1,
};

static struct mtd_info *int0800_mtd;

static int int0800_probe(struct platform_device *pdev)
{
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res) {
		int0800_map.phys = res->start;
		int0800_map.size = resource_size(res);
	} else {
		int0800_map.phys = INT0800_DEFAULT_PHYS;
		int0800_map.size = INT0800_DEFAULT_SIZE;
	}

	/*
	 * Plain ioremap on purpose: the window is already claimed by the
	 * ACPI/pnp resource reservation, so devm_ioremap_resource() would
	 * fail with -EBUSY.
	 */
	int0800_map.virt = ioremap(int0800_map.phys, int0800_map.size);
	if (!int0800_map.virt)
		return -ENOMEM;

	simple_map_init(&int0800_map);
	int0800_mtd = do_map_probe("map_rom", &int0800_map);
	if (!int0800_mtd) {
		iounmap(int0800_map.virt);
		return -ENODEV;
	}
	int0800_mtd->dev.parent = &pdev->dev;

	dev_info(&pdev->dev, "mapped firmware window 0x%lx-0x%lx\n",
		 int0800_map.phys,
		 int0800_map.phys + int0800_map.size - 1);

	return mtd_device_register(int0800_mtd, NULL, 0);
}

static void int0800_remove(struct platform_device *pdev)
{
	mtd_device_unregister(int0800_mtd);
	map_destroy(int0800_mtd);
	iounmap(int0800_map.virt);
}

static const struct acpi_device_id int0800_ids[] = {
	{ "INT0800", 0 },
	{ }
};
MODULE_DEVICE_TABLE(acpi, int0800_ids);

static struct platform_driver int0800_driver = {
	.probe	= int0800_probe,
	.remove	= int0800_remove,
	.driver	= {
		.name		= "int0800",
		.acpi_match_table = int0800_ids,
	},
};
module_platform_driver(int0800_driver);

MODULE_AUTHOR("Devin");
MODULE_DESCRIPTION("Read-only MTD map over the INT0800 firmware flash window");
MODULE_LICENSE("GPL");
