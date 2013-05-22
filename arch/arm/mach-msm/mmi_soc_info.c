/*
 * Copyright (C) 2013 Motorola Mobility LLC
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/apanic_mmc.h>
#include <linux/persistent_ram.h>
#include <mach/socinfo.h>

static inline void mmi_panic_annotate(const char *str)
{
	apanic_mmc_annotate(str);
	persistent_ram_ext_oldbuf_print(str);
}

static void __init mmi_msm_annotate_socinfo(void)
{
	char socinfo[32];

	snprintf(socinfo, sizeof(socinfo), "socinfo: id=%u, ",
			socinfo_get_id());
	mmi_panic_annotate(socinfo);

	snprintf(socinfo, sizeof(socinfo), "ver=%u.%u, ",
			SOCINFO_VERSION_MAJOR(socinfo_get_version()),
			SOCINFO_VERSION_MINOR(socinfo_get_version()));
	mmi_panic_annotate(socinfo);

	snprintf(socinfo, sizeof(socinfo), "raw_id=%u, ",
			socinfo_get_raw_id());
	mmi_panic_annotate(socinfo);

	snprintf(socinfo, sizeof(socinfo), "raw_ver=%u, ",
			socinfo_get_raw_version());
	mmi_panic_annotate(socinfo);

	snprintf(socinfo, sizeof(socinfo), "hw_plat=%u, ",
			socinfo_get_platform_type());
	mmi_panic_annotate(socinfo);

	snprintf(socinfo, sizeof(socinfo), "hw_plat_ver=%u, ",
			socinfo_get_platform_version());
	mmi_panic_annotate(socinfo);

	snprintf(socinfo, sizeof(socinfo), "hw_plat_subtype=%u\n",
			socinfo_get_platform_subtype());
	mmi_panic_annotate(socinfo);
}

static int __init init_mmi_soc_info(void)
{
	mmi_msm_annotate_socinfo();
	return 0;
}
module_init(init_mmi_soc_info);
MODULE_DESCRIPTION("Motorola Mobility LLC. SOC Info");
MODULE_LICENSE("GPL v2");
