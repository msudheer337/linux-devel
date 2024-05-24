/* SPDX-License-Identifier: ((GPL-2.0 WITH Linux-syscall-note) OR BSD-3-Clause) */
/* Do not edit directly, auto-generated from: */
/*	Documentation/netlink/specs/shaper.yaml */
/* YNL-GEN kernel header */

#ifndef _LINUX_NET_SHAPER_GEN_H
#define _LINUX_NET_SHAPER_GEN_H

#include <net/netlink.h>
#include <net/genetlink.h>

#include <uapi/linux/net_shaper.h>

/* Common nested types */
extern const struct nla_policy net_shaper_info_nl_policy[NET_SHAPER_A_INFO_WEIGHT + 1];
extern const struct nla_policy net_shaper_pair_nl_policy[NET_SHAPER_A_PAIR_NEW_PARENT + 1];

int net_shaper_nl_get_doit(struct sk_buff *skb, struct genl_info *info);
int net_shaper_nl_get_dumpit(struct sk_buff *skb, struct netlink_callback *cb);
int net_shaper_nl_set_doit(struct sk_buff *skb, struct genl_info *info);
int net_shaper_nl_reset_doit(struct sk_buff *skb, struct genl_info *info);
int net_shaper_nl_move_doit(struct sk_buff *skb, struct genl_info *info);

extern struct genl_family net_shaper_nl_family;

#endif /* _LINUX_NET_SHAPER_GEN_H */
