/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/of.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/batterydata-lib.h>

#define MAX_TABLE_NAME 64

struct batt_id_rng {
	int			min;
	int			max;
};

static int of_qpnp_read_u32_darray(const struct device_node *np,
				   const char *propname,
				   int *out_values,
				   size_t sz, int rows, int cols,
				   int max_cols)
{
	struct property *prop = of_find_property(np, propname, NULL);
	const __be32 *val;
	int i = 0;
	int j = 0;
	int *start_addr = out_values;

	if (!prop)
		return -EINVAL;
	if (!prop->value)
		return -ENODATA;
	if ((sz * sizeof(*out_values)) > prop->length)
		return -EOVERFLOW;

	val = prop->value;
	for (i = 0; i < rows; i++) {
		out_values = start_addr + (max_cols * i);
		for (j = 0; j < cols; j++)
			*out_values++ = be32_to_cpup(val++);
	}

	return 0;
}

/*
 * Allocate struct single_row_lut for qpnp-bms
 */
static int of_qpnp_alloc_single_row_lut(struct device_node *np,
					struct single_row_lut **srl,
					const char *tbl)
{
	int rc = 0;
	char buf[MAX_TABLE_NAME];

	if (!np || !tbl) {
		pr_err("missing np or tbl name!!\n");
		return -ENODEV;
	}

	*srl = kzalloc(sizeof(struct single_row_lut), GFP_KERNEL);

	if (!(*srl))
		return -ENOMEM;

	rc = scnprintf ((char *)buf, MAX_TABLE_NAME, "%s-cols", tbl);
	if (!rc) {
		pr_err("Error making cols table name %s\n", tbl);
		rc = -ENODATA;
		goto srl_error;
	}

	rc = of_property_read_u32(np, buf, &((*srl)->cols));
	if (rc) {
		pr_err("%s-cols missing in dt\n", tbl);
		goto srl_error;
	}

	rc = scnprintf ((char *)buf, MAX_TABLE_NAME, "%s-x", tbl);
	if (!rc) {
		pr_err("Error making x table name %s\n", tbl);
		rc = -ENODATA;
		goto srl_error;
	}

	rc = of_property_read_u32_array(np, buf, (*srl)->x, (*srl)->cols);
	if (rc) {
		pr_err("%s-x missing in dt\n", tbl);

		goto srl_error;
	}

	rc = scnprintf ((char *)buf, MAX_TABLE_NAME, "%s-y", tbl);
	if (!rc) {
		pr_err("Error making y table name %s\n", tbl);
		rc = -ENODATA;
		goto srl_error;
	}

	rc = of_property_read_u32_array(np, buf, (*srl)->y, (*srl)->cols);
	if (rc) {
		pr_err("%s-y missing in dt\n", tbl);
		goto srl_error;
	}

	return 0;

srl_error:
	kfree(*srl);
	*srl = NULL;
	return rc;
}

/*
 * Allocate struct sf_lut for qpnp-bms
 */
static int of_qpnp_alloc_sf_lut(struct device_node *np,
				struct sf_lut **sfl,
				const char *tbl)
{
	int rc = 0;
	char buf[MAX_TABLE_NAME];

	if (!np || !tbl) {
		pr_err("missing np or tbl name!!\n");
		return -ENODEV;
	}

	*sfl = kzalloc(sizeof(struct sf_lut), GFP_KERNEL);

	if (!*sfl)
		return -ENOMEM;

	rc = scnprintf ((char *)buf, MAX_TABLE_NAME, "%s-rows", tbl);
	if (!rc) {
		pr_err("Error making rows table name %s\n", tbl);
		rc = -ENODATA;
		goto sfl_error;
	}

	rc = of_property_read_u32(np, buf, &((*sfl)->rows));
	if (rc) {
		pr_err("%s-rows missing in dt\n", tbl);
		goto sfl_error;
	}

	rc = scnprintf ((char *)buf, MAX_TABLE_NAME, "%s-cols", tbl);
	if (!rc) {
		pr_err("Error making cols table name %s\n", tbl);
		rc = -ENODATA;
		goto sfl_error;
	}

	rc = of_property_read_u32(np, buf, &((*sfl)->cols));
	if (rc) {
		pr_err("%s-cols missing in dt\n", tbl);
		return rc;
		goto sfl_error;
	}

	rc = scnprintf ((char *)buf, MAX_TABLE_NAME, "%s-row-entries", tbl);
	if (!rc) {
		pr_err("Error making row-entries table name %s\n", tbl);
		rc = -ENODATA;
		goto sfl_error;
	}

	rc = of_property_read_u32_array(np, buf,
					(*sfl)->row_entries,
					(*sfl)->cols);
	if (rc) {
		pr_err("%s-row-entries missing in dt\n", tbl);
		goto sfl_error;
	}

	rc = scnprintf ((char *)buf, MAX_TABLE_NAME, "%s-percent", tbl);
	if (!rc) {
		pr_err("Error making percent table name %s\n", tbl);
		rc = -ENODATA;
		goto sfl_error;
	}

	rc = of_property_read_u32_array(np, buf,
					(*sfl)->percent,
					(*sfl)->rows);
	if (rc) {
		pr_err("%s-percent missing in dt\n", tbl);
		goto sfl_error;
	}

	rc = scnprintf ((char *)buf, MAX_TABLE_NAME, "%s-sf", tbl);
	if (!rc) {
		pr_err("Error making sf table name %s\n", tbl);
		rc = -ENODATA;
		goto sfl_error;
	}

	rc = of_qpnp_read_u32_darray(np,
				     buf,
				     (int *)((*sfl)->sf),
				     (*sfl)->rows * (*sfl)->cols,
				     (*sfl)->rows,
				     (*sfl)->cols,
				     PC_CC_COLS);
	if (rc) {
		pr_err("%s-sf missing in dt\n", tbl);
		goto sfl_error;
	}

	return 0;

sfl_error:
	kfree(*sfl);
	*sfl = NULL;
	return rc;
}

/*
 * Allocate struct pc_temp_ocv_lut for qpnp-bms
 */
static int of_qpnp_alloc_pc_temp_ocv_lut(struct device_node *np,
					 struct pc_temp_ocv_lut **lut,
					 const char *tbl)
{
	int rc = 0;
	char buf[MAX_TABLE_NAME];

	if (!np || !tbl) {
		pr_err("missing np or tbl name!!\n");
		return -ENODEV;
	}

	*lut = kzalloc(sizeof(struct pc_temp_ocv_lut), GFP_KERNEL);

	if (!(*lut))
		return -ENOMEM;

	rc = scnprintf ((char *)buf, MAX_TABLE_NAME, "%s-rows", tbl);
	if (!rc) {
		pr_err("Error making rows table name %s\n", tbl);
		rc = -ENODATA;
		goto lut_error;
	}

	rc = of_property_read_u32(np, buf, &((*lut)->rows));
	if (rc) {
		pr_err("%s-rows missing in dt\n", tbl);
		goto lut_error;
	}

	rc = scnprintf ((char *)buf, MAX_TABLE_NAME, "%s-cols", tbl);
	if (!rc) {
		pr_err("Error making cols table name %s\n", tbl);
		rc = -ENODATA;
		goto lut_error;
	}

	rc = of_property_read_u32(np, buf, &((*lut)->cols));
	if (rc) {
		pr_err("%s-cols missing in dt\n", tbl);
		goto lut_error;
	}

	rc = scnprintf ((char *)buf, MAX_TABLE_NAME, "%s-temp", tbl);
	if (!rc) {
		pr_err("Error making temp table name %s\n", tbl);
		rc = -ENODATA;
		goto lut_error;
	}

	rc = of_property_read_u32_array(np, buf,
					(*lut)->temp,
					(*lut)->cols);
	if (rc) {
		pr_err("%s-temp missing in dt\n", tbl);
		goto lut_error;
	}

	rc = scnprintf ((char *)buf, MAX_TABLE_NAME, "%s-percent", tbl);
	if (!rc) {
		pr_err("Error making percent table name %s\n", tbl);
		rc = -ENODATA;
		goto lut_error;
	}

	rc = of_property_read_u32_array(np, buf,
					(*lut)->percent,
					(*lut)->rows);
	if (rc) {
		pr_err("%s-percent missing in dt\n", tbl);
		goto lut_error;
	}

	rc = scnprintf ((char *)buf, MAX_TABLE_NAME, "%s-ocv", tbl);
	if (!rc) {
		pr_err("Error making ocv table name %s\n", tbl);
		rc = -ENODATA;
		goto lut_error;
	}

	rc = of_qpnp_read_u32_darray(np,
				     buf,
				     (int *)((*lut)->ocv),
				     (*lut)->rows * (*lut)->cols,
				     (*lut)->rows,
				     (*lut)->cols,
				     PC_TEMP_COLS);
	if (rc) {
		pr_err("%s-ocv missing in dt\n", tbl);
		goto lut_error;
	}

	return 0;

lut_error:
	kfree(*lut);
	*lut = NULL;
	return rc;
}

int of_qpnp_populate_bms_tbls(struct bms_battery_data **batt_data,
			      int64_t batt_id)
{
	struct device_node *nbatt, *nbatt_chd;
	int rc;
	const char *batt_name[MAX_TABLE_NAME];
	struct batt_id_rng id_range;
	size_t sz = sizeof(struct batt_id_rng) / sizeof(int);

	nbatt = of_find_compatible_node(NULL, NULL, "qcom,batt-tbls");
	if (!nbatt) {
		pr_err("missing qcom,batt-tbl!!\n");
		return -ENODEV;
	}

	for_each_child_of_node(nbatt, nbatt_chd)
		if (nbatt_chd) {
			rc = of_property_read_u32_array(nbatt_chd,
							"qcom,batt-tbl-id",
							(u32 *)&id_range,
							sz);
			if (!rc &&
			    is_between(id_range.min, id_range.max,
				       batt_id))
				break;
		}

	of_node_put(nbatt);

	if (!nbatt_chd) {
		pr_err("No Battery Table found batt_id = %d\n", (int)batt_id);
		return -ENODATA;
	}

	*batt_data = kzalloc(sizeof(struct bms_battery_data), GFP_KERNEL);

	if (!(*batt_data)) {
		of_node_put(nbatt_chd);
		return -ENOMEM;
	}

	/* Initialize defaults */
	(*batt_data)->max_voltage_uv = -EINVAL;
	(*batt_data)->cutoff_uv = -EINVAL;
	(*batt_data)->iterm_ua = -EINVAL;

	of_property_read_string(nbatt_chd, "qcom,batt-tbl-name",
				batt_name);
	pr_err("Battery Table %s found batt_id = %d\n", *batt_name,
	       (int)batt_id);

	rc = of_qpnp_alloc_single_row_lut(nbatt_chd,
					  &(*batt_data)->fcc_temp_lut,
					  "qcom,fcc-temp-lut");
	if (rc)
		goto fcc_temp_lut_err;

	rc = of_qpnp_alloc_single_row_lut(nbatt_chd,
					  &(*batt_data)->fcc_sf_lut,
					  "qcom,fcc-sf-lut");
	if (rc)
		goto fcc_sf_lut_err;

	rc = of_qpnp_alloc_pc_temp_ocv_lut(nbatt_chd,
					   &(*batt_data)->pc_temp_ocv_lut,
					   "qcom,pc-temp-ocv-lut");
	if (rc)
		goto pc_temp_ocv_lut_err;

	rc = of_qpnp_alloc_sf_lut(nbatt_chd,
				  &(*batt_data)->pc_sf_lut,
				  "qcom,pc-sf-lut");
	if (rc)
		goto pc_sf_lut_err;

	rc = of_qpnp_alloc_sf_lut(nbatt_chd,
				  &(*batt_data)->rbatt_sf_lut,
				  "qcom,rbatt-sf-lut");
	if (rc)
		pr_err("qcom,rbatt-sf-lut missing in dt\n");

	rc = of_property_read_u32(nbatt_chd, "qcom,batt-tbl-fcc",
				  &(*batt_data)->fcc);
	if (rc)
		pr_err("qcom,batt-tbl-fcc missing in dt\n");

	rc = of_property_read_u32(nbatt_chd, "qcom,batt-tbl-default-rbatt-mohm",
				  &(*batt_data)->default_rbatt_mohm);
	if (rc)
		pr_err("qcom,batt-tbl-default-rbatt-mohm missing in dt\n");

	rc = of_property_read_u32(nbatt_chd, "qcom,batt-tbl-rbatt-capacitive-mohm",
				  &(*batt_data)->rbatt_capacitive_mohm);
	if (rc)
		pr_err("qcom,batt-tbl-rbatt-capacitive-mohm missing in dt\n");

	rc = of_property_read_u32(nbatt_chd, "qcom,batt-tbl-flat-ocv-threshold-uv",
				  &(*batt_data)->flat_ocv_threshold_uv);
	if (rc)
		pr_err("qcom,batt-tbl-flat-ocv-threshold-uv missing in dt\n");

	rc = of_property_read_u32(nbatt_chd, "qcom,batt-tbl-delta-rbatt-mohm",
				  &(*batt_data)->delta_rbatt_mohm);
	if (rc)
		pr_err("qcom,batt-tbl-delta-rbatt-mohm missing in dt\n");

	of_node_put(nbatt_chd);
	return 0;

pc_sf_lut_err:
	kfree((*batt_data)->pc_temp_ocv_lut);
	(*batt_data)->pc_temp_ocv_lut = NULL;
pc_temp_ocv_lut_err:
	kfree((*batt_data)->fcc_sf_lut);
	(*batt_data)->fcc_sf_lut = NULL;
fcc_sf_lut_err:
	kfree((*batt_data)->fcc_temp_lut);
	(*batt_data)->fcc_temp_lut = NULL;
fcc_temp_lut_err:
	kfree(*batt_data);
	*batt_data = NULL;

	of_node_put(nbatt_chd);
	return rc;
}

MODULE_LICENSE("GPL v2");
