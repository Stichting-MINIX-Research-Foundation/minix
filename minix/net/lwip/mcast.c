/* LWIP service - mcast.c - per-socket multicast membership tracking */
/*
 * Each socket has a linked list of multicast groups of which it is a member.
 * The linked list consists of 'mcast_member' elements.  There is both a global
 * limit (the number of elements in 'mcast_array') and a per-socket limit on
 * group membership.  Since multiple sockets may join the same multicast
 * groups, there is not a one-to-one relationship between our membership
 * structures and the lwIP IGMP/MLD membership structures.  Moreover, linking
 * to the latter structures directly is not intended by lwIP, so we have to
 * keep our own tracking independent, which in particular means that we have to
 * make a copy of the multicast group address.
 *
 * We currently put no effort into saving memory on storing that group address.
 * Optimization is complicated by the fact that we have to be able to remove
 * membership structures when their corresponding interface disappears, which
 * currently involves removal without knowing about the corresponding socket,
 * and therefore the socket's address family.  All of this can be changed.
 *
 * There is no function to test whether a particular socket is a member of a
 * multicast group.  The pktsock module currently makes the assumption that if
 * a socket has been joined to any multicast groups, or set any multicast
 * options, the application is multicast aware and therefore able to figure out
 * whether it is interested in particular packets, and so we do not filter
 * incoming packets against the receiving socket's multicast list.  This should
 * be more or less in line with what W. Richard Stevens say that the BSDs do.
 */

#include "lwip.h"
#include "mcast.h"

#include "lwip/igmp.h"
#include "lwip/mld6.h"

/*
 * The per-socket limit on group membership.  In theory, the limit should be
 * high enough that a single socket can join a particular multicast group on
 * all interfaces that support multicast.  In practice, we set it a bit lower
 * to prevent one socket from using up half of the entries per address family.
 * Setting it to IP_MAX_MEMBERSHIPS is definitely excessive right now..
 */
#define MAX_GROUPS_PER_SOCKET	8

static struct mcast_member {
	LIST_ENTRY(mcast_member) mm_next;	/* next in socket, free list */
	struct ifdev * mm_ifdev;		/* interface (NULL: free) */
	ip_addr_t mm_group;			/* group address */
} mcast_array[NR_IPV4_MCAST_GROUP + NR_IPV6_MCAST_GROUP];

static LIST_HEAD(, mcast_member) mcast_freelist;

/*
 * Initialize the per-socket multicast membership module.
 */
void
mcast_init(void)
{
	unsigned int slot;

	/* Initialize the list of free multicast membership entries. */
	LIST_INIT(&mcast_freelist);

	for (slot = 0; slot < __arraycount(mcast_array); slot++) {
		mcast_array[slot].mm_ifdev = NULL;

		LIST_INSERT_HEAD(&mcast_freelist, &mcast_array[slot], mm_next);
	}
}

/*
 * Reset the multicast head for a socket.  The socket must not have any
 * previous multicast group memberships.
 */
void
mcast_reset(struct mcast_head * mcast_head)
{

	LIST_INIT(&mcast_head->mh_list);
}

/*
 * Attempt to add a per-socket multicast membership association.  The given
 * 'mcast_head' pointer is part of a socket.  The 'group' parameter is the
 * multicast group to join.  It is a properly zoned address, but has not been
 * checked in any other way.  If 'ifdev' is not NULL, it is the interface for
 * the membership; if it is NULL, an interface will be selected using routing.
 * Return OK if the membership has been successfully removed, or a negative
 * error code otherwise.
 */
int
mcast_join(struct mcast_head * mcast_head, const ip_addr_t * group,
	struct ifdev * ifdev)
{
	struct mcast_member *mm;
	struct netif *netif;
	unsigned int count;
	err_t err;

	/*
	 * The callers of this function perform only checks that depend on the
	 * address family.  We check everything else here.
	 */
	if (!ip_addr_ismulticast(group))
		return EADDRNOTAVAIL;

	if (!addr_is_valid_multicast(group))
		return EINVAL;

	/*
	 * If no interface was specified, pick one with a routing query.  Note
	 * that scoped IPv6 addresses do require an interface to be specified.
	 */
	if (ifdev == NULL) {
		netif = ip_route(IP46_ADDR_ANY(IP_GET_TYPE(group)), group);

		if (netif == NULL)
			return EHOSTUNREACH;

		ifdev = netif_get_ifdev(netif);
	}

	assert(ifdev != NULL);
	assert(!IP_IS_V6(group) ||
	    !ip6_addr_lacks_zone(ip_2_ip6(group), IP6_MULTICAST));

	/* The interface must support multicast. */
	if (!(ifdev_get_ifflags(ifdev) & IFF_MULTICAST))
		return EADDRNOTAVAIL;

	/*
	 * First see if this socket is already joined to the given group, which
	 * is an error.  While looking, also count the number of groups the
	 * socket has joined already, to enforce the per-socket limit.
	 */
	count = 0;

	LIST_FOREACH(mm, &mcast_head->mh_list, mm_next) {
		if (mm->mm_ifdev == ifdev && ip_addr_cmp(&mm->mm_group, group))
			return EEXIST;

		count++;
	}

	if (count >= MAX_GROUPS_PER_SOCKET)
		return ENOBUFS;

	/* Do we have a free membership structure available? */
	if (LIST_EMPTY(&mcast_freelist))
		return ENOBUFS;

	/*
	 * Nothing can go wrong as far as we are concerned.  Ask lwIP to join
	 * the multicast group.  This may result in a multicast list update at
	 * the driver end.
	 */
	netif = ifdev_get_netif(ifdev);

	if (IP_IS_V6(group))
		err = mld6_joingroup_netif(netif, ip_2_ip6(group));
	else
		err = igmp_joingroup_netif(netif, ip_2_ip4(group));

	if (err != ERR_OK)
		return util_convert_err(err);

	/*
	 * Success.  Allocate, initialize, and attach a membership structure to
	 * the socket.
	 */
	mm = LIST_FIRST(&mcast_freelist);

	LIST_REMOVE(mm, mm_next);

	mm->mm_ifdev = ifdev;
	mm->mm_group = *group;

	LIST_INSERT_HEAD(&mcast_head->mh_list, mm, mm_next);

	return OK;
}

/*
 * Free the given per-socket multicast membership structure, which must
 * previously have been associated with a socket.  If 'leave_group' is set,
 * also tell lwIP to leave the corresponding multicast group.
 */
static void
mcast_free(struct mcast_member * mm, int leave_group)
{
	struct netif *netif;
	err_t err;

	assert(mm->mm_ifdev != NULL);

	if (leave_group) {
		netif = ifdev_get_netif(mm->mm_ifdev);

		if (IP_IS_V6(&mm->mm_group))
			err = mld6_leavegroup_netif(netif,
			    ip_2_ip6(&mm->mm_group));
		else
			err = igmp_leavegroup_netif(netif,
			    ip_2_ip4(&mm->mm_group));

		if (err != ERR_OK)
			panic("lwIP multicast membership desynchronization");
	}

	LIST_REMOVE(mm, mm_next);

	mm->mm_ifdev = NULL;

	LIST_INSERT_HEAD(&mcast_freelist, mm, mm_next);
}

/*
 * Attempt to remove a per-socket multicast membership association.  The given
 * 'mcast_head' pointer is part of a socket.  The 'group' parameter is the
 * multicast group to leave.  It is a properly zoned address, but has not been
 * checked in any other way.  If 'ifdev' is not NULL, it is the interface of
 * the membership; if it is NULL, a membership matching the address on any
 * interface will suffice.  As such, the parameter requirements mirror those of
 * mcast_join().  Return OK if the membership has been successfully removed, or
 * a negative error code otherwise.
 */
int
mcast_leave(struct mcast_head * mcast_head, const ip_addr_t * group,
	struct ifdev * ifdev)
{
	struct mcast_member *mm;

	/*
	 * Look up a matching entry.  The fact that we must find a match for
	 * the given address and interface, keeps us from having to perform
	 * various other checks, such as whether the given address is a
	 * multicast address at all.  The exact error codes are not specified.
	 */
	LIST_FOREACH(mm, &mcast_head->mh_list, mm_next) {
		if ((ifdev == NULL || mm->mm_ifdev == ifdev) &&
		    ip_addr_cmp(&mm->mm_group, group))
			break;
	}

	if (mm == NULL)
		return ESRCH;

	mcast_free(mm, TRUE /*leave_group*/);

	return OK;
}

/*
 * Remove all per-socket multicast membership associations of the given socket.
 * This function is called when the socket is closed.
 */
void
mcast_leave_all(struct mcast_head * mcast_head)
{
	struct mcast_member *mm;

	while (!LIST_EMPTY(&mcast_head->mh_list)) {
		mm = LIST_FIRST(&mcast_head->mh_list);

		mcast_free(mm, TRUE /*leave_group*/);
	}
}

/*
 * The given interface is about to disappear.  Remove and free any per-socket
 * multicast membership structures associated with the interface, without
 * leaving the multicast group itself (as that will happen a bit later anyway).
 */
void
mcast_clear(struct ifdev * ifdev)
{
	unsigned int slot;

	for (slot = 0; slot < __arraycount(mcast_array); slot++) {
		if (mcast_array[slot].mm_ifdev != ifdev)
			continue;

		mcast_free(&mcast_array[slot], FALSE /*leave_group*/);
	}
}
