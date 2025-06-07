// SPDX-License-Identifier: GPL-2.0+
/*
 * OF_LIVE devicetree fixup.
 *
 * This file implements runtime fixups for Qualcomm DT to improve
 * compatibility with U-Boot. This includes adjusting the USB nodes
 * to only use USB high-speed, as well as remapping volume buttons
 * to behave as up/down for navigating U-Boot.
 *
 * We use OF_LIVE for this rather than early FDT fixup for a couple
 * of reasons: it has a much nicer API, is most likely more efficient,
 * and our changes are only applied to U-Boot. This allows us to use a
 * DT designed for Linux, run U-Boot with a modified version, and then
 * boot Linux with the original FDT.
 *
 * Copyright (c) 2024 Linaro Ltd.
 *   Author: Caleb Connolly <caleb.connolly@linaro.org>
 */

#define pr_fmt(fmt) "of_fixup: " fmt

#include <dt-bindings/input/linux-event-codes.h>
#include <dm/of_access.h>
#include <dm/of.h>
#include <fdt_support.h>
#include <linux/errno.h>
#include <stdlib.h>
#include <time.h>
#include <efi_loader.h>
#include <efi_api.h>
#include "qcom-priv.h"

static u64 get_ram_size_bytes(void)
{
	struct bd_info *bd = gd->bd;
	u64 total_size = 0;
	int i;

	for (i = 0; i < CONFIG_NR_DRAM_BANKS && bd->bi_dram[i].size; i++) {
		total_size += bd->bi_dram[i].size;
	}

	return total_size;
}

bool is_retroid_pocketmini(void)
{
	u64 ram_size = get_ram_size_bytes();
	return (ram_size < 7ULL * 1024 * 1024 * 1024); // Less than 7GB threshold
}

bool is_retroid_pocketmini_v2(void) {
	static const uint8_t splash_pattern[288] = {
		// Pattern data
		0x00, 0x00, 0x00, 0x00, 0xF0, 0xFB, 0xFF, 0x00, 0xF0, 0xFB, 0xFF, 0x00, 0xF0, 0xFB, 0xFF, 0x00,
		0xF0, 0xFB, 0xFF, 0x00, 0xF0, 0xFB, 0xFF, 0x00, 0xF0, 0xFB, 0xFF, 0x00, 0xF0, 0xFB, 0xFF, 0x00,
		0xF0, 0xFB, 0xFF, 0x00, 0xF0, 0xFB, 0xFF, 0x00, 0xF0, 0xFB, 0xFF, 0x00, 0xF0, 0xFB, 0xFF, 0x00,
		0xF0, 0xFB, 0xFF, 0x00, 0xF0, 0xFB, 0xFF, 0x00, 0xF0, 0xFB, 0xFF, 0x00, 0xF0, 0xFB, 0xFF, 0x00,
		0xF0, 0xFB, 0xFF, 0x00, 0xF0, 0xFB, 0xFF, 0x00, 0xF0, 0xFB, 0xFF, 0x00, 0xF0, 0xFB, 0xFF, 0x00,
		0xF0, 0xFB, 0xFF, 0x00, 0xF0, 0xFB, 0xFF, 0x00, 0xF0, 0xFB, 0xFF, 0x00, 0xF0, 0xFB, 0xFF, 0x00,
		0xF0, 0xFB, 0xFF, 0x00, 0xF0, 0xFB, 0xFF, 0x00, 0xF0, 0xFB, 0xFF, 0x00, 0xF0, 0xFB, 0xFF, 0x00,
		0xF0, 0xFB, 0xFF, 0x00, 0xF0, 0xFB, 0xFF, 0x00, 0xF0, 0xFB, 0xFF, 0x00, 0xF0, 0xFB, 0xFF, 0x00,
		0xF0, 0xFB, 0xFF, 0x00, 0xF0, 0xFB, 0xFF, 0x00, 0xF0, 0xFB, 0xFF, 0x00, 0xF0, 0xFB, 0xFF, 0x00,
		0xF0, 0xFB, 0xFF, 0x00, 0xF0, 0xFB, 0xFF, 0x00, 0xF0, 0xFB, 0xFF, 0x00, 0xF0, 0xFB, 0xFF, 0x00,
		0xF0, 0xFB, 0xFF, 0x00, 0xF0, 0xFB, 0xFF, 0x00, 0xF0, 0xFB, 0xFF, 0x00, 0xF0, 0xFB, 0xFF, 0x00,
		0xF0, 0xFB, 0xFF, 0x00, 0xF0, 0xFB, 0xFF, 0x00, 0xF0, 0xFB, 0xFF, 0x00, 0xF0, 0xFB, 0xFF, 0x00,
		0xF0, 0xFB, 0xFF, 0x00, 0xF0, 0xFB, 0xFF, 0x00, 0xF0, 0xFB, 0xFF, 0x00, 0xF0, 0xFB, 0xFF, 0x00,
		0xF0, 0xFB, 0xFF, 0x00, 0xF0, 0xFB, 0xFF, 0x00, 0xF0, 0xFB, 0xFF, 0x00, 0xF0, 0xFB, 0xFF, 0x00,
		0xF0, 0xFB, 0xFF, 0x00, 0xF0, 0xFB, 0xFF, 0x00, 0xF0, 0xFB, 0xFF, 0x00, 0xF0, 0xFB, 0xFF, 0x00,
		0xF0, 0xFB, 0xFF, 0x00, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};

	const uint8_t *mem_ptr = (const uint8_t *)0x9c396060;
	return (memcmp(mem_ptr, splash_pattern, sizeof(splash_pattern)) == 0);
}

static int fixup_retroid_fdt(void *blob)
{
	int nodeoff;
	int ret;
	efi_status_t efi_ret;

	const char *model;
	const char *compatible;
	const char *panel;
	uint32_t width, height, stride;

	/* Ensure FDT has enough space for modifications */
	ret = fdt_increase_size(blob, 512); /* Add 512 bytes of padding */
	if (ret < 0) {
		printf("Failed to increase FDT size\n");
		return -1;
	}

	/* Get root node first */
	nodeoff = fdt_path_offset(blob, "/");
	if (nodeoff < 0) {
		printf("ERROR: Cannot find root node\n");
		return -1;
	}

	/* Set rocknix,u-boot property */
	ret = fdt_setprop_empty(blob, nodeoff, "rocknix,u-boot");
	if (ret < 0) {
		printf("ERROR: Failed to set rocknix,u-boot property\n");
		return -1;
	}

	if (is_retroid_pocketmini()) {
		if (!is_retroid_pocketmini_v2()) {
			model = "Retroid Pocket Mini";
			compatible = "retroidpocket,rpmini\0qcom,sm8250";
			panel = "ch13726a,rpmini";
			width = 0x3c0;   // 960
			height = 0x500;  // 1280
			stride = 0xf00;  // 3840
		} else {
			model = "Retroid Pocket Mini V2";
			compatible = "retroidpocket,rpminiv2\0qcom,sm8250";
			panel = "ch13726a,rpminiv2";
			width = 0x438;   // 1080
			height = 0x4d8;  // 1240
			stride = 0x10e0;  // 4320
		}

		/* Set model and compatible properties */
		ret = fdt_setprop_string(blob, nodeoff, "model", model);
		if (ret < 0) {
			printf("ERROR: Failed to set model property\n");
			return -1;
		}

		ret = fdt_setprop(blob, nodeoff, "compatible", compatible, sizeof(compatible));
		if (ret < 0) {
			printf("ERROR: Failed to set compatible property\n");
			return -1;
		}

		/* Framebuffer modifications */
		nodeoff = fdt_path_offset(blob, "/chosen/framebuffer@9c000000");
		if (nodeoff >= 0) {
			ret = fdt_setprop_u32(blob, nodeoff, "width", width);
			ret |= fdt_setprop_u32(blob, nodeoff, "height", height);
			ret |= fdt_setprop_u32(blob, nodeoff, "stride", stride);
			if (ret < 0) {
				printf("ERROR: Failed to modify framebuffer properties\n");
				return -1;
			}
		}

		/* Touchscreen modifications */
		nodeoff = fdt_path_offset(blob, "/soc@0/geniqup@ac0000/i2c@a94000/touchscreen@38");
		if (nodeoff >= 0) {
			ret = fdt_setprop_u32(blob, nodeoff, "touchscreen-size-x", width);
			ret |= fdt_setprop_u32(blob, nodeoff, "touchscreen-size-y", height);
			ret |= fdt_delprop(blob, nodeoff, "touchscreen-inverted-x");
			ret |= fdt_delprop(blob, nodeoff, "touchscreen-inverted-y");
			if (ret < 0) {
				printf("ERROR: Failed to modify touchscreen properties\n");
				return -1;
			}
		}

		/* Panel modifications */
		nodeoff = fdt_path_offset(blob, "/soc@0/display-subsystem@ae00000/dsi@ae94000/panel@0");
		if (nodeoff >= 0) {
			ret = fdt_setprop_string(blob, nodeoff, "compatible", panel);
			ret |= fdt_setprop_u32(blob, nodeoff, "rotation", 0x5a);
			if (ret < 0) {
				printf("ERROR: Failed to modify panel properties\n");
				return -1;
			}
		}

		/* After all modifications are done, install the modified DTB in the EFI configuration table */
		efi_ret = efi_install_configuration_table(&efi_guid_fdt, blob);
		if (efi_ret != EFI_SUCCESS) {
			printf("ERROR: Failed to install FDT in configuration table: %ld\n", efi_ret);
			return efi_ret;
		}
	}

	return 0;
}

/* U-Boot only supports USB high-speed mode on Qualcomm platforms with DWC3
 * USB controllers. Rather than requiring source level DT changes, we fix up
 * DT here. This improves compatibility with upstream DT and simplifies the
 * porting process for new devices.
 */
static int fixup_qcom_dwc3(struct device_node *glue_np)
{
	struct device_node *dwc3;
	int ret, len, hsphy_idx = 1;
	const __be32 *phandles;
	const char *second_phy_name;

	debug("Fixing up %s\n", glue_np->name);

	/* Tell the glue driver to configure the wrapper for high-speed only operation */
	ret = of_write_prop(glue_np, "qcom,select-utmi-as-pipe-clk", 0, NULL);
	if (ret) {
		log_err("Failed to add property 'qcom,select-utmi-as-pipe-clk': %d\n", ret);
		return ret;
	}

	/* Find the DWC3 node itself */
	dwc3 = of_find_compatible_node(glue_np, NULL, "snps,dwc3");
	if (!dwc3) {
		log_err("Failed to find dwc3 node\n");
		return -ENOENT;
	}

	phandles = of_get_property(dwc3, "phys", &len);
	len /= sizeof(*phandles);
	if (len == 1) {
		log_debug("Only one phy, not a superspeed controller\n");
		return 0;
	}

	/* Figure out if the superspeed phy is present and if so then which phy is it? */
	ret = of_property_read_string_index(dwc3, "phy-names", 1, &second_phy_name);
	if (ret == -ENODATA) {
		log_debug("Only one phy, not a super-speed controller\n");
		return 0;
	} else if (ret) {
		log_err("Failed to read second phy name: %d\n", ret);
		return ret;
	}

	if (!strncmp("usb3-phy", second_phy_name, strlen("usb3-phy"))) {
		log_debug("Second phy isn't superspeed (is '%s') assuming first phy is SS\n",
			  second_phy_name);
		hsphy_idx = 0;
	}

	/* Overwrite the "phys" property to only contain the high-speed phy */
	ret = of_write_prop(dwc3, "phys", sizeof(*phandles), phandles + hsphy_idx);
	if (ret) {
		log_err("Failed to overwrite 'phys' property: %d\n", ret);
		return ret;
	}

	/* Overwrite "phy-names" to only contain a single entry */
	ret = of_write_prop(dwc3, "phy-names", strlen("usb2-phy"), "usb2-phy");
	if (ret) {
		log_err("Failed to overwrite 'phy-names' property: %d\n", ret);
		return ret;
	}

	ret = of_write_prop(dwc3, "maximum-speed", strlen("high-speed"), "high-speed");
	if (ret) {
		log_err("Failed to set 'maximum-speed' property: %d\n", ret);
		return ret;
	}

	return 0;
}

static void fixup_usb_nodes(void)
{
	struct device_node *glue_np = NULL;
	int ret;

	while ((glue_np = of_find_compatible_node(glue_np, NULL, "qcom,dwc3"))) {
		ret = fixup_qcom_dwc3(glue_np);
		if (ret)
			log_warning("Failed to fixup node %s: %d\n", glue_np->name, ret);
	}
}

/* Remove all references to the rpmhpd device */
static void fixup_power_domains(void)
{
	struct device_node *pd = NULL, *np = NULL;
	struct property *prop;
	const __be32 *val;

	/* All Qualcomm platforms name the rpm(h)pd "power-controller" */
	for_each_of_allnodes(pd) {
		if (pd->name && !strcmp("power-controller", pd->name))
			break;
	}

	/* Sanity check that this is indeed a power domain controller */
	if (!of_find_property(pd, "#power-domain-cells", NULL)) {
		log_err("Found power-controller but it doesn't have #power-domain-cells\n");
		return;
	}

	/* Remove all references to the power domain controller */
	for_each_of_allnodes(np) {
		if (!(prop = of_find_property(np, "power-domains", NULL)))
			continue;

		val = prop->value;
		if (val[0] == cpu_to_fdt32(pd->phandle))
			of_remove_property(np, prop);
	}
}

#define time_call(func, ...) \
	do { \
		u64 start = timer_get_us(); \
		func(__VA_ARGS__); \
		debug(#func " took %lluus\n", timer_get_us() - start); \
	} while (0)

void qcom_of_fixup_nodes(void)
{
	time_call(fixup_usb_nodes);
	time_call(fixup_power_domains);
}

int ft_board_setup(void *blob, struct bd_info __maybe_unused *bd)
{
	struct fdt_header *fdt = blob;
	int node;
	int ret = 0;

	ret = fixup_retroid_fdt(blob);
	if (ret < 0)
		printf("WARNING: Failed to modify device tree: %d\n", ret);

	/* On RB2 we need to fix-up the dr_mode */
	if (!fdt_node_check_compatible(fdt, 0, "qcom,qrb4210-rb2")) {
		fdt_for_each_node_by_compatible(node, blob, 0, "snps,dwc3") {
			log_debug("%s: Setting 'dr_mode' to OTG\n", fdt_get_name(blob, node, NULL));
			fdt_setprop_string(fdt, node, "dr_mode", "otg");
			break;
		}
	}

	return 0;
}
