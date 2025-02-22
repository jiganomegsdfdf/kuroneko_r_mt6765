// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/uaccess.h>
#include "mtk_perfmgr_internal.h"
#ifdef CONFIG_TRACING
#include <linux/kallsyms.h>
#include <linux/trace_events.h>
#endif

char *perfmgr_copy_from_user_for_proc(const char __user *buffer,
		size_t count)
{
	return 0;
}

int check_proc_write(int *data, const char *ubuf, size_t cnt)
{

	char buf[128];

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, ubuf, cnt))
		return -EFAULT;
	buf[cnt] = 0;

	if (kstrtoint(buf, 10, data))
		return -1;
	return 0;
}

int check_group_proc_write(int *cgroup, int *data,
		const char *ubuf, size_t cnt)
{
	char buf[128];

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, ubuf, cnt))
		return -EFAULT;
	buf[cnt] = 0;

	if (sscanf(buf, "%d %d", cgroup, data) != 2)
		return -1;
	return 0;
}

#ifdef CONFIG_TRACING
static unsigned long __read_mostly tracing_mark_write_addr;
static inline void __mt_update_tracing_mark_write_addr(void)
{
	if (unlikely(tracing_mark_write_addr == 0))
		tracing_mark_write_addr =
			kallsyms_lookup_name("tracing_mark_write");
}

void perfmgr_trace_count(int val, const char *fmt, ...)
{
	char log[128];
	va_list args;
	int len;

	if (powerhal_tid <= 0)
		return;

	memset(log, ' ', sizeof(log));
	va_start(args, fmt);
	len = vsnprintf(log, sizeof(log), fmt, args);
	va_end(args);

	if (unlikely(len < 0))
		return;
	else if (unlikely(len == 128))
		log[127] = '\0';

	__mt_update_tracing_mark_write_addr();
}

void perfmgr_trace_printk(char *module, char *string)
{
	__mt_update_tracing_mark_write_addr();
}

void perfmgr_trace_begin(char *name, int id, int a, int b)
{
	__mt_update_tracing_mark_write_addr();
}

void perfmgr_trace_end(void)
{
	__mt_update_tracing_mark_write_addr();
}

void perfmgr_trace_log(char *module, const char *fmt, ...)
{
	char log[256];
	va_list args;
	int len;

	va_start(args, fmt);
	len = vsnprintf(log, sizeof(log), fmt, args);

	if (unlikely(len == 256))
		log[255] = '\0';
	va_end(args);
	perfmgr_trace_printk(module, log);
}
#else
static unsigned long __read_mostly tracing_mark_write_addr;
static inline void __mt_update_tracing_mark_write_addr(void)
{
}

void perfmgr_trace_count(int val, const char *fmt, ...)
{
}

void perfmgr_trace_printk(char *module, char *string)
{
}

void perfmgr_trace_begin(char *name, int id, int a, int b)
{
}

void perfmgr_trace_end(void)
{
}

void perfmgr_trace_log(char *module, const char *fmt, ...)
{
}
#endif

