/*
arp.c

Created:	Jan 2001 by Philip Homburg <philip@f-mnx.phicoh.com>

Manipulate ARP table
*/

#define _POSIX_C_SOURCE 2
#define _NETBSD_SOURCE 1

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/ioctl.h>

#include <net/netlib.h>
#include <net/gen/ether.h>
#include <net/gen/if_ether.h>
#include <net/gen/in.h>
#include <net/gen/inet.h>
#include <net/gen/ip_io.h>
#include <net/gen/netdb.h>
#include <net/gen/socket.h>

#include <net/gen/arp_io.h>

char *progname;
static int ipfd= -1;
static int do_setuid= 0;

static void do_open(char *devname);
static void show_one(char *hostname, int do_num);
static void show_all(int do_num);
static void print_one(ipaddr_t ipaddr, nwio_arp_t *arpp, int do_num);
static void delete_all(void);
static void delete(char *hostname);
static void do_set(char *hostname, char *ethername, int temp, int pub,
	int optdelete);
static ipaddr_t nametoipaddr(char *hostname);
static void fatal(const char *fmt, ...);
static void usage(void);

int main(int argc, char *argv[])
{
	int c;
	char *hostname, *ethername;
	int do_temp, do_pub;
	int a_flag, d_flag, n_flag, s_flag, S_flag;
	char *I_arg;

	(progname=strrchr(argv[0],'/')) ? progname++ : (progname=argv[0]);

	a_flag= d_flag= n_flag= s_flag= S_flag= 0;
	I_arg= NULL;
	while(c= getopt(argc, argv, "adnsS?I:"), c != -1)
	{
		switch(c)
		{
		case '?':	usage();
		case 'a':	a_flag= 1; break;
		case 'd':	d_flag= 1; break;
		case 'n':	n_flag= 1; break;
		case 's':	s_flag= 1; break;
		case 'S':	S_flag= 1; break;
		case 'I':	I_arg= optarg; break;
		default:	fatal("getopt failed: '%c'", c);
		}
	}

	hostname= NULL;		/* lint */
	ethername= NULL;	/* lint */
	do_temp= do_pub= 0;	/* lint */

	if (n_flag + d_flag + s_flag + S_flag > 1)
		usage();
	if (s_flag || S_flag)
	{
		if (optind >= argc) usage();
		hostname= argv[optind++];

		if (optind >= argc) usage();
		ethername= argv[optind++];

		do_temp= do_pub= 0;
		while (optind < argc) 
		{
			if (strcasecmp(argv[optind], "temp") == 0)
			{
				do_temp= 1;
				optind++;
				continue;
			}
			if (strcasecmp(argv[optind], "pub") == 0)
			{
				do_pub= 1;
				optind++;
				continue;
			}
			usage();
		}
	}
	else if (d_flag)
	{
		if (!a_flag)
		{
			if (optind >= argc)
				usage();
			hostname= argv[optind++];
			if (optind != argc)
				usage();
		}
	}
	else if (a_flag)
	{
		if (optind != argc)
			usage();
		do_setuid= 1;
	}
	else
	{
		if (optind >= argc)
			usage();
		hostname= argv[optind++];
		if (optind != argc)
			usage();
		do_setuid= 1;
	}

	do_open(I_arg);
	if (d_flag)
	{
		if (a_flag)
			delete_all();
		else
			delete(hostname);
	}
	else if (s_flag || S_flag)
		do_set(hostname, ethername, do_temp, do_pub, S_flag);
	else if (a_flag)
		show_all(n_flag);
	else
		show_one(hostname, n_flag);
	exit(0);
}

static void do_open(char *devname)
{
	size_t l;
	char *check;

	if (do_setuid && devname)
	{
		/* Only strings that consist of IP_DEVICE optionally 
		 * followed by a number are allowed.
		 */
		l= strlen(IP_DEVICE);
		if (strncmp(devname, IP_DEVICE, l) != 0)
			do_setuid= 0;
		else if (strlen(devname) == l)
			; /* OK */
		else
		{
			strtoul(devname+l, &check, 10);
			if (check[0] != '\0')
				do_setuid= 0;
		}
	}
	if (!devname)
		devname= IP_DEVICE;
	if (!do_setuid)
	{
		setuid(getuid());
		setgid(getgid());
	}
	ipfd= open(devname, O_RDWR);
	if (ipfd == -1)
		fatal("unable to open '%s': %s", devname, strerror(errno));
}

static void show_one(char *hostname, int do_num)
{
	int r;
	ipaddr_t ipaddr;
	nwio_arp_t arp;

	ipaddr= nametoipaddr(hostname);

	arp.nwa_ipaddr= ipaddr;
	r= ioctl(ipfd, NWIOARPGIP, &arp);
	if (r == -1 && errno == ENOENT)
	{
		print_one(ipaddr, NULL, do_num);
		exit(1);
	}
	if (r == -1)
		fatal("NWIOARPGIP failed: %s", strerror(errno));
	print_one(ipaddr, &arp, do_num);
}

static void show_all(int do_num)
{
	int ind, max, i, r;
	nwio_arp_t *arptab;
	nwio_arp_t arp;

	/* First get all entries */
	max= 10;
	ind= 0;
	arptab= malloc(max * sizeof(*arptab));
	if (arptab == NULL)
	{
		fatal("out of memory, can't get %d bytes",
			max*sizeof(*arptab));
	}
	arp.nwa_entno= 0;
	for (;;)
	{
		if (ind == max)
		{
			max *= 2;
			arptab= realloc(arptab, max * sizeof(*arptab));
			if (!arptab)
			{
				fatal("out of memory, can't get %d bytes",
					max*sizeof(*arptab));
			}
		}
		r= ioctl(ipfd, NWIOARPGNEXT, &arp);
		if (r == -1 && errno == ENOENT)
			break;
		if (r == -1)
			fatal("NWIOARPGNEXT failed: %s", strerror(errno));
		arptab[ind]= arp;
		ind++;
	}

	for (i= 0; i<ind; i++)
		print_one(0, &arptab[i], do_num);
}

static void print_one(ipaddr_t ipaddr, nwio_arp_t *arpp, int do_num)
{
	u32_t flags;
	struct hostent *he;

	if (arpp)
		ipaddr= arpp->nwa_ipaddr;
	if (!do_num)
		he= gethostbyaddr((char *)&ipaddr, sizeof(ipaddr), AF_INET);
	else
		he= NULL;
	if (he)
		printf("%s (%s)", he->h_name, inet_ntoa(ipaddr));
	else
		printf("%s", inet_ntoa(ipaddr));
	if (!arpp)
	{
		printf(" -- no entry\n");
		return;
	}
	flags= arpp->nwa_flags;
	if (flags & NWAF_INCOMPLETE)
		printf(" is incomplete");
	else if (flags & NWAF_DEAD)
		printf(" is dead");
	else
	{
		printf(" is at %s", ether_ntoa(&arpp->nwa_ethaddr));
		if (flags & NWAF_PERM)
			printf(" permanent");
		if (flags & NWAF_PUB)
			printf(" published");
	}
	printf("\n");
}

static void delete_all(void)
{
	int ind, max, i, r;
	nwio_arp_t *arptab;
	nwio_arp_t arp;

	/* First get all entries */
	max= 10;
	ind= 0;
	arptab= malloc(max * sizeof(*arptab));
	if (arptab == NULL)
	{
		fatal("out of memory, can't get %d bytes",
			max*sizeof(*arptab));
	}
	arp.nwa_entno= 0;
	for (;;)
	{
		if (ind == max)
		{
			max *= 2;
			arptab= realloc(arptab, max * sizeof(*arptab));
			if (arptab == NULL)
			{
				fatal("out of memory, can't get %d bytes",
					max*sizeof(*arptab));
			}
		}
		r= ioctl(ipfd, NWIOARPGNEXT, &arp);
		if (r == -1 && errno == ENOENT)
			break;
		if (r == -1)
			fatal("NWIOARPGNEXT failed: %s", strerror(errno));
		arptab[ind]= arp;
		ind++;
	}

	for (i= 0; i<ind; i++)
	{
		r= ioctl(ipfd, NWIOARPDIP, &arptab[i]);
		if (r == 0)
			continue;
		if (errno == EINVAL || errno == ENOENT)
		{
			/* Entry is incomplete of entry is already deleted */
			continue;
		}
		fatal("unable to delete host %s: %s",
			inet_ntoa(arptab[i].nwa_ipaddr), strerror(errno));
	}
}

static void delete(char *hostname)
{
	int r;
	ipaddr_t ipaddr;
	nwio_arp_t arp;

	ipaddr= nametoipaddr(hostname);
	arp.nwa_ipaddr= ipaddr;
	r= ioctl(ipfd, NWIOARPDIP, &arp);
	if (r == 0)
		return;
	if (errno == ENOENT)
	{
		print_one(ipaddr, NULL, 0);
		exit(1);
	}
	fatal("unable to delete host %s: %s", inet_ntoa(ipaddr),
		errno == EINVAL ? "entry is incomplete" : strerror(errno));
}

static void do_set(char *hostname, char *ethername, int temp, int pub,
	int optdelete)
{
	int r;
	ipaddr_t ipaddr;
	ether_addr_t *eap;
	ether_addr_t ethaddr;
	nwio_arp_t arp;
	nwio_ipconf_t ipconf;

	ipaddr= nametoipaddr(hostname);
	if (pub && strcasecmp(ethername, "auto") == 0)
	{
		r= ioctl(ipfd, NWIOGIPCONF, &ipconf);
		if (r == -1)
			fatal("NWIOGIPCONF failed: %s", strerror(errno));
		arp.nwa_ipaddr= ipconf.nwic_ipaddr;
		r= ioctl(ipfd, NWIOARPGIP, &arp);
		if (r == -1)
			fatal("NWIOARPGIP failed: %s", strerror(errno));
		ethaddr= arp.nwa_ethaddr;
	}
	else if (eap= ether_aton(ethername), eap != NULL)
		ethaddr= *eap;
	else if (ether_hostton(ethername, &ethaddr) != 0)
	{
		fatal("unable to parse ethernet address '%s'",
			ethername);
	}

	if (optdelete)
	{
		arp.nwa_ipaddr= ipaddr;
		r= ioctl(ipfd, NWIOARPDIP, &arp);
		if (r == -1 && errno != ENOENT)
		{
			fatal("unable to delete entry for host %s: %s",
				inet_ntoa(ipaddr),
				errno == EINVAL ? "incomplete entry" :
				strerror(errno));
		}
	}

	arp.nwa_ipaddr= ipaddr;
	arp.nwa_ethaddr= ethaddr;
	arp.nwa_flags= 0;
	if (pub)
		arp.nwa_flags |= NWAF_PUB;
	if (!temp)
		arp.nwa_flags |= NWAF_PERM;
	r= ioctl(ipfd, NWIOARPSIP, &arp);
	if (r == -1)
	{
		fatal("unable to set arp entry: %s",
			errno == EEXIST ? "entry exists" : strerror(errno));
	}
}

static ipaddr_t nametoipaddr(char *hostname)
{
	ipaddr_t ipaddr;
	struct hostent *he;

	if (inet_aton(hostname, &ipaddr) == 0)
	{
		he= gethostbyname(hostname);
		if (!he)
			fatal("unknown hostname '%s'", hostname);
		if (he->h_addrtype != AF_INET ||
			he->h_length != sizeof(ipaddr))
		{
			fatal("strange host '%s': addrtype %d, length %d",
				he->h_addrtype, he->h_length);
		}
		memcpy(&ipaddr, he->h_addr, sizeof(ipaddr));
	}
	return ipaddr;
}

#if 0
static char *ether_ntoa(struct ether_addr *eap)
{
	static char buf[]= "xx:xx:xx:xx:xx:xx";

	sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x",
		eap->ea_addr[0], eap->ea_addr[1],
		eap->ea_addr[2], eap->ea_addr[3],
		eap->ea_addr[4], eap->ea_addr[5]);
	return buf;
}
#endif

static void fatal(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	fprintf(stderr, "%s: ", progname);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);

	exit(1);
}

static void usage(void)
{
	fprintf(stderr, "Usage:\tarp [-I ip-dev] [-n] hostname\n"
		"\tarp [-I ip-dev] [-n] -a\n"
		"\tarp [-I ip-dev] -d hostname\n"
		"\tarp [-I ip-dev] -d -a\n"
		"\tarp [-I ip-dev] -s hostname ether-addr [temp] [pub]\n"
		"\tarp [-I ip-dev] -S hostname ether-addr [temp] [pub]\n");
	exit(1);
}

/*
 * $PchId: arp.c,v 1.3 2005/01/31 22:31:45 philip Exp $
 */
