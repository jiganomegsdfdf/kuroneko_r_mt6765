/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MMDVFS_INTERNAL_H__
#define __MMDVFS_INTERNAL_H__

#include "mmdvfs_mgr.h"

#define MMDVFS_LOG_TAG	"MMDVFS"

#define MMDVFSMSG(string, args...) \
	pr_no_notice("[pid=%d]"string, current->tgid, ##args)

#define MMDVFSDEBUG(level, x...)            \
	do {                        \
		if (g_mmdvfs_debug_level \
		&& (*g_mmdvfs_debug_level) >= (level))    \
			MMDVFSMSG(x);            \
	} while (0)

#define MMDVFSMSG2(string, args...) pr_no_debug(string, ##args)

#define MMDVFSTMP(string, args...) \
	pr_no_debug("[pid=%d]"string, current->tgid, ##args)

#define MMDVFSERR(string, args...) \
	do {\
		pr_no_notice("error: "string, ##args); \
		aee_kernel_warning(MMDVFS_LOG_TAG, "error: "string, ##args); \
	} while (0)

extern void mmdvfs_internal_handle_state_change(
	struct mmdvfs_state_change_event *event);
extern void mmdvfs_internal_notify_vcore_calibration(
	struct mmdvfs_prepare_action_event *event);
extern unsigned int *g_mmdvfs_debug_level;
extern unsigned int *g_mmdvfs_scen_log_mask;
extern int mmdvfs_internal_set_vpu_step(int current_step, int update_step);
extern void mmdvfs_internal_get_cam_setting(
	struct mmdvfs_cam_property *cam_setting);
#endif				/* __MMDVFS_INTERNAL_H__ */
