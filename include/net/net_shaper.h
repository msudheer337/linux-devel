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
 * @metric: Specify if the bw limits refers to PPS or BPS
 * @bw_min: Minimum guaranteed rate for this shaper
 * @bw_max: Maximum peak bw allowed for this shaper
 * @burst: Maximum burst for the peek rate of this shaper
 * @priority: Scheduling priority for this shaper
 * @weight: Scheduling weight for this shaper
 */
struct net_shaper_info {
	enum net_shaper_metric metric;
	u64 bw_min;	/* minimum guaranteed bandwidth, according to metric */
	u64 bw_max;	/* maximum allowed bandwidth */
	u32 burst;	/* maximum burst in bytes for bw_max */
	u32 priority;	/* scheduling strict priority */
	u32 weight;	/* scheduling WRR weight*/
};

/**
 * enum net_shaper_scope - the different scopes where a shaper could be attached
 * @NET_SHAPER_SCOPE_PORT:   The root shaper for the whole H/W.
 * @NET_SHAPER_SCOPE_NETDEV: The main shaper for the given network device.
 * @NET_SHAPER_SCOPE_VF:     The shaper is attached to the given virtual
 * function.
 * @NET_SHAPER_SCOPE_QUEUE_GROUP: The shaper groups multiple queues under the
 * same device.
 * @NET_SHAPER_SCOPE_QUEUE:  The shaper is attached to the given device queue.
 *
 * NET_SHAPER_SCOPE_PORT and NET_SHAPER_SCOPE_VF are only available on
 * PF devices, usually inside the host/hypervisor.
 * NET_SHAPER_SCOPE_NETDEV, NET_SHAPER_SCOPE_QUEUE_GROUP and
 * NET_SHAPER_SCOPE_QUEUE are available on both PFs and VFs devices.
 */
enum net_shaper_scope {
	NET_SHAPER_SCOPE_PORT,
	NET_SHAPER_SCOPE_NETDEV,
	NET_SHAPER_SCOPE_VF,
	NET_SHAPER_SCOPE_QUEUE_GROUP,
	NET_SHAPER_SCOPE_QUEUE,
};

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
	 * @nr: The number of items in the @handles and @shapers array
	 * @handles: The shapers identifier
	 * @shapers: onfiguration of shaper.
	 * @extack: Netlink extended ACK for reporting errors.
	 *
	 * Return:
	 * * %0 - Success
	 * * %-EOPNOTSUPP - Operation is not supported by hardware, driver,
	 *                  or core for any reason. @extack should be set to
	 *                  text describing the reason.
	 * * Other negative error values on failure.
	 */
	int (*set)(struct net_device *dev, int nr, const u32 *handles,
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

	/** Move - change the parent id of the specified shapers
	 * @dev: netdevice to operate on.
	 * @nr: The number of entries in the @handles and @new_parent_id
	 * @handles: unique identifier for the shapers to be moved
	 * @new_parent_ids: identifier of the new parents for these shapers
	 * @extack: Netlink extended ACK for reporting errors.
	 *
	 * Move the specified shapers in the hierarchy replacing its
	 * current parent shapers with @new_parent_id
	 *
	 * Return:
	 * * %0 - Success
	 * * %-EOPNOTSUPP - Operation is not supported by hardware, driver,
	 *                  or core for any reason. @extack should be set to
	 *                  text describing the reason.
	 * * Other negative error values on failure.
	 */
	int (*move)(struct net_device *dev, int nr, const u32 *handles,
		    const u32 *new_parent_handles, struct netlink_ext_ack *extack);
};

#define NET_SHAPER_MAJOR_SHIFT	16
#define NET_SHAPER_SCOPE_SHIFT	28
#define NET_SHAPER_MINOR_MASK	GENMASK(NET_SHAPER_MAJOR_SHIFT, 0)
#define NET_SHAPER_MAJOR_MASK	GENMASK(NET_SHAPER_SCOPE_SHIFT, NET_SHAPER_MAJOR_SHIFT)
#define NET_SHAPER_SCOPE_MASK	GENMASK(32, NET_SHAPER_SCOPE_SHIFT)

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
 *   u32 handle = shaper_make_handle(NET_SHAPER_SCOPE_QUEUE, 0, queue_id);
 *   dev->shaper_ops->set(dev, 1, &handle, &info, NULL);
 *
 * - create a queue group with a queue group shaping limits.
 *   Assuming the following topology already exists:
 *                       < netdev shaper  >
 *                        /              \
 *               <queue 0 shaper> . . .  <queue N shaper>
 *
 *   struct shaper_info ginfo = { }; // fill this
 *   u32 ghandle = shaper_make_handle(NET_SHAPER_SCOPE_QUEUE_GROUP, 0, 0);
 *   dev->shaper_ops->set(dev, 1, &ghandle, &ginfo);
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
 *       u32 qhandle = net_shaper_make_handle(NET_SHAPER_SCOPE_QUEUE, 0, i);
 *       dev->netshaper_ops->move(dev, 1, &qhandle, &ghandle, NULL);
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

