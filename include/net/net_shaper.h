/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef _NET_SHAPER_H_
#define _NET_SHAPER_H_

#include <linux/types.h>
#include <linux/bits.h>
#include <linux/bitfield.h>
#include <linux/netdevice.h>
#include <linux/netlink.h>

#include <uapi/linux/net_shaper.h>

/**
 * struct net_shaper_info - represents a shaping node on the NIC H/W
 * zeroed field are considered not set.
 * @handle: Unique identifier for the shaper, see @net_shaper_make_handle
 * @parent: Unique identifier for the shaper parent, usually implied. Only
 *   NET_SHAPER_SCOPE_QUEUE, NET_SHAPER_SCOPE_NETDEV and NET_SHAPER_SCOPE_DETACHED
 *   can have the parent handle explictly set, placing such shaper under
 *   the specified parent.
 * @metric: Specify if the bw limits refers to PPS or BPS
 * @bw_min: Minimum guaranteed rate for this shaper
 * @bw_max: Maximum peak bw allowed for this shaper
 * @burst: Maximum burst for the peek rate of this shaper
 * @priority: Scheduling priority for this shaper
 * @weight: Scheduling weight for this shaper
 */
struct net_shaper_info {
	u32 handle;
	u32 parent;
	enum net_shaper_metric metric;
	u64 bw_min;	/* minimum guaranteed bandwidth, according to metric */
	u64 bw_max;	/* maximum allowed bandwidth */
	u64 burst;	/* maximum burst in bytes for bw_max */
	u32 priority;	/* scheduling strict priority */
	u32 weight;	/* scheduling WRR weight*/
};

/**
 * Shaper scope non exposed to user-space; the shaper is attached to the
 * given virtual function.
 *
 * NET_SHAPER_SCOPE_PORT and NET_SHAPER_SCOPE_VF are only available on
 * PF devices, usually inside the host/hypervisor.
 * NET_SHAPER_SCOPE_NETDEV, NET_SHAPER_SCOPE_QUEUE_GROUP and
 * NET_SHAPER_SCOPE_QUEUE are available on both PFs and VFs devices.
 */
#define NET_SHAPER_SCOPE_VF __NET_SHAPER_SCOPE_MAX

/**
 * struct net_shaper_ops - Operations on device H/W shapers
 * @add: Creates a new shaper in the specified scope.
 * @set: Modify the existing shaper.
 * @delete: Delete the specified shaper.
 * @move: Move an existing shaper under a different parent.
 *
 * The initial shaping configuration ad device initialization is empty/
 * a no-op/does not constraint the b/w in any way.
 * The network core keeps track of the applied user-configuration in
 * per device storage.
 *
 * Each shaper is uniquely identified within the device with an 'handle',
 * dependent on the shaper scope and other data, see @shaper_make_handle()
 */
struct net_shaper_ops {
	/** set - Update or create the specified shapers
	 * @dev: Netdevice to operate on.
	 * @nr: The number of items in the @shapers array
	 * @shapers: Configuration of shapers.
	 * @extack: Netlink extended ACK for reporting errors.
	 *
	 * Return:
	 * * %0 - Success
	 * * %-EOPNOTSUPP - Operation is not supported by hardware, driver,
	 *                  or core for any reason. @extack should be set to
	 *                  text describing the reason.
	 * * Other negative error values on failure.
	 */
	int (*set)(struct net_device *dev, int nr,
		   const struct net_shaper_info *shapers,
		   struct netlink_ext_ack *extack);

	/** delete - Removes the specified shapers from the NIC
	 * @dev: netdevice to operate on
	 * @nr: The number of entries in the @handles array
	 * @handles: The shapers identifier
	 * @extack: Netlink extended ACK for reporting errors.
	 *
	 * Removes the shapers configuration, restoring the default behavior
	 *
	 * Return:
	 * * %0 - Success
	 * * %-EOPNOTSUPP - Operation is not supported by hardware, driver,
	 *                  or core for any reason. @extack should be set to
	 *                  text describing the reason.
	 * * Other negative error value on failure.
	 */
	int (*delete)(struct net_device *dev, int nr, const u32 *handles,
		      struct netlink_ext_ack *extack);
};

#define NET_SHAPER_MAJOR_SHIFT	16
#define NET_SHAPER_SCOPE_SHIFT	28
#define NET_SHAPER_MINOR_MASK	GENMASK(NET_SHAPER_MAJOR_SHIFT - 1, 0)
#define NET_SHAPER_MAJOR_MASK	GENMASK(NET_SHAPER_SCOPE_SHIFT - 1, NET_SHAPER_MAJOR_SHIFT)
#define NET_SHAPER_SCOPE_MASK	GENMASK(31, NET_SHAPER_SCOPE_SHIFT)

/**
 * net_shaper_make_handle - creates an unique shaper identifier
 * @scope: the shaper scope
 * @major: the shaper major number
 * @minor: the shaper minor number
 *
 * Return: an unique identifier for the shaper
 *
 * Combines the specified arguments to create an unique identifier for
 * the shaper. The @major and @minor arguments semantic depends on the
 * specificed scope.
 * For @NET_SHAPER_SCOPE_VF, @NET_SHAPER_SCOPE_QUEUE_GROUP and
 * @NET_SHAPER_SCOPE_QUEUE.@major refers to the VF number.
 * For @NET_SHAPER_SCOPE_QUEUE_GROUP, @minor is the queue group id
 * For @NET_SHAPER_SCOPE_QUEUE, @minor is the queue number.
 * For @NET_SHAPER_SCOPE_VF, @minor is ignored.
 * For all the other scopes both @minor and @major are ignored.
 */
static inline u32 net_shaper_make_handle(enum net_shaper_scope scope,
					 int major, int minor)
{
	return FIELD_PREP(NET_SHAPER_SCOPE_MASK, scope) |
		FIELD_PREP(NET_SHAPER_MAJOR_MASK, major) |
		FIELD_PREP(NET_SHAPER_MINOR_MASK, minor);
}

/**
 * net_shaper_handle_scope - extract the scope from the given handle
 * @handle: the shaper handle
 *
 * Return: the corresponding scope
 */
static inline enum net_shaper_scope net_shaper_handle_scope(u32 handle)
{
	return FIELD_GET(NET_SHAPER_SCOPE_MASK, handle);
}

/**
 * net_shaper_handle_major - extract the major number from the given handle
 * @handle: the shaper handle
 *
 * Return: the corresponding major number
 */
static inline int net_shaper_handle_major(u32 handle)
{
	return FIELD_GET(NET_SHAPER_MAJOR_MASK, handle);
}

/**
 * net_shaper_handle_minor - extract the minor number from the given handle
 * @handle: the shaper handle
 *
 * Return: the corresponding scope
 */
static inline int net_shaper_handle_minor(u32 handle)
{
	return FIELD_GET(NET_SHAPER_MINOR_MASK, handle);
}
/*
 * Examples:
 * - set shaping on a given queue
 *   struct shaper_info info = { }; // fill this
 *   info.handle = shaper_make_handle(NET_SHAPER_SCOPE_QUEUE, 0, queue_id);
 *   dev->netdev_ops->net_shaper_ops->set(dev, 1, &info, NULL);
 *
 * - create a queue group with a queue group shaping limits.
 *   Assuming the following topology already exists:
 *                       < netdev shaper  >
 *                        /              \
 *               <queue 0 shaper> . . .  <queue N shaper>
 *
 *   struct shaper_info ginfo = { }; // fill this
 *   u32 ghandle = shaper_make_handle(NET_SHAPER_SCOPE_DETACHED, 0, 0);
 *   ginfo.handle = ghandle;
 *   dev->netdev_ops->net_shaper_ops->set(dev, 1, &ginfo);
 *
 *   // now topology is:
 *   //                              < netdev shaper  >
 *   //                             /         |          \
 *   //                            /          |       < newly created shaper  >
 *   //                           /           |
 *   //	<queue 0 shaper> . . .    <queue N shaper>
 *
 *   // move a shapers for queues 3..n out of such queue group
 *   for (i = 0; i <= 2; ++i) {
 *       struct shaper_info qinfo = {}; // fill this
 *
 *       info.handle = net_shaper_make_handle(NET_SHAPER_SCOPE_QUEUE, 0, i);
 *       info.parent = ghandle;
 *       dev->netdev_ops->shaper_ops->set(dev, &ginfo, NULL);
 *   }
 *
 *   // now the topology is:
 *   //                                < netdev shaper  >
 *   //                                 /            \
 *   //               < newly created shaper>   <queue 3 shaper> .. <queue n shaper>
 *   //                /               \
 *   //	<queue 0 shaper> . . .    <queue 2 shaper>
 */
#endif

