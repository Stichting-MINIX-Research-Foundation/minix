/*
rarpd.c

Created:	Nov 12, 1992 by Philip Homburg

Changed:	May 13, 1995 by Kees J. Bot
		Rewrite to handle multiple ethernets.

Changed:	Jul 18, 1995 by Kees J. Bot
		Do RARP requests (formerly inet's job)

Changed:	Dec 14, 1996 by Kees J. Bot
		Query the netmask

Changed:	Dec 11, 2000 by Kees J. Bot
		Dressed down to be only a RARP server, giving the floor to DHCP
*/

#include <sys/types.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/asynchio.h>
#include <net/hton.h>
#include <net/gen/socket.h>
#include <netdb.h>
#include <net/gen/in.h>
#include <net/gen/inet.h>
#include <net/gen/ether.h>
#include <net/gen/eth_io.h>
#include <net/gen/if_ether.h>
#include <net/gen/ip_io.h>
#include <arpa/nameser.h>

#define MAX_RARP_RETRIES	5
#define RARP_TIMEOUT		5

#undef HTONS
#define HTONS htons

typedef struct rarp46
{
	ether_addr_t a46_dstaddr;
	ether_addr_t a46_srcaddr;
	ether_type_t a46_ethtype;
	u16_t a46_hdr, a46_pro;
	u8_t a46_hln, a46_pln;
	u16_t a46_op;
	ether_addr_t a46_sha;
	u8_t a46_spa[4];
	ether_addr_t a46_tha;
	u8_t a46_tpa[4];
	char a46_padding[ETH_MIN_PACK_SIZE - (4*6 + 2*4 + 4*2 + 2*1)];
} rarp46_t;

#define ETH_RARP_PROTO	0x8035

#define RARP_ETHERNET	1

#define RARP_REQUEST	3
#define RARP_REPLY	4

static char *program;
static unsigned debug;

#define between(a, c, z)	((unsigned) (c) - (a) <= (unsigned) (z) - (a))

static void report(const char *label)
{
    fprintf(stderr, "%s: %s: %s\n", program, label, strerror(errno));
}

static void fatal(const char *label)
{
    report(label);
    exit(1);
}

static void *allocate(size_t size)
{
    void *mem;

    if ((mem= malloc(size)) == NULL) fatal("Can't allocate memory");
    return mem;
}

static char *ethdev(int n)
{
    static char an_ethdev[]= "/dev/ethNNN";

    sprintf(an_ethdev + sizeof(an_ethdev)-4, "%d", n);
    return an_ethdev;
}

static char *ipdev(int n)
{
    static char an_ipdev[]= "/dev/ipNNN";

    sprintf(an_ipdev + sizeof(an_ipdev)-4, "%d", n);
    return an_ipdev;
}

typedef struct ethernet {
	int		n;		/* Network number. */
	int		eth_fd;		/* Open low level ethernet device. */
	ether_addr_t	eth_addr;	/* Ethernet address of this net. */
	char		packet[ETH_MAX_PACK_SIZE];	/* Incoming packet. */
	ipaddr_t	ip_addr;	/* IP address of this net. */
	ipaddr_t	ip_mask;	/* Associated netmask. */
} ethernet_t;

static ethernet_t *ethernets;

static void onsig(int sig)
{
    switch (sig) {
    case SIGUSR1:	debug++;	break;
    case SIGUSR2:	debug= 0;	break;
    }
}

static void rarp_reply(ethernet_t *ep, char *hostname, ipaddr_t ip_addr,
						ether_addr_t eth_addr)
{
    rarp46_t rarp46;

    /* Construct a RARP reply packet and send it. */
    rarp46.a46_dstaddr= eth_addr;
    rarp46.a46_hdr= HTONS(RARP_ETHERNET);
    rarp46.a46_pro= HTONS(ETH_IP_PROTO);
    rarp46.a46_hln= 6;
    rarp46.a46_pln= 4;
    rarp46.a46_op= HTONS(RARP_REPLY);
    rarp46.a46_sha= ep->eth_addr;
    memcpy(rarp46.a46_spa, &ep->ip_addr, sizeof(ipaddr_t));
    rarp46.a46_tha= eth_addr;
    memcpy(rarp46.a46_tpa, &ip_addr, sizeof(ipaddr_t));

    if (debug >= 1) {
	printf("%s: Replying %s (%s) to %s\n",
	    ethdev(ep->n), inet_ntoa(ip_addr), hostname, ether_ntoa(&eth_addr));
    }
    (void) write(ep->eth_fd, &rarp46, sizeof(rarp46));
}

static int addhostname(char *addname, char *hostname, int n)
{
    /* Create an additional hostname for a given hostname by adding "-n" to
     * the first part.  E.g. given "wombat.cs.vu.nl" and n=2 return
     * "wombat-2.cs.vu.nl".  This is useful for VU practical work where
     * people get a few extra ethernet addresses on a machine and are asked
     * to build a TCP/IP stack on it.
     */
    char *dot;

    if (strlen(hostname) + 4 >= 1024) return 0;
    if ((dot= strchr(hostname, '.')) == NULL) dot= strchr(hostname, 0);
    sprintf(addname, "%.*s-%d%s", (dot - hostname), hostname, n, dot);
    return 1;
}

static void usage(void)
{
    fprintf(stderr, "Usage: %s [-d[level]] network-name ...\n", program);
    exit(1);
}

static int ifname2n(const char *name)
{
    /* Translate an interface name, ip0, ip1, etc, to a number. */
    const char *np;
    char *end;
    unsigned long n;

    np= name;
    if (*np++ != 'i' || *np++ != 'p') usage();
    n= strtoul(np, &end, 10);
    if (end == np || *end != 0) usage();
    if (n >= 1000) {
	fprintf(stderr, "%s: Network number of \"%s\" is a bit large\n",
	    program, name);
	exit(1);
    }
    return n;
}

int main(int argc, char **argv)
{
    int i;
    ethernet_t *ep;
    nwio_ethopt_t ethopt;
    nwio_ethstat_t ethstat;
    char hostname[1024];
    struct hostent *hostent;
    struct sigaction sa;
    nwio_ipconf_t ipconf;
    asynchio_t asyn;
    ssize_t n;
    ipaddr_t ip_addr;
    rarp46_t rarp46;
    int fd;
    int n_eths;

    program= argv[0];
    asyn_init(&asyn);

    debug= 0;
    i= 1;
    while (i < argc && argv[i][0] == '-') {
	char *opt= argv[i++]+1;

	if (opt[0] == '-' && opt[1] == 0) break;	/* -- */

	while (*opt != 0) switch (*opt++) {
	case 'd':
	    debug= 1;
	    if (between('0', *opt, '9')) debug= strtoul(opt, &opt, 10);
	    break;
	default:
	    usage();
	}
    }

    if ((n_eths= (argc - i)) == 0) usage();

#if __minix_vmd
    /* Minix-vmd can handle all nets at once using async I/O. */
    ethernets= allocate(n_eths * sizeof(ethernets[0]));
    for (i= 0; i < n_eths; i++) {
	ethernets[i].n= ifname2n(argv[argc - n_eths + i]);
    }
#else
    /* Minix forks n-1 times to handle each net in a process each. */
    for (i= 0; i < n_eths; i++) {
	if (i+1 < n_eths) {
	    switch (fork()) {
	    case -1:	fatal("fork()");
	    case 0:		break;
	    default:	continue;
	    }
	}
	ethernets= allocate(1 * sizeof(ethernets[0]));
	ethernets[0].n= ifname2n(argv[argc - n_eths + i]);
    }
    n_eths= 1;
#endif

    sa.sa_handler= onsig;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags= 0;
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);

    for (i= 0; i < n_eths; i++) {
	ep= &ethernets[i];
	if ((ep->eth_fd= open(ethdev(ep->n), O_RDWR)) < 0) fatal(ethdev(ep->n));

	if (ioctl(ep->eth_fd, NWIOGETHSTAT, &ethstat) < 0) {
	    fprintf(stderr, "%s: %s: Unable to get eth statistics: %s\n",
		program, ethdev(ep->n), strerror(errno));
	    exit(1);
	}
	ep->eth_addr= ethstat.nwes_addr;
	if (debug >= 1) {
	    printf("%s: Ethernet address is %s\n",
		ethdev(ep->n), ether_ntoa(&ep->eth_addr));
	}

	ethopt.nweo_flags= NWEO_COPY | NWEO_EN_LOC | NWEO_EN_BROAD |
		NWEO_TYPESPEC;
	ethopt.nweo_type= HTONS(ETH_RARP_PROTO);

	if (ioctl(ep->eth_fd, NWIOSETHOPT, &ethopt) < 0) {
	    fprintf(stderr, "%s: %s: Unable to set eth options: %s\n",
		program, ethdev(ep->n), strerror(errno));
	    exit(1);
	}

	/* What are my address and netmask? */
	if ((fd= open(ipdev(ep->n), O_RDWR)) < 0) fatal(ipdev(ep->n));
	if (ioctl(fd, NWIOGIPCONF, &ipconf) < 0) fatal(ipdev(ep->n));

	ep->ip_addr= ipconf.nwic_ipaddr;
	ep->ip_mask= ipconf.nwic_netmask;
	close(fd);
	if (debug >= 1) {
	    printf("%s: IP address is %s / ",
		ipdev(ep->n), inet_ntoa(ep->ip_addr));
	    printf("%s\n", inet_ntoa(ep->ip_mask));
	}
    }

    /* Wait for RARP requests, reply, repeat. */
    for(;;) {
	fflush(NULL);

	/* Wait for a RARP request. */
	for (i= 0; i < n_eths; i++) {
	    ep= &ethernets[i];

	    n= asyn_read(&asyn, ep->eth_fd, ep->packet, sizeof(ep->packet));
	    if (n != -1) break;
	    if (errno != EINPROGRESS) {
		report(ethdev(ep->n));
		sleep(10);
	    }
	}

	/* RARP request? */
	if (i < n_eths
	    && n >= sizeof(rarp46)
	    && (memcpy(&rarp46, ep->packet, sizeof(rarp46)), 1)
	    && rarp46.a46_hdr == HTONS(RARP_ETHERNET)
	    && rarp46.a46_pro == HTONS(ETH_IP_PROTO)
	    && rarp46.a46_hln == 6
	    && rarp46.a46_pln == 4
	    && rarp46.a46_op == HTONS(RARP_REQUEST)
	) {
	    if ((ether_ntohost(hostname, &rarp46.a46_tha) == 0
		  || (rarp46.a46_tha.ea_addr[0] == 'v'
		    && (memcpy(&ip_addr, rarp46.a46_tha.ea_addr+2, 4), 1)
		    && (hostent= gethostbyaddr((char*) &ip_addr,
						4, AF_INET)) != NULL
		    && addhostname(hostname, hostent->h_name,
						rarp46.a46_tha.ea_addr[1])))
		&& (hostent= gethostbyname(hostname)) != NULL
		&& hostent->h_addrtype == AF_INET
	    ) {
		/* Host is found in the ethers file and the DNS, or the
		 * ethernet address denotes a special additional address
		 * used for implementing a TCP/IP stack in user space.
		 */
		for (i= 0; hostent->h_addr_list[i] != NULL; i++) {
		    memcpy(&ip_addr, hostent->h_addr_list[i], sizeof(ipaddr_t));

		    /* Check if the address is on this network. */
		    if (((ip_addr ^ ep->ip_addr) & ep->ip_mask) == 0) break;
		}

		if (hostent->h_addr_list[i] != NULL) {
		    rarp_reply(ep, hostname, ip_addr, rarp46.a46_tha);
		} else {
		    if (debug >= 2) {
			printf("%s: Host '%s' (%s) is on the wrong net\n",
			    ethdev(ep->n),
			    hostname, ether_ntoa(&rarp46.a46_tha));
		    }
		}
	    } else {
		if (debug >= 2) {
		    printf("%s: RARP request from unknown host '%s'\n",
			ethdev(ep->n), ether_ntoa(&rarp46.a46_tha));
		}
	    }
	}

	/* Wait for another request. */
	if (asyn_wait(&asyn, 0, NULL) < 0) {
	    report("asyn_wait()");
	    sleep(10);
	}
    }
}
