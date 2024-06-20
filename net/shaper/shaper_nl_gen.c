// SPDX-License-Identifier: ((GPL-2.0 WITH Linux-syscall-note) OR BSD-3-Clause)
/* Do not edit directly, auto-generated from: */
/*	Documentation/netlink/specs/shaper.yaml */
/* YNL-GEN kernel source */

#include <net/netlink.h>
#include <net/genetlink.h>

#include "shaper_nl_gen.h"

#include <uapi/linux/net_shaper.h>

/* Common nested types */
const struct nla_policy net_shaper_info_nl_policy[NET_SHAPER_A_INFO_WEIGHT + 1] = {
	[NET_SHAPER_A_INFO_HANDLE] = { .type = NLA_U32, },
	[NET_SHAPER_A_INFO_METRIC] = NLA_POLICY_MAX(NLA_U32, 1),
	[NET_SHAPER_A_INFO_BW_MIN] = { .type = NLA_U64, },
	[NET_SHAPER_A_INFO_BW_MAX] = { .type = NLA_U64, },
	[NET_SHAPER_A_INFO_PRIORITY] = { .type = NLA_U32, },
	[NET_SHAPER_A_INFO_WEIGHT] = { .type = NLA_U32, },
};

const struct nla_policy net_shaper_pair_nl_policy[NET_SHAPER_A_PAIR_NEW_PARENT + 1] = {
	[NET_SHAPER_A_PAIR_HANDLE] = { .type = NLA_U32, },
	[NET_SHAPER_A_PAIR_NEW_PARENT] = { .type = NLA_U32, },
};

/* NET_SHAPER_CMD_GET - do */
static const struct nla_policy net_shaper_get_do_nl_policy[NET_SHAPER_A_HANDLES + 1] = {
	[NET_SHAPER_A_IFINDEX] = { .type = NLA_U32, },
	[NET_SHAPER_A_HANDLES] = { .type = NLA_U32, },
};

/* NET_SHAPER_CMD_GET - dump */
static const struct nla_policy net_shaper_get_dump_nl_policy[NET_SHAPER_A_IFINDEX + 1] = {
	[NET_SHAPER_A_IFINDEX] = { .type = NLA_U32, },
};

/* NET_SHAPER_CMD_SET - do */
static const struct nla_policy net_shaper_set_nl_policy[NET_SHAPER_A_INFO_LIST + 1] = {
	[NET_SHAPER_A_IFINDEX] = { .type = NLA_U32, },
	[NET_SHAPER_A_INFO_LIST] = NLA_POLICY_NESTED(net_shaper_info_nl_policy),
};

/* NET_SHAPER_CMD_RESET - do */
static const struct nla_policy net_shaper_reset_nl_policy[NET_SHAPER_A_HANDLES + 1] = {
	[NET_SHAPER_A_IFINDEX] = { .type = NLA_U32, },
	[NET_SHAPER_A_HANDLES] = { .type = NLA_U32, },
};

/* NET_SHAPER_CMD_MOVE - do */
static const struct nla_policy net_shaper_move_nl_policy[NET_SHAPER_A_HANDLE_PAIRS + 1] = {
	[NET_SHAPER_A_IFINDEX] = { .type = NLA_U32, },
	[NET_SHAPER_A_HANDLE_PAIRS] = NLA_POLICY_NESTED(net_shaper_pair_nl_policy),
};

/* Ops table for net_shaper */
static const struct genl_split_ops net_shaper_nl_ops[] = {
	{
		.cmd		= NET_SHAPER_CMD_GET,
		.doit		= net_shaper_nl_get_doit,
		.policy		= net_shaper_get_do_nl_policy,
		.maxattr	= NET_SHAPER_A_HANDLES,
		.flags		= GENL_ADMIN_PERM | GENL_CMD_CAP_DO,
	},
	{
		.cmd		= NET_SHAPER_CMD_GET,
		.dumpit		= net_shaper_nl_get_dumpit,
		.policy		= net_shaper_get_dump_nl_policy,
		.maxattr	= NET_SHAPER_A_IFINDEX,
		.flags		= GENL_ADMIN_PERM | GENL_CMD_CAP_DUMP,
	},
	{
		.cmd		= NET_SHAPER_CMD_SET,
		.doit		= net_shaper_nl_set_doit,
		.policy		= net_shaper_set_nl_policy,
		.maxattr	= NET_SHAPER_A_INFO_LIST,
		.flags		= GENL_ADMIN_PERM | GENL_CMD_CAP_DO,
	},
	{
		.cmd		= NET_SHAPER_CMD_RESET,
		.doit		= net_shaper_nl_reset_doit,
		.policy		= net_shaper_reset_nl_policy,
		.maxattr	= NET_SHAPER_A_HANDLES,
		.flags		= GENL_ADMIN_PERM | GENL_CMD_CAP_DO,
	},
	{
		.cmd		= NET_SHAPER_CMD_MOVE,
		.doit		= net_shaper_nl_move_doit,
		.policy		= net_shaper_move_nl_policy,
		.maxattr	= NET_SHAPER_A_HANDLE_PAIRS,
		.flags		= GENL_ADMIN_PERM | GENL_CMD_CAP_DO,
	},
};

struct genl_family net_shaper_nl_family __ro_after_init = {
	.name		= NET_SHAPER_FAMILY_NAME,
	.version	= NET_SHAPER_FAMILY_VERSION,
	.netnsok	= true,
	.parallel_ops	= true,
	.module		= THIS_MODULE,
	.split_ops	= net_shaper_nl_ops,
	.n_split_ops	= ARRAY_SIZE(net_shaper_nl_ops),
};
