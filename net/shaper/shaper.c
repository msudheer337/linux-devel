/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/xarray.h>
#include <net/net_shaper.h>

#include "shaper_nl_gen.h"

#include "../core/dev.h"

struct net_shaper_data {
	struct xarray shapers;
};

struct net_shaper_nl_ctx {
	u32 start_handle;
};

static u32 default_parent(u32 handle)
{
	enum net_shaper_scope parent, scope = net_shaper_handle_scope(handle);

	switch (scope) {
	case NET_SHAPER_SCOPE_DETACHED:
	case NET_SHAPER_SCOPE_PORT:
	case NET_SHAPER_SCOPE_UNSPEC:
		parent = NET_SHAPER_SCOPE_UNSPEC;
		break;

	case NET_SHAPER_SCOPE_QUEUE:
		parent = NET_SHAPER_SCOPE_NETDEV;
		break;

	case NET_SHAPER_SCOPE_NETDEV:
	case NET_SHAPER_SCOPE_VF:
		parent = NET_SHAPER_SCOPE_PORT;
		break;
	}

	return net_shaper_make_handle(parent, 0);
}

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
	    nla_put_u32(msg, NET_SHAPER_A_ID,
			net_shaper_handle_id(handle)))
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

/* On success sets pdev to the relevant device and acquires a reference
 * to it
 */
static int fetch_dev(const struct genl_info *info, int type,
		     struct net_device **pdev)
{
	struct net *ns = genl_info_net(info);
	struct net_device *dev;
	int ifindex;

	if (GENL_REQ_ATTR_CHECK(info, type))
		return -EINVAL;

	ifindex = nla_get_u32(info->attrs[type]);
	dev = dev_get_by_index(ns, ifindex);
	if (!dev) {
		GENL_SET_ERR_MSG_FMT(info, "device %d not found", ifindex);
		return -EINVAL;
	}

	if (!dev->netdev_ops->net_shaper_ops) {
		GENL_SET_ERR_MSG_FMT(info, "device %s does not support H/W shaper",
				     dev->name);
		dev_put(dev);
		return -EOPNOTSUPP;
	}

	*pdev = dev;
	return 0;
}

static int parse_handle(const struct nlattr *attr, const struct genl_info *info,
			u32 *handle)
{
	struct nlattr *tb[NET_SHAPER_A_ID + 1];
	struct nlattr *scope, *id;
	int ret;

	ret = nla_parse_nested(tb, NET_SHAPER_A_ID, attr,
			       net_shaper_handle_nl_policy, info->extack);
	if (ret < 0)
		return ret;

	scope = tb[NET_SHAPER_A_SCOPE];
	if (!scope) {
		GENL_SET_ERR_MSG(info, "Missing 'scope' attribute for handle");
		return -EINVAL;
	}

	id = tb[NET_SHAPER_A_ID];
	*handle = net_shaper_make_handle(nla_get_u32(scope),
					 id ? nla_get_u32(id) : 0);
	return 0;
}

int net_shaper_nl_get_doit(struct sk_buff *skb, struct genl_info *info)
{
	struct net_shaper_info *shaper;
	struct net_device *dev;
	struct sk_buff *msg;
	u32 handle;
	int ret;

	ret = fetch_dev(info, NET_SHAPER_A_IFINDEX, &dev);
	if (ret)
		return ret;

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
	if (!msg) {
		ret = -ENOMEM;
		goto put;
	}

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
	struct net_shaper_info *shaper;
	struct net_device *dev;
	unsigned long handle;
	int ret;

	ret = fetch_dev(info, NET_SHAPER_A_IFINDEX, &dev);
	if (ret)
		return ret;

	BUILD_BUG_ON(sizeof(struct net_shaper_nl_ctx) > sizeof(cb->ctx));

	if (!dev->net_shaper_data) {
		ret = 0;
		goto put;
	}

	xa_for_each_range(&dev->net_shaper_data->shapers, handle, shaper,
			  ctx->start_handle, U32_MAX) {
		ret = net_shaper_fill_one(skb, shaper, info);
		if (ret)
			goto put;

		ctx->start_handle = handle;
	}

put:
	dev_put(dev);
	return ret;
}

/* count the number of [multi] attributes of the given type */
static int attr_list_len(struct genl_info *info, int type)
{
	struct nlattr *attr;
	int rem, cnt = 0;

	nla_for_each_attr_type(attr, type, genlmsg_data(info->genlhdr),
			       genlmsg_len(info->genlhdr), rem)
		cnt++;
	return cnt;
}

/* fetch the cached shaper info and update them with the user-provided
 * attributes
 */
static int fill_shaper(struct net_device *dev, const struct nlattr *attr,
		       const struct genl_info *info,
		       struct net_shaper_info *shaper)
{
	struct xarray *xa = &dev->net_shaper_data->shapers;
	struct nlattr *tb[NET_SHAPER_A_MAX + 1];
	int ret;

	ret = nla_parse_nested(tb, NET_SHAPER_A_MAX, attr,
			       net_shaper_ns_info_nl_policy, info->extack);
	if (ret < 0)
		return ret;

	/* the shaper handle is the only mandatory attribute */
	if (NL_REQ_ATTR_CHECK(info->extack, NULL, tb, NET_SHAPER_A_HANDLE))
		return -EINVAL;

	ret = parse_handle(tb[NET_SHAPER_A_HANDLE], info, &shaper->handle);
	if (ret)
		return ret;

	/* fetch existing data, if any, so that user provide info will
	 * incrementally update the existing shaper configuration
	 */
	if (xa) {
		struct net_shaper_info *old = xa_load(xa, shaper->handle);

		if (old)
			*shaper = *old;
	}

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

/* Update the H/W and on success update the local cache, too */
static int net_shaper_set(struct net_device *dev, int nr,
			  struct net_shaper_info *shapers,
			  struct netlink_ext_ack *extack)
{
	struct net_shaper_info *cur, *prev;
	unsigned long index;
	struct xarray *xa;
	int i, ret;

	/* allocate on demand the per device shaper's storage */
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
		/* ensure 'parent' is non zero only when the driver must move
		 * the shaper around
		 */
		prev = xa_load(xa, shapers[i].handle);
		if (prev) {
			if (shapers[i].parent == prev->parent)
				shapers[i].parent = 0;
			continue;
		}
		if (shapers[i].parent == default_parent(shapers[i].handle))
			shapers[i].parent = 0;

		cur = kmalloc(sizeof(struct net_shaper_info), GFP_KERNEL);
		if (!cur)
			goto out;

		*cur = shapers[i];
		xa_lock(xa);
		prev = __xa_store(xa, shapers[i].handle, cur, GFP_KERNEL);
		__xa_set_mark(xa, shapers[i].handle, XA_MARK_0);
		xa_unlock(xa);
		if (xa_err(prev)) {
			NL_SET_ERR_MSG(extack, "Can't update shaper store");
			ret = xa_err(prev);
			goto out;
		}
	}

	ret = dev->netdev_ops->net_shaper_ops->set(dev, nr, shapers, extack);

	/* be careful with possibly bugged drivers */
	if (WARN_ON_ONCE(ret > nr))
		ret = nr;

out:
	/* commit updated shapers and free failed tentative ones */
	xa_lock(xa);
	for (i = 0; i < ret; ++i) {
		cur = xa_load(xa, shapers[i].handle);
		if (WARN_ON_ONCE(!cur))
			continue;

		__xa_clear_mark(xa, shapers[i].handle, XA_MARK_0);
		*cur = shapers[i];

		/* ensure that get operation always specify the
		 * parent handle
		 */
		if (net_shaper_handle_scope(cur->parent) ==
		    NET_SHAPER_SCOPE_UNSPEC)
			cur->parent = default_parent(cur->handle);
	}
	xa_for_each_marked(xa, index, cur, XA_MARK_0) {
		__xa_erase(xa, index);
		kfree(cur);
	}
	xa_unlock(xa);
	return ret;
}

static int modify_send_reply(struct genl_info *info, int modified)
{
	struct sk_buff *msg;
	void *hdr;
	int ret;

	msg = nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	hdr = genlmsg_iput(msg, info);
	if (!hdr)
		goto free_msg;

	if (nla_put_u32(msg, NET_SHAPER_A_MODIFIED, modified))
		goto cancel_msg;

	genlmsg_end(msg, hdr);

	ret =  genlmsg_reply(msg, info);
	if (ret)
		goto free_msg;

	return ret;

cancel_msg:
	ret = -EMSGSIZE;
	genlmsg_cancel(msg, hdr);

free_msg:
	nlmsg_free(msg);
	return ret;
}

int net_shaper_nl_set_doit(struct sk_buff *skb, struct genl_info *info)
{
	struct net_shaper_info *shapers;
	int i, ret, nr_shapers, rem;
	struct net_device *dev;
	struct nlattr *attr;

	ret = fetch_dev(info, NET_SHAPER_A_IFINDEX, &dev);
	if (ret)
		return ret;

	nr_shapers = attr_list_len(info, NET_SHAPER_A_SHAPERS);
	shapers = kcalloc(nr_shapers, sizeof(struct net_shaper_info), GFP_KERNEL);
	if (!shapers) {
		GENL_SET_ERR_MSG_FMT(info, "Can't allocate memory for %d shapers",
				     nr_shapers);
		ret = -ENOMEM;
		goto put;
	}

	i = 0;
	nla_for_each_attr_type(attr, NET_SHAPER_A_SHAPERS,
			       genlmsg_data(info->genlhdr),
			       genlmsg_len(info->genlhdr), rem) {
		if (WARN_ON_ONCE(i >= nr_shapers))
			goto free_shapers;

		ret = fill_shaper(dev, attr, info, &shapers[i++]);
		if (ret)
			goto free_shapers;
	}

	ret = net_shaper_set(dev, nr_shapers, shapers, info->extack);
	if (ret < 0)
		goto free_shapers;

	ret = modify_send_reply(info, ret);

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
	if (ret < 0 || !xa)
		return ret;

	/* be careful with possibly bugged drivers */
	if (WARN_ON_ONCE(ret > nr))
		ret = nr;

	xa_lock(xa);
	for (i = 0; i < ret; ++i) {
		cur = xa_load(xa, handles[i]);
		__xa_erase(xa, handles[i]);
		kfree(cur);
	}
	xa_unlock(xa);
	return ret;
}

int net_shaper_nl_delete_doit(struct sk_buff *skb, struct genl_info *info)
{
	int i, ret, nr_handles, rem;
	struct net_device *dev;
	struct nlattr *attr;
	u32 *handles;

	ret = fetch_dev(info, NET_SHAPER_A_IFINDEX, &dev);
	if (ret)
		return ret;

	nr_handles = attr_list_len(info, NET_SHAPER_A_HANDLES);
	handles = kmalloc_array(nr_handles, sizeof(u32), GFP_KERNEL);
	if (!handles) {
		ret = -ENOMEM;
		GENL_SET_ERR_MSG_FMT(info, "Can't allocate memory for %d handles",
				     nr_handles);
		goto put;
	}

	i = 0;
	nla_for_each_attr_type(attr, NET_SHAPER_A_HANDLES,
			       genlmsg_data(info->genlhdr),
			       genlmsg_len(info->genlhdr), rem) {

		if (WARN_ON_ONCE(i >= nr_handles))
			goto free_handles;

		ret = parse_handle(attr, info, &handles[i++]);
		if (ret)
			goto free_handles;
	}

	ret = net_shaper_delete(dev, nr_handles, handles, info->extack);
	if (ret < 0)
		goto free_handles;

	ret = modify_send_reply(info, ret);

free_handles:
	kfree(handles);

put:
	dev_put(dev);
	return ret;
}

static int
net_shaper_cap_fill_one(struct sk_buff *msg, unsigned long flags,
			const struct genl_info *info)
{
	unsigned long cur;
	void *hdr;

	hdr = genlmsg_iput(msg, info);
	if (!hdr)
		return -EMSGSIZE;

	for (cur = NET_SHAPER_A_CAPABILITIES_SUPPORT_METRIC_BPS;
	     cur <= NET_SHAPER_A_CAPABILITIES_MAX; ++cur) {
		if (flags & BIT(cur) && nla_put_flag(msg, cur))
			goto nla_put_failure;
	}

	genlmsg_end(msg, hdr);

	return 0;

nla_put_failure:
	genlmsg_cancel(msg, hdr);
	return -EMSGSIZE;
}

int net_shaper_nl_cap_get_doit(struct sk_buff *skb, struct genl_info *info)
{
	const struct net_shaper_ops *ops;
	enum net_shaper_scope scope;
	struct net_device *dev;
	struct sk_buff *msg;
	unsigned long flags;
	int ret;

	ret = fetch_dev(info, NET_SHAPER_A_CAPABILITIES_IFINDEX, &dev);
	if (ret)
		return ret;

	if (GENL_REQ_ATTR_CHECK(info, NET_SHAPER_A_CAPABILITIES_SCOPE)) {
		ret = -EINVAL;
		goto put;
	}

	scope = nla_get_u32(info->attrs[NET_SHAPER_A_CAPABILITIES_SCOPE]);
	ops = dev->netdev_ops->net_shaper_ops;
	ret = ops->capabilities(dev, scope, &flags);
	if (ret)
		goto put;

	msg = nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_KERNEL);
	if (!msg)
		goto put;

	ret = net_shaper_cap_fill_one(msg, flags, info);
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

int net_shaper_nl_cap_get_dumpit(struct sk_buff *skb,
				 struct netlink_callback *cb)
{
	const struct genl_info *info = genl_info_dump(cb);
	const struct net_shaper_ops *ops;
	enum net_shaper_scope scope;
	struct net_device *dev;
	unsigned long flags;
	int ret;

	ret = fetch_dev(info, NET_SHAPER_A_CAPABILITIES_IFINDEX, &dev);
	if (ret)
		return ret;

	ops = dev->netdev_ops->net_shaper_ops;
	for (scope = 0; scope <= NET_SHAPER_SCOPE_MAX; ++scope) {
		if (ops->capabilities(dev, scope, &flags))
			continue;

		ret = net_shaper_cap_fill_one(skb, flags, info);
		if (ret)
			goto put;
	}

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
