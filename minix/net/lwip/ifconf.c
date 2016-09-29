/* LWIP service - ifconf.c - interface configuration */

#include "lwip.h"
#include "ifaddr.h"
#include "lldata.h"

#include <net/if_media.h>
#include <minix/if.h>

#define LOOPBACK_IFNAME		"lo0"	/* name of the loopback interface */

/*
 * Initialize the first loopback device, which is present by default.
 */
void
ifconf_init(void)
{
	const struct sockaddr_in addr = {
	    .sin_family = AF_INET,
	    .sin_addr = { htonl(INADDR_LOOPBACK) }
	};
	struct sockaddr_in6 ll_addr6 = {
	    .sin6_family = AF_INET6,
	};
	const struct sockaddr_in6 lo_addr6 = {
	    .sin6_family = AF_INET6,
	    .sin6_addr = IN6ADDR_LOOPBACK_INIT
	};
	const struct in6_addrlifetime lifetime = {
	    .ia6t_vltime = ND6_INFINITE_LIFETIME,
	    .ia6t_pltime = ND6_INFINITE_LIFETIME
	};
	struct sockaddr_in6 mask6;
	struct ifdev *ifdev;
	socklen_t addr_len;
	int r;

	if ((r = ifdev_create(LOOPBACK_IFNAME)) != OK)
		panic("unable to create loopback interface: %d", r);

	if ((ifdev = ifdev_find_by_name(LOOPBACK_IFNAME)) == NULL)
		panic("unable to find loopback interface");

	if ((r = ifaddr_v4_add(ifdev, &addr, NULL, NULL, NULL, 0)) != OK)
		panic("unable to set IPv4 address on loopback interface: %d",
		    r);

	addr_len = sizeof(mask6);
	addr_put_netmask((struct sockaddr *)&mask6, &addr_len, IPADDR_TYPE_V6,
	    64 /*prefix*/);

	ll_addr6.sin6_addr.s6_addr[0] = 0xfe;
	ll_addr6.sin6_addr.s6_addr[1] = 0x80;
	ll_addr6.sin6_addr.s6_addr[15] = ifdev_get_index(ifdev);

	if ((r = ifaddr_v6_add(ifdev, &ll_addr6, &mask6, NULL, 0,
	    &lifetime)) != OK)
		panic("unable to set IPv6 address on loopback interface: %d",
		    r);

	addr_len = sizeof(mask6);
	addr_put_netmask((struct sockaddr *)&mask6, &addr_len, IPADDR_TYPE_V6,
	    128 /*prefix*/);

	if ((r = ifaddr_v6_add(ifdev, &lo_addr6, &mask6, NULL, 0,
	    &lifetime)) != OK)
		panic("unable to set IPv6 address on loopback interface: %d",
		    r);

	if ((r = ifdev_set_ifflags(ifdev, IFF_UP)) != OK)
		panic("unable to bring up loopback interface");
}

/*
 * Process an address family independent IOCTL request with an "ifreq"
 * structure.
 */
static int
ifconf_ioctl_ifreq(unsigned long request, const struct sockdriver_data * data)
{
	struct ifdev *ifdev;
	struct ifreq ifr;
	int r;

	if ((r = sockdriver_copyin(data, 0, &ifr, sizeof(ifr))) != OK)
		return r;

	if (request != SIOCIFCREATE) {
		ifr.ifr_name[sizeof(ifr.ifr_name) - 1] = '\0';

		if ((ifdev = ifdev_find_by_name(ifr.ifr_name)) == NULL)
			return ENXIO;
	} else
		ifdev = NULL;

	switch (request) {
	case SIOCGIFFLAGS:
		ifr.ifr_flags = ifdev_get_ifflags(ifdev);

		return sockdriver_copyout(data, 0, &ifr, sizeof(ifr));

	case SIOCSIFFLAGS:
		/*
		 * Unfortunately, ifr_flags is a signed integer and the sign
		 * bit is in fact used as a flag, so without explicit casting
		 * we end up setting all upper bits of the (full) integer.  If
		 * NetBSD ever extends the field, this assert should trigger..
		 */
		assert(sizeof(ifr.ifr_flags) == sizeof(short));

		return ifdev_set_ifflags(ifdev, (unsigned short)ifr.ifr_flags);

	case SIOCGIFMETRIC:
		ifr.ifr_metric = ifdev_get_metric(ifdev);

		return sockdriver_copyout(data, 0, &ifr, sizeof(ifr));

	case SIOCSIFMETRIC:
		/* The metric is not used within the operating system. */
		ifdev_set_metric(ifdev, ifr.ifr_metric);

		return OK;

	case SIOCSIFMEDIA:
		return ifdev_set_ifmedia(ifdev, ifr.ifr_media);

	case SIOCGIFMTU:
		ifr.ifr_mtu = ifdev_get_mtu(ifdev);

		return sockdriver_copyout(data, 0, &ifr, sizeof(ifr));

	case SIOCSIFMTU:
		return ifdev_set_mtu(ifdev, ifr.ifr_mtu);

	case SIOCIFCREATE:
		if (memchr(ifr.ifr_name, '\0', sizeof(ifr.ifr_name)) == NULL)
			return EINVAL;

		return ifdev_create(ifr.ifr_name);

	case SIOCIFDESTROY:
		return ifdev_destroy(ifdev);

	case SIOCGIFDLT:
		ifr.ifr_dlt = ifdev_get_dlt(ifdev);

		return sockdriver_copyout(data, 0, &ifr, sizeof(ifr));

	case SIOCGIFINDEX:
		ifr.ifr_index = ifdev_get_index(ifdev);

		return sockdriver_copyout(data, 0, &ifr, sizeof(ifr));

	default:
		return ENOTTY;
	}
}

/*
 * Process an address family independent IOCTL request with an "ifcapreq"
 * structure.
 */
static int
ifconf_ioctl_ifcap(unsigned long request,
	const struct sockdriver_data * data)
{
	struct ifdev *ifdev;
	struct ifcapreq ifcr;
	int r;

	if ((r = sockdriver_copyin(data, 0, &ifcr, sizeof(ifcr))) != OK)
		return r;

	ifcr.ifcr_name[sizeof(ifcr.ifcr_name) - 1] = '\0';

	if ((ifdev = ifdev_find_by_name(ifcr.ifcr_name)) == NULL)
		return ENXIO;

	switch (request) {
	case SIOCSIFCAP:
		return ifdev_set_ifcap(ifdev, ifcr.ifcr_capenable);

	case SIOCGIFCAP:
		ifdev_get_ifcap(ifdev, &ifcr.ifcr_capabilities,
		    &ifcr.ifcr_capenable);

		return sockdriver_copyout(data, 0, &ifcr, sizeof(ifcr));

	default:
		return ENOTTY;
	}
}

/*
 * Process an address family independent IOCTL request with an "ifmediareq"
 * structure.
 */
static int
ifconf_ioctl_ifmedia(unsigned long request,
	const struct sockdriver_data * data)
{
	struct ifdev *ifdev;
	struct ifmediareq ifm;
	int r;

	if ((r = sockdriver_copyin(data, 0, &ifm, sizeof(ifm))) != OK)
		return r;

	ifm.ifm_name[sizeof(ifm.ifm_name) - 1] = '\0';

	if ((ifdev = ifdev_find_by_name(ifm.ifm_name)) == NULL)
		return ENXIO;

	switch (request) {
	case MINIX_SIOCGIFMEDIA:
		if ((r = ifdev_get_ifmedia(ifdev, &ifm.ifm_current,
		    &ifm.ifm_active)) != OK)
			return r;
		ifm.ifm_mask = 0;

		switch (ifdev_get_link(ifdev)) {
		case LINK_STATE_UP:
			ifm.ifm_status = IFM_AVALID | IFM_ACTIVE;
			break;
		case LINK_STATE_DOWN:
			ifm.ifm_status = IFM_AVALID;
			break;
		default:
			ifm.ifm_status = 0;
			break;
		}

		/*
		 * TODO: support for the list of supported media types.  This
		 * one is not easy, because we cannot simply suspend the IOCTL
		 * and query the driver.  For now, return only entry (which is
		 * the minimum for ifconfig(8) not to complain), namely the
		 * currently selected one.
		 */
		if (ifm.ifm_ulist != NULL) {
			if (ifm.ifm_count < 1)
				return ENOMEM;

			/*
			 * Copy out the 'list', which consists of one entry.
			 * If we were to produce multiple entries, we would
			 * have to check against the MINIX_IF_MAXMEDIA limit.
			 */
			if ((r = sockdriver_copyout(data,
			    offsetof(struct minix_ifmediareq, mifm_list),
			    &ifm.ifm_current, sizeof(ifm.ifm_current))) != OK)
				return r;
		}
		ifm.ifm_count = 1;

		return sockdriver_copyout(data, 0, &ifm, sizeof(ifm));

	default:
		return ENOTTY;
	}
}

/*
 * Process an address family independent IOCTL request with an "if_clonereq"
 * structure.
 */
static int
ifconf_ioctl_ifclone(unsigned long request,
	const struct sockdriver_data * data)
{
	struct if_clonereq ifcr;
	const char *ptr;
	char name[IFNAMSIZ];
	size_t off;
	unsigned int num;
	int r;

	if ((r = sockdriver_copyin(data, 0, &ifcr, sizeof(ifcr))) != OK)
		return r;

	if (ifcr.ifcr_count < 0)
		return EINVAL;

	off = offsetof(struct minix_if_clonereq, mifcr_buffer);

	for (num = 0; (ptr = ifdev_enum_vtypes(num)) != NULL; num++) {
		/* Prevent overflow in case we ever have over 128 vtypes.. */
		if (num == MINIX_IF_MAXCLONERS)
			break;

		if (ifcr.ifcr_buffer == NULL ||
		    num >= (unsigned int)ifcr.ifcr_count)
			continue;

		memset(name, 0, sizeof(name));
		strlcpy(name, ptr, sizeof(name));

		if ((r = sockdriver_copyout(data, off, name,
		    sizeof(name))) != OK)
			return r;

		off += sizeof(name);
	}

	ifcr.ifcr_total = num;

	return sockdriver_copyout(data, 0, &ifcr, sizeof(ifcr));
}

/*
 * Process an address family independent IOCTL request with an "if_addrprefreq"
 * structure.
 */
static int
ifconf_ioctl_ifaddrpref(unsigned long request,
	const struct sockdriver_data * data)
{
	struct ifdev *ifdev;
	struct if_addrprefreq ifap;
	int r;

	if ((r = sockdriver_copyin(data, 0, &ifap, sizeof(ifap))) != OK)
		return r;

	ifap.ifap_name[sizeof(ifap.ifap_name) - 1] = '\0';

	if ((ifdev = ifdev_find_by_name(ifap.ifap_name)) == NULL)
		return ENXIO;

	/*
	 * For now, we simply support only a preference of 0.  We do not try to
	 * look up the given address, nor do we return the looked up address.
	 */
	switch (request) {
	case SIOCSIFADDRPREF:
		if (ifap.ifap_preference != 0)
			return EINVAL;

		return OK;

	case SIOCGIFADDRPREF:
		ifap.ifap_preference = 0;

		return sockdriver_copyout(data, 0, &ifap, sizeof(ifap));

	default:
		return ENOTTY;
	}
}

/*
 * Process an IOCTL request for AF_INET with an "ifreq" structure.
 */
static int
ifconf_ioctl_v4_ifreq(unsigned long request,
	const struct sockdriver_data * data)
{
	struct sockaddr_in addr, mask, bcast, dest, *sin = NULL /*gcc*/;
	struct ifdev *ifdev;
	struct ifreq ifr;
	ifaddr_v4_num_t num;
	int r, flags;

	if ((r = sockdriver_copyin(data, 0, &ifr, sizeof(ifr))) != OK)
		return r;

	ifr.ifr_name[sizeof(ifr.ifr_name) - 1] = '\0';

	if ((ifdev = ifdev_find_by_name(ifr.ifr_name)) == NULL)
		return ENXIO;

	switch (request) {
	case SIOCGIFADDR:
	case SIOCGIFNETMASK:
	case SIOCGIFBRDADDR:
	case SIOCGIFDSTADDR:
		/* Retrieve all addresses, then copy out the desired one. */
		switch (request) {
		case SIOCGIFADDR:	sin = &addr; break;
		case SIOCGIFNETMASK:	sin = &mask; break;
		case SIOCGIFBRDADDR:	sin = &bcast; break;
		case SIOCGIFDSTADDR:	sin = &dest; break;
		}

		sin->sin_len = 0;

		if ((r = ifaddr_v4_get(ifdev, (ifaddr_v4_num_t)0, &addr, &mask,
		    &bcast, &dest)) != OK)
			return r;

		if (sin->sin_len == 0) /* not filled in */
			return EADDRNOTAVAIL;

		memcpy(&ifr.ifr_addr, sin, sizeof(*sin));

		return sockdriver_copyout(data, 0, &ifr, sizeof(ifr));

	case SIOCGIFAFLAG_IN:
		if ((r = ifaddr_v4_find(ifdev,
		    (struct sockaddr_in *)&ifr.ifr_addr, &num)) != OK)
			return r;

		ifr.ifr_addrflags = ifaddr_v4_get_flags(ifdev, num);

		return sockdriver_copyout(data, 0, &ifr, sizeof(ifr));

	case SIOCSIFADDR:
		/*
		 * This one is slightly different from the rest, in that we
		 * either set or update the primary address: if we set it, we
		 * must let _add() generate a matching netmask automatically,
		 * while if we update it, _add() would fail unless we first
		 * delete the old entry.
		 */
		sin = (struct sockaddr_in *)&ifr.ifr_addr;

		if ((r = ifaddr_v4_get(ifdev, (ifaddr_v4_num_t)0, &addr, &mask,
		    &bcast, &dest)) == OK) {
			flags = ifaddr_v4_get_flags(ifdev, (ifaddr_v4_num_t)0);

			ifaddr_v4_del(ifdev, (ifaddr_v4_num_t)0);

			/*
			 * If setting the new address fails, reinstating the
			 * old address should always work.  This is really ugly
			 * as it generates routing socket noise, but this call
			 * is deprecated anyway.
			 */
			if ((r = ifaddr_v4_add(ifdev, sin, &mask, &bcast,
			    &dest, 0 /*flags*/)) != OK)
				(void)ifaddr_v4_add(ifdev, &addr, &mask,
				    &bcast, &dest, flags);

			return r;
		} else
			return ifaddr_v4_add(ifdev, sin, NULL /*mask*/,
			    NULL /*bcast*/, NULL /*dest*/, 0 /*flags*/);

	case SIOCSIFNETMASK:
	case SIOCSIFBRDADDR:
	case SIOCSIFDSTADDR:
		/* These calls only update the existing primary address. */
		if ((r = ifaddr_v4_get(ifdev, (ifaddr_v4_num_t)0, &addr, &mask,
		    &bcast, &dest)) != OK)
			return r;

		sin = (struct sockaddr_in *)&ifr.ifr_addr;

		switch (request) {
		case SIOCSIFNETMASK: memcpy(&mask, sin, sizeof(mask)); break;
		case SIOCSIFBRDADDR: memcpy(&bcast, sin, sizeof(bcast)); break;
		case SIOCSIFDSTADDR: memcpy(&dest, sin, sizeof(dest)); break;
		}

		return ifaddr_v4_add(ifdev, &addr, &mask, &bcast, &dest,
		    ifaddr_v4_get_flags(ifdev, (ifaddr_v4_num_t)0));

	case SIOCDIFADDR:
		if ((r = ifaddr_v4_find(ifdev,
		    (struct sockaddr_in *)&ifr.ifr_addr, &num)) != OK)
			return r;

		ifaddr_v4_del(ifdev, num);

		return OK;

	default:
		return ENOTTY;
	}
}

/*
 * Process an IOCTL request for AF_INET with an "ifaliasreq" structure.
 */
static int
ifconf_ioctl_v4_ifalias(unsigned long request,
	const struct sockdriver_data * data)
{
	struct ifdev *ifdev;
	struct ifaliasreq ifra;
	struct sockaddr_in dest;
	ifaddr_v4_num_t num;
	int r;

	if ((r = sockdriver_copyin(data, 0, &ifra, sizeof(ifra))) != OK)
		return r;

	ifra.ifra_name[sizeof(ifra.ifra_name) - 1] = '\0';

	if ((ifdev = ifdev_find_by_name(ifra.ifra_name)) == NULL)
		return ENXIO;

	switch (request) {
	case SIOCAIFADDR:
		return ifaddr_v4_add(ifdev,
		    (struct sockaddr_in *)&ifra.ifra_addr,
		    (struct sockaddr_in *)&ifra.ifra_mask,
		    (struct sockaddr_in *)&ifra.ifra_broadaddr,
		    (struct sockaddr_in *)&ifra.ifra_dstaddr, 0 /*flags*/);

	case SIOCGIFALIAS:
		if ((r = ifaddr_v4_find(ifdev,
		    (struct sockaddr_in *)&ifra.ifra_addr, &num)) != OK)
			return r;

		/*
		 * The broadcast and destination address are stored in the same
		 * ifaliasreq field.  We cannot pass a pointer to the same
		 * field to ifaddr_v4_get().  So, use a temporary variable.
		 */
		(void)ifaddr_v4_get(ifdev, num,
		    (struct sockaddr_in *)&ifra.ifra_addr,
		    (struct sockaddr_in *)&ifra.ifra_mask,
		    (struct sockaddr_in *)&ifra.ifra_broadaddr, &dest);

		if (ifra.ifra_broadaddr.sa_len == 0)
			memcpy(&ifra.ifra_dstaddr, &dest, sizeof(dest));

		return sockdriver_copyout(data, 0, &ifra, sizeof(ifra));

	default:
		return ENOTTY;
	}
}

/*
 * Process an IOCTL request for AF_INET.
 */
static int
ifconf_ioctl_v4(unsigned long request, const struct sockdriver_data * data,
	endpoint_t user_endpt)
{

	switch (request) {
	case SIOCSIFADDR:
	case SIOCSIFDSTADDR:
	case SIOCSIFBRDADDR:
	case SIOCSIFNETMASK:
	case SIOCDIFADDR:
		if (!util_is_root(user_endpt))
			return EPERM;

		/* FALLTHROUGH */
	case SIOCGIFADDR:
	case SIOCGIFDSTADDR:
	case SIOCGIFBRDADDR:
	case SIOCGIFNETMASK:
	case SIOCGIFAFLAG_IN:
		return ifconf_ioctl_v4_ifreq(request, data);

	case SIOCAIFADDR:
		if (!util_is_root(user_endpt))
			return EPERM;

		/* FALLTHROUGH */
	case SIOCGIFALIAS:
		return ifconf_ioctl_v4_ifalias(request, data);

	default:
		return ENOTTY;
	}
}

#ifdef INET6
/*
 * Process an IOCTL request for AF_INET6 with an "in6_ifreq" structure.
 */
static int
ifconf_ioctl_v6_ifreq(unsigned long request,
	const struct sockdriver_data * data)
{
	struct ifdev *ifdev;
	struct in6_ifreq ifr;
	ifaddr_v6_num_t num;
	int r;

	if ((r = sockdriver_copyin(data, 0, &ifr, sizeof(ifr))) != OK)
		return r;

	ifr.ifr_name[sizeof(ifr.ifr_name) - 1] = '\0';

	if ((ifdev = ifdev_find_by_name(ifr.ifr_name)) == NULL)
		return ENXIO;

	if ((r = ifaddr_v6_find(ifdev, &ifr.ifr_addr, &num)) != OK)
		return r;

	switch (request) {
	case SIOCGIFADDR_IN6:
		/* This IOCTL basically checks if the given address exists. */
		ifaddr_v6_get(ifdev, num, &ifr.ifr_addr, NULL, NULL);

		return sockdriver_copyout(data, 0, &ifr, sizeof(ifr));

	case SIOCDIFADDR_IN6:
		ifaddr_v6_del(ifdev, num);

		return OK;

	case SIOCGIFNETMASK_IN6:
		ifaddr_v6_get(ifdev, num, NULL, &ifr.ifr_addr, NULL);

		return sockdriver_copyout(data, 0, &ifr, sizeof(ifr));

	case SIOCGIFAFLAG_IN6:
		ifr.ifr_ifru.ifru_flags6 = ifaddr_v6_get_flags(ifdev, num);

		return sockdriver_copyout(data, 0, &ifr, sizeof(ifr));

	case SIOCGIFALIFETIME_IN6:
		ifaddr_v6_get_lifetime(ifdev, num,
		    &ifr.ifr_ifru.ifru_lifetime);

		return sockdriver_copyout(data, 0, &ifr, sizeof(ifr));

	default:
		return ENOTTY;
	}
}

/*
 * Process an IOCTL request for AF_INET6 with an "in6_aliasreq" structure.
 */
static int
ifconf_ioctl_v6_ifalias(unsigned long request,
	const struct sockdriver_data * data)
{
	struct ifdev *ifdev;
	struct in6_aliasreq ifra;
	int r;

	if ((r = sockdriver_copyin(data, 0, &ifra, sizeof(ifra))) != OK)
		return r;

	ifra.ifra_name[sizeof(ifra.ifra_name) - 1] = '\0';

	if ((ifdev = ifdev_find_by_name(ifra.ifra_name)) == NULL)
		return ENXIO;

	switch (request) {
	case SIOCAIFADDR_IN6:
		return ifaddr_v6_add(ifdev, &ifra.ifra_addr,
		    &ifra.ifra_prefixmask, &ifra.ifra_dstaddr,
		    ifra.ifra_flags, &ifra.ifra_lifetime);

	default:
		return ENOTTY;
	}
}

/*
 * Process an IOCTL request for AF_INET6 with an "in6_ndireq" structure.
 */
static int
ifconf_ioctl_v6_ndireq(unsigned long request,
	const struct sockdriver_data * data)
{
	struct ifdev *ifdev;
	struct in6_ndireq ndi;
	int r;

	if ((r = sockdriver_copyin(data, 0, &ndi, sizeof(ndi))) != OK)
		return r;

	ndi.ifname[sizeof(ndi.ifname) - 1] = '\0';

	if ((ifdev = ifdev_find_by_name(ndi.ifname)) == NULL)
		return ENXIO;

	switch (request) {
	case SIOCGIFINFO_IN6:
		memset(&ndi.ndi, 0, sizeof(ndi.ndi));

		ndi.ndi.linkmtu = ifdev_get_mtu(ifdev);
		ndi.ndi.flags = ifdev_get_nd6flags(ifdev);
		ndi.ndi.initialized = 1;
		/* TODO: all the other fields.. */

		return sockdriver_copyout(data, 0, &ndi, sizeof(ndi));

	case SIOCSIFINFO_IN6:
		/* TODO: all the other fields.. */

		/* FALLTHROUGH */
	case SIOCSIFINFO_FLAGS:
		return ifdev_set_nd6flags(ifdev, ndi.ndi.flags);

	default:
		return ENOTTY;
	}
}

/*
 * Process an IOCTL request for AF_INET6 with an "in6_nbrinfo" structure.
 */
static int
ifconf_ioctl_v6_nbrinfo(unsigned long request,
	const struct sockdriver_data * data)
{
	struct ifdev *ifdev;
	struct sockaddr_in6 addr;
	struct in6_nbrinfo nbri;
	lldata_ndp_num_t num;
	int r;

	if ((r = sockdriver_copyin(data, 0, &nbri, sizeof(nbri))) != OK)
		return r;

	nbri.ifname[sizeof(nbri.ifname) - 1] = '\0';

	if ((ifdev = ifdev_find_by_name(nbri.ifname)) == NULL)
		return ENXIO;

	switch (request) {
	case SIOCGNBRINFO_IN6:
		/*
		 * Convert the given in6_addr to a full sockaddr_in6, mainly
		 * for internal consistency.  It would have been nice if the
		 * KAME management API had had any sort of consistency itself.
		 */
		memset(&addr, 0, sizeof(addr));
		addr.sin6_family = AF_INET6;
		memcpy(&addr.sin6_addr.s6_addr, &nbri.addr,
		    sizeof(addr.sin6_addr.s6_addr));

		if ((r = lldata_ndp_find(ifdev, &addr, &num)) != OK)
			return r;

		lldata_ndp_get_info(num, &nbri.asked, &nbri.isrouter,
		    &nbri.state, &nbri.expire);

		return sockdriver_copyout(data, 0, &nbri, sizeof(nbri));

	default:
		return ENOTTY;
	}
}

/*
 * Process an IOCTL request for AF_INET6.
 */
static int
ifconf_ioctl_v6(unsigned long request, const struct sockdriver_data * data,
	endpoint_t user_endpt)
{

	switch (request) {
	case SIOCDIFADDR_IN6:
		if (!util_is_root(user_endpt))
			return EPERM;

		/* FALLTHROUGH */
	case SIOCGIFADDR_IN6:
	case SIOCGIFNETMASK_IN6:
	case SIOCGIFAFLAG_IN6:
	case SIOCGIFALIFETIME_IN6:
		return ifconf_ioctl_v6_ifreq(request, data);

	case SIOCAIFADDR_IN6:
		if (!util_is_root(user_endpt))
			return EPERM;

		return ifconf_ioctl_v6_ifalias(request, data);

	case SIOCSIFINFO_IN6:
	case SIOCSIFINFO_FLAGS:
		if (!util_is_root(user_endpt))
			return EPERM;

		/* FALLTHROUGH */
	case SIOCGIFINFO_IN6:
		return ifconf_ioctl_v6_ndireq(request, data);

	case SIOCGNBRINFO_IN6:
		return ifconf_ioctl_v6_nbrinfo(request, data);

	default:
		return ENOTTY;
	}
}
#endif /* INET6 */

/*
 * Process an IOCTL request for AF_LINK with an "if_laddrreq" structure.
 */
static int
ifconf_ioctl_dl_lifaddr(unsigned long request,
	const struct sockdriver_data * data)
{
	struct ifdev *ifdev;
	struct if_laddrreq iflr;
	ifaddr_dl_num_t num;
	int r;

	if ((r = sockdriver_copyin(data, 0, &iflr, sizeof(iflr))) != OK)
		return r;

	iflr.iflr_name[sizeof(iflr.iflr_name) - 1] = '\0';

	if ((ifdev = ifdev_find_by_name(iflr.iflr_name)) == NULL)
		return ENXIO;

	switch (request) {
	case SIOCGLIFADDR:
		if (iflr.flags & IFLR_PREFIX) {
			/* We ignore the prefix length, like NetBSD does. */
			if ((r = ifaddr_dl_find(ifdev,
			    (struct sockaddr_dlx *)&iflr.addr,
			    sizeof(iflr.addr), &num)) != OK)
				return r;
		} else
			num = (ifaddr_dl_num_t)0; /* this always works */

		ifaddr_dl_get(ifdev, num, (struct sockaddr_dlx *)&iflr.addr);
		iflr.flags = ifaddr_dl_get_flags(ifdev, num);
		memset(&iflr.dstaddr, 0, sizeof(iflr.dstaddr));

		return sockdriver_copyout(data, 0, &iflr, sizeof(iflr));

	case SIOCALIFADDR:
		return ifaddr_dl_add(ifdev, (struct sockaddr_dlx *)&iflr.addr,
		    sizeof(iflr.addr), iflr.flags);

	case SIOCDLIFADDR:
		if ((r = ifaddr_dl_find(ifdev,
		    (struct sockaddr_dlx *)&iflr.addr, sizeof(iflr.addr),
		    &num)) != OK)
			return r;

		return ifaddr_dl_del(ifdev, num);

	default:
		return ENOTTY;
	}
}

/*
 * Process an IOCTL request for AF_LINK.
 */
static int
ifconf_ioctl_dl(unsigned long request, const struct sockdriver_data * data,
	endpoint_t user_endpt)
{

	switch (request) {
	case SIOCALIFADDR:
	case SIOCDLIFADDR:
		if (!util_is_root(user_endpt))
			return EPERM;

		/* FALLTHROUGH */
	case SIOCGLIFADDR:
		return ifconf_ioctl_dl_lifaddr(request, data);

	default:
		return ENOTTY;
	}
}

/*
 * Process an IOCTL request.  This routine is shared between TCP, UDP, RAW, and
 * link sockets.  The given socket may be used to obtain the target domain:
 * AF_INET, AF_INET6, or AF_LINK.
 */
int
ifconf_ioctl(struct sock * sock, unsigned long request,
	const struct sockdriver_data * data, endpoint_t user_endpt)
{
	int domain;

	domain = sockevent_get_domain(sock);

	switch (request) {
	case SIOCSIFFLAGS:
	case SIOCSIFMETRIC:
	case SIOCSIFMEDIA:
	case SIOCSIFMTU:
	case SIOCIFCREATE:
	case SIOCIFDESTROY:
		if (!util_is_root(user_endpt))
			return EPERM;

		/* FALLTHROUGH */
	case SIOCGIFFLAGS:
	case SIOCGIFMETRIC:
	case SIOCGIFMTU:
	case SIOCGIFDLT:
	case SIOCGIFINDEX:
		return ifconf_ioctl_ifreq(request, data);

	case SIOCSIFCAP:
		if (!util_is_root(user_endpt))
			return EPERM;

		/* FALLTHROUGH */
	case SIOCGIFCAP:
		return ifconf_ioctl_ifcap(request, data);

	case MINIX_SIOCGIFMEDIA:
		return ifconf_ioctl_ifmedia(request, data);

	case MINIX_SIOCIFGCLONERS:
		return ifconf_ioctl_ifclone(request, data);

	case SIOCSIFADDRPREF:
		if (!util_is_root(user_endpt))
			return EPERM;

		/* FALLTHROUGH */
	case SIOCGIFADDRPREF:
		return ifconf_ioctl_ifaddrpref(request, data);

	default:
		switch (domain) {
		case AF_INET:
			return ifconf_ioctl_v4(request, data, user_endpt);

#ifdef INET6
		case AF_INET6:
			return ifconf_ioctl_v6(request, data, user_endpt);
#endif /* INET6 */

		case AF_LINK:
			return ifconf_ioctl_dl(request, data, user_endpt);

		default:
			return ENOTTY;
		}
	}
}
