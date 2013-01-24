#include <namespace.h>

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <net/if.h>
#include <net/gen/in.h>
#include <net/gen/ip_io.h>
#include <net/gen/tcp.h>
#include <net/gen/udp.h>

#include <netinet/in.h>

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ifaddrs.h>

#if defined(__weak_alias)
__weak_alias(getifaddrs,_getifaddrs)
__weak_alias(freeifaddrs,_freeifaddrs)
#endif

int
getifaddrs(struct ifaddrs **ifap)
{
	static int fd = -1;
	nwio_ipconf_t ipconf;
	int flags;
	static struct ifaddrs ifa;
	static struct sockaddr_in addr, netmask;

	memset(&ifa, 0, sizeof(ifa));
	memset(&addr, 0, sizeof(addr));
	memset(&netmask, 0, sizeof(netmask));
	ifa.ifa_next = NULL;
	ifa.ifa_name = __UNCONST("ip");
	addr.sin_family = netmask.sin_family = AF_INET;
	ifa.ifa_addr = (struct sockaddr *) &addr;
	ifa.ifa_netmask = (struct sockaddr *) &netmask;
	addr.sin_addr.s_addr = 0;
	netmask.sin_addr.s_addr = 0;

	*ifap = NULL;

	if(fd < 0) {
		char *ipd;

		if(!(ipd = getenv("IP_DEVICE")))
			ipd = __UNCONST("/dev/ip");
		if((fd = open(ipd, O_RDWR)) < 0)
			return -1;
	}

	/* Code taken from commands/simple/ifconfig.c. */

	if((flags = fcntl(fd, F_GETFL)) < 0 ||
	   fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0 ||
	   ioctl(fd, NWIOGIPCONF, &ipconf))
		return 0;	/* Report interface as down. */

	addr.sin_addr.s_addr = ipconf.nwic_ipaddr;
	netmask.sin_addr.s_addr = ipconf.nwic_netmask;
	if(addr.sin_addr.s_addr) ifa.ifa_flags = IFF_UP;

	/* Just report on this interface. */

	*ifap = &ifa;

	return 0;
}

void
freeifaddrs(struct ifaddrs *ifp)
{
	/* getifaddrs points to static data, so no need to free. */
	;
}

