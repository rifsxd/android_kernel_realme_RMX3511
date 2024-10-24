// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 Unisoc Communications Inc.
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

#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#define DEFINE_STR_SHOW_FOPS(__info, __name)			\
static int __name ## _show(struct seq_file *m, void *v)		\
{								\
	seq_printf(m, "%s\n", __info.__name);			\
	return 0;						\
}								\
DEFINE_SHOW_ATTRIBUTE(__name)


struct ver_info {
	char *prj_name;
	char *pcb_version;
	char *boot_mode;
	char *rf_type;
	char *secure_type;
	char *secure_stage;
	char *ocp;
	char *serial_id;
	char *eng_version;
	char *is_confidential;
	char *dtsi_name;
	char *audio_name;
};

static struct ver_info oppo_ver_info;

DEFINE_STR_SHOW_FOPS(oppo_ver_info, prj_name);
DEFINE_STR_SHOW_FOPS(oppo_ver_info, pcb_version);
DEFINE_STR_SHOW_FOPS(oppo_ver_info, boot_mode);
DEFINE_STR_SHOW_FOPS(oppo_ver_info, rf_type);
DEFINE_STR_SHOW_FOPS(oppo_ver_info, secure_type);
DEFINE_STR_SHOW_FOPS(oppo_ver_info, secure_stage);
DEFINE_STR_SHOW_FOPS(oppo_ver_info, ocp);
DEFINE_STR_SHOW_FOPS(oppo_ver_info, serial_id);
DEFINE_STR_SHOW_FOPS(oppo_ver_info, eng_version);
DEFINE_STR_SHOW_FOPS(oppo_ver_info, is_confidential);
DEFINE_STR_SHOW_FOPS(oppo_ver_info, dtsi_name);
DEFINE_STR_SHOW_FOPS(oppo_ver_info, audio_name);

void create_entry(const char *name, struct proc_dir_entry *parent,
		  const struct file_operations *proc_fops)
{
	struct proc_dir_entry *ent;

	ent = proc_create(name, 0444, parent, proc_fops);
	if (ent == NULL)
		pr_err("Unable to create /proc/oppoVersion/%s\n", name);
}

static int create_prjinfo_entries(void)
{
	struct proc_dir_entry *oppo_ver_dir;

	oppo_ver_dir = proc_mkdir("oppoVersion", NULL);
	if (oppo_ver_dir == NULL) {
		pr_err("Unable to create /proc/oppoVersion directory\n");
		return -ENOMEM;
	}

	create_entry("prjName", oppo_ver_dir, &prj_name_fops);
	create_entry("pcbVersion", oppo_ver_dir, &pcb_version_fops);
	create_entry("oppoBootmode", oppo_ver_dir, &boot_mode_fops);
	create_entry("RFType", oppo_ver_dir, &rf_type_fops);
#ifndef VENDOR_KERNEL
	create_entry("secureType", oppo_ver_dir, &secure_type_fops);
	create_entry("secureStage", oppo_ver_dir, &secure_stage_fops);
#endif
	create_entry("ocp", oppo_ver_dir, &ocp_fops);
	create_entry("serialID", oppo_ver_dir, &serial_id_fops);
	create_entry("engVersion", oppo_ver_dir, &eng_version_fops);
	create_entry("isConfidential", oppo_ver_dir, &is_confidential_fops);
	create_entry("dtsiName", oppo_ver_dir, &dtsi_name_fops);
	create_entry("audioName", oppo_ver_dir, &audio_name_fops);

	return 0;
}
late_initcall_sync(create_prjinfo_entries);

#define DEFINE_STR_INFO_SETUP(__info, __match)			\
static int __init __match ## _setup(char *str)			\
{								\
	__info.__match = str;					\
	return 0;						\
}								\
__setup(#__match"=", __match ## _setup)


DEFINE_STR_INFO_SETUP(oppo_ver_info, prj_name);
DEFINE_STR_INFO_SETUP(oppo_ver_info, pcb_version);
DEFINE_STR_INFO_SETUP(oppo_ver_info, boot_mode);
DEFINE_STR_INFO_SETUP(oppo_ver_info, rf_type);
DEFINE_STR_INFO_SETUP(oppo_ver_info, secure_type);
DEFINE_STR_INFO_SETUP(oppo_ver_info, secure_stage);
DEFINE_STR_INFO_SETUP(oppo_ver_info, ocp);
DEFINE_STR_INFO_SETUP(oppo_ver_info, serial_id);
DEFINE_STR_INFO_SETUP(oppo_ver_info, eng_version);
DEFINE_STR_INFO_SETUP(oppo_ver_info, is_confidential);
DEFINE_STR_INFO_SETUP(oppo_ver_info, dtsi_name);
DEFINE_STR_INFO_SETUP(oppo_ver_info, audio_name);
