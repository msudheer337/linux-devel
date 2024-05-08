/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/xarray.h>
#include <net/net_shaper.h>

#include "shaper_nl_gen.h"
#include "shaper_nl_gen.c"

#include "../core/dev.h"

struct net_shaper_data {
	struct xarray shapers;
};

struct net_shaper_nl_ctx {
	u32 start_handle;
};

static int fill_handle(struct sk_buff *msg, u32 handle, u32 type,
		       const struct genl_info *info)
{
	struct nlattr *handle_attr;

	if (!handle)
		return 0;

	handle_attr = nla_nest_start_noflag(msg, type);
	if (!handle_attr)
		return -EMSGSIZE;

	if (nla_put_u32(msg, NET_SHAPER_A_SCOPE,
			net_shaper_handle_scope(handle)) ||
	    nla_put_u32(msg, NET_SHAPER_A_MAJOR,
			net_shaper_handle_major(handle)) ||
	    nla_put_u32(msg, NET_SHAPER_A_MINOR,
			net_shaper_handle_minor(handle)))
		goto handle_nest_cancel;

	nla_nest_end(msg, handle_attr);
	return 0;

handle_nest_cancel:
	nla_nest_cancel(msg, handle_attr);
	return -EMSGSIZE;
}

static int
net_shaper_fill_one(struct sk_buff *msg, struct net_shaper_info *shaper,
		    const struct genl_info *info)
{
	void *hdr;

	hdr = genlmsg_iput(msg, info);
	if (!hdr)
		return -EMSGSIZE;

	if (fill_handle(msg, shaper->parent, NET_SHAPER_A_PARENT, info) ||
	    fill_handle(msg, shaper->handle, NET_SHAPER_A_HANDLE, info) ||
	    nla_put_u32(msg, NET_SHAPER_A_METRIC, shaper->metric) ||
	    nla_put_u64_64bit(msg, NET_SHAPER_A_BW_MIN, shaper->bw_min,
			      NET_SHAPER_A_PAD) ||
	    nla_put_u64_64bit(msg, NET_SHAPER_A_BW_MAX, shaper->bw_max,
			      NET_SHAPER_A_PAD) ||
	    nla_put_u64_64bit(msg, NET_SHAPER_A_BURST, shaper->burst,
			      NET_SHAPER_A_PAD) ||
	    nla_put_u32(msg, NET_SHAPER_A_PRIORITY, shaper->priority) ||
	    nla_put_u32(msg, NET_SHAPER_A_WEIGHT, shaper->weight))
		goto nla_put_failure;

	genlmsg_end(msg, hdr);

	return 0;

nla_put_failure:
	genlmsg_cancel(msg, hdr);
	return -EMSGSIZE;
}

static struct net_device *fetch_dev(const struct genl_info *info)
{
	struct net *ns = genl_info_net(info);
	struct net_device *dev;
	int ifindex;

	if (GENL_REQ_ATTR_CHECK(info, NET_SHAPER_A_IFINDEX))
		return NULL;

	ifindex = nla_get_u32(info->attrs[NET_SHAPER_A_IFINDEX]);
	dev = dev_get_by_index(ns, ifindex);
	if (!dev) {
		GENL_SET_ERR_MSG_FMT(info, "device %d not found", ifindex);
		return NULL;
	}

	if (!dev->netdev_ops->net_shaper_ops) {
		GENL_SET_ERR_MSG_FMT(info, "device %s does not support H/W shaper",
				     dev->name);
		dev_put(dev);
		return NULL;
	}

	return dev;
}

static int parse_handle(const struct nlattr *attr, const struct genl_info *info,
			u32 *handle)
{
	struct nlattr *tb[NET_SHAPER_A_MINOR + 1];
	struct nlattr *scope, *major, *minor;
	int ret;

	ret = nla_parse_nested(tb, NET_SHAPER_A_MINOR, attr,
			       net_shaper_handle_nl_policy, info->extack);
	if (ret < 0)
		return ret;

	scope = tb[NET_SHAPER_A_SCOPE];
	if (!scope) {
		GENL_SET_ERR_MSG(info, "Missing 'scope' attribute for handle");
		return -EINVAL;
	}

	major = tb[NET_SHAPER_A_MAJOR];
	minor = tb[NET_SHAPER_A_MINOR];
	*handle = net_shaper_make_handle(nla_get_u32(scope),
					 major ? nla_get_u32(major) : 0,
					 minor ? nla_get_u32(minor) : 0);
	return 0;
}

int net_shaper_nl_get_doit(struct sk_buff *skb, struct genl_info *info)
{
	struct net_shaper_info *shaper;
	struct net_device *dev;
	struct sk_buff *msg;
	u32 handle;
	int ret;

	dev = fetch_dev(info);
	if (!dev)
		return -EINVAL;

	if (GENL_REQ_ATTR_CHECK(info, NET_SHAPER_A_HANDLE))
		goto put;

	ret = parse_handle(info->attrs[NET_SHAPER_A_HANDLE], info, &handle);
	if (ret < 0)
		goto put;

	if (!dev->net_shaper_data) {
		GENL_SET_ERR_MSG_FMT(info, "no shaper is initialized on device %s",
				     dev->name);
		ret = -EINVAL;
		goto put;
	}

	shaper = xa_load(&dev->net_shaper_data->shapers, handle);
	if (!shaper) {
		GENL_SET_ERR_MSG_FMT(info, "Can't find shaper for handle %x", handle);
		ret = -EINVAL;
		goto put;
	}

	msg = nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	ret = net_shaper_fill_one(msg, shaper, info);
	if (ret)
		goto free_msg;

	ret =  genlmsg_reply(msg, info);
	if (ret)
		goto free_msg;

put:
	dev_put(dev);
	return ret;

free_msg:
	nlmsg_free(msg);
	goto put;
}

int net_shaper_nl_get_dumpit(struct sk_buff *skb,
			     struct netlink_callback *cb)
{
	struct net_shaper_nl_ctx *ctx = (struct net_shaper_nl_ctx *)cb->ctx;
	const struct genl_info *info = genl_info_dump(cb);
	struct net_device *dev = fetch_dev(info);
	struct net_shaper_info *shaper;
	unsigned long handle;
	int ret;

	if (!dev)
		return -EINVAL;

	BUILD_BUG_ON(sizeof(struct net_shaper_nl_ctx) > sizeof(cb->ctx));

	if (!dev->net_shaper_data) {
		ret = 0;
		goto put;
	}

	xa_for_each_range(&dev->net_shaper_data->shapers, handle, shaper,
			  ctx->start_handle, U32_MAX) {
		ret = net_shaper_fill_one(skb, shaper, info);
		if (ret) {
			ctx->start_handle = handle;
			goto put;
		}
	}

put:
	dev_put(dev);
	return ret;
}

static int attr_list_len(struct genl_info *info, int type)
{
	struct nlattr *attr;
	int rem, cnt = 0;

	nla_for_each_attr_type(attr, type, genlmsg_data(info->genlhdr),
			       genlmsg_len(info->genlhdr), rem)
		cnt++;
	return cnt;
}

static int parse_shaper(const struct nlattr *attr, const struct genl_info *info,
			struct net_shaper_info *shaper)
{
	struct nlattr *tb[NET_SHAPER_A_MAX + 1];
	int ret;

	ret = nla_parse_nested(tb, NET_SHAPER_A_MAX, attr,
			       net_shaper_ns_info_nl_policy, info->extack);
	if (ret < 0)
		return ret;

	if (NL_REQ_ATTR_CHECK(info->extack, NULL, tb, NET_SHAPER_A_HANDLE))
		return -EINVAL;

	ret = parse_handle(tb[NET_SHAPER_A_HANDLE], info, &shaper->handle);
	if (ret)
		return ret;

	/* TODO: should we fetch current shaper values - if any - and
	 * let NL provided values just override the existing fields?
	 */

	if (tb[NET_SHAPER_A_PARENT]) {
		ret = parse_handle(tb[NET_SHAPER_A_PARENT], info,
				   &shaper->parent);
		if (ret)
			return ret;
	}

	if (tb[NET_SHAPER_A_METRIC])
		shaper->metric = nla_get_u32(tb[NET_SHAPER_A_METRIC]);

	if (tb[NET_SHAPER_A_BW_MIN])
		shaper->bw_min = nla_get_u64(tb[NET_SHAPER_A_BW_MIN]);

	if (tb[NET_SHAPER_A_BW_MAX])
		shaper->bw_max = nla_get_u64(tb[NET_SHAPER_A_BW_MAX]);

	if (tb[NET_SHAPER_A_BURST])
		shaper->burst = nla_get_u64(tb[NET_SHAPER_A_BURST]);

	if (tb[NET_SHAPER_A_PRIORITY])
		shaper->priority = nla_get_u32(tb[NET_SHAPER_A_PRIORITY]);

	if (tb[NET_SHAPER_A_WEIGHT])
		shaper->weight = nla_get_u32(tb[NET_SHAPER_A_WEIGHT]);

	return 0;
}

static int net_shaper_set(struct net_device *dev, int nr,
			  const struct net_shaper_info *shapers,
			  struct netlink_ext_ack *extack)
{
	struct net_shaper_info *cur;
	unsigned long index;
	struct xarray *xa;
	int i, ret;

	if (!dev->net_shaper_data) {
		dev->net_shaper_data = kmalloc(sizeof(struct net_shaper_data),
					       GFP_KERNEL);
		if (!dev->net_shaper_data) {
			NL_SET_ERR_MSG(extack, "Can't allocate memory for shaper data");
			return -ENOMEM;
		}

		xa_init(&dev->net_shaper_data->shapers);
	}

	/* allocate the memory for newly crated shapers. While at that,
	 * tentatively insert into the shaper store
	 */
	ret = -ENOMEM;
	xa = &dev->net_shaper_data->shapers;
	for (i = 0; i < nr; ++i) {
		if (xa_load(xa, shapers[i].handle))
			continue;

		cur = kmalloc(sizeof(struct net_shaper_info), GFP_KERNEL);
		if (!cur)
			goto out;

		*cur = shapers[i];
		xa_lock(xa);
		__xa_store(xa, shapers[i].handle, cur, GFP_KERNEL);
		__xa_set_mark(xa, shapers[i].handle, XA_MARK_0);
		xa_unlock(xa);
	}

	ret = dev->netdev_ops->net_shaper_ops->set(dev, nr, shapers, extack);

out:
	/* free tentative shaper on errors */
	xa_lock(xa);
	xa_for_each_marked(xa, index, cur, XA_MARK_0) {
		if (ret) {
			__xa_erase(xa, index);
			kfree(cur);
		} else {
			__xa_clear_mark(xa, index, XA_MARK_0);
		}
	}
	xa_unlock(xa);
	return ret;
}

int net_shaper_nl_set_doit(struct sk_buff *skb, struct genl_info *info)
{
	struct net_device *dev = fetch_dev(info);
	struct net_shaper_info *shapers;
	int i, ret, nr_shapers, rem;
	struct nlattr *attr;

	if (!dev)
		return -EINVAL;

	nr_shapers = attr_list_len(info, NET_SHAPER_A_SHAPERS);
	shapers = kcalloc(nr_shapers, sizeof(struct net_shaper_info), GFP_KERNEL);
	if (!shapers) {
		ret = -ENOMEM;
		GENL_SET_ERR_MSG_FMT(info, "Can't allocate memory for %d shapers",
				     nr_shapers);
		goto put;
	}

	i = -1;
	nla_for_each_attr_type(attr, NET_SHAPER_A_SHAPERS,
			       genlmsg_data(info->genlhdr),
			       genlmsg_len(info->genlhdr), rem) {

		if (WARN_ON_ONCE(++i >= nr_shapers))
			goto free_shapers;

		ret = parse_shaper(attr, info, &shapers[i]);
		if (ret)
			goto free_shapers;
	}

	ret = net_shaper_set(dev, nr_shapers, shapers, info->extack);

free_shapers:
	kfree(shapers);

put:
	dev_put(dev);
	return ret;
}

static int net_shaper_delete(struct net_device *dev, int nr,
			     const u32 *handles,
			     struct netlink_ext_ack *extack)
{
	struct xarray *xa = &dev->net_shaper_data->shapers;
	struct net_shaper_info *cur;
	int i, ret;

	ret = dev->netdev_ops->net_shaper_ops->delete(dev, nr, handles,
						      extack);
	if (ret || !xa)
		return ret;

	xa_lock(xa);
	for (i = 0; i < nr; ++i) {
		cur = xa_load(xa, handles[i]);
		__xa_erase(xa, handles[i]);
		kfree(cur);
	}
	xa_unlock(xa);
	return 0;
}

int net_shaper_nl_delete_doit(struct sk_buff *skb, struct genl_info *info)
{
	struct net_device *dev = fetch_dev(info);
	int i, ret, nr_handles, rem;
	struct nlattr *attr;
	u32 *handles;

	if (!dev)
		return -EINVAL;

	nr_handles = attr_list_len(info, NET_SHAPER_A_HANDLES);
	handles = kmalloc_array(nr_handles, sizeof(u32), GFP_KERNEL);
	if (!handles) {
		ret = -ENOMEM;
		GENL_SET_ERR_MSG_FMT(info, "Can't allocate memory for %d handles",
				     nr_handles);
		goto put;
	}

	i = -1;
	nla_for_each_attr_type(attr, NET_SHAPER_A_HANDLES,
			       genlmsg_data(info->genlhdr),
			       genlmsg_len(info->genlhdr), rem) {

		if (WARN_ON_ONCE(++i >= nr_handles))
			goto free_handles;

		ret = parse_handle(attr, info, &handles[i]);
		if (ret)
			goto free_handles;
	}

	ret = net_shaper_delete(dev, nr_handles, handles, info->extack);

free_handles:
	kfree(handles);

put:
	dev_put(dev);
	return ret;
}

void dev_shaper_flush(struct net_device *dev)
{
	struct net_shaper_info *cur;
	unsigned long index;
	struct xarray *xa;

	if (!dev->net_shaper_data)
		return;

	xa = &dev->net_shaper_data->shapers;
	xa_lock(xa);
	xa_for_each(xa, index, cur) {
		__xa_erase(xa, index);
		kfree(cur);
	}
	xa_unlock(xa);
	kfree(xa);
}

static int __init shaper_init(void)
{
	return genl_register_family(&net_shaper_nl_family);
}

subsys_initcall(shaper_init);
