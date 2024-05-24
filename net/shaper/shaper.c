/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <linux/kernel.h>

static int __init shaper_init(void)
{
	return 0;
}

subsys_initcall(shaper_init);
