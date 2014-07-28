/*	dhcpd.h - Dynamic Host Configuration Protocol daemon.
 *							Author: Kees J. Bot
 *								16 Dec 2000
 */

#define nil ((void*)0)

#include <minix/paths.h>
#include <net/if_ether.h>

/* Paths to files. */
#define PATH_DHCPCONF	_PATH_DHCPCONF
#define PATH_DHCPPID	_PATH_DHCPPID
#define PATH_DHCPCACHE	_PATH_DHCPCACHE
#define PATH_DHCPPOOL	_PATH_DHCPPOOL

#define CLID_MAX	32	/* Maximum client ID length. */

#ifndef EXTERN
#define EXTERN	extern
#endif

extern int lwip;

EXTERN char *program;		/* This program's name. */
extern char *configfile;	/* Configuration file. */
extern char *poolfile;		/* Dynamic address pool. */
EXTERN int serving;		/* True if being a DHCP server. */
EXTERN unsigned test;		/* Test level. */
EXTERN unsigned debug;		/* Debug level. */
EXTERN asynchio_t asyn;		/* Bookkeeping for all async I/O. */

/* BOOTP UDP ports:  (That they are different is quite stupid.) */
EXTERN u16_t port_server;	/* Port server listens on. */
EXTERN u16_t port_client;	/* Port client listens on. */

#define arraysize(a)	(sizeof(a) / sizeof((a)[0]))
#define arraylimit(a)	((a) + arraysize(a))
#define between(a,c,z)	(sizeof(c) <= sizeof(unsigned) ? \
	(unsigned) (c) - (a) <= (unsigned) (z) - (a) : \
	(unsigned long) (c) - (a) <= (unsigned long) (z) - (a))

/* To treat objects as octet arrays: */
#define B(a)		((u8_t *) (a))

/* Times. */
EXTERN time_t start, now;		/* Start and current time. */
EXTERN time_t event;			/* Time of the next timed event. */

/* Special times and periods: */
#define NEVER	(sizeof(time_t) <= sizeof(int) ? INT_MAX : LONG_MAX)
#define DELTA_FIRST		   4	/* Between first and second query. */
#define DELTA_FAST		  64	/* Unbound queries this often. */
#define DELTA_SLOW		 512	/* Bound queries are more relaxed. */
#define N_SOLICITS		   3	/* Number of solicitations. */
#define DELTA_SOL		   3	/* Time between solicitations. */
#define DELTA_ADV		2048	/* Router adverts to self lifetime. */

/* Buffers for packets. */
typedef struct buf {
	eth_hdr_t	*eth;		/* Ethernet header in payload. */
	ip_hdr_t	*ip;		/* IP header in payload. */
	udp_hdr_t	*udp;		/* UDP header in payload. */
	udp_io_hdr_t	*udpio;		/* UDP I/O header in payload. */
	dhcp_t		*dhcp;		/* DHCP data in payload. */
	u8_t		pad[2];		/* buf[] must start at 2 mod 4. */
					/* Payload: */
	u8_t		buf[ETH_MAX_PACK_SIZE];
} buf_t;

#define BUF_ETH_SIZE	(ETH_MAX_PACK_SIZE)
#define BUF_IP_SIZE	(BUF_ETH_SIZE - sizeof(eth_hdr_t))
#define BUF_UDP_SIZE	(BUF_IP_SIZE - sizeof(ip_hdr_t) - sizeof(udp_hdr_t) \
				+ sizeof(udp_io_hdr_t))

/* Type of network device open: Ethernet, ICMP, BOOTP client, BOOTP server. */
typedef enum { FT_CLOSED, FT_ETHERNET, FT_ICMP, FT_BOOTPC, FT_BOOTPS } fdtype_t;

#define FT_ALL	FT_CLOSED	/* To close all open descriptors at once. */

typedef struct fd {		/* An open descriptor. */
	i8_t		fd;		/* Open descriptor. */
	u8_t		fdtype;		/* Type of network open. */
	char		device[sizeof("/dev/eth###")];	/* Device name. */
	u8_t		n;		/* Network that owns it. */
	buf_t		*bp;		/* Associated packet buffer. */
	time_t		since;		/* Open since when? */
} fd_t;

/* Network state: Any IP device, Ethernet in sink mode, True Ethernet. */
typedef enum { NT_IP, NT_SINK, NT_ETHERNET } nettype_t;

typedef struct network {	/* Information on a network. */
	u8_t		n;		/* Network number. */
	ether_addr_t	eth;		/* Ethernet address of this net. */
	u8_t		type;		/* What kind of net is this? */
	i8_t		sol_ct;		/* Router solicitation count. */
	ether_addr_t	conflict;	/* Address conflict with this one. */
	unsigned	flags;		/* Various flags. */
	fd_t		*fdp;		/* Current open device. */
	struct network	*wait;		/* Wait for a resource list. */
	ipaddr_t	ip;		/* IP address of this net. */
	ipaddr_t	mask;		/* Associated netmask. */
	ipaddr_t	gateway;	/* My router. */
	ipaddr_t	server;		/* My DHCP server. */
	const char	*hostname;	/* Optional hostname to query for. */
	time_t		start;		/* Query or lease start time. */
	time_t		delta;		/* Query again after delta seconds. */
	time_t		renew;		/* Next query or go into renewal. */
	time_t		rebind;		/* When to go into rebind. */
	time_t		lease;		/* When our lease expires. */
	time_t		solicit;	/* Time to do a router solicitation. */
} network_t;

/* Flags. */
#define NF_NEGOTIATING	0x001		/* Negotiating with a DHCP server. */
#define NF_BOUND	0x002		/* Address configured through DHCP. */
#define NF_SERVING	0x004		/* I'm a server on this network. */
#define NF_RELAYING	0x008		/* I'm relaying for this network. */
#define NF_WAIT		0x010		/* Wait for a resource to free up. */
#define NF_IRDP		0x020		/* IRDP is used on this net. */
#define NF_CONFLICT	0x040		/* There is an address conflict. */
#define NF_POSSESSIVE	0x080		/* Keep address if lease expires. */
#define NF_INFORM	0x100		/* It's ok to answer DHCPINFORM. */

/* Functions defined in dhcpd.c. */
void report(const char *label);
void fatal(const char *label);
void *allocate(size_t size);
int ifname2if(const char *name);
network_t *if2net(int n);

/* Devices.c */
void get_buf(buf_t **bp);
void put_buf(buf_t **bp);
void give_buf(buf_t **dbp, buf_t **sbp);
network_t *newnetwork(void);
void closefd(fd_t *fdp);
int opendev(network_t *np, fdtype_t fdtype, int compete);
void closedev(network_t *np, fdtype_t fdtype);
char *ipdev(int n);
void set_ipconf(char *device, ipaddr_t ip, ipaddr_t mask, unsigned mtu);

/* Ether.c */
void udp2ether(buf_t *bp, network_t *np);
int ether2udp(buf_t *bp);
void make_arp(buf_t *bp, network_t *np);
int is_arp_me(buf_t *bp, network_t *np);
void icmp_solicit(buf_t *bp);
void icmp_advert(buf_t *bp, network_t *np);
ipaddr_t icmp_is_advert(buf_t *bp);

/* Tags.c */
#define gettag(dp, st, pd, pl)	dhcp_gettag((dp), (st), (pd), (pl))
void settag(dhcp_t *dp, int tag, void *data, size_t len);
char *cidr_ntoa(ipaddr_t addr, ipaddr_t mask);
void ether2clid(u8_t *clid, ether_addr_t *eth);
void initdhcpconf(void);
int makedhcp(dhcp_t *dp, u8_t *class, size_t calen, u8_t *client, size_t cilen,
				ipaddr_t ip, ipaddr_t ifip, network_t *np);
char *dhcptypename(int type);
void printdhcp(dhcp_t *dp);
