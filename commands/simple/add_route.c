/*
add_route.c

Created August 7, 1991 by Philip Homburg
*/

#define _POSIX_C_SOURCE	2

#include <sys/types.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <net/hton.h>
#include <net/netlib.h>
#include <net/gen/netdb.h>
#include <net/gen/in.h>
#include <net/gen/inet.h>
#include <net/gen/route.h>
#include <net/gen/socket.h>
#include <net/gen/ip_io.h>

static char *prog_name;
static enum { ADD, DEL } action;

static void usage(void);
static int name_to_ip(char *name, ipaddr_t *addr);
static int parse_cidr(char *cidr, ipaddr_t *addr, ipaddr_t *mask);

void main(int argc, char *argv[])
{
	struct netent *netent;
	ipaddr_t gateway, destination, netmask, defaultmask=0;
	u8_t high_byte;
	nwio_route_t route;
	int ip_fd, itab;
	int r;
	int metric;
	char *check;
	char *ip_device;
	char *netmask_str, *metric_str, *destination_str, *gateway_str;
	int c;
	char *d_arg, *g_arg, *m_arg, *n_arg, *I_arg;
	int i_flag, o_flag, D_flag, v_flag;
	int cidr;

	prog_name= strrchr(argv[0], '/');
	if (prog_name == NULL) prog_name= argv[0]; else prog_name++;

	if (strcmp(prog_name, "add_route") == 0)
		action= ADD;
	else if (strcmp(prog_name, "del_route") == 0)
		action= DEL;
	else
	{
		fprintf(stderr, "Don't know what to do when named '%s'\n",
			prog_name);
		exit(1);
	}

	i_flag= 0;
	o_flag= 0;
	D_flag= 0;
	v_flag= 0;
	g_arg= NULL;
	d_arg= NULL;
	m_arg= NULL;
	n_arg= NULL;
	I_arg= NULL;
	while ((c= getopt(argc, argv, "iovDg:d:m:n:I:?")) != -1)
	{
		switch(c)
		{
		case 'i':
			if (i_flag)
				usage();
			i_flag= 1;
			break;
		case 'o':
			if (o_flag)
				usage();
			o_flag= 1;
			break;
		case 'v':
			if (v_flag)
				usage();
			v_flag= 1;
			break;
		case 'D':
			if (D_flag)
				usage();
			D_flag= 1;
			break;
		case 'g':
			if (g_arg)
				usage();
			g_arg= optarg;
			break;
		case 'd':
			if (d_arg)
				usage();
			d_arg= optarg;
			break;
		case 'm':
			if (m_arg)
				usage();
			m_arg= optarg;
			break;
		case 'n':
			if (n_arg)
				usage();
			n_arg= optarg;
			break;
		case 'I':
			if (I_arg)
				usage();
			I_arg= optarg;
			break;
		case '?':
			usage();
		default:
			fprintf(stderr, "%s: getopt failed\n", prog_name);
			exit(1);
		}
	}
	if (optind != argc)
		usage();
	if (i_flag && o_flag)
		usage();
	itab= i_flag;

	if (i_flag)
	{
		if (g_arg == NULL || d_arg == NULL || m_arg == NULL)
			usage();
	}
	else
	{
		if (g_arg == NULL || (d_arg == NULL && n_arg != NULL))
		{
			usage();
		}
	}
		
	gateway_str= g_arg;
	destination_str= d_arg;
	metric_str= m_arg;
	netmask_str= n_arg;
	ip_device= I_arg;

	if (!name_to_ip(gateway_str, &gateway))
	{
		fprintf(stderr, "%s: unknown host '%s'\n", prog_name,
								gateway_str);
		exit(1);
	}

	destination= 0;
	netmask= 0;
	cidr= 0;

	if (destination_str)
	{
		if (parse_cidr(destination_str, &destination, &netmask))
			cidr= 1;
		else if (inet_aton(destination_str, &destination))
			;
		else if ((netent= getnetbyname(destination_str)) != NULL)
			destination= netent->n_net;
		else if (!name_to_ip(destination_str, &destination))
		{
			fprintf(stderr, "%s: unknown network/host '%s'\n",
				prog_name, destination_str);
			exit(1);
		}
		high_byte= *(u8_t *)&destination;
		if (!(high_byte & 0x80))	/* class A or 0 */
		{
			if (destination)
				defaultmask= HTONL(0xff000000);
		}
		else if (!(high_byte & 0x40))	/* class B */
		{
			defaultmask= HTONL(0xffff0000);
		}
		else if (!(high_byte & 0x20))	/* class C */
		{
			defaultmask= HTONL(0xffffff00);
		}
		else				/* class D is multicast ... */
		{
			fprintf(stderr, "%s: Warning: Martian address '%s'\n",
				prog_name, inet_ntoa(destination));
			defaultmask= HTONL(0xffffffff);
		}
		if (destination & ~defaultmask)
		{
			/* host route */
			defaultmask= HTONL(0xffffffff);
		}
		if (!cidr)
			netmask= defaultmask;
	}

	if (netmask_str)
	{
		if (cidr)
			usage();
		if (inet_aton(netmask_str, &netmask) == 0)
		{
			fprintf(stderr, "%s: illegal netmask'%s'\n", prog_name,
				netmask_str);
			exit(1);
		}
	}

	if (metric_str)
	{
		metric= strtol(metric_str, &check, 0);
		if (check[0] != '\0' || metric < 1)
		{
			fprintf(stderr, "%s: illegal metric %s\n",
				prog_name, metric_str);
		}
	}
	else
		metric= 1;
		
	if (!ip_device)
		ip_device= getenv("IP_DEVICE");
	if (!ip_device)
		ip_device= IP_DEVICE;

	ip_fd= open(ip_device, O_RDWR);
	if (ip_fd == -1)
	{
		fprintf(stderr, "%s: unable to open('%s'): %s\n",
			prog_name, ip_device, strerror(errno));
		exit(1);
	}

	if (v_flag)
	{
		printf("%s %s route to %s ",
			action == ADD ? "adding" : "deleting",
			itab ? "input" : "output",
			inet_ntoa(destination));
		printf("with netmask %s ", inet_ntoa(netmask));
		printf("using gateway %s", inet_ntoa(gateway));
		if (itab && action == ADD)
			printf(" at distance %d", metric);
		printf("\n");
	}

	route.nwr_ent_no= 0;
	route.nwr_dest= destination;
	route.nwr_netmask= netmask;
	route.nwr_gateway= gateway;
	route.nwr_dist= action == ADD ? metric : 0;
	route.nwr_flags= (action == DEL && D_flag) ? 0 : NWRF_STATIC;
	route.nwr_pref= 0;
	route.nwr_mtu= 0;

	if (action == ADD)
		r= ioctl(ip_fd, itab ? NWIOSIPIROUTE : NWIOSIPOROUTE, &route);
	else
		r= ioctl(ip_fd, itab ? NWIODIPIROUTE : NWIODIPOROUTE, &route);
	if (r == -1)
	{
		fprintf(stderr, "%s: NWIO%cIP%cROUTE: %s\n",
			prog_name,
			action == ADD ? 'S' : 'D',
			itab ? 'I' : 'O',
			strerror(errno));
		exit(1);
	}
	exit(0);
}

static void usage(void)
{
	fprintf(stderr,
		"Usage: %s\n"
		"\t[-o] %s-g gw [-d dst [-n netmask]] %s[-I ipdev] [-v]\n"
		"\t-i %s-g gw -d dst [-n netmask] %s[-I ipdev] [-v]\n"
		"Note: <dst> may be in CIDR notation\n",
		prog_name,
		action == DEL ? "[-D] " : "",
		action == ADD ? "[-m metric] " : "",
		action == DEL ? "[-D] " : "",
		action == ADD ? "-m metric " : ""
	);
	exit(1);
}

static int name_to_ip(char *name, ipaddr_t *addr)
{
	/* Translate a name to an IP address.  Try first with inet_aton(), then
	 * with gethostbyname().  (The latter can also recognize an IP address,
	 * but only decimals with at least one dot).)
	 */
	struct hostent *hostent;

	if (!inet_aton(name, addr)) {
		if ((hostent= gethostbyname(name)) == NULL) return 0;
		if (hostent->h_addrtype != AF_INET) return 0;
		if (hostent->h_length != sizeof(*addr)) return 0;
		memcpy(addr, hostent->h_addr, sizeof(*addr));
	}
	return 1;
}

static int parse_cidr(char *cidr, ipaddr_t *addr, ipaddr_t *mask)
{
	char *slash, *check;
	ipaddr_t a;
	int ok;
	unsigned long len;

	if ((slash= strchr(cidr, '/')) == NULL)
		return 0;

	*slash++= 0;
	ok= 1;

	if (!inet_aton(cidr, &a))
		ok= 0;

	len= strtoul(slash, &check, 10);
	if (check == slash || *check != 0 || len > 32)
		ok= 0;

	*--slash= '/';
	if (!ok)
		return 0;
	*addr= a;
	*mask= htonl(len == 0 ? 0 : (0xFFFFFFFF << (32-len)) & 0xFFFFFFFF);
	return 1;
}

/*
 * $PchId: add_route.c,v 1.6 2001/04/20 10:45:07 philip Exp $
 */
