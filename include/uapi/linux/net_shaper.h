/* SPDX-License-Identifier: ((GPL-2.0 WITH Linux-syscall-note) OR BSD-3-Clause) */
/* Do not edit directly, auto-generated from: */
/*	Documentation/netlink/specs/shaper.yaml */
/* YNL-GEN uapi header */

#ifndef _UAPI_LINUX_NET_SHAPER_H
#define _UAPI_LINUX_NET_SHAPER_H

#define NET_SHAPER_FAMILY_NAME		"net_shaper"
#define NET_SHAPER_FAMILY_VERSION	1

/**
 * enum net_shaper_metric - different metric each shaper can support
 * @NET_SHAPER_METRIC_PPS: Shaper operates on a packets per second basis
 * @NET_SHAPER_METRIC_BPS: Shaper operates on a bits per second basis
 */
enum net_shaper_metric {
	NET_SHAPER_METRIC_PPS,
	NET_SHAPER_METRIC_BPS,
};

enum {
	NET_SHAPER_A_INFO_HANDLE = 1,
	NET_SHAPER_A_INFO_METRIC,
	NET_SHAPER_A_INFO_BW_MIN,
	NET_SHAPER_A_INFO_BW_MAX,
	NET_SHAPER_A_INFO_PRIORITY,
	NET_SHAPER_A_INFO_WEIGHT,

	__NET_SHAPER_A_INFO_MAX,
	NET_SHAPER_A_INFO_MAX = (__NET_SHAPER_A_INFO_MAX - 1)
};

enum {
	NET_SHAPER_A_PAIR_HANDLE = 1,
	NET_SHAPER_A_PAIR_NEW_PARENT,

	__NET_SHAPER_A_PAIR_MAX,
	NET_SHAPER_A_PAIR_MAX = (__NET_SHAPER_A_PAIR_MAX - 1)
};

enum {
	NET_SHAPER_A_IFINDEX = 1,
	NET_SHAPER_A_HANDLES,
	NET_SHAPER_A_INFO_LIST,
	NET_SHAPER_A_HANDLE_PAIRS,

	__NET_SHAPER_A_MAX,
	NET_SHAPER_A_MAX = (__NET_SHAPER_A_MAX - 1)
};

enum {
	NET_SHAPER_CMD_GET = 1,
	NET_SHAPER_CMD_SET,
	NET_SHAPER_CMD_RESET,
	NET_SHAPER_CMD_MOVE,

	__NET_SHAPER_CMD_MAX,
	NET_SHAPER_CMD_MAX = (__NET_SHAPER_CMD_MAX - 1)
};

#endif /* _UAPI_LINUX_NET_SHAPER_H */
