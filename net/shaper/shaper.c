/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/xarray.h>
#include <net/net_shaper.h>

#include "shaper_nl_gen.h"
#include "shaper_nl_gen.c"

struct net_shaper_data {
	struct xarray shapers;
};

int net_shaper_nl_get_doit(struct sk_buff *skb, struct genl_info *info)
{
	struct net *ns = genl_info_net(info);
	int rem, ifindex, handle;
	struct net_device *dev;
	struct nlattr *cur;
	int ret;

	if (GENL_REQ_ATTR_CHECK(info, NET_SHAPER_A_IFINDEX)) {
		GENL_SET_ERR_MSG(info, "missing ifindex attribute");
		return -EINVAL;
	}

	ifindex = nla_get_u32(info->attrs[NET_SHAPER_A_IFINDEX]);
	dev = dev_get_by_index(ns, ifindex);
	if (!dev) {
		GENL_SET_ERR_MSG(info, "device not found");
		return -EINVAL;
	}

	nla_for_each_nested(cur, info->attrs[NET_SHAPER_A_HANDLES], rem) {
		handle = nla_get_u32(cur);

		if (!dev->net_shaper_data) {
			GENL_SET_ERR_MSG_FMT(info, "can't find shaper %x: no shaper is initialized on device %d",
					   handle, ifindex);
			ret = -EINVAL;
			goto out;
		}

		// TODO fill out the given shapers
	}

out:
	dev_put(dev);
	return ret;
}

int net_shaper_nl_get_dumpit(struct sk_buff *skb,
			     struct netlink_callback *cb)
{
	return -EOPNOTSUPP;
}

int net_shaper_nl_set_doit(struct sk_buff *skb, struct genl_info *info)
{
	return -EOPNOTSUPP;
}

int net_shaper_nl_reset_doit(struct sk_buff *skb, struct genl_info *info)
{
	return -EOPNOTSUPP;
}

int net_shaper_nl_move_doit(struct sk_buff *skb, struct genl_info *info)
{
	return -EOPNOTSUPP;
}

static int __init shaper_init(void)
{
	return genl_register_family(&net_shaper_nl_family);
}

subsys_initcall(shaper_init);
