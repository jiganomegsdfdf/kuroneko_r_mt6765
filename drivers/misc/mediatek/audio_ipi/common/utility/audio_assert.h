/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 */

#ifndef AUDIO_ASSERT_H
#define AUDIO_ASSERT_H

#ifdef DEBUG_AUDIO_AEE /* def CONFIG_MTK_AEE_FEATURE */
#include <mt-plat/aee.h>
#endif

#ifdef DEBUG_AUDIO_AEE /* def CONFIG_MTK_AEE_FEATURE */
#define AUD_ASSERT(exp) \
	do { \
		if (!(exp)) { \
			aee_kernel_exception_api(__FILE__, \
						 __LINE__, \
						 DB_OPT_DEFAULT, \
						 "[Audio]", \
						 "ASSERT("#exp") fail!!"); \
		} \
	} while (0)
#else
#define AUD_ASSERT(exp) \
	do { \
		if (!(exp)) { \
			pr_no_notice("ASSERT("#exp")!! \""  __FILE__ "\", %uL\n", \
				  __LINE__); \
		} \
	} while (0)
#endif

#define AUD_WARNING(string) \
	do { \
		pr_no_notice("AUD_WARNING(" string"): \""  __FILE__ "\", %uL\n", \
			  __LINE__); \
		WARN_ON(1); \
	} while (0)



#endif /* end of AUDIO_ASSERT_H */

