/*
vmd/cmd/simple/pr_routes.c
*/

#define _POSIX_C_SOURCE 2

#include <sys/types.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <net/netlib.h>
#include <net/hton.h>
#include <net/gen/in.h>
#include <net/gen/ip_io.h>
#include <net/gen/route.h>
#include <netdb.h>
#include <net/gen/inet.h>

#define N_IF	64	/* More than enough? */

char *prog_name;
int all_devices;
char *ifname;
ipaddr_t iftab[N_IF];

static void print_header(void);
static void print_route(nwio_route_t *route);
static void fill_iftab(void);
static char *get_ifname(ipaddr_t addr);
static void fatal(char *fmt, ...);
static void usage(void);

int main(int argc, char *argv[])
{
	int nr_routes, i;
	nwio_route_t route;
	nwio_ipconf_t ip_conf;
	unsigned long ioctl_cmd;
	int ip_fd;
	int result;
	int c;
	char *ip_device, *cp;
	int a_flag, i_flag, o_flag;
	char *I_arg;

	prog_name= argv[0];

	a_flag= 0;
	i_flag= 0;
	o_flag= 0;
	I_arg= NULL;
	while ((c =getopt(argc, argv, "?aI:io")) != -1)
	{
		switch(c)
		{
		case '?':
			usage();
		case 'a':
			if (a_flag)
				usage();
			a_flag= 1;
			break;
		case 'I':
			if (I_arg)
				usage();
			I_arg= optarg;
			break;
		case 'i':
			if (i_flag || o_flag)
				usage();
			i_flag= 1;
			break;
		case 'o':
			if (i_flag || o_flag)
				usage();
			o_flag= 1;
			break;
		default:
			fprintf(stderr, "%s: getopt failed: '%c'\n",
				prog_name, c);
			exit(1);
		}
	}
	if (optind != argc)
		usage();

	ip_device= I_arg;
	all_devices= a_flag;

	if (i_flag)
		ioctl_cmd= NWIOGIPIROUTE;
	else
		ioctl_cmd= NWIOGIPOROUTE;

	if (ip_device == NULL)
		ip_device= getenv("IP_DEVICE");
	ifname= ip_device;
	if (ip_device == NULL)
		ip_device= IP_DEVICE;
		
	ip_fd= open(ip_device, O_RDONLY);
	if (ip_fd == -1)
	{
		fprintf(stderr, "%s: unable to open %s: %s\n", prog_name,
			ip_device, strerror(errno));
		exit(1);
	}

	if (!all_devices && ifname)
	{
		cp= strrchr(ip_device, '/');
		if (cp)
			ifname= cp+1;
	}
	else
	{
		ifname= NULL;
		fill_iftab();
	}

	result= ioctl(ip_fd, NWIOGIPCONF, &ip_conf);
	if (result == -1)
	{
		fprintf(stderr, "%s: unable to NWIOIPGCONF: %s\n",
			prog_name, strerror(errno));
		exit(1);
	}

	route.nwr_ent_no= 0;
	result= ioctl(ip_fd, ioctl_cmd, &route);
	if (result == -1)
	{
		fprintf(stderr, "%s: unable to NWIOGIPxROUTE: %s\n",
			prog_name, strerror(errno));
		exit(1);
	}
	print_header();
	nr_routes= route.nwr_ent_count;
	for (i= 0; i<nr_routes; i++)
	{
		route.nwr_ent_no= i;
		result= ioctl(ip_fd, ioctl_cmd, &route);
		if (result == -1)
		{
			fprintf(stderr, "%s: unable to NWIOGIPxROUTE: %s\n",
				prog_name, strerror(errno));
			exit(1);
		}
		if (all_devices || route.nwr_ifaddr == ip_conf.nwic_ipaddr)
			print_route(&route);
	}
	exit(0);
}

int ent_width= 5;
int if_width= 4;
int dest_width= 18;
int gateway_width= 15;
int dist_width= 4;
int pref_width= 5;
int mtu_width= 4;

static void print_header(void)
{
	printf("%*s ", ent_width, "ent #");
	printf("%*s ", if_width, "if");
	printf("%*s ", dest_width, "dest");
	printf("%*s ", gateway_width, "gateway");
	printf("%*s ", dist_width, "dist");
	printf("%*s ", pref_width, "pref");
	printf("%*s ", mtu_width, "mtu");
	printf("%s", "flags");
	printf("\n");
}

static char *cidr2a(ipaddr_t addr, ipaddr_t mask)
{
	ipaddr_t testmask= 0xFFFFFFFF;
	int n;
	static char result[sizeof("255.255.255.255/255.255.255.255")];

	for (n= 32; n >= 0; n--)
	{
		if (mask == htonl(testmask))
			break;
		testmask= (testmask << 1) & 0xFFFFFFFF;
	}

	sprintf(result, "%s/%-2d", inet_ntoa(addr), n);
	if (n == -1)
		strcpy(strchr(result, '/')+1, inet_ntoa(mask));
	return result;
}

static void print_route(nwio_route_t *route)
{
	if (!(route->nwr_flags & NWRF_INUSE))
		return;

	printf("%*lu ", ent_width, (unsigned long) route->nwr_ent_no);
	printf("%*s ", if_width,
		ifname ?  ifname : get_ifname(route->nwr_ifaddr));
	printf("%*s ", dest_width, cidr2a(route->nwr_dest, route->nwr_netmask));
	printf("%*s ", gateway_width, inet_ntoa(route->nwr_gateway));
	printf("%*lu ", dist_width, (unsigned long) route->nwr_dist);
	printf("%*ld ", pref_width, (long) route->nwr_pref);
	printf("%*lu", mtu_width, (long) route->nwr_mtu);
	if (route->nwr_flags & NWRF_STATIC)
		printf(" static");
	if (route->nwr_flags & NWRF_UNREACHABLE)
		printf(" dead");
	printf("\n");
}

static void fill_iftab(void)
{
	int i, j, r, fd;
	nwio_ipconf_t ip_conf;
	char dev_name[12];	/* /dev/ipXXXX */

	for (i= 0; i<N_IF; i++)
	{
		iftab[i]= 0;

		sprintf(dev_name, "/dev/ip%d", i);
		fd= open(dev_name, O_RDWR);
		if (fd == -1)
		{
			if (errno == EACCES || errno == ENOENT || errno == ENXIO)
				continue;
			fatal("unable to open '%s': %s",
				dev_name, strerror(errno));
		}
		fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
		r= ioctl(fd, NWIOGIPCONF, &ip_conf);
		if (r == -1 && errno == EAGAIN)
		{
			/* interface is down */
			close(fd);
			continue;
		}
		if (r == -1)
		{
			fatal("NWIOGIPCONF failed on %s: %s",
				dev_name, strerror(errno));
		}

		iftab[i]= ip_conf.nwic_ipaddr;
		close(fd);

		for (j= 0; j<i; j++)
		{
			if (iftab[j] == iftab[i])
			{
				fatal("duplicate address in ip%d and ip%d: %s",
					i, j, inet_ntoa(iftab[i]));
			}
		}

	}
}

static char *get_ifname(ipaddr_t addr)
{
	static char name[7];	/* ipXXXX */

	int i;

	for (i= 0; i<N_IF; i++)
	{
		if (iftab[i] != addr)
			continue;
		sprintf(name, "ip%d", i);
		return name;
	}

	return inet_ntoa(addr);
}

static void fatal(char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	fprintf(stderr, "%s: ", prog_name);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
	exit(1);
}

static void usage(void)
{
	fprintf(stderr, "Usage: %s [-i|-o] [ -a ] [ -I <ip-device> ]\n",
		prog_name);
	exit(1);
}

/*
 * $PchId: pr_routes.c,v 1.8 2002/04/11 10:58:58 philip Exp $
 */
