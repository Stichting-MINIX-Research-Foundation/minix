/*	dhcpd 1.15 - Dynamic Host Configuration Protocol daemon.
 *							Author: Kees J. Bot
 *								11 Jun 1999
 */
#include <sys/types.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <configfile.h>
#include <sys/ioctl.h>
#include <sys/asynchio.h>
#include <net/hton.h>
#include <net/gen/socket.h>
#include <net/gen/netdb.h>
#include <net/gen/in.h>
#include <net/gen/inet.h>
#include <net/gen/ether.h>
#include <net/gen/if_ether.h>
#include <net/gen/eth_hdr.h>
#include <net/gen/ip_hdr.h>
#include <net/gen/udp.h>
#include <net/gen/udp_hdr.h>
#include <net/gen/udp_io.h>
#include <net/gen/dhcp.h>
#include "arp.h"
#define EXTERN
#include "dhcpd.h"

char *configfile= PATH_DHCPCONF;
char *poolfile= PATH_DHCPPOOL;
static char *cachefile= PATH_DHCPCACHE;
static int qflag;		/* True if printing cached DHCP data. */
static int aflag, rflag;	/* True if adding or deleting pool addresses. */

#define BCAST_IP	HTONL(0xFFFFFFFFUL)

/* We try to play with up to this many networks. */
#define N_NETS		32
static unsigned n_nets;		/* Actual number of networks. */

void report(const char *label)
{
    fprintf(stderr, "%s: %s: %s\n", program, label, strerror(errno));
}

void fatal(const char *label)
{
    report(label);
    exit(1);
}

void *allocate(size_t size)
{
    void *mem;

    if ((mem= malloc(size)) == nil) fatal("Can't allocate memory");
    return mem;
}

/* Choose a DHCP xid based on the start time and network number.  Not really
 * random, but we don't have anything more random than the clock anyway.
 */
#define XID(np)		htonl(((u32_t) (np)->start << 8) | (np)->n)

static network_t *network[N_NETS];

int ifname2if(const char *name)
{
    /* Translate an interface name to a number, -1 if bad. */
    char *end;
    unsigned long n;

    if (*name++ != 'i' || *name++ != 'p') return -1;
    n= strtoul(name, &end, 10);
    if (end == name || *end != 0) return -1;
    if (n >= N_NETS) return -1;
    return n;
}

network_t *if2net(int n)
{
    /* Translate an interface number to a network struct. */
    int i;

    for (i= 0; i < n_nets; i++) {
	if (network[i]->n == n) return network[i];
    }
    return nil;
}

static ipaddr_t defaultmask(ipaddr_t ip)
{
    /* Compute netmask by the oldfashioned Class rules. */
    if (B(&ip)[0] < 0x80) return HTONL(0xFF000000UL);	/* Class A. */
    if (B(&ip)[0] < 0xC0) return HTONL(0xFFFF0000UL);	/* Class B. */
    if (B(&ip)[0] < 0xE0) return HTONL(0xFFFFFF00UL);	/* Class C. */
    return HTONL(0xFFFFFFFFUL);  /* Multicast?  Shouldn't happen... */
}

#define POOL_MAGIC	HTONL(0x81F85D00UL)

typedef struct pool {		/* Dynamic pool entry. */
	u32_t		magic;		/* Pool file magic number. */
	ipaddr_t	ip;		/* IP address. */
	u32_t		expire;		/* When does/did the lease expire? */
	u8_t		len;		/* Client ID length. */
	u8_t		unused[19];	/* Space for extensions. */
	u8_t		clid[CLID_MAX];	/* Client ID of current/last user. */
} pool_t;

static int openpool(int mode)
{
    /* Open the dynamic pool and lock it, return fd on success or -1. */
    int fd;
    struct flock lck;

    if ((fd= open(poolfile, mode, 0644)) < 0) {
	if (errno != ENOENT) fatal(poolfile);
	return -1;
    }
    if (mode != O_RDONLY) {
	lck.l_type= F_WRLCK;
	lck.l_whence= SEEK_SET;
	lck.l_start= 0;
	lck.l_len= 0;
	if (fcntl(fd, F_SETLKW, &lck) < 0) fatal(poolfile);
    }
    return fd;
}

static int readpool(int fd, pool_t *entry)
{
    /* Read one pool table entry, return true unless EOF. */
    ssize_t r;

    if ((r= read(fd, entry, sizeof(*entry))) < 0) fatal(poolfile);
    if (r == 0) return 0;

    if (r != sizeof(*entry) || entry->magic != POOL_MAGIC) {
	fprintf(stderr, "%s: %s: Pool table is corrupt\n",
	    program, poolfile);
	close(fd);
	return 0;
    }
    return 1;
}

#if !__minix_vmd	/* No fsync() for Minix. */
#define fsync(fd)	sync()
#endif

static void writepool(int fd, pool_t *entry)
{
    /* (Over)write a pool table entry. */
    if (write(fd, entry, sizeof(*entry)) < 0
	|| (entry->expire > now && fsync(fd) < 0)
    ) {
	fatal(poolfile);
    }
}

static ipaddr_t findpool(u8_t *client, size_t len, ipaddr_t ifip)
{
    /* Look for a client ID in the dynamic address pool within the same network
     * as 'ifip'.  Select an unused one for a new client if necessary.  Return
     * 0 if nothing is available, otherwise the IP address we can offer.
     */
    int fd, found;
    pool_t entry, oldest;
    dhcp_t dhcp;
    u8_t *pmask;
    ipaddr_t mask;

    /* Any information available on the network the client is at? */
    if (!makedhcp(&dhcp, nil, 0, nil, 0, ifip, ifip, nil)) return 0;

    if ((fd= openpool(O_RDWR)) < 0) return 0;

    (void) gettag(&dhcp, DHCP_TAG_NETMASK, &pmask, nil);
    memcpy(&mask, pmask, sizeof(mask));

    oldest.expire= NEVER;
    while ((found= readpool(fd, &entry))) {
	/* Deleted entry? */
	if (entry.ip == 0) continue;

	/* Correct network? */
	if (((entry.ip ^ ifip) & mask) != 0) continue;

	/* Client present? */
	if (entry.len == len && memcmp(entry.clid, client, len) == 0) break;

	/* Oldest candidate for a new lease? */
	entry.expire= ntohl(entry.expire);
	if (entry.expire < oldest.expire) oldest= entry;
    }
    close(fd);

    if (found) return entry.ip;
    if (oldest.expire <= now) return oldest.ip;
    return 0;
}

static int commitpool(ipaddr_t ip, u8_t *client, size_t len, time_t expire)
{
    /* Commit a new binding to stable storage, return true on success. */
    int fd;
    pool_t entry;

    if ((fd= openpool(O_RDWR)) < 0) return 0;

    do {
	if (!readpool(fd, &entry)) {
	    close(fd);
	    return 0;
	}
    } while (entry.ip != ip);

    entry.expire= htonl(expire);
    entry.len= len;
    memcpy(entry.clid, client, len);
    if (lseek(fd, -(off_t)sizeof(entry), SEEK_CUR) == -1) fatal(poolfile);
    writepool(fd, &entry);
    close(fd);
    return 1;
}

static void updatepool(int add, const char *name)
{
    /* Add a new IP address to the dynamic pool. */
    ipaddr_t ip;
    int fd, i;
    pool_t entry;
    struct hostent *he;
    off_t off, off0;

    if ((he= gethostbyname(name)) == nil || he->h_addrtype != AF_INET) {
	fprintf(stderr, "%s: %s: Unknown host\n", program, name);
	exit(1);
    }
    for (i= 0; he->h_addr_list[i] != nil; i++) {}
    if (i != 1) {
	fprintf(stderr, "%s: %s has %d addresses\n", program, name, i);
	exit(1);
    }
    memcpy(&ip, he->h_addr_list[0], sizeof(ip));

    if ((fd= openpool(O_RDWR|O_CREAT)) < 0) fatal(poolfile);

    off= 0;
    off0= -1;
    while (readpool(fd, &entry)) {
	if (add) {
	    if (entry.ip == ip) {
		fprintf(stderr, "%s: %s: %s is already present\n",
		    program, poolfile, name);
		exit(1);
	    }
	    if (entry.ip == 0 && off0 == -1) off0= off;
	} else {
	    if (entry.ip == ip) {
		memset(&entry, 0, sizeof(entry));
		entry.magic= POOL_MAGIC;
		entry.ip= 0;
		if (lseek(fd, off, SEEK_SET) == -1) fatal(poolfile);
		writepool(fd, &entry);
	    }
	}
	off+= sizeof(entry);
    }

    if (add) {
	if (off0 != -1 && lseek(fd, off0, SEEK_SET) == -1) fatal(poolfile);
	memset(&entry, 0, sizeof(entry));
	entry.magic= POOL_MAGIC;
	entry.ip= ip;
	writepool(fd, &entry);
    }
    close(fd);
}

static void cachedhcp(int n, dhcp_t *dp)
{
    /* Store a DHCP packet in a cache where those who care can find it. */
    static int inited;
    FILE *fp;
    int fd;
    int mode;

    if (test > 0) return;

    if (!inited) {
	/* First time, clear store and also save my pid. */
	if ((fp= fopen(PATH_DHCPPID, "w")) != nil) {
	    if (fprintf(fp, "%d\n", getpid()) == EOF || fclose(fp) == EOF) {
		fatal(PATH_DHCPPID);
	    }
	}
	inited= 1;
	mode= O_WRONLY | O_CREAT | O_TRUNC;
    } else {
	mode= O_WRONLY;
    }

    dp->xid= htonl(now);	/* To tell how old this data is. */

    if ((fd= open(cachefile, mode, 0666)) < 0
	|| lseek(fd, (off_t) n * sizeof(*dp), SEEK_SET) == -1
	|| write(fd, dp, sizeof(*dp)) < 0
	|| close(fd) < 0
    ) {
	if (errno != ENOENT) fatal(cachefile);
    }
}

static void printdata(void)
{
    /* Show the contents of the cache and the dynamic pool. */
    int fd;
    dhcp_t d;
    ssize_t r;
    int i;
    pool_t entry;
    unsigned long expire;
    char delta[3*sizeof(u32_t)];

    initdhcpconf();

    if ((fd= open(cachefile, O_RDONLY)) < 0) fatal(cachefile);
    i= 0;
    while ((r= read(fd, &d, sizeof(d))) == sizeof(d)) {
	if (d.yiaddr != 0) {
	    printf("DHCP data for network %d:\n", i);
	    printdhcp(&d);
	}
	i++;
    }
    if (r < 0) fatal(cachefile);
    close(fd);

    if ((fd= openpool(O_RDONLY)) >= 0) {
	printf("Dynamic address pool since %ld:\n", (long) now);
	while (readpool(fd, &entry)) {
	    if (entry.ip == 0) continue;
	    expire= ntohl(entry.expire);
	    if (expire == 0) {
		strcpy(delta, "unused");
	    } else
	    if (expire == 0xFFFFFFFFUL) {
		strcpy(delta, "infinite");
	    } else
	    if (expire < now) {
		sprintf(delta, "-%lu", now - expire);
	    } else {
		sprintf(delta, "+%lu", expire - now);
	    }
	    printf("\t%-15s %8s  ", inet_ntoa(entry.ip), delta);
	    for (i= 0; i < entry.len; i++) {
		printf("%02X", entry.clid[i]);
	    }
	    fputc('\n', stdout);
	}
	close(fd);
    }
}

static udpport_t portbyname(const char *name)
{
    struct servent *se;

    if ((se= getservbyname(name, "udp")) == nil) {
	fprintf(stderr, "%s: Unknown port \"%s\"\n", program, name);
	exit(1);
    }
    return se->s_port;
}

static int send(network_t *np, void *data, size_t len)
{
    /* Send out a packet using a filedescriptor that is probably in async mode,
     * so first dup() a sync version, then write.  Return true on success.
     */
    int fd;
    ssize_t r;

    if ((fd= dup(np->fdp->fd)) < 0) fatal("Can't dup()");
    if ((r= write(fd, data, len)) < 0) {
	report(np->fdp->device);
	sleep(10);
    }
    close(fd);
    return r >= 0;
}

static size_t servdhcp(network_t *np, buf_t *bp, size_t dlen)
{
    buf_t *abp= nil;
    ipaddr_t cip, ifip;
    u8_t defclid[1+sizeof(bp->dhcp->chaddr)];
    u8_t *pdata, *client, *class, *server, *reqip, *lease;
    u32_t expire;
    size_t len, cilen, calen;
    int type, dyn;
    u8_t atype;
    static char NAKMESS[] = "IP address requested isn't yours";

    if (test > 0) return 0;

    /* The IP address of the interface close to the client. */
    ifip= bp->dhcp->giaddr != 0 ? bp->dhcp->giaddr : np->ip;

    /* All kinds of DHCP tags. */
    if (gettag(bp->dhcp, DHCP_TAG_TYPE, &pdata, nil)) {
	type= *pdata;
    } else {
	type= -1;		/* BOOTP? */
    }

    if (!gettag(bp->dhcp, DHCP_TAG_CLIENTID, &client, &cilen)) {
	defclid[0]= bp->dhcp->htype;
	memcpy(defclid+1, bp->dhcp->chaddr, bp->dhcp->hlen);
	client= defclid;
	cilen= 1+bp->dhcp->hlen;
    }

    if (!gettag(bp->dhcp, DHCP_TAG_CLASSID, &class, &calen)) {
	calen= 0;
    }

    if (!gettag(bp->dhcp, DHCP_TAG_SERVERID, &server, nil)) {
	server= B(&np->ip);
    }

    if (!gettag(bp->dhcp, DHCP_TAG_REQIP, &reqip, nil)) {
	reqip= nil;
    }

    /* I'm a server?  Then see if I know this client. */
    if ((np->flags & NF_SERVING)
	&& bp->dhcp->op == DHCP_BOOTREQUEST
	&& between(1, bp->dhcp->hlen, sizeof(bp->dhcp->chaddr))
	&& (server == nil || memcmp(server, &np->ip, sizeof(np->ip)) == 0)
    ) {
	get_buf(&abp);

	/* Is the client in my tables? */
	(void) makedhcp(abp->dhcp, class, calen, client, cilen, 0, ifip, nil);
	cip= abp->dhcp->yiaddr;

	dyn= 0;
	/* If not, do we have a dynamic address? */
	if (cip == 0 && (cip= findpool(client, cilen, ifip)) != 0) dyn= 1;

	if (type == DHCP_INFORM) {
	    /* The client already has an address, it just wants information.
	     * We only answer if we could answer a normal request (cip != 0),
	     * unless configured to answer anyone.
	     */
	    if (cip != 0 || (np->flags & NF_INFORM)) cip= bp->dhcp->ciaddr;
	}

	if (cip == 0 || !makedhcp(abp->dhcp, class, calen,
					client, cilen, cip, ifip, nil)) {
	    put_buf(&abp);
	}

	if (abp != nil) {
	    if (gettag(abp->dhcp, DHCP_TAG_LEASE, &lease, nil)) {
		memcpy(&expire, lease, sizeof(expire));
		expire= now + ntohl(expire);
		if (expire < now) expire= 0xFFFFFFFFUL;
	    } else {
		if (dyn) {
		    /* A dynamic address must have a lease. */
		    fprintf(stderr, "%s: No lease set for address %s\n",
			program, inet_ntoa(cip));
		    exit(1);
		}
		lease= nil;
		expire= 0xFFFFFFFFUL;
	    }

	    /* What does our client want, and what do we say? */
	    switch (type) {
	    case DHCP_DISCOVER:
		atype= DHCP_OFFER;

		/* Assign this address for a short moment. */
		if (dyn && !commitpool(cip, client, cilen, now + DELTA_FAST)) {
		    put_buf(&abp);
		}
		break;

	    case -1:/* BOOTP */
	    case DHCP_REQUEST:
	    case DHCP_INFORM:
		atype= DHCP_ACK;
		/* The address wanted must be the address we offer. */
		if ((reqip != nil && memcmp(reqip, &cip, sizeof(cip)) != 0)
		    || (bp->dhcp->ciaddr != 0 && bp->dhcp->ciaddr != cip)
		) {
		    atype= DHCP_NAK;
		} else
		if (dyn && type == DHCP_REQUEST) {
		    /* Assign this address for the duration of the lease. */
		    if (!commitpool(cip, client, cilen, expire)) put_buf(&abp);
		}
		break;

	    case DHCP_DECLINE:
		/* Our client doesn't want the offered address! */
		if (dyn
		    && reqip != nil
		    && memcmp(reqip, &cip, sizeof(cip)) == 0
		) {
		    int i;

		    fprintf(stderr, "%s: ", program);
		    for (i= 0; i < cilen; i++) {
			fprintf(stderr, "%02X", client[i]);
		    }
		    fprintf(stderr, " declines %s", inet_ntoa(cip));
		    if (gettag(bp->dhcp, DHCP_TAG_MESSAGE, &pdata, &len)) {
			fprintf(stderr, " saying: \"%.*s\"", (int)len, pdata);
		    }
		    fputc('\n', stderr);

		    /* Disable address for the duration of the lease. */
		    (void) commitpool(cip, nil, 0, expire);
		}
		put_buf(&abp);
		break;

	    case DHCP_RELEASE:
		/* Our client is nice enough to return its address. */
		if (dyn) (void) commitpool(cip, client, cilen, now);
		put_buf(&abp);
		break;

	    default:	/* Anything else is ignored. */
		put_buf(&abp);
	    }
	}

	if (abp != nil) {
	    /* Finish the return packet. */
	    abp->dhcp->htype= bp->dhcp->htype;
	    abp->dhcp->hlen= bp->dhcp->hlen;
	    abp->dhcp->hops= 0;
	    abp->dhcp->xid= bp->dhcp->xid;
	    abp->dhcp->secs= 0;
	    abp->dhcp->flags= bp->dhcp->flags;
	    abp->dhcp->ciaddr= 0;
	    abp->dhcp->yiaddr= atype == DHCP_NAK ? 0 : cip;
	    if (atype == DHCP_NAK) abp->dhcp->siaddr= 0;
	    abp->dhcp->giaddr= bp->dhcp->giaddr;
	    memcpy(abp->dhcp->chaddr,bp->dhcp->chaddr,sizeof(bp->dhcp->chaddr));

	    settag(abp->dhcp, DHCP_TAG_SERVERID, &np->ip, sizeof(np->ip));

	    if (lease == nil) {
		/* No lease specified?  Then give an infinite lease. */
		settag(abp->dhcp, DHCP_TAG_LEASE, &expire, sizeof(expire));
	    }

	    if (type == DHCP_INFORM) {
		/* Oops, this one has a fixed address, so no lease business. */
		abp->dhcp->yiaddr= 0;
		settag(abp->dhcp, DHCP_TAG_LEASE, nil, 0);
		settag(abp->dhcp, DHCP_TAG_RENEWAL, nil, 0);
		settag(abp->dhcp, DHCP_TAG_REBINDING, nil, 0);
	    }

	    if (atype == DHCP_NAK) {
		/* A NAK doesn't need much. */
		memset(abp->dhcp->sname, 0, sizeof(abp->dhcp->sname));
		memset(abp->dhcp->file, 0, sizeof(abp->dhcp->file));
		memset(abp->dhcp->options, 255, sizeof(abp->dhcp->options));
		settag(abp->dhcp, DHCP_TAG_MESSAGE, NAKMESS, sizeof(NAKMESS));
	    }

	    settag(abp->dhcp, DHCP_TAG_TYPE, &atype, sizeof(atype));

	    /* Figure out where to send this to. */
	    abp->udpio->uih_src_addr= np->ip;
	    abp->udpio->uih_src_port= port_server;
	    if (bp->dhcp->giaddr != 0) {
		abp->udpio->uih_dst_addr= bp->dhcp->giaddr;
		abp->udpio->uih_dst_port= port_server;
	    } else
	    if (bp->dhcp->flags & DHCP_FLAGS_BCAST) {
		abp->udpio->uih_dst_addr= BCAST_IP;
		abp->udpio->uih_dst_port= port_client;
	    } else
	    if (bp->udpio->uih_src_addr != 0
		&& bp->udpio->uih_dst_addr == np->ip
	    ) {
		abp->udpio->uih_dst_addr= bp->udpio->uih_src_addr;
		abp->udpio->uih_dst_port= port_client;
	    } else {
		abp->udpio->uih_dst_addr= BCAST_IP;
		abp->udpio->uih_dst_port= port_client;
	    }
	    abp->udpio->uih_ip_opt_len= 0;
	    abp->udpio->uih_data_len= sizeof(dhcp_t);

	    /* Copy the packet to the input buffer, and return the new size. */
	    memcpy(bp->buf, abp->buf, sizeof(bp->buf));
	    put_buf(&abp);
	    return sizeof(udp_io_hdr_t) + sizeof(dhcp_t);
	}
    }

    /* I'm a relay?  If it is a not already a relayed request then relay. */
    if ((np->flags & NF_RELAYING)
	&& bp->dhcp->op == DHCP_BOOTREQUEST
	&& bp->dhcp->giaddr == 0
    ) {
	bp->dhcp->giaddr= np->ip;
	bp->udpio->uih_src_addr= np->ip;
	bp->udpio->uih_src_port= port_server;
	bp->udpio->uih_dst_addr= np->server;
	bp->udpio->uih_dst_port= port_server;
	return dlen;
    }

    /* I'm a relay?  If the server sends a reply to me then relay back. */
    if ((np->flags & NF_RELAYING)
	&& bp->dhcp->op == DHCP_BOOTREPLY
	&& bp->dhcp->giaddr == np->ip
    ) {
	bp->dhcp->giaddr= 0;
	bp->udpio->uih_src_addr= np->ip;
	bp->udpio->uih_src_port= port_server;
	bp->udpio->uih_dst_addr= BCAST_IP;
	bp->udpio->uih_dst_port= port_client;
	return dlen;
    }

    /* Don't know what to do otherwise, so doing nothing seems wise. */
    return 0;
}

static void onsig(int sig)
{
    switch (sig) {
    case SIGUSR1:	debug++;	break;
    case SIGUSR2:	debug= 0;	break;
    }
}

static void usage(void)
{
    fprintf(stderr,
"Usage: %s [-qar] [-t[L]] [-d[L]] [-f config] [-c cache] [-p pool] [host ...]\n",
	program);
    exit(1);
}

int main(int argc, char **argv)
{
    int i;
    network_t *np;
    struct sigaction sa;
    ssize_t r= -1;
    buf_t *bp= nil;
    static struct timeval eventtv;

    program= argv[0];
    start= now= time(nil);

    debug= 0;
    i= 1;
    while (i < argc && argv[i][0] == '-') {
	char *opt= argv[i++]+1;

	if (opt[0] == '-' && opt[1] == 0) break;	/* -- */

	while (*opt != 0) switch (*opt++) {
	case 'f':
	    if (*opt == 0) {
		if (i == argc) usage();
		opt= argv[i++];
	    }
	    configfile= opt;
	    opt= "";
	    break;
	case 'c':
	    if (*opt == 0) {
		if (i == argc) usage();
		opt= argv[i++];
	    }
	    cachefile= opt;
	    opt= "";
	    break;
	case 'p':
	    if (*opt == 0) {
		if (i == argc) usage();
		opt= argv[i++];
	    }
	    poolfile= opt;
	    opt= "";
	    break;
	case 't':
	    test= 1;
	    if (between('0', *opt, '9')) test= strtoul(opt, &opt, 10);
	    break;
	case 'd':
	    debug= 1;
	    if (between('0', *opt, '9')) debug= strtoul(opt, &opt, 10);
	    break;
	case 'q':
	    qflag= 1;
	    break;
	case 'a':
	    aflag= 1;
	    break;
	case 'r':
	    rflag= 1;
	    break;
	default:
	    usage();
	}
    }
    if (aflag + rflag + qflag > 1) usage();

    if (aflag || rflag) {
	/* Add or remove addresses from the dynamic pool. */
	while (i < argc) updatepool(aflag, argv[i++]);
	exit(0);
    }

    if (i != argc) usage();

    if (qflag) {
	/* Only show the contents of the cache and dynamic pool to the user. */
	printdata();
	exit(0);
    }

    /* BOOTP ports. */
    port_server= portbyname("bootps");
    port_client= portbyname("bootpc");

    sa.sa_handler= onsig;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags= 0;
    sigaction(SIGUSR1, &sa, nil);
    sigaction(SIGUSR2, &sa, nil);

    /* Initial configuration. */
    for (i= 0; i < N_NETS; i++) {
	int fd;
	ipaddr_t ip, mask;

	/* Is there something there? */
	if ((fd= open(ipdev(i), O_RDWR|O_NONBLOCK)) < 0) {
	    if (errno != ENOENT && errno != ENODEV && errno != ENXIO) {
		fatal(ipdev(i));
	    }
	    continue;
	}
	close(fd);

	network[n_nets++]= np= newnetwork();
	np->n= i;

	/* Ethernet? */
	if (opendev(np, FT_ETHERNET, 1)) {
	    np->type= B(&np->eth)[0] != 'Z' ? NT_ETHERNET : NT_SINK;
	    if (debug >= 1) {
		printf("%s: Ethernet address is %s%s\n",
		    np->fdp->device, ether_ntoa(&np->eth),
		    np->type == NT_SINK ? " (sink)" : "");
	    }
	    closedev(np, FT_ETHERNET);
	}

	/* Only true Ethernets worry about DHCP. */
	if (np->type != NT_ETHERNET) np->renew= np->rebind= np->lease= NEVER;
    }

    /* Try to find my interfaces in the DHCP table. */
    for (i= 0; i < n_nets; i++) {
	ipaddr_t cip;
	u8_t clid[1+DHCP_HLEN_ETH];
	size_t cilen;

	np= network[i];
	if (np->flags & NF_BOUND) continue;

	if (np->type == NT_IP) {
	    cilen= 0;
	} else {
	    ether2clid(clid, &np->eth);
	    cilen= 1+DHCP_HLEN_ETH;
	}

	/* Try to find an Ethernet address, or the IP address of an already
	 * configured network.  If we have data we get an IP address back.
	 */
	get_buf(&bp);
	(void) makedhcp(bp->dhcp, (u8_t *) "Minix", 5,
					clid, cilen, np->ip, 0, np);
	cip= bp->dhcp->yiaddr;

	/* Gather information on the interface. */
	if (cip != 0
	    && makedhcp(bp->dhcp, (u8_t *) "Minix", 5,
					clid, cilen, cip, cip, np)
	    && test < 2
	) {
	    u8_t *pdata;
	    u16_t mtu;

	    cachedhcp(np->n, bp->dhcp);
	    np->ip= cip;
	    (void) gettag(bp->dhcp, DHCP_TAG_NETMASK, &pdata, nil);
	    memcpy(&np->mask, pdata, sizeof(np->mask));
	    if (gettag(bp->dhcp, DHCP_TAG_GATEWAY, &pdata, nil)) {
		memcpy(&np->gateway, pdata, sizeof(np->gateway));
	    } else {
		np->gateway= 0;
	    }
	    if (gettag(bp->dhcp, DHCP_TAG_IPMTU, &pdata, nil)) {
		memcpy(&mtu, pdata, sizeof(mtu));
		mtu= ntohs(mtu);
	    } else {
		mtu= 0;
	    }
	    set_ipconf(ipdev(np->n), np->ip, np->mask, mtu);
	    if (debug >= 1) {
		printf("%s: IP address is %s\n",
		    ipdev(np->n), cidr_ntoa(np->ip, np->mask));
	    }
	    np->flags |= NF_BOUND;
	    np->renew= np->rebind= np->lease= NEVER;
	    np->sol_ct= N_SOLICITS;
	    np->solicit= 0;

	    /* Other (previous) interfaces may have been defined. */
	    i= 0;
	}
	put_buf(&bp);
    }

    for (;;) {
	now= time(nil);
	event= NEVER;

	/* Is it time to request/renew a lease? */
	for (i= 0; i < n_nets; i++) {
	    np= network[i];

	    if (np->renew <= now) {
		u8_t type;
		static u8_t taglist[] = {
		    DHCP_TAG_NETMASK, DHCP_TAG_GATEWAY, DHCP_TAG_DNS
		};
		u8_t ethclid[1+DHCP_HLEN_ETH];

		/* We may have lost our binding or even our lease. */
		if (np->rebind <= now) np->server= BCAST_IP;

		if (np->lease <= now) {
		    if (np->flags & NF_BOUND) closedev(np, FT_ALL);

		    if ((np->flags & (NF_BOUND | NF_POSSESSIVE)) == NF_BOUND) {
			set_ipconf(ipdev(np->n), np->ip= 0, np->mask= 0, 0);
			if (debug >= 1) {
			    printf("%s: Interface disabled (lease expired)\n",
				ipdev(np->n));
			}
		    }
		    np->flags &= ~NF_BOUND;
		}

		/* See if we can open the network we need to send on. */
		if (!(np->flags & NF_BOUND)) {
		    if (!opendev(np, FT_ETHERNET, 1)) continue;
		} else {
		    if (!opendev(np, FT_BOOTPC, 1)) continue;
		}

		if (!(np->flags & NF_NEGOTIATING)) {
		    /* We need to start querying a DHCP server. */
		    np->start= now;
		    np->delta= DELTA_FIRST;
		    np->flags |= NF_NEGOTIATING;
		}

		/* Fill in a DHCP query packet. */
		get_buf(&bp);
		dhcp_init(bp->dhcp);
		bp->dhcp->op= DHCP_BOOTREQUEST;
		bp->dhcp->htype= DHCP_HTYPE_ETH;
		bp->dhcp->hlen= DHCP_HLEN_ETH;
		bp->dhcp->xid= XID(np);
		bp->dhcp->secs= htons(now - np->start > 0xFFFF
					? 0xFFFF : now - np->start);
		memcpy(bp->dhcp->chaddr, &np->eth, sizeof(np->eth));

		if (np->lease <= now) {
		    /* First time, or my old server is unresponsive. */
		    type= DHCP_DISCOVER;
		} else {
		    /* Request an offered address or renew an address. */
		    type= DHCP_REQUEST;
		    if (np->flags & NF_BOUND) {
			/* A renewal, I claim my current address. */
			bp->dhcp->ciaddr= np->ip;
		    } else {
			/* Nicely ask for the address just offered. */
			settag(bp->dhcp, DHCP_TAG_REQIP, &np->ip,
							sizeof(np->ip));
			settag(bp->dhcp, DHCP_TAG_SERVERID, &np->server,
							sizeof(np->server));
		    }
		}
		settag(bp->dhcp, DHCP_TAG_TYPE, &type, 1);

		/* My client ID.  Simply use the default. */
		ether2clid(ethclid, &np->eth);
		settag(bp->dhcp, DHCP_TAG_CLIENTID, ethclid, sizeof(ethclid));

		/* The Class ID may serve to recognize Minix hosts. */
		settag(bp->dhcp, DHCP_TAG_CLASSID, "Minix", 5);

		/* The few tags that Minix can make good use of. */
		settag(bp->dhcp, DHCP_TAG_REQPAR, taglist, sizeof(taglist));

		/* Some weird sites use a hostname, not a client ID. */
		if (np->hostname != nil) {
		    settag(bp->dhcp, DHCP_TAG_HOSTNAME,
					np->hostname, strlen(np->hostname));
		}

		bp->udpio->uih_src_addr= np->ip;
		bp->udpio->uih_dst_addr= np->server;
		bp->udpio->uih_src_port= port_client;
		bp->udpio->uih_dst_port= port_server;
		bp->udpio->uih_ip_opt_len= 0;
		bp->udpio->uih_data_len= sizeof(dhcp_t);

		if (!(np->flags & NF_BOUND)) {
		    /* Rebind over Ethernet. */
		    udp2ether(bp, np);
		    if (send(np, bp->eth, sizeof(eth_hdr_t) + sizeof(ip_hdr_t)
					+ sizeof(udp_hdr_t) + sizeof(dhcp_t))) {
			if (debug >= 1) {
			    printf("%s: Broadcast DHCP %s\n",
				np->fdp->device, dhcptypename(type));
			    if (debug >= 2) printdhcp(bp->dhcp);
			}
		    }
		} else {
		    /* Renew over UDP. */
		    if (send(np, bp->udpio, sizeof(udp_io_hdr_t)
							+ sizeof(dhcp_t))) {
			if (debug >= 1) {
			    printf("%s: Sent DHCP %s to %s\n",
				np->fdp->device,
				dhcptypename(type),
				inet_ntoa(np->server));
			    if (debug >= 2) printdhcp(bp->dhcp);
			}
		    }
		}
		put_buf(&bp);

		/* When to continue querying a DHCP server? */
		if (np->flags & NF_BOUND) {
		    /* Still bound, keep halving time till next event. */
		    time_t e, d;

		    e= now < np->rebind ? np->rebind : np->lease;
		    d= (e - now) / 2;
		    if (d < DELTA_SLOW) d= DELTA_SLOW;
		    np->renew= now + d;
		    if (np->renew > e) np->renew= e;
		} else {
		    /* Not bound, be desparate. */
		    np->renew= now + np->delta;
		    if ((np->delta *= 2) > DELTA_FAST) np->delta= DELTA_FAST;
		}
	    }
	    if (np->renew < event) event= np->renew;
	}

	/* Read DHCP responses. */
	for (i= 0; i < n_nets; i++) {
	    np= network[i];
	    if (!(np->flags & NF_NEGOTIATING)) continue;

	    if (!(np->flags & NF_BOUND)) {
		if (!opendev(np, FT_ETHERNET, 0)) continue;
		get_buf(&np->fdp->bp);
		r= asyn_read(&asyn, np->fdp->fd, np->fdp->bp->eth,
							BUF_ETH_SIZE);
	    } else {
		if (!opendev(np, FT_BOOTPC, 0)) continue;
		get_buf(&np->fdp->bp);
		r= asyn_read(&asyn, np->fdp->fd, np->fdp->bp->udpio,
							BUF_UDP_SIZE);
	    }
	    if (r != -1) break;
	    if (errno != ASYN_INPROGRESS && errno != EPACKSIZE) {
		report(np->fdp->device);
		sleep(10);
	    }
	}

	/* Is there a response? */
	if (i < n_nets) {
	    give_buf(&bp, &np->fdp->bp);
	    if (((!(np->flags & NF_BOUND)
		    && r >= (sizeof(eth_hdr_t) + sizeof(ip_hdr_t)
				+ sizeof(udp_hdr_t) + offsetof(dhcp_t, options))
		    && ether2udp(bp)
		    && bp->udpio->uih_dst_port == port_client)
		  ||
		  ((np->flags & NF_BOUND)
		    && r >= sizeof(udp_io_hdr_t) + offsetof(dhcp_t, options)))
		&& bp->dhcp->op == DHCP_BOOTREPLY
		&& bp->dhcp->htype == DHCP_HTYPE_ETH
		&& bp->dhcp->hlen == DHCP_HLEN_ETH
		&& bp->dhcp->xid == XID(np)
		&& memcmp(bp->dhcp->chaddr, &np->eth, sizeof(np->eth)) == 0
	    ) {
		/* Pfew!  We got a DHCP reply! */
		u8_t *pdata;
		size_t len;
		int type;
		ipaddr_t mask, gateway, relay, server;
		u16_t mtu;
		u32_t lease, renew, rebind, t;

		relay= bp->udpio->uih_src_addr;
		if (gettag(bp->dhcp, DHCP_TAG_SERVERID, &pdata, nil)) {
		    memcpy(&server, pdata, sizeof(server));
		} else {
		    server= relay;
		}

		if (gettag(bp->dhcp, DHCP_TAG_TYPE, &pdata, nil)) {
		    type= pdata[0];
		} else {
		    type= DHCP_ACK;	/* BOOTP? */
		}

		if (debug >= 1) {
		    printf("%s: Got a DHCP %s from %s",
			np->fdp->device, dhcptypename(type), inet_ntoa(server));
		    printf(relay != server ? " through %s\n" : "\n",
			inet_ntoa(relay));
		    if (debug >= 2) printdhcp(bp->dhcp);
		}

		if (gettag(bp->dhcp, DHCP_TAG_NETMASK, &pdata, nil)) {
		    memcpy(&mask, pdata, sizeof(mask));
		} else {
		    mask= defaultmask(bp->dhcp->ciaddr);
		}

		if (gettag(bp->dhcp, DHCP_TAG_IPMTU, &pdata, nil)) {
		    memcpy(&mtu, pdata, sizeof(mtu));
		    mtu= ntohs(mtu);
		} else {
		    mtu= 0;
		}

		if (gettag(bp->dhcp, DHCP_TAG_GATEWAY, &pdata, nil)) {
		    memcpy(&gateway, pdata, sizeof(gateway));
		} else {
		    gateway= 0;
		}

		lease= NEVER;
		if (gettag(bp->dhcp, DHCP_TAG_LEASE, &pdata, nil)) {
		    memcpy(&lease, pdata, sizeof(lease));
		    lease= ntohl(lease);
		}

		rebind= lease - lease / 8;
		if (gettag(bp->dhcp, DHCP_TAG_REBINDING, &pdata, nil)) {
		    memcpy(&t, pdata, sizeof(t));
		    t= ntohl(t);
		    if (t < rebind) rebind= t;
		}

		renew= lease / 2;
		if (gettag(bp->dhcp, DHCP_TAG_RENEWAL, &pdata, nil)) {
		    memcpy(&t, pdata, sizeof(t));
		    t= ntohl(t);
		    if (t < renew) renew= t;
		}

		if (type == DHCP_OFFER && np->rebind <= np->renew) {
		    /* It's an offer for an address and we haven't taken one
		     * yet.  It's all the same to us, so take this one.
		     */
		    np->ip= bp->dhcp->yiaddr;
		    np->mask= mask;
		    np->server= server;
		    np->gateway= gateway;
		    np->delta= DELTA_FIRST;
		    np->renew= now;
		    np->rebind= np->lease= now + DELTA_FAST;

		    /* Send out an ARP request to see if the offered address
		     * is in use already.
		     */
		    make_arp(bp, np);
		    if (send(np, bp->eth, sizeof(arp46_t))) {
			if (debug >= 2) {
			    printf("Sent ARP for %s\n", inet_ntoa(np->ip));
			}
		    }
		    np->flags &= ~NF_CONFLICT;
		}

		if (type == DHCP_ACK && !(np->flags & NF_CONFLICT)) {
		    /* An acknowledgment.  The address is all mine. */
		    cachedhcp(np->n, bp->dhcp);
		    np->ip= bp->dhcp->yiaddr;
		    np->mask= mask;
		    np->server= server;
		    set_ipconf(ipdev(np->n), np->ip, np->mask, mtu);
		    if (debug >= 1) {
			printf("%s: Address set to %s\n",
			    ipdev(np->n), cidr_ntoa(np->ip, np->mask));
		    }
		    if (lease >= NEVER - now) {
			/* The lease is infinite! */
			np->renew= np->rebind= np->lease= NEVER;
		    } else {
			np->lease= now + lease;
			np->renew= now + renew;
			np->rebind= now + rebind;
		    }
		    if (test >= 3) {
			np->renew= now + 60;
			np->rebind= test >= 4 ? np->renew : np->renew + 60;
			np->lease= test >= 5 ? np->rebind : np->rebind + 60;
		    }
		    if (!(np->flags & NF_IRDP)) {
			np->sol_ct= (np->flags & NF_BOUND) ? 1 : N_SOLICITS;
			np->solicit= 0;
		    }
		    np->flags &= ~NF_NEGOTIATING;
		    np->flags |= NF_BOUND;
		    closedev(np, FT_ETHERNET);
		    closedev(np, FT_BOOTPC);
		}

		if (type == DHCP_ACK && (np->flags & NF_CONFLICT)) {
		    /* Alas there is a conflict.  Decline to use the address. */
		    u8_t ethclid[1+DHCP_HLEN_ETH];
		    static char USED[]= "Address in use by 00:00:00:00:00:00";

		    type= DHCP_DECLINE;
		    dhcp_init(bp->dhcp);
		    bp->dhcp->op= DHCP_BOOTREQUEST;
		    bp->dhcp->htype= DHCP_HTYPE_ETH;
		    bp->dhcp->hlen= DHCP_HLEN_ETH;
		    bp->dhcp->xid= XID(np);
		    bp->dhcp->secs= 0;
		    memcpy(bp->dhcp->chaddr, &np->eth, sizeof(np->eth));
		    settag(bp->dhcp, DHCP_TAG_REQIP, &np->ip, sizeof(np->ip));
		    settag(bp->dhcp, DHCP_TAG_TYPE, &type, 1);
		    ether2clid(ethclid, &np->eth);
		    settag(bp->dhcp, DHCP_TAG_CLIENTID,ethclid,sizeof(ethclid));
		    strcpy(USED+18, ether_ntoa(&np->conflict));
		    settag(bp->dhcp, DHCP_TAG_MESSAGE, USED, strlen(USED));

		    bp->udpio->uih_src_port= port_client;
		    bp->udpio->uih_dst_port= port_server;
		    bp->udpio->uih_ip_opt_len= 0;
		    bp->udpio->uih_data_len= sizeof(dhcp_t);
		    udp2ether(bp, np);

		    if (send(np, bp->eth, sizeof(eth_hdr_t) + sizeof(ip_hdr_t)
					+ sizeof(udp_hdr_t) + sizeof(dhcp_t))) {
			if (debug >= 1) {
			    printf("%s: Broadcast DHCP %s\n",
				np->fdp->device, dhcptypename(type));
			    if (debug >= 2) printdhcp(bp->dhcp);
			}
		    }
		    
		    np->renew= np->rebind= np->lease= now + DELTA_FAST;
		    np->delta= DELTA_FIRST;
		}

		if (type == DHCP_NAK) {
		    /* Oops, a DHCP server doesn't like me, start over! */
		    np->renew= np->rebind= np->lease= now + DELTA_FAST;
		    np->delta= DELTA_FIRST;

		    fprintf(stderr, "%s: Got a NAK from %s",
			program, inet_ntoa(server));
		    if (relay != server) {
			fprintf(stderr, " through %s", inet_ntoa(relay));
		    }
		    if (gettag(bp->dhcp, DHCP_TAG_MESSAGE, &pdata, &len)) {
			fprintf(stderr, " saying: \"%.*s\"", (int)len, pdata);
		    }
		    fputc('\n', stderr);
		}
	    } else
	    if (!(np->flags & NF_BOUND)
		&& np->rebind > now
		&& r >= sizeof(arp46_t)
		&& is_arp_me(bp, np)
	    ) {
		/* Oh no, someone else is using the address offered to me! */
		np->flags |= NF_CONFLICT;

		fprintf(stderr, "%s: %s: %s offered by ",
			program,
			np->fdp->device,
			inet_ntoa(np->ip));
		fprintf(stderr, "%s is already in use by %s\n",
			inet_ntoa(np->server),
			ether_ntoa(&np->conflict));
	    }
	    put_buf(&bp);
	    if (np->renew < event) event= np->renew;
	}

	/* Perform router solicitations. */
	for (i= 0; i < n_nets; i++) {
	    np= network[i];
	    if (!(np->flags & NF_BOUND)) continue;

	    if (np->solicit <= now) {
		if (!opendev(np, FT_ICMP, 1)) continue;
		np->solicit= NEVER;

		get_buf(&bp);
		if (np->gateway != 0) {
		    /* No IRDP response seen yet, advertise the router given
		     * by DHCP to my own interface.
		     */
		    icmp_advert(bp, np);
		    if (send(np, bp->ip, sizeof(ip_hdr_t) + 16)) {
			if (debug >= 2) {
			    printf("%s: Sent advert for %s to self\n",
				np->fdp->device, inet_ntoa(np->gateway));
			}
		    }
		    np->solicit= now + DELTA_ADV/2;
		}

		if (np->sol_ct >= 0 && --np->sol_ct >= 0) {
		    /* Send a router solicitation. */
		    icmp_solicit(bp);
		    if (send(np, bp->ip, sizeof(*bp->ip) + 8)) {
			if (debug >= 2) {
			    printf("%s: Broadcast router solicitation\n",
				np->fdp->device);
			}
		    }
		    np->solicit= now + DELTA_SOL;
		} else {
		    /* No response, or not soliciting right now. */
		    closedev(np, FT_ICMP);
		}

		put_buf(&bp);
	    }
	    if (np->solicit < event) event= np->solicit;
	}

	/* Read router adverts. */
	for (i= 0; i < n_nets; i++) {
	    np= network[i];
	    if (!(np->flags & NF_BOUND)) continue;
	    if (np->sol_ct < 0) continue;

	    if (!opendev(np, FT_ICMP, 0)) continue;
	    get_buf(&np->fdp->bp);
	    r= asyn_read(&asyn, np->fdp->fd, np->fdp->bp->ip, BUF_IP_SIZE);
	    if (r != -1) break;
	    if (errno != ASYN_INPROGRESS && errno != EPACKSIZE) {
		report(np->fdp->device);
		sleep(10);
	    }
	}

	/* Is there an advert? */
	if (i < n_nets && r >= sizeof(ip_hdr_t) + 8) {
	    ipaddr_t router;

	    give_buf(&bp, &np->fdp->bp);
	    if ((router= icmp_is_advert(bp)) != 0) {
		if (debug >= 2) {
		    printf("%s: Router advert received from %s\n",
			np->fdp->device, inet_ntoa(router));
		}
		np->solicit= NEVER;
		np->sol_ct= -1;
		np->flags |= NF_IRDP;
		closedev(np, FT_ICMP);
	    }
	    put_buf(&bp);
	}

	/* We start serving if all the interfaces so marked are configured. */
	for (i= 0; i < n_nets; i++) {
	    np= network[i];
	    if ((np->flags & NF_RELAYING) && (np->flags & NF_BOUND)) {
		if (((np->ip ^ np->server) & np->mask) == 0) {
		    /* Don't relay to a server that is on this same net. */
		    np->flags &= ~NF_RELAYING;
		}
	    }
	    if (!(np->flags & (NF_SERVING|NF_RELAYING))) continue;
	    if (!(np->flags & NF_BOUND)) { serving= 0; break; }
	    serving= 1;
	}

	/* Read DHCP requests. */
	for (i= 0; i < n_nets; i++) {
	    np= network[i];
	    if (!(np->flags & NF_BOUND)) continue;
	    if (!(np->flags & (NF_SERVING|NF_RELAYING)) || !serving) continue;

	    if (!opendev(np, FT_BOOTPS, 0)) continue;
	    get_buf(&np->fdp->bp);
	    r= asyn_read(&asyn, np->fdp->fd, np->fdp->bp->udpio, BUF_UDP_SIZE);

	    if (r != -1) break;
	    if (errno != ASYN_INPROGRESS && errno != EPACKSIZE) {
		report(np->fdp->device);
		sleep(10);
	    }
	}

	/* Is there a request? */
	if (i < n_nets
	    && r >= sizeof(udp_io_hdr_t) + offsetof(dhcp_t, options)
	) {
	    give_buf(&bp, &np->fdp->bp);

	    if (debug >= 1) {
		printf("%s: Got DHCP packet from %s to ",
		    np->fdp->device, inet_ntoa(bp->udpio->uih_src_addr));
		printf("%s\n", inet_ntoa(bp->udpio->uih_dst_addr));
		if (debug >= 2) printdhcp(bp->dhcp);
	    }

	    /* Can we do something with this DHCP packet? */
	    if ((r= servdhcp(np, bp, r)) > 0) {
		/* Yes, we have something to send somewhere. */
		if (send(np, bp->udpio, r)) {
		    if (debug >= 1) {
			printf("%s: Sent DHCP packet to %s\n",
			    np->fdp->device,
			    inet_ntoa(bp->udpio->uih_dst_addr));
			if (debug >= 2) printdhcp(bp->dhcp);
		    }
		}
	    }
	    put_buf(&bp);
	}

	if (debug >= 1) {
	    static char *lastbrk;
	    extern char _end;

	    if (sbrk(0) != lastbrk) {
		lastbrk= sbrk(0);
		printf("Memory use = %lu\n",
		    (unsigned long) (lastbrk - &_end));
	    }
	    fflush(stdout);
	}

	/* Bail out if not a server, and there is nothing else to do ever. */
	if (!serving && event == NEVER) break;

	/* Wait for something to do. */
	eventtv.tv_sec= event;
	if (asyn_wait(&asyn, 0, event == NEVER ? nil : &eventtv) < 0) {
	    if (errno != EINTR) {
		report("asyn_wait()");
		sleep(10);
	    }
	}
    }
    if (debug >= 1) printf("Nothing more to do! Bailing out...\n");
    return 0;
}
