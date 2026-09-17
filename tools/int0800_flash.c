// SPDX-License-Identifier: GPL-2.0
/*
 * Read-only MTD access to the system firmware flash via the ACPI
 * INT0800 "82802 firmware hub" device. The flash contents are decoded
 * into the memory space below 4GB, so a plain ioremap is sufficient
 * for reads. No writes or erases are supported.
 *
 * The declared resource window may be larger than the real flash
 * (INT0800 commonly claims the whole top-16MB decode range); unmapped
 * holes simply read as 0xff.
 */
#include <linux/module.h>
#include <linux/acpi.h>
#include <linux/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/platform_device.h>

static struct map_info fw_map = {
	.name		= "int0800-flash",
	.bankwidth	= 1,
};

static struct mtd_info *fw_mtd;

static int int0800_flash_probe(struct platform_device *pdev)
{
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res) {
		fw_map.phys = res->start;
		fw_map.size = resource_size(res);
	} else {
		/* _CRS absent/empty: assume the usual top-2MiB decode */
		fw_map.phys = 0xffe00000;
		fw_map.size = 0x200000;
	}

	fw_map.virt = ioremap(fw_map.phys, fw_map.size);
	if (!fw_map.virt)
		return -ENOMEM;
	simple_map_init(&fw_map);
	fw_mtd = do_map_probe("map_rom", &fw_map);
	if (!fw_mtd) {
		iounmap(fw_map.virt);
		return -ENODEV;
	}
	fw_mtd->dev.parent = &pdev->dev;
	return mtd_device_register(fw_mtd, NULL, 0);
}

static void int0800_flash_remove(struct platform_device *pdev)
{
	mtd_device_unregister(fw_mtd);
	map_destroy(fw_mtd);
	iounmap(fw_map.virt);
}

static const struct acpi_device_id int0800_flash_ids[] = {
	{ "INT0800", 0 },
	{ }
};
MODULE_DEVICE_TABLE(acpi, int0800_flash_ids);

static struct platform_driver int0800_flash_driver = {
	.probe	= int0800_flash_probe,
	.remove	= int0800_flash_remove,
	.driver	= {
		.name		= "int0800-flash",
		.acpi_match_table = int0800_flash_ids,
	},
};
module_platform_driver(int0800_flash_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Read-only MTD map over the INT0800 firmware flash");
