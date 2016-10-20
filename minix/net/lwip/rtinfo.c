/*
 * Mock net.route sysctl(2) subtree implementation using RMIB.  This code
 * serves as a temporary bridge to allow libc to switch from the original,
 * native MINIX3 getifaddrs(3) to the NetBSD getifaddrs(3).  As such, it
 * implements only a small subset of NetBSD's full net.route functionality,
 * although also more than needed only to imitate the MINIX3 getifaddrs(3).
 */

#include <minix/netsock.h>
#include <netinet/in.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>
#include <sys/sysctl.h>
#include <minix/rmib.h>
#include <assert.h>

#include "proto.h"
#include "driver.h"

/* Max. number of bytes for a full sockaddr_dl structure, including data. */
#define SDL_BUFSIZE	(sizeof(struct sockaddr_dl) + 32)

static const char padbuf[RT_ROUNDUP(0)] = { 0 };

/*
 * Compute the length for, and possibly copy out, an interface information or
 * interface address record with an associated set of zero or more routing
 * table addresses.  The addresses are padded as necessary.  Store the full
 * record length and the address bitmap before copying out the entire record.
 */
static ssize_t
copyout_rta(void * hdr, size_t size, u_short * msglen, int * addrs,
	void * rta_map[RTAX_MAX], size_t rta_len[RTAX_MAX],
	struct rmib_oldp * oldp, ssize_t off)
{
	iovec_t iov[1 + RTAX_MAX * 2];
	size_t len, total, padlen;
	unsigned int i, iovcnt;
	int mask;

	iovcnt = 0;
	iov[iovcnt].iov_addr = (vir_bytes)hdr;
	iov[iovcnt++].iov_size = size;

	total = size;
	mask = 0;

	/*
	 * Any addresses in the given map should be stored in the numbering
	 * order of the map.
	 */
	for (i = 0; i < RTAX_MAX; i++) {
		if (rta_map[i] == NULL)
			continue;

		assert(iovcnt < __arraycount(iov));
		iov[iovcnt].iov_addr = (vir_bytes)rta_map[i];
		iov[iovcnt++].iov_size = len = rta_len[i];

		padlen = RT_ROUNDUP(len) - len;
		if (padlen > 0) {
			assert(iovcnt < __arraycount(iov));
			iov[iovcnt].iov_addr = (vir_bytes)padbuf;
			iov[iovcnt++].iov_size = padlen;
		}

		total += len + padlen;
		mask |= (1 << i);
	}

	/* If only the length was requested, return it now. */
	if (oldp == NULL)
		return total;

	/*
	 * Casting 'hdr' would violate C99 strict aliasing rules, so store the
	 * computed header values through direct pointers.  Bah.
	 */
	*msglen = total;
	*addrs = mask;

	return rmib_vcopyout(oldp, off, iov, iovcnt);
}

/*
 * Compute the length for, and possibly generate, a sockaddr_dl structure for
 * the given interface.  The complication here is that the structure contains
 * various field packed together dynamically, making it variable sized.
 */
static size_t
make_sdl(const struct nic * nic, int ndx, char * buf, size_t max)
{
	struct sockaddr_dl sdl;
	size_t hdrlen, namelen, addrlen, padlen, len;

	namelen = strlen(nic->name);
	addrlen = sizeof(nic->netif.hwaddr);

	/*
	 * Compute the unpadded and padded length of the structure.  We pad the
	 * structure ourselves here, even though the caller will otherwise pad
	 * it later, because it is easy to do so and saves on a vector element.
	 */
	hdrlen = offsetof(struct sockaddr_dl, sdl_data);
	len = hdrlen + namelen + addrlen;
	padlen = RT_ROUNDUP(len) - len;
	assert(len + padlen <= max);

	/* If we are asked not to generate the actual data, stop here. */
	if (buf == NULL)
		return len + padlen;

	/*
	 * Fill the sockaddr_dl structure header.  The C99 strict aliasing
	 * rules prevent us from filling 'buf' through a pointer structure
	 * directly.
	 */
	memset(&sdl, 0, hdrlen);
	sdl.sdl_len = len;
	sdl.sdl_family = AF_LINK;
	sdl.sdl_index = ndx;
	sdl.sdl_type = IFT_ETHER;
	sdl.sdl_nlen = namelen;
	sdl.sdl_alen = addrlen;
	sdl.sdl_slen = 0;

	/*
	 * Generate the full sockaddr_dl structure in the given buffer.  These
	 * memory sizes are typically small, so the extra memory copies are not
	 * too expensive.  The advantage of generating a single sockaddr_dl
	 * structure buffer is that we can use copyout_rta() on it.
	 */
	memcpy(buf, &sdl, hdrlen);
	if (namelen > 0)
		memcpy(&buf[hdrlen], nic->name, namelen);
	if (addrlen > 0)
		memcpy(&buf[hdrlen + namelen], nic->netif.hwaddr, addrlen);
	if (padlen > 0)
		memset(&buf[len], 0, padlen);

	return len + padlen;
}

/*
 * Compute the length for, and possibly generate, an interface information
 * record for the given interface.
 */
static ssize_t
gen_ifm(const struct nic * nic, int ndx, int is_up, struct rmib_oldp * oldp,
	ssize_t off)
{
	struct if_msghdr ifm;
	char buf[SDL_BUFSIZE];
	void *rta_map[RTAX_MAX];
	size_t rta_len[RTAX_MAX], size;

	if (oldp != NULL) {
		memset(&ifm, 0, sizeof(ifm));
		ifm.ifm_version = RTM_VERSION;
		ifm.ifm_type = RTM_IFINFO;
		ifm.ifm_flags = (is_up) ? (IFF_UP | IFF_RUNNING) : 0;
		ifm.ifm_index = ndx;
		ifm.ifm_data.ifi_type = IFT_ETHER;
		/* TODO: other ifm_flags, other ifm_data fields, etc. */
	}

	/*
	 * Note that we add padding even in this case, to ensure that the
	 * following structures are properly aligned as well.
	 */
	size = make_sdl(nic, ndx, (oldp != NULL) ? buf : NULL, sizeof(buf));

	memset(rta_map, 0, sizeof(rta_map));
	rta_map[RTAX_IFP] = buf;
	rta_len[RTAX_IFP] = size;

	return copyout_rta(&ifm, sizeof(ifm), &ifm.ifm_msglen, &ifm.ifm_addrs,
	    rta_map, rta_len, oldp, off);
}

/*
 * Compute the length for, and possibly generate, an AF_LINK-family interface
 * address record.
 */
static ssize_t
gen_ifam_dl(const struct nic * nic, int ndx, int is_up,
	struct rmib_oldp * oldp, ssize_t off)
{
	struct ifa_msghdr ifam;
	char buf[SDL_BUFSIZE];
	void *rta_map[RTAX_MAX];
	size_t rta_len[RTAX_MAX], size;

	if (oldp != NULL) {
		memset(&ifam, 0, sizeof(ifam));
		ifam.ifam_version = RTM_VERSION;
		ifam.ifam_type = RTM_NEWADDR;
		ifam.ifam_index = ndx;
		ifam.ifam_metric = 0; /* unknown and irrelevant */
	}

	size = make_sdl(nic, ndx, (oldp != NULL) ? buf : NULL, sizeof(buf));

	/*
	 * We do not generate a netmask.  NetBSD seems to generate a netmask
	 * with all-one bits for the number of bytes equal to the name length,
	 * for reasons unknown to me.  If we did the same, we would end up with
	 * a conflict on the static 'namebuf' buffer.
	 */
	memset(rta_map, 0, sizeof(rta_map));
	rta_map[RTAX_IFA] = buf;
	rta_len[RTAX_IFA] = size;

	return copyout_rta(&ifam, sizeof(ifam), &ifam.ifam_msglen,
	    &ifam.ifam_addrs, rta_map, rta_len, oldp, off);
}

/*
 * Compute the length for, and possibly generate, an AF_INET-family interface
 * address record.
 */
static ssize_t
gen_ifam_inet(const struct nic * nic, int ndx, int is_up,
	struct rmib_oldp * oldp, ssize_t off)
{
	struct ifa_msghdr ifam;
	struct sockaddr_in ipaddr, netmask;
	void *rta_map[RTAX_MAX];
	size_t rta_len[RTAX_MAX];

	if (oldp != NULL) {
		memset(&ifam, 0, sizeof(ifam));
		ifam.ifam_msglen = sizeof(ifam);
		ifam.ifam_version = RTM_VERSION;
		ifam.ifam_type = RTM_NEWADDR;
		ifam.ifam_addrs = 0;
		ifam.ifam_index = ndx;
		ifam.ifam_metric = 0; /* unknown and irrelevant */
	}

	memset(rta_map, 0, sizeof(rta_map));

	if (!ip_addr_isany(&nic->netif.ip_addr)) {
		if (oldp != NULL) {
			memset(&ipaddr, 0, sizeof(ipaddr));
			ipaddr.sin_family = AF_INET;
			ipaddr.sin_len = sizeof(ipaddr);
			ipaddr.sin_addr.s_addr =
			    ip4_addr_get_u32(&nic->netif.ip_addr);
		}

		rta_map[RTAX_IFA] = &ipaddr;
		rta_len[RTAX_IFA] = sizeof(ipaddr);
	}

	if (!ip_addr_isany(&nic->netif.netmask)) {
		/*
		 * TODO: BSD goes through the trouble of compressing the
		 * netmask for some reason.  We need to figure out if
		 * compression is actually required by any part of userland.
		 */
		if (oldp != NULL) {
			memset(&netmask, 0, sizeof(netmask));
			netmask.sin_family = AF_INET;
			netmask.sin_len = sizeof(netmask);
			netmask.sin_addr.s_addr =
			    ip4_addr_get_u32(&nic->netif.netmask);
		}

		rta_map[RTAX_NETMASK] = &netmask;
		rta_len[RTAX_NETMASK] = sizeof(netmask);
	}

	return copyout_rta(&ifam, sizeof(ifam), &ifam.ifam_msglen,
	    &ifam.ifam_addrs, rta_map, rta_len, oldp, off);
}

/*
 * Compute the size needed for, and optionally copy out, the interface and
 * address information for the given interface.
 */
static ssize_t
do_one_if(const struct nic * nic, int ndx, struct rmib_oldp * oldp,
	ssize_t off, int filter)
{
	ssize_t r, len;
	int is_up;

	/*
	 * If the interface is not configured, we mark it as down and do not
	 * provide IP address information.
	 */
	is_up = !ip_addr_isany(&nic->netif.ip_addr);

	len = 0;

	/* There is always a full interface information record. */
	if ((r = gen_ifm(nic, ndx, is_up, oldp, off)) < 0)
		return r;
	len += r;

	/* If not filtered, there is a datalink address record. */
	if (filter == 0 || filter == AF_LINK) {
		if ((r = gen_ifam_dl(nic, ndx, is_up, oldp, off + len)) < 0)
			return r;
		len += r;
	}

	/* If configured and not filtered, there is an IPv4 address record. */
	if (is_up && (filter == 0 || filter == AF_INET)) {
		if ((r = gen_ifam_inet(nic, ndx, is_up, oldp, off + len)) < 0)
			return r;
		len += r;
	}

	/*
	 * Whether or not anything was copied out, upon success we return the
	 * full length of the data.
	 */
	return len;
}

/*
 * Remote MIB implementation of CTL_NET PF_ROUTE 0.  This function handles all
 * queries on the "net.route.rtable" sysctl(2) node.
 */
static ssize_t
net_route_rtable(struct rmib_call * call, struct rmib_node * node __unused,
	struct rmib_oldp * oldp, struct rmib_newp * newp __unused)
{
	const struct nic *nic;
	ssize_t r, off;
	int i, filter, ndx;

	if (call->call_namelen != 3)
		return EINVAL;

	/* We only support listing interfaces for now. */
	if (call->call_name[1] != NET_RT_IFLIST)
		return EOPNOTSUPP;

	filter = call->call_name[0];
	ndx = call->call_name[2];

	off = 0;

	for (i = 0; i < MAX_DEVS; i++) {
		if (!(nic = nic_get(i)))
			continue;

		/*
		 * If information about a specific interface index is requested
		 * then skip all other entries.  Interface indices must be
		 * nonzero, so we shift the numbers by one.  We can avoid going
		 * through the loop altogether here, but getifaddrs(3) does not
		 * query specific interfaces anyway.
		 */
		if (ndx != 0 && ndx != i + 1)
			continue;

		/* Avoid generating results that are never copied out. */
		if (oldp != NULL && !rmib_inrange(oldp, off))
			oldp = NULL;

		if ((r = do_one_if(nic, i + 1, oldp, off, filter)) < 0)
			return r;

		off += r;
	}

	return off;
}

/* The CTL_NET PF_ROUTE subtree. */
static struct rmib_node net_route_table[] = {
	[0]	= RMIB_FUNC(RMIB_RO | CTLTYPE_NODE, 0, net_route_rtable,
		    "rtable", "Routing table information")
};

/* The CTL_NET PF_ROUTE node. */
static struct rmib_node net_route_node =
    RMIB_NODE(RMIB_RO, net_route_table, "route", "PF_ROUTE information");

/*
 * Register the net.route RMIB subtree with the MIB service.  Since inet does
 * not support clean shutdowns, there is no matching cleanup function.
 */
void
rtinfo_init(void)
{
	const int mib[] = { CTL_NET, PF_ROUTE };
	int r;

	if ((r = rmib_register(mib, __arraycount(mib), &net_route_node)) != OK)
		panic("unable to register remote MIB tree: %d", r);
}
