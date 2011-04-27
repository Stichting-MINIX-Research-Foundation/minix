/*	irdpd 1.10 - Internet router discovery protocol daemon.
 *							Author: Kees J. Bot
 *								28 May 1994
 * Activily solicitate or passively look for routers.
 * Based heavily on its forerunners, the irdp_sol and rip2icmp daemons by
 * Philip Homburg.
 */
#define nil 0
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/asynchio.h>
#include <net/hton.h>
#include <net/netlib.h>
#include <net/gen/in.h>
#include <net/gen/ip_hdr.h>
#include <net/gen/icmp.h>
#include <net/gen/icmp_hdr.h>
#include <net/gen/ip_io.h>
#include <net/gen/inet.h>
#include <net/gen/netdb.h>
#include <net/gen/oneCsum.h>
#include <net/gen/socket.h>
#include <net/gen/udp.h>
#include <net/gen/udp_hdr.h>
#include <net/gen/udp_io.h>

#define MAX_SOLICITATIONS	    3	/* # router solicitations. */
#define SOLICITATION_INTERVAL	    3	/* Secs between solicitate retries. */
#define DEST_TO 	      (10*60)	/* 10 minutes */
#define NEW_ROUTE	       (5*60)	/* 5 minutes */
#define DANGER		       (2*60)	/* Nearing a advert timeout? */
#define DEAD_TO		    (24L*60*60)	/* 24 hours */
#define DEAD_PREF	    0x80000000L	/* From RFC 1256 */
#define MaxAdvertisementInterval (DEST_TO/2)	/* Chosen to jive with RIP */
#define AdvertisementLifetime	 DEST_TO
#define PRIO_OFF_DEF		-1024
#define RIP_REPLY		    2

/* It's now or never. */
#define IMMEDIATELY	((time_t) ((time_t) -1 < 0 ? LONG_MIN : 0))
#define NEVER		((time_t) ((time_t) -1 < 0 ? LONG_MAX : ULONG_MAX))

#if !__minix_vmd
/* Standard Minix needs to choose between router discovery and RIP info. */
int do_rdisc= 1;
int do_rip= 0;
#else
/* VMD Minix can do both at once. */
#define do_rdisc 1
#define do_rip 1
#endif

int rip_fd;			/* For incoming RIP packet. */
int irdp_fd;			/* Receive or transmit IRDP packets. */

char *udp_device;		/* UDP device to use. */
char *ip_device;		/* IP device to use. */

int priority_offset;		/* Offset to make my routes less preferred. */

int bcast= 0;			/* Broadcast adverts to all. */
int debug= 0;

char rip_buf[8192];		/* Incoming RIP packet buffer. */
char irdp_buf[1024];		/* IRDP buffer. */

typedef struct routeinfo
{
	u8_t		command;
	u8_t		version;
	u16_t		zero1;
	struct routedata {
		u16_t	family;
		u16_t	zero2;
		u32_t	ip_addr;
		u32_t	zero3, zero4;
		u32_t	metric;
	} data[1];
} routeinfo_t;

typedef struct table
{
	ipaddr_t	tab_gw;
	i32_t		tab_pref;
	time_t		tab_time;
} table_t;

table_t *table;			/* Collected table of routing info. */
size_t table_size;

int sol_retries= MAX_SOLICITATIONS;
time_t next_sol= IMMEDIATELY;
time_t next_advert= NEVER;
time_t router_advert_valid= IMMEDIATELY;
time_t now;

void report(const char *label)
/* irdpd: /dev/hd0: Device went up in flames */
{
	fprintf(stderr, "irdpd: %s: %s\n", label, strerror(errno));
}

void fatal(const char *label)
/* irdpd: /dev/house: Taking this with it */
{
	report(label);
	exit(1);
}

#if DEBUG
char *addr2name(ipaddr_t host)
/* Translate an IP address to a printable name. */
{
	struct hostent *hostent;

	hostent= gethostbyaddr((char *) &host, sizeof(host), AF_INET);
	return hostent == nil ? inet_ntoa(host) : hostent->h_name;
}
#else
#define addr2name(host)	inet_ntoa(host)
#endif

void print_table(void)
/* Show the collected routing table. */
{
	int i;
	table_t *ptab;
	struct tm *tm;

	for (i= 0, ptab= table; i < table_size; i++, ptab++) {
		if (ptab->tab_time < now - DEAD_TO) continue;

		tm= localtime(&ptab->tab_time);
		printf("%-40s %6ld %02d:%02d:%02d\n",
			addr2name(ptab->tab_gw),
			(long) ptab->tab_pref,
			tm->tm_hour, tm->tm_min, tm->tm_sec);
	}
}

void advertize(ipaddr_t host)
/* Send a router advert to a host. */
{
	char *buf, *data;
	ip_hdr_t *ip_hdr;
	icmp_hdr_t *icmp_hdr;
	int i;
	table_t *ptab;

	buf= malloc(sizeof(*ip_hdr) + offsetof(icmp_hdr_t, ih_dun.uhd_data)
			+ table_size * (sizeof(ipaddr_t) + sizeof(u32_t)));
	if (buf == nil) fatal("heap error");

	ip_hdr= (ip_hdr_t *) buf;
	icmp_hdr= (icmp_hdr_t *) (ip_hdr + 1);

	ip_hdr->ih_vers_ihl= 0x45;
	ip_hdr->ih_dst= host;

	icmp_hdr->ih_type= ICMP_TYPE_ROUTER_ADVER;
	icmp_hdr->ih_code= 0;
	icmp_hdr->ih_hun.ihh_ram.iram_na= 0;
	icmp_hdr->ih_hun.ihh_ram.iram_aes= 2;
	icmp_hdr->ih_hun.ihh_ram.iram_lt= htons(AdvertisementLifetime);
	data= (char *) icmp_hdr->ih_dun.uhd_data;

	/* Collect gateway entries from the table. */
	for (i= 0, ptab= table; i < table_size; i++, ptab++) {
		if (ptab->tab_time < now - DEAD_TO) continue;

		icmp_hdr->ih_hun.ihh_ram.iram_na++;
		if (ptab->tab_time < now - DEST_TO) ptab->tab_pref= DEAD_PREF;
		* (ipaddr_t *) data= ptab->tab_gw;
		data+= sizeof(ipaddr_t);
		* (i32_t *) data= htonl(ptab->tab_pref);
		data+= sizeof(i32_t);
	}
	icmp_hdr->ih_chksum= 0;
	icmp_hdr->ih_chksum= ~oneC_sum(0, icmp_hdr, data - (char *) icmp_hdr);

	if (icmp_hdr->ih_hun.ihh_ram.iram_na > 0) {
		/* Send routing info. */

		if (debug) {
			printf("Routing table send to %s:\n", addr2name(host));
			print_table();
		}

		if (write(irdp_fd, buf, data - buf) < 0) {
			(errno == EIO ? fatal : report)(ip_device);
		}
	}
	free(buf);
}

void time_functions(void)
/* Perform time dependend functions: router solicitation, router advert. */
{
	if (now >= next_sol) {
		char buf[sizeof(ip_hdr_t) + 8];
		ip_hdr_t *ip_hdr;
		icmp_hdr_t *icmp_hdr;

		if (sol_retries == 0) {
			/* Stop soliciting. */
			next_sol= NEVER;
#if !__minix_vmd
			/* Switch to RIP if no router responded. */
			if (table_size == 0) {
				do_rip= 1;
				do_rdisc= 0;
			}
#endif
			return;
		}

		/* Broadcast a router solicitation to find a router. */
		ip_hdr= (ip_hdr_t *) buf;
		icmp_hdr= (icmp_hdr_t *) (ip_hdr + 1);

		ip_hdr->ih_vers_ihl= 0x45;
#ifdef __NBSD_LIBC
		ip_hdr->ih_dst= htonl(0xFFFFFFFFL);
#else
		ip_hdr->ih_dst= HTONL(0xFFFFFFFFL);
#endif
		icmp_hdr->ih_type= ICMP_TYPE_ROUTE_SOL;
		icmp_hdr->ih_code= 0;
		icmp_hdr->ih_chksum= 0;
		icmp_hdr->ih_hun.ihh_unused= 0;
		icmp_hdr->ih_chksum= ~oneC_sum(0, icmp_hdr, 8);

		if (debug) printf("Broadcasting router solicitation\n");

		if (write(irdp_fd, buf, sizeof(buf)) < 0)
			fatal("sending router solicitation failed");

		/* Schedule the next packet. */
		next_sol= now + SOLICITATION_INTERVAL;

		sol_retries--;
	}

	if (now >= next_advert) {
		/* Advertize routes to the local host (normally), or
		 * broadcast them (to keep bad hosts up.)
		 */

#ifdef __NBSD_LIBC
		advertize(bcast ? htonl(0xFFFFFFFFL) : htonl(0x7F000001L));
#else
		advertize(bcast ? HTONL(0xFFFFFFFFL) : HTONL(0x7F000001L));
#endif
		next_advert= now + MaxAdvertisementInterval;
#if !__minix_vmd
		/* Make sure we are listening to RIP now. */
		do_rip= 1;
		do_rdisc= 0;
#endif
	}
}

void add_gateway(ipaddr_t host, i32_t pref)
/* Add a router with given address and preference to the routing table. */
{
	table_t *oldest, *ptab;
	int i;

	/* Look for the host, or select the oldest entry. */
	oldest= nil;
	for (i= 0, ptab= table; i < table_size; i++, ptab++) {
		if (ptab->tab_gw == host) break;

		if (oldest == nil || ptab->tab_time < oldest->tab_time)
			oldest= ptab;
	}

	/* Don't evict the oldest if it is still valid. */
	if (oldest != nil && oldest->tab_time >= now - DEST_TO) oldest= nil;

	/* Expand the table? */
	if (i == table_size && oldest == nil) {
		table_size++;
		table= realloc(table, table_size * sizeof(*table));
		if (table == nil) fatal("heap error");
		oldest= &table[table_size - 1];
	}

	if (oldest != nil) {
		ptab= oldest;
		ptab->tab_gw= host;
		ptab->tab_pref= DEAD_PREF;
	}

	/* Replace an entry if the new one looks more promising. */
	if (pref >= ptab->tab_pref || ptab->tab_time <= now - NEW_ROUTE) {
		ptab->tab_pref= pref;
		ptab->tab_time= now;
	}
}

void rip_incoming(ssize_t n)
/* Use a RIP packet to add to the router table.  (RIP packets are really for
 * between routers, but often it is the only information around.)
 */
{
	udp_io_hdr_t *udp_io_hdr;
	u32_t default_dist;
	i32_t pref;
	routeinfo_t *routeinfo;
	struct routedata *data, *end;

	/* We don't care about RIP packets when there are router adverts. */
	if (now + MaxAdvertisementInterval < router_advert_valid) return;

	udp_io_hdr= (udp_io_hdr_t *) rip_buf;
	if (udp_io_hdr->uih_data_len != n - sizeof(*udp_io_hdr)) {
		if (debug) printf("Bad sized route packet (discarded)\n");
		return;
	}
	routeinfo= (routeinfo_t *) (rip_buf + sizeof(*udp_io_hdr)
			+ udp_io_hdr->uih_ip_opt_len);

	if (routeinfo->command != RIP_REPLY) {
		if (debug) {
			printf("RIP-%d packet command %d ignored\n",
				routeinfo->version, routeinfo->command);
		}
		return;
	}

	/* Look for a default route, the route to the gateway. */
	end= (struct routedata *) (rip_buf + n);
	default_dist= (u32_t) -1;
	for (data= routeinfo->data; data < end; data++) {
		if (ntohs(data->family) != AF_INET || data->ip_addr != 0)
			continue;
		default_dist= ntohl(data->metric);
		if (default_dist >= 256) {
			if (debug) {
				printf("Strange metric %lu\n",
					(unsigned long) default_dist);
			}
		}
	}
	pref= default_dist >= 256 ? 1 : 512 - default_dist;
	pref+= priority_offset;

	/* Add the gateway to the table with the calculated preference. */
	add_gateway(udp_io_hdr->uih_src_addr, pref);

	if (debug) {
		printf("Routing table after RIP-%d packet from %s:\n",
			routeinfo->version,
			addr2name(udp_io_hdr->uih_src_addr));
		print_table();
	}

	/* Start advertizing. */
	if (next_advert == NEVER) next_advert= IMMEDIATELY;
}

void irdp_incoming(ssize_t n)
/* Look for router solicitations and router advertisements.  The solicitations
 * are probably from other irdpd daemons, we answer them if we do not expect
 * a real router to answer.  The advertisements cause this daemon to shut up.
 */
{
	ip_hdr_t *ip_hdr;
	icmp_hdr_t *icmp_hdr;
	int ip_hdr_len;
	char *data;
	int i;
	int router;
	ipaddr_t addr;
	i32_t pref;
	time_t valid;

	ip_hdr= (ip_hdr_t *) irdp_buf;
	ip_hdr_len= (ip_hdr->ih_vers_ihl & IH_IHL_MASK) << 2;
	if (n < ip_hdr_len + 8) {
		if (debug) printf("Bad sized ICMP (discarded)\n");
		return;
	}

	icmp_hdr= (icmp_hdr_t *)(irdp_buf + ip_hdr_len);

	/* Did I send this myself? */
	if (ip_hdr->ih_src == ip_hdr->ih_dst) return;
	if ((htonl(ip_hdr->ih_src) & 0xFF000000L) == 0x7F000000L) return;

	if (icmp_hdr->ih_type != ICMP_TYPE_ROUTER_ADVER) return;

	/* Incoming router advertisement, the kind of packet the TCP/IP task
	 * is very happy with.  No need to solicit further.
	 */
	sol_retries= 0;

	/* Add router info to our table.  Also see if the packet really came
	 * from a router.  If so then we can go dormant for the lifetime of
	 * the ICMP.
	 */
	router= 0;
	data= (char *) icmp_hdr->ih_dun.uhd_data;
	for (i= 0; i < icmp_hdr->ih_hun.ihh_ram.iram_na; i++) {
		addr= * (ipaddr_t *) data;
		data+= sizeof(ipaddr_t);
		pref= htonl(* (i32_t *) data);
		data+= sizeof(i32_t);

		if (addr == ip_hdr->ih_src) {
			/* The sender is in the routing table! */
			router= 1;
		}
		add_gateway(addr, pref);
	}

	valid= now + ntohs(icmp_hdr->ih_hun.ihh_ram.iram_lt);
	if (router) router_advert_valid= valid;

	/* Restart advertizing close to the timeout of the advert.  (No more
	 * irdpd adverts if the router stays alive.)
	 */
	if (router || next_advert > valid - DANGER)
		next_advert= valid - DANGER;

	if (debug) {
		printf("Routing table after advert received from %s:\n",
			addr2name(ip_hdr->ih_src));
		print_table();
		if (router) {
			struct tm *tm= localtime(&router_advert_valid);
			printf(
			"This router advert is valid until %02d:%02d:%02d\n",
				tm->tm_hour, tm->tm_min, tm->tm_sec);
		}
	}
}

void sig_handler(int sig)
/* A signal changes the debug level. */
{
	switch (sig) {
	case SIGUSR1:	debug++;		break;
	case SIGUSR2:	debug= 0;		break;
	}
}

void usage(void)
{
	fprintf(stderr,
"Usage: irdpd [-bd] [-U udp-device] [-I ip-device] [-o priority-offset]\n");
	exit(1);
}

int main(int argc, char **argv)
{
	int i;
	struct servent *service;
	udpport_t route_port;
	nwio_udpopt_t udpopt;
	nwio_ipopt_t ipopt;
	asynchio_t asyn;
	time_t timeout;
	struct timeval tv;
	struct sigaction sa;
	char *offset_arg, *offset_end;
	long arg;

	udp_device= ip_device= nil;
	offset_arg= nil;

	for (i = 1; i < argc && argv[i][0] == '-'; i++) {
		char *p= argv[i] + 1;

		if (p[0] == '-' && p[1] == 0) { i++; break; }

		while (*p != 0) {
			switch (*p++) {
			case 'U':
				if (udp_device != nil) usage();
				if (*p == 0) {
					if (++i == argc) usage();
					p= argv[i];
				}
				udp_device= p;
				p= "";
				break;
			case 'I':
				if (ip_device != nil) usage();
				if (*p == 0) {
					if (++i == argc) usage();
					p= argv[i];
				}
				ip_device= p;
				p= "";
				break;
			case 'o':
				if (offset_arg != nil) usage();
				if (*p == 0) {
					if (++i == argc) usage();
					p= argv[i];
				}
				offset_arg= p;
				p= "";
				break;
			case 'b':
				bcast= 1;
				break;
			case 's':
				/*obsolete*/
				break;
			case 'd':
				debug= 1;
				break;
			default:
				usage();
			}
		}
	}
	if (i != argc) usage();

	/* Debug level signals. */
	sa.sa_handler= sig_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags= 0;
	sigaction(SIGUSR1, &sa, nil);
	sigaction(SIGUSR2, &sa, nil);

	if (udp_device == nil && (udp_device= getenv("UDP_DEVICE")) == nil)
		udp_device= UDP_DEVICE;

	if (ip_device == nil && (ip_device= getenv("IP_DEVICE")) == nil)
		ip_device= IP_DEVICE;

	if (offset_arg == nil) {
		priority_offset= PRIO_OFF_DEF;
	} else {
		arg= strtol(offset_arg, &offset_end, 0);
		if (*offset_end != 0 || (priority_offset= arg) != arg) usage();
	}

	if ((service= getservbyname("route", "udp")) == nil) {
		fprintf(stderr,
	"irdpd: unable to look up the port number for the 'route' service\n");
		exit(1);
	}

	route_port= (udpport_t) service->s_port;

	if ((rip_fd= open(udp_device, O_RDWR)) < 0) fatal(udp_device);

	udpopt.nwuo_flags= NWUO_COPY | NWUO_LP_SET | NWUO_DI_LOC
		| NWUO_EN_BROAD | NWUO_RP_SET | NWUO_RA_ANY | NWUO_RWDATALL
		| NWUO_DI_IPOPT;
	udpopt.nwuo_locport= route_port;
	udpopt.nwuo_remport= route_port;
	if (ioctl(rip_fd, NWIOSUDPOPT, &udpopt) < 0)
		fatal("setting UDP options failed");

	if ((irdp_fd= open(ip_device, O_RDWR)) < 0) fatal(ip_device);

	ipopt.nwio_flags= NWIO_COPY | NWIO_EN_LOC | NWIO_EN_BROAD
			| NWIO_REMANY | NWIO_PROTOSPEC
			| NWIO_HDR_O_SPEC | NWIO_RWDATALL;
	ipopt.nwio_tos= 0;
	ipopt.nwio_ttl= 1;
	ipopt.nwio_df= 0;
	ipopt.nwio_hdropt.iho_opt_siz= 0;
#ifdef __NBSD_LIBC
	ipopt.nwio_rem= htonl(0xFFFFFFFFL);
#else
	ipopt.nwio_rem= HTONL(0xFFFFFFFFL);
#endif
	ipopt.nwio_proto= IPPROTO_ICMP;

	if (ioctl(irdp_fd, NWIOSIPOPT, &ipopt) < 0)
		fatal("can't configure ICMP channel");

	asyn_init(&asyn);

	while (1) {
		ssize_t r;

		if (do_rip) {
			/* Try a RIP read. */
			r= asyn_read(&asyn, rip_fd, rip_buf, sizeof(rip_buf));
			if (r < 0) {
				if (errno == EIO) fatal(udp_device);
				if (errno != EINPROGRESS) report(udp_device);
			} else {
				now= time(nil);
				rip_incoming(r);
			}
		}

		if (do_rdisc) {
			/* Try an IRDP read. */
			r= asyn_read(&asyn, irdp_fd, irdp_buf,
							sizeof(irdp_buf));
			if (r < 0) {
				if (errno == EIO) fatal(ip_device);
				if (errno != EINPROGRESS) report(ip_device);
			} else {
				now= time(nil);
				irdp_incoming(r);
			}
		}
		fflush(stdout);

		/* Compute the next wakeup call. */
		timeout= next_sol < next_advert ? next_sol : next_advert;

		/* Wait for a RIP or IRDP packet or a timeout. */
		tv.tv_sec= timeout;
		tv.tv_usec= 0;
		if (asyn_wait(&asyn, 0, timeout == NEVER ? nil : &tv) < 0) {
			/* Timeout? */
			if (errno != EINTR && errno != EAGAIN)
				fatal("asyn_wait()");
			now= time(nil);
			time_functions();
		}
	}
}
