/*	nonamed - Not a name daemon, but plays one on TV.
 *							Author: Kees J. Bot
 *								29 Nov 1994
 */
static const char version[] = "2.7";

/* Use the file reading gethostent() family of functions. */
#define sethostent	_sethostent
#define gethostent	_gethostent
#define endhostent	_endhostent

#define nil ((void*)0)
#include <sys/types.h>
#include <stdio.h>
#include <syslog.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <signal.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/asynchio.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <net/hton.h>
#include <net/netlib.h>
#include <net/gen/in.h>
#include <net/gen/inet.h>
#include <net/gen/nameser.h>
#include <net/gen/resolv.h>
#include <net/gen/netdb.h>
#include <net/gen/socket.h>
#include <net/gen/tcp.h>
#include <net/gen/tcp_io.h>
#include <net/gen/udp.h>
#include <net/gen/udp_hdr.h>
#include <net/gen/udp_io.h>
#include <net/gen/dhcp.h>

#include <paths.h>

#undef HTONL
#undef HTONS
#define HTONL htonl
#define HTONS htons

#define HTTL	  	3600L	/* Default time to live for /etc/hosts data. */
#define SHORT_TIMEOUT	   2	/* If you expect an answer soon. */
#define MEDIUM_TIMEOUT	   4	/* Soon, but not that soon. */
#define LONG_TIMEOUT	 300	/* For stream connections to a real named. */
#define N_IDS		 256	/* Keep track of this many queries. */
#define N_DATAMAX (4096*sizeof(char *))	/* Default response cache size. */
#define N_NAMEDS	   8	/* Max # name daemons we can keep track of. */
#define NO_FD		(-1)	/* No name daemon channel here. */
#define T_NXD	((u16_t) -1)	/* A "type" signalling a nonexistent domain. */

/* Can't do async I/O under standard Minix, so forget about TCP. */
#define DO_TCP (__minix_vmd || !__minix)

/* Host data, file to store our process id in, our cache, DHCP's cache. */
static char HOSTS[]=	_PATH_HOSTS;
static char PIDFILE[]=	"/usr/run/nonamed.pid";
static char NNCACHE[]=	"/usr/adm/nonamed.cache";
static char DHCPCACHE[]= _PATH_DHCPCACHE;

/* Magic string to head the cache file. */
static char MAGIC[4]=	"NND\2";

#define arraysize(a)	(sizeof(a) / sizeof((a)[0]))
#define arraylimit(a)	((a) + arraysize(a))
#define between(a, c, z) ((unsigned) ((c) - (a)) <= (unsigned) ((z) - (a)))

/* The start of time and the far future. */
#define IMMEDIATE	((time_t) 0)
#define NEVER		((time_t) ((time_t) -1 < 0 ? LONG_MAX : ULONG_MAX))

static unsigned debug;		/* Debug level. */
static time_t now;		/* Current time. */
static u32_t stale;		/* Extension time for stale data. */
static u32_t httl;		/* TTL for /etc/hosts data. */
static int reinit, done;	/* Reinit config / program is done. */
static int single;		/* Run single on a nondefault interface. */
static int localonly;		/* Only accept local queries. */
#define LOCALHOST	0x7F000001

static void report(const char *label)
{
    fprintf(stderr, "nonamed: %s: %s\n", label, strerror(errno));
}

static void fatal(const char *label)
{
    report(label);
    if (debug >= 3) { fflush(nil); abort(); }
    exit(1);
}

static void *allocate(void *mem, size_t size)
{
    if ((mem= realloc(mem, size)) == nil) fatal("malloc()");
    return mem;
}

static void deallocate(void *mem)
{
    free(mem);
}

static char *timegmt(time_t t)
/* Simple "time in seconds to GMT time today" converter. */
{
    unsigned h, m, s;
    static char asctime[sizeof("00:00:00")];

    s= t % 60;
    t /= 60;
    m= t % 60;
    t /= 60;
    h= t % 24;
    sprintf(asctime, "%02u:%02u:%02u", h, m, s);
    return asctime;
}

static char *nowgmt(void)
{
    return timegmt(now);
}

#define PC(n)	((void) sizeof(char [sizeof(*(n)) == 1]), (char *) (n))
#define namecpy(n1, n2)		strcpy(PC(n1), PC(n2))
#define namecat(n1, n2)		strcat(PC(n1), PC(n2))
#define namechr(n, c)		((u8_t *) strchr(PC(n), (c)))
#define namecmp(n1, n2)		strcasecmp(PC(n1), PC(n2))
#define namencmp(n1, n2, len)	strncasecmp(PC(n1), PC(n2), len)

typedef struct dns {		/* A DNS packet. */
	HEADER		hdr;		/* DNS header. */
	u8_t		data[PACKETSZ - sizeof(HEADER)];	/* DNS data. */
} dns_t;

/* Addres of DNS packet to octet address, or vv. */
#define dns2oct(dp)		((u8_t *) (dp))
#define oct2dns(dp)		((dns_t *) (dp))

typedef struct query {		/* One cached answer to a query. */
	struct query	*less;		/* Less recently used. */
	struct query	*more;		/* More recently used. */
	time_t		age;		/* Time it was added. */
	time_t		stale;		/* Time it goes stale by TTL. */
	u16_t		usage;		/* Counts of queries answered. */
	u8_t		flags;		/* QF_REFRESH. */
	size_t		size;		/* Size of DNS packet. */
	dns_t		dns;		/* Answer to query as a DNS packet. */
} query_t;

#define QF_REFRESH	0x01		/* This stale data must be refreshed. */
#define QU_SHIFT	1		/* To shift usage by when evicting. */

/* Size of new query_t or existing query_t. */
#define query_allocsize(dnssize)	(offsetof(query_t, dns) + (dnssize))
#define query_size(qp)			query_allocsize((qp)->size)

static query_t *mru, *lru;	/* Most and least recently used answers. */
static int q_refresh;		/* Set when an entry needs refreshing. */

static void pack16(u8_t *buf, u16_t s)
/* Pack a 16 bit value into a byte array. */
{
    buf[0]= ((u8_t *) &s)[0];
    buf[1]= ((u8_t *) &s)[1];
}

static void pack32(u8_t *buf, u32_t l)
/* Pack a 32 bit value into a byte array. */
{
    buf[0]= ((u8_t *) &l)[0];
    buf[1]= ((u8_t *) &l)[1];
    buf[2]= ((u8_t *) &l)[2];
    buf[3]= ((u8_t *) &l)[3];
}

static u16_t upack16(u8_t *buf)
/* Unpack a 16 bit value from a byte array. */
{
    u16_t s;

    ((u8_t *) &s)[0]= buf[0];
    ((u8_t *) &s)[1]= buf[1];
    return s;
}

static u32_t upack32(u8_t *buf)
/* Unpack a 32 bit value from a byte array. */
{
    u32_t l;

    ((u8_t *) &l)[0]= buf[0];
    ((u8_t *) &l)[1]= buf[1];
    ((u8_t *) &l)[2]= buf[2];
    ((u8_t *) &l)[3]= buf[3];
    return l;
}

/* Encoding of RRs: i(paddr), d(omain), l(ong), c(har), s(tring), (s)h(ort). */
static char *encoding[] = {
	"c*",		/* anything unknown is c* */
	"i",		/* A */
	"d",		/* NS */
	"d",		/* MD */
	"d",		/* MF */
	"d",		/* CNAME */
	"ddlllll",	/* SOA */
	"d",		/* MB */
	"d",		/* MG */
	"d",		/* MR */
	"c*",		/* NULL */
	"icc*",		/* WKS */
	"d",		/* PTR */
	"ss",		/* HINFO */
	"dd",		/* MINFO */
	"hd",		/* MX */
	"s*",		/* TXT */
};

static char *itoa(char *fmt, u32_t i)
{
    static char output[32 + 3 * sizeof(i)];

    sprintf(output, fmt, (unsigned long) i);
    return output;
}

static char *classname(unsigned class)
/* Class name of a resource record, for debug purposes. */
{
    static char *classes[] = { "IN", "CS", "CHAOS", "HS" };

    if ((class - C_IN) < arraysize(classes)) return classes[class - C_IN];
    return itoa("C_%u", class);
}

static char *typename(unsigned type)
/* Type name of a resource record, for debug purposes. */
{
    static char type_A[][6] = {
	"A", "NS", "MD", "MF", "CNAME", "SOA", "MB", "MG", "MR", "NULL",
	"WKS", "PTR", "HINFO", "MINFO", "MX", "TXT",
    };
    static char type_AXFR[][6] = {
	"AXFR", "MAILB", "MAILA", "ANY",
    };
    if ((type - T_A) < arraysize(type_A)) return type_A[type - T_A];
    if ((type - T_AXFR) < arraysize(type_AXFR)) return type_AXFR[type - T_AXFR];
    return itoa("T_%u", type);
}

static int print_qrr(dns_t *dp, size_t size, u8_t *cp0, int q)
/* Print a query (q) or resource record (!q) from 'cp0' in a DNS packet for
 * debug purposes.  Return number of bytes skipped or -1 on error.
 */
{
    u8_t name[MAXDNAME+1];
    u8_t *cp;
    char *ep;
    u8_t *dlim, *rlim;
    u16_t type, class, rdlength;
    u32_t ttl;
    int r;

    cp= cp0;
    dlim= dns2oct(dp) + size;
    r= dn_expand(dns2oct(dp), dlim, cp, name, MAXDNAME);
    if (r == -1) return -1;
    cp += r;
    if (cp + 2 * sizeof(u16_t) > dlim) return -1;
    type= ntohs(upack16(cp));
    cp += sizeof(u16_t);
    class= ntohs(upack16(cp));
    cp += sizeof(u16_t);
    printf("%-25s", (char *) name);
    if (q) {
	/* We're just printing a query segment, stop right here. */
	printf(" %8s", classname(class));
	printf(" %-5s", typename(type));
	return cp - cp0;
    }
    if (cp + sizeof(u32_t) + sizeof(u16_t) > dlim) return -1;
    ttl= ntohl(upack32(cp));
    cp += sizeof(u32_t);
    rdlength= ntohs(upack16(cp));
    cp += sizeof(u16_t);
    if (cp + rdlength > dlim) return -1;
    rlim = cp + rdlength;
    printf(" %5lu", (unsigned long) ttl);
    printf(" %s", classname(class));
    printf(" %-5s", typename(type));
    ep= type < arraysize(encoding) ? encoding[type] : encoding[0];
    while (*ep != 0) {
	switch (*ep++) {
	case 'i':
	    if (cp + sizeof(u32_t) > rlim) return -1;
	    printf(" %s", inet_ntoa(upack32(cp)));
	    cp += sizeof(u32_t);
	    break;
	case 'l':
	    if (cp + sizeof(u32_t) > rlim) return -1;
	    printf(" %ld", (long)(i32_t) ntohl(upack32(cp)));
	    cp += sizeof(u32_t);
	    break;
	case 'd':
	    r= dn_expand(dns2oct(dp), dlim, cp, name, MAXDNAME);
	    if (r == -1) return -1;
	    printf(" %s", (char *) name);
	    cp += r;
	    break;
	case 'c':
	    if (cp >= rlim) return -1;
	    printf(" %02X", *cp++);
	    break;
	case 's':
	    r= *cp + 1;
	    if (cp + r > rlim) return -1;
	    printf(" \"%.*s\"", *cp, (char *) (cp + 1));
	    cp += r;
	    break;
	case 'h':
	    if (cp + sizeof(u16_t) > rlim) return -1;
	    printf(" %u", ntohs(upack16(cp)));
	    cp += sizeof(u16_t);
	    break;
	}
	if (*ep == '*') ep= cp < rlim ? ep-1 : ep+1;
    }
    return cp - cp0;
}

static void dns_tell(int indent, dns_t *dp, size_t size)
/* Explain a DNS packet, for debug purposes. */
{
    u8_t *cp;
    int r, i;
    unsigned count[4];
    static char label[4][4]= { "QD:", "AN:", "NS:", "AR:" };
    static char rcodes[][9] = {
	"NOERROR", "FORMERR", "SERVFAIL", "NXDOMAIN", "NOTIMP", "REFUSED"
    };

    if (size < sizeof(HEADER)) return;

    printf("%*s", indent, "");
    printf("DNS %s:", (dp->hdr.qr) ? "reply" : "query");
    r = dp->hdr.rcode;
    printf(" %s", r < arraysize(rcodes) ? rcodes[r] : itoa("ERR_%lu", r));
    if (dp->hdr.aa) printf(" AA");
    if (dp->hdr.tc) printf(" TC");
    if (dp->hdr.rd) printf(" RD");
    if (dp->hdr.ra) printf(" RA");
    if (dp->hdr.ad) printf(" AD");
    if (dp->hdr.cd) printf(" CD");
    fputc('\n', stdout);

    count[0]= ntohs(dp->hdr.dh_qdcount);
    count[1]= ntohs(dp->hdr.dh_ancount);
    count[2]= ntohs(dp->hdr.dh_nscount);
    count[3]= ntohs(dp->hdr.dh_arcount);
    cp = dp->data;
    for (i= 0; i < 4; i++) {
	while (count[i] > 0) {
	    printf("%*s", indent, "");
	    printf(" %s ", label[i]);
	    r= print_qrr(dp, size, cp, (i == 0));
	    fputc('\n', stdout);
	    if (r == -1) return;
	    cp += r;
	    count[i]--;
	}
    }
}

static u32_t dns_ttl(dns_t *dp, size_t size, u32_t delta)
/* Compute the minimum TTL of all RRs in a DNS packet and subtract delta from
 * all TTLs.  (We are actually only interested in the minimum (delta = 0) or
 * the subtraction (delta > 0).  It was easier to roll this into one routine.)
 */
{
    u8_t *cp, *rdp, *dlim;
    int r, i, hasttl, hassoa;
    unsigned type, count[4];
    u32_t ttl, minimum, minttl;
    unsigned rcode;
    u8_t name[MAXDNAME+1];

    hasttl= hassoa= 0;
    minttl= 365*24*3600L;
    dlim= dns2oct(dp) + size;
    if (size < sizeof(HEADER)) return 0;

    rcode= dp->hdr.rcode;
    count[0]= ntohs(dp->hdr.dh_qdcount);
    count[1]= ntohs(dp->hdr.dh_ancount);
    count[2]= ntohs(dp->hdr.dh_nscount);
    count[3]= ntohs(dp->hdr.dh_arcount);
    cp = dp->data;
    for (i= 0; i < 4 && cp < dlim; i++) {
	while (count[i] > 0) {
	    r= dn_expand(dns2oct(dp), dlim, cp, name, MAXDNAME);
	    if (r == -1) break;
	    cp += r + 2 * sizeof(u16_t);
	    if (i != 0) {
		if (cp + sizeof(u32_t) + sizeof(u16_t) > dlim) break;
		type= upack16(cp - 2 * sizeof(u16_t));
		ttl= ntohl(upack32(cp));
		ttl= ttl < delta ? 0 : ttl - delta;
		if (rcode == NXDOMAIN && i == 2 && type == HTONS(T_SOA)) {
		    rdp= cp + sizeof(u32_t) + sizeof(u16_t);
		    r= dn_expand(dns2oct(dp), dlim, rdp, name, MAXDNAME);
		    if (r == -1) break;
		    rdp += r;
		    r= dn_expand(dns2oct(dp), dlim, rdp, name, MAXDNAME);
		    if (r == -1) break;
		    rdp += r + 4 * sizeof(u32_t);
		    if (rdp + sizeof(u32_t) > dlim) break;
		    minimum= ntohl(upack32(rdp));
		    if (ttl > minimum) ttl= minimum;
		    hassoa= 1;
		}
		if (delta != 0) pack32(cp, htonl(ttl));
		if (ttl < minttl) minttl= ttl;
		hasttl= 1;
		cp += sizeof(u32_t);
		cp += sizeof(u16_t) + ntohs(upack16(cp));
	    }
	    count[i]--;
	}
    }
    return ((rcode == NOERROR && hasttl) || (rcode == NXDOMAIN && hassoa))
		? minttl : 0;
}

/* Total cached query data. */
static size_t n_datamax= N_DATAMAX;
static size_t n_data;

static query_t *extract_query(query_t *qp)
/* Take a query out of the query cache. */
{
    assert(qp != nil);
    *(qp->less != nil ? &qp->less->more : &lru) = qp->more;
    *(qp->more != nil ? &qp->more->less : &mru) = qp->less;
    n_data -= query_size(qp);
    return qp;
}

static query_t *get_query(u8_t *name, unsigned type)
/* Find a query and if so remove it from the cache and return it. */
{
    query_t *qp, *less;
    u8_t qname[MAXDNAME+1];
    int r;

    for (qp= mru; qp != nil; qp= less) {
	less= qp->less;
	if (qp->stale <= now - stale) {
	    /* This answer has expired. */
	    deallocate(extract_query(qp));
	} else {
	    r= dn_expand(dns2oct(&qp->dns), dns2oct(&qp->dns) + qp->size,
		qp->dns.data, qname, MAXDNAME);
	    if (r == -1) continue;
	    if (namecmp(qname, name) == 0 && upack16(qp->dns.data+r) == type) {
		/* Found an answer to the query. */
		return extract_query(qp);
	    }
	}
    }
    return nil;
}

static void insert_query(query_t *qp)
/* (Re)insert a query into the cache. */
{
    *(qp->less != nil ? &qp->less->more : &lru) = qp;
    *(qp->more != nil ? &qp->more->less : &mru) = qp;
    n_data += query_size(qp);

    /* Try to delete the LRU while there is too much memory in use.  If
     * its usage count is too high then it gets a second chance.
     */
    while (n_data > n_datamax && lru != nil) {
	if ((lru->usage >>= QU_SHIFT) == 0 || lru->stale <= now - stale) {
	    deallocate(extract_query(lru));
	} else {
	    lru->less= mru;	/* Make list circular. */
	    mru->more= lru;
	    mru= lru;		/* Move one over, making LRU the MRU. */
	    lru= lru->more;
	    lru->less= nil;	/* Break the circle. */
	    mru->more= nil;
	}
    }

    if (debug >= 2) {
	unsigned n= 0;
	for (qp= mru; qp != nil; qp= qp->less) n++;
	printf("%u cached repl%s, %u bytes, sbrk(0) = %u\n",
	    n, n == 1 ? "y" : "ies",
	    (unsigned) n_data,
	    (unsigned) sbrk(0));
    }
}

static void put_query(query_t *qp)
/* Add a new query to the cache as the MRU. */
{
    qp->less= mru;
    qp->more= nil;
    insert_query(qp);
}

static void cache2file(void)
/* Store the cached data into the cache file. */
{
    FILE *fp;
    query_t *qp;
    u8_t data[4+1+2+2];
    u16_t usage;
    char newcache[sizeof(NNCACHE) + sizeof(".new")];

    if (single) return;

    strcpy(newcache, NNCACHE);
    strcat(newcache, ".new");

    if ((fp= fopen(newcache, "w")) == nil) {
	if ((errno != ENOENT && errno != EROFS) || debug >= 2) report(newcache);
	return;
    }
    if (debug >= 2) printf("Writing %s:\n", newcache);

    /* Magic number: */
    fwrite(MAGIC, 1, sizeof(MAGIC), fp);

    for (qp= lru; qp != nil; qp= qp->more) {
	if (qp->stale <= now - stale) continue;
	if (debug >= 2) {
	    printf("Usage = %u, Age = %ld, Flags = %02X:\n",
		qp->usage, (long) (now - qp->age), qp->flags);
	    dns_tell(2, &qp->dns, qp->size);
	}
	pack32(data+0, htonl(qp->age));
	data[4]= qp->flags;
	pack16(data+5, htons(qp->size));
	pack16(data+7, htons(qp->usage));
	fwrite(data, 1, sizeof(data), fp);
	fwrite(&qp->dns, 1, qp->size, fp);
	if (ferror(fp)) break;
    }

    if (ferror(fp) || fclose(fp) == EOF) {
	report(newcache);
	(void) unlink(newcache);
	return;
    }

    if (debug >= 2) printf("mv %s %s\n", newcache, NNCACHE);
    if (rename(newcache, NNCACHE) < 0) {
	fprintf(stderr, "nonamed: mv %s %s: %s\n",
	    newcache, NNCACHE, strerror(errno));
	(void) unlink(newcache);
    }
}

static void file2cache(void)
/* Read cached data from the cache file. */
{
    query_t *qp;
    FILE *fp;
    u8_t data[4+1+2+2];
    size_t dlen;

    if (single) return;

    if ((fp= fopen(NNCACHE, "r")) == nil) {
	if (errno != ENOENT || debug >= 2) report(NNCACHE);
	return;
    }
    if (debug >= 2) printf("Reading %s:\n", NNCACHE);

    /* Magic number? */
    fread(data, 1, sizeof(MAGIC), fp);
    if (ferror(fp) || memcmp(MAGIC, data, sizeof(MAGIC)) != 0) goto err;

    for (;;) {
	fread(data, 1, sizeof(data), fp);
	if (feof(fp) || ferror(fp)) break;
	dlen= ntohs(upack16(data+5));
	qp= allocate(nil, query_allocsize(dlen));
	qp->age= htonl(upack32(data+0));
	qp->flags= data[4];
	if (qp->flags & QF_REFRESH) q_refresh= 1;
	qp->size= dlen;
	qp->usage= htons(upack16(data+7));
	fread(&qp->dns, 1, qp->size, fp);
	if (feof(fp) || ferror(fp)) {
	    deallocate(qp);
	    goto err;
	}
	qp->stale= qp->age + dns_ttl(&qp->dns, dlen, 0);
	if (debug >= 2) {
	    printf("Usage = %u, Age = %ld, Flags = %02X:\n",
		qp->usage, (long) (now - qp->age), qp->flags);
	    dns_tell(2, &qp->dns, dlen);
	}
	put_query(qp);
    }
    if (ferror(fp)) {
    err:
	/* The cache file did not end at EOF or is otherwise a mess. */
	fprintf(stderr, "nonamed: %s: %s\n", NNCACHE,
		ferror(fp) ? strerror(errno) : "Corrupt");
	while (lru != nil) deallocate(extract_query(lru));
    }
    fclose(fp);
}

typedef int handler_t(void *data, int expired);

/* All actions are in the form of "jobs". */
typedef struct job {
	struct job	*next, **prev;	/* To make a job queue. */
	handler_t	*handler;	/* Function to handle this job. */
	time_t		timeout;	/* Moment it times out. */
	void		*data;		/* Data associated with the job. */
} job_t;

static job_t *queue;		/* Main job queue. */

static void newjob(handler_t *handler, time_t timeout, void *data)
/* Create a new job with the given handler, timeout time and data. */
{
    job_t *job, **prev;

    job= allocate(nil, sizeof(*job));
    job->handler= handler;
    job->timeout= timeout;
    job->data= data;

    for (prev= &queue; *prev != nil; prev= &(*prev)->next) {
	if (job->timeout < (*prev)->timeout) break;
    }
    job->next= *prev;
    job->prev= prev;
    *prev= job;
    if (job->next != nil) job->next->prev= &job->next;
}

static int execjob(job_t *job, int expired)
/* Execute a job by calling the handler.  Remove the job if it returns true,
 * indicating that it is done.  Expired is set if the job timed out.  It is
 * otherwise called to check for I/O.
 */
{
    if ((*job->handler)(job->data, expired)) {
	*job->prev= job->next;
	if (job->next != nil) job->next->prev= job->prev;
	deallocate(job);
	return 1;
    }
    return 0;
}

static void force_expire(handler_t *handler)
/* Force jobs to expire immediately, the named searcher for instance. */
{
    job_t *job, **prev= &queue;

    while ((job= *prev) != nil) {
	if (job->handler == handler && job->timeout != IMMEDIATE) {
	    *prev= job->next;
	    if (job->next != nil) job->next->prev= prev;
	    newjob(job->handler, IMMEDIATE, job->data);
	    deallocate(job);
	} else {
	    prev= &job->next;
	}
    }
}

static int nxdomain(u8_t *name)
/* True iff the two top level components in a name are repeated in the name,
 * or if in-addr.arpa is found within a name.  Such things happen often in a
 * search for an already fully qualified local name.  For instance:
 * flotsam.cs.vu.nl.cs.vu.nl.  (We don't want this at boot time.)
 */
{
    u8_t *end, *top, *p;
    size_t n;

    end= namechr(name, 0);
    top= end;
    while (top > name && *--top != '.') {}
    while (top > name && *--top != '.') {}
    n= end - top;
    p= top;
    for (;;) {
	if (p == name) return 0;
	if (*--p == '.') {
	    if (namencmp(p, top, n) == 0 && p[n] == '.') return 1;
	    if (namencmp(p, ".in-addr.arpa.", 14) == 0) return 1;
	}
    }
}

typedef struct id2id {
	u16_t		id;		/* ID of old query. */
	u16_t		port;		/* Reply port. */
	ipaddr_t	ip;		/* Reply address. */
} id2id_t;

static id2id_t id2id[N_IDS];
static u16_t id_counter;

static u16_t new_id(u16_t in_id, u16_t in_port, ipaddr_t in_ip)
/* An incoming UDP query must be relabeled with a new ID before it can be
 * send on to a real name daemon.
 */
{
    id2id_t *idp;
    u16_t id;

    id= id_counter++;
    idp= &id2id[id % N_IDS];
    idp->id= in_id;
    idp->port= in_port;
    idp->ip= in_ip;
    return htons(id);
}

static int old_id(u16_t id, u16_t *out_id, u16_t *out_port, ipaddr_t *out_ip)
/* Translate a reply id back to the id, port, and address used in the query.
 * Return true if the translation is possible.
 */
{
    id= ntohs(id);
    if ((u16_t) (id_counter - id) > N_IDS) {
	/* Too old. */
	return 0;
    } else {
	/* We know this one. */
	id2id_t *idp= &id2id[id % N_IDS];

	if (idp->port == 0) return 0;	/* Named is trying to fool us? */
	*out_id= idp->id;
	*out_port= idp->port;
	*out_ip= idp->ip;
	idp->port= 0;
	return 1;
    }
}

/* IDs used to mark my own queries to name servers, must be new_id translated
 * to make them unique "on the wire".
 */
#define ID_IPSELF	HTONL(0)	/* "I did it myself" address. */
#define ID_PROBE	HTONS(0)	/* Name server probe. */
#define ID_REFRESH	HTONS(1)	/* Query to refresh a cache entry. */

static char *tcp_device, *udp_device;	/* TCP and UDP device names. */
static int udp_fd;			/* To send or receive UDP packets. */
static asynchio_t asyn;			/* For I/O in progress. */
static ipaddr_t my_ip;			/* My IP address. */
static u16_t my_port, named_port;	/* Port numbers, normally "domain". */

static ipaddr_t named[N_NAMEDS];	/* Addresses of all name servers. */
static unsigned n_nameds;		/* Number of configured name daemons. */
static unsigned i_named;		/* Index to current name server. */
static int expect;			/* Set when we expect an answer. */
static int search_ct= -1;		/* Named search count and state. */
static int dirty;			/* True when new entry put in cache. */

#define current_named()		(+named[i_named])
#define searching()		(search_ct > 0)
#define start_searching()	((void) (search_ct= -1))
#define stop_searching()	((void) (search_ct= 0))
#define expecting()		(+expect)
#define start_expecting()	((void) (expect= 1))
#define stop_expecting()	((void) (expect= 0))

static time_t filetime(const char *file)
/* Get the modified time of a file. */
{
    struct stat st;

    return stat(file, &st) == 0 ? st.st_mtime : 0;
}

static void init_config(ipaddr_t ifip)
/* Read name daemon list and other special stuff from the hosts file. */
{
    struct hostent *he;
    u32_t nip, hip;
    static time_t hosts_time, dhcp_time;
    time_t ht, dt;

    /* See if anything really changed. */
    if (((ifip ^ HTONL(LOCALHOST)) & HTONL(0xFF000000)) == 0) ifip= my_ip;
    ht= filetime(HOSTS);
    dt= filetime(DHCPCACHE);
    if (ifip == my_ip && ht == hosts_time && dt == dhcp_time) return;
    my_ip= ifip;
    hosts_time= ht;
    dhcp_time= dt;

    if (debug >= 2) {
	printf("%s: I am nonamed %s at %s:%u\n",
	    nowgmt(), version, inet_ntoa(my_ip), ntohs(my_port));
    }

    httl= HTONL(HTTL);
    stale= 0;
    n_nameds= 0;

    if (!single) {
	sethostent(0);
	while ((he= gethostent()) != nil) {
	    memcpy(&nip, he->h_addr, sizeof(u32_t));
	    hip= ntohl(nip);
	    if (namecmp(he->h_name, "%ttl") == 0) httl= nip;
	    if (namecmp(he->h_name, "%stale") == 0) stale= hip;
	    if (namecmp(he->h_name, "%memory") == 0) n_datamax= hip;
	    if (namecmp(he->h_name, "%nameserver") == 0) {
		if (nip != my_ip || named_port != my_port) {
		    if (n_nameds < N_NAMEDS) named[n_nameds++]= nip;
		}
	    }
	}
	endhostent();
    }

    if (n_nameds == 0) {
	/* No name daemons found in the host file.  What about DHCP? */
	int fd;
	dhcp_t d;
	ssize_t r;
	u8_t *data;
	size_t len;

	if ((fd= open(DHCPCACHE, O_RDONLY)) < 0) {
	    if (errno != ENOENT) fatal(DHCPCACHE);
	} else {
	    while ((r= read(fd, &d, sizeof(d))) == sizeof(d)) {
		if (d.yiaddr == my_ip) break;
	    }
	    if (r < 0) fatal(DHCPCACHE);
	    close(fd);

	    if (r == sizeof(d) && dhcp_gettag(&d, DHCP_TAG_DNS, &data, &len)) {
		while (len >= sizeof(nip)) {
		    memcpy(&nip, data, sizeof(nip));
		    data += sizeof(nip);
		    len -= sizeof(nip);
		    if (nip != my_ip || named_port != my_port) {
			if (n_nameds < N_NAMEDS) named[n_nameds++]= nip;
		    }
		}
	    }
	}
    }
    i_named= 0;
}

static handler_t job_save_cache, job_read_udp, job_find_named, job_expect_named;
#if DO_TCP
static handler_t job_setup_listen, job_listen, job_setup_connect, job_connect;
static handler_t job_read_query, job_write_query;
static handler_t job_read_reply, job_write_reply;
#endif

static int query_hosts(u8_t *qname, unsigned type, dns_t *dp, size_t *pdlen)
/* Read the /etc/hosts file to try and answer an A or PTR query.  Return
 * true iff an answer can be found, with the answer copied to *dp.
 */
{
    struct hostent *he;
    int i, r;
    dns_t dns;
    u8_t *domain;
    u8_t *cp;
    u8_t name[MAXDNAME+1];
    u8_t *dnvec[40];
    unsigned ancount;
    struct hostent localhost;
    static char *noaliases[]= { nil };
    static ipaddr_t localaddr;
    static char *localaddrlist[]= { (char *) &localaddr, nil };

    localaddr = HTONL(LOCALHOST);

    if (single) return 0;

    /* Assume we can answer. */
    dns.hdr.qr = 1;
    dns.hdr.opcode = 0;
    dns.hdr.aa = 1;
    dns.hdr.tc = 0;
    dns.hdr.rd = 0;
    dns.hdr.ra = 1;
    dns.hdr.unused = 0;
    dns.hdr.ad = 0;
    dns.hdr.cd = 0;
    dns.hdr.rcode = 0;
    dns.hdr.dh_qdcount= HTONS(1);
    ancount= 0;
    dns.hdr.dh_nscount= HTONS(0);
    dns.hdr.dh_arcount= HTONS(0);

    dnvec[0]= dns2oct(&dns);
    dnvec[1]= nil;
    cp= dns.data;
    r= dn_comp(qname, cp, arraysize(dns.data), dnvec, arraylimit(dnvec));
    if (r == -1) return 0;
    cp += r;
    pack16(cp, type);
    cp += sizeof(u16_t);
    pack16(cp, HTONS(C_IN));
    cp += sizeof(u16_t);

    /* Localhost is fixed to 127.0.0.1. */
    localhost.h_name=
	namencmp(qname, "localhost.", 10) == 0 ? (char *) qname : "localhost";
    localhost.h_aliases= noaliases;
    localhost.h_addr_list= localaddrlist;
    he= &localhost;

    sethostent(0);
    do {
    	int type_host = NTOHS(type);
	switch (type_host) {
	case T_A:
	    if (namecmp(qname, he->h_name) == 0) {
	      addA:
		r= dn_comp((u8_t *) he->h_name, cp, arraylimit(dns.data) - cp,
		    dnvec, arraylimit(dnvec));
		if (r == -1) return 0;
		cp += r;
		if (cp + 3 * sizeof(u16_t) + 2 * sizeof(u32_t)
		    > arraylimit(dns.data)) { r= -1; break; }
		pack16(cp, HTONS(T_A));
		cp += sizeof(u16_t);
		pack16(cp, HTONS(C_IN));
		cp += sizeof(u16_t);
		pack32(cp, httl);
		cp += sizeof(u32_t);
		pack16(cp, HTONS(sizeof(u32_t)));
		cp += sizeof(u16_t);
		memcpy(cp, he->h_addr, sizeof(u32_t));
		cp += sizeof(u32_t);
		ancount++;
		break;
	    }
	    /*FALL THROUGH*/
	case T_CNAME:
	    domain= namechr(he->h_name, '.');
	    for (i= 0; he->h_aliases[i] != nil; i++) {
		namecpy(name, he->h_aliases[i]);
		if (domain != nil && namechr(name, '.') == nil) {
		    namecat(name, domain);
		}
		if (namecmp(qname, name) == 0) {
		    r= dn_comp(name, cp, arraylimit(dns.data) - cp,
			dnvec, arraylimit(dnvec));
		    if (r == -1) break;
		    cp += r;
		    if (cp + 3 * sizeof(u16_t)
			+ 1 * sizeof(u32_t) > arraylimit(dns.data)) return 0;
		    pack16(cp, HTONS(T_CNAME));
		    cp += sizeof(u16_t);
		    pack16(cp, HTONS(C_IN));
		    cp += sizeof(u16_t);
		    pack32(cp, httl);
		    cp += sizeof(u32_t);
		    /* pack16(cp, htonl(RDLENGTH)) */
		    cp += sizeof(u16_t);
		    r= dn_comp((u8_t *) he->h_name, cp,
			arraylimit(dns.data) - cp,
			dnvec, arraylimit(dnvec));
		    if (r == -1) break;
		    pack16(cp - sizeof(u16_t), htons(r));
		    cp += r;
		    ancount++;
		    if (type == HTONS(T_A)) goto addA;	/* really wants A */
		    break;
		}
	    }
	    break;
	case T_PTR:
	    if (ancount > 0) break;
	    if (he->h_name[0] == '%') break;
	    sprintf((char *) name, "%d.%d.%d.%d.in-addr.arpa",
		    ((u8_t *) he->h_addr)[3],
		    ((u8_t *) he->h_addr)[2],
		    ((u8_t *) he->h_addr)[1],
		    ((u8_t *) he->h_addr)[0]);
	    if (namecmp(qname, name) == 0) {
		r= dn_comp(name, cp, arraylimit(dns.data) - cp,
		    dnvec, arraylimit(dnvec));
		if (r == -1) break;
		cp += r;
		if (cp + 3 * sizeof(u16_t) + 1 * sizeof(u32_t)
		    > arraylimit(dns.data)) { r= -1; break; }
		pack16(cp, HTONS(T_PTR));
		cp += sizeof(u16_t);
		pack16(cp, HTONS(C_IN));
		cp += sizeof(u16_t);
		pack32(cp, httl);
		cp += sizeof(u32_t);
		/* pack16(cp, htonl(RDLENGTH)) */
		cp += sizeof(u16_t);
		r= dn_comp((u8_t *) he->h_name, cp,
		    arraylimit(dns.data) - cp, dnvec, arraylimit(dnvec));
		if (r == -1) return 0;
		pack16(cp - sizeof(u16_t), htons(r));
		cp += r;
		ancount++;
	    }
	    break;
	}
    } while (r != -1 && (he= gethostent()) != nil);
    endhostent();

    if (r == -1 || ancount == 0) return 0;

    dns.hdr.dh_ancount= htons(ancount);
    memcpy(dp, &dns, *pdlen= cp - dns2oct(&dns));
    return 1;
}

static int query_chaos(u8_t *qname, unsigned type, dns_t *dp, size_t *pdlen)
/* Report my version.  Can't let BIND take all the credit. :-) */
{
    int i, n, r;
    dns_t dns;
    u8_t *cp;
    u8_t *dnvec[40];

    if (type != HTONS(T_TXT) || namecmp(qname, "version.bind") != 0) return 0;

    dns.hdr.qr = 1;
    dns.hdr.opcode = 0;
    dns.hdr.aa = 1;
    dns.hdr.tc = 0;
    dns.hdr.rd = 0;
    dns.hdr.ra = 1;
    dns.hdr.unused = 0;
    dns.hdr.ad = 0;
    dns.hdr.cd = 0;
    dns.hdr.rcode = 0;
    dns.hdr.dh_qdcount= HTONS(1);
    dns.hdr.dh_ancount= HTONS(1);
    dns.hdr.dh_nscount= HTONS(0);
    dns.hdr.dh_arcount= htons(n_nameds);

    dnvec[0]= dns2oct(&dns);
    dnvec[1]= nil;
    cp= dns.data;
    r= dn_comp(qname, cp, arraysize(dns.data), dnvec, arraylimit(dnvec));
    if (r == -1) return 0;
    cp += r;
    pack16(cp, type);
    cp += sizeof(u16_t);
    pack16(cp, HTONS(C_CHAOS));
    cp += sizeof(u16_t);

    r= dn_comp(qname, cp, arraylimit(dns.data) - cp, dnvec, arraylimit(dnvec));
    if (r == -1) return 0;
    cp += r;
    pack16(cp, HTONS(T_TXT));
    cp += sizeof(u16_t);
    pack16(cp, HTONS(C_CHAOS));
    cp += sizeof(u16_t);
    pack32(cp, HTONL(0));
    cp += sizeof(u32_t);
    /* pack16(cp, htonl(RDLENGTH)) */
    cp += sizeof(u16_t);
    sprintf((char *) cp + 1, "nonamed %s at %s:%u",
	    version, inet_ntoa(my_ip), ntohs(my_port));
    r= strlen((char *) cp + 1) + 1;
    pack16(cp - sizeof(u16_t), htons(r));
    *cp= r-1;
    cp += r;
    for (n= 0, i= i_named; n < n_nameds; n++, i= (i+1) % n_nameds) {
	r= dn_comp((u8_t *) "%nameserver", cp, arraylimit(dns.data) - cp,
	    dnvec, arraylimit(dnvec));
	if (r == -1) return 0;
	cp += r;
	if (cp + 3 * sizeof(u16_t)
	    + 2 * sizeof(u32_t) > arraylimit(dns.data)) return 0;
	pack16(cp, HTONS(T_A));
	cp += sizeof(u16_t);
	pack16(cp, HTONS(C_IN));
	cp += sizeof(u16_t);
	pack32(cp, HTONL(0));
	cp += sizeof(u32_t);
	pack16(cp, HTONS(sizeof(u32_t)));
	cp += sizeof(u16_t);
	memcpy(cp, &named[i], sizeof(u32_t));
	cp += sizeof(u32_t);
    }

    memcpy(dp, &dns, *pdlen= cp - dns2oct(&dns));
    return 1;
}

static void cache_reply(dns_t *dp, size_t dlen)
/* Store a DNS packet in the cache. */
{
    int r;
    query_t *qp, *less, *more;
    unsigned usage;
    u16_t type;
    u8_t *cp;
    u8_t name[MAXDNAME];
    u32_t minttl;

    if ((dp->hdr.rd && !dp->hdr.tc)) return;
    if (dp->hdr.dh_qdcount != HTONS(1)) return;
    cp= dp->data;
    r= dn_expand(dns2oct(dp), dns2oct(dp) + dlen, cp, name, MAXDNAME);
    if (r == -1) return;
    cp += r;
    type= upack16(cp);
    cp += sizeof(u16_t);
    if (upack16(cp) != HTONS(C_IN)) return;

    /* Delete old cached data, if any.  Note where it is in the LRU. */
    if ((qp= get_query(name, type)) != nil) {
	less= qp->less;
	more= qp->more;
	usage= qp->usage;
	deallocate(qp);
    } else {
	/* Not yet in the cache. */
	less= mru;
	more= nil;
	usage= 1;
    }

    /* Determine minimum TTL.  Discard if zero, never cache zero TTLs. */
    if ((minttl= dns_ttl(dp, dlen, 0)) == 0) return;

    /* Enter new reply in cache. */
    qp= allocate(nil, query_allocsize(dlen));
    qp->less= less;
    qp->more= more;
    qp->age= now;
    qp->flags= 0;
    qp->usage= usage;
    qp->size= dlen;
    memcpy(&qp->dns, dp, dlen);
    qp->stale= qp->age + minttl;
    insert_query(qp);
    if (debug >= 1) printf("Answer cached\n");

    /* Save the cache soon. */
    if (!dirty) {
	dirty= 1;
	newjob(job_save_cache, now + LONG_TIMEOUT, nil);
    }
}

static int job_save_cache(void *data, int expired)
/* Some time after the cache is changed it is written back to disk. */
{
    if (!expired) return 0;
    cache2file();
    dirty= 0;
    return 1;
}

static int compose_reply(dns_t *dp, size_t *pdlen)
/* Try to compose a reply to a request in *dp using the hosts file or
 * cached data.  Return answer in *dp with its size in *pdlen.  Return true
 * iff an answer is given.
 */
{
    size_t dlen= *pdlen;
    int r, rd;
    query_t *qp;
    unsigned id, type, class;
    u8_t *cp;
    u8_t name[MAXDNAME];

    cp= dp->data;
    r= dn_expand(dns2oct(dp), dns2oct(dp) + dlen, cp, name, MAXDNAME);
    if (r != -1) {
	cp += r;
	if (cp + 2 * sizeof(u16_t) > dns2oct(dp) + dlen) {
	    r= -1;
	} else {
	    type= upack16(cp);
	    cp += sizeof(u16_t);
	    class= upack16(cp);
	    cp += sizeof(u16_t);
	}
    }

    /* Remember ID and RD. */
    id= dp->hdr.dh_id;
    rd= dp->hdr.rd;

    if (r == -1) {
	/* Malformed query, reply "FORMERR". */
	dp->hdr.tc = 0;
	dp->hdr.qr = 1;
	dp->hdr.aa = 1;
	dp->hdr.unused = 0;
	dp->hdr.ra = 1;
	dp->hdr.rcode = FORMERR;
    } else
    if (class == HTONS(C_IN) && query_hosts(name, type, dp, pdlen)) {
	/* Answer to this query is in the hosts file. */
	dlen= *pdlen;
    } else
    if (class == HTONS(C_IN) && (qp= get_query(name, type)) != nil) {
	/* Answer to this query is present in the cache. */
	memcpy(dp, &qp->dns, dlen= qp->size);
	dp->hdr.aa = 1;
	(void) dns_ttl(dp, dlen, now - qp->age);
	if (rd) {
	    if (qp->stale <= now) {
		qp->flags |= QF_REFRESH;
		q_refresh= 1;
	    }
	    qp->usage++;
	}
	put_query(qp);
    } else
    if (class == HTONS(C_CHAOS) && query_chaos(name, type, dp, pdlen)) {
	/* Return our version numbers. */
	dlen= *pdlen;
    } else
    if (n_nameds == 0 || nxdomain(name)) {
	/* No real name daemon present, or this name has a repeated top level
	 * domain sequence.  Reply "no such domain".
	 */
	dp->hdr.tc = 0;
	dp->hdr.qr = 1;
	dp->hdr.aa = 1;
	dp->hdr.unused = 0;
	dp->hdr.ra = 1;
	dp->hdr.rcode = NXDOMAIN;
    } else
    if (!rd) {
	/* "Recursion Desired" is off, so don't bother to relay. */
	dp->hdr.tc = 0;
	dp->hdr.qr = 1;
	dp->hdr.unused = 0;
	dp->hdr.ra = 1;
	dp->hdr.rcode = NOERROR;
    } else {
	/* Caller needs to consult with a real name daemon. */
	return 0;
    }

    /* Copy ID and RD back to answer. */
    dp->hdr.dh_id= id;
    dp->hdr.rd = rd;
    *pdlen= dlen;
    return 1;
}

typedef struct udp_dns {	/* One DNS packet over UDP. */
	udp_io_hdr_t	hdr;		/* UDP header (source/destination). */
	dns_t		dns;		/* DNS packet. */
} udp_dns_t;

static void refresh_cache(void)
/* Find a stale entry in the cache that was used to answer a query, and send
 * a request to a name server that should refresh this entry.
 */
{
    query_t *qp;
    unsigned type;
    int r;
    u8_t *cp;
    size_t dlen, ulen;
    u8_t qname[MAXDNAME+1];
    u8_t *dnvec[40];
    udp_dns_t udp;

    if (!q_refresh) return;
    for (qp= lru; qp != nil; qp= qp->more) {
	if ((qp->flags & QF_REFRESH) && qp->stale > now - stale) break;
    }
    if (qp == nil) {
	q_refresh= 0;
	return;
    }

    /* Found one to refresh. */
    qp->flags &= ~QF_REFRESH;
    r= dn_expand(dns2oct(&qp->dns), dns2oct(&qp->dns) + qp->size,
	qp->dns.data, qname, MAXDNAME);
    if (r == -1) return;
    type= upack16(qp->dns.data+r);

    dnvec[0]= dns2oct(&udp.dns);
    dnvec[1]= nil;
    cp= udp.dns.data;
    r= dn_comp(qname, cp, arraysize(udp.dns.data), dnvec, arraylimit(dnvec));
    if (r == -1) return;
    cp += r;
    pack16(cp, type);
    cp += sizeof(u16_t);
    pack16(cp, HTONS(C_IN));
    cp += sizeof(u16_t);
    dlen= cp - dns2oct(&udp.dns);

    udp.dns.hdr.dh_id= new_id(ID_REFRESH, my_port, ID_IPSELF);
    udp.dns.hdr.qr = 0;
    udp.dns.hdr.opcode = 0;
    udp.dns.hdr.aa = 0;
    udp.dns.hdr.tc = 0;
    udp.dns.hdr.rd = 1;

    udp.dns.hdr.ra = 0;
    udp.dns.hdr.unused = 0;
    udp.dns.hdr.ad = 0;
    udp.dns.hdr.cd = 0;
    udp.dns.hdr.rcode = 0;
    udp.dns.hdr.dh_qdcount= HTONS(1);
    udp.dns.hdr.dh_ancount= HTONS(0);
    udp.dns.hdr.dh_nscount= HTONS(0);
    udp.dns.hdr.dh_arcount= HTONS(0);

    udp.hdr.uih_dst_addr= current_named();
    udp.hdr.uih_dst_port= named_port;
    udp.hdr.uih_ip_opt_len= 0;
    udp.hdr.uih_data_len= dlen;

    if (debug >= 1) {
	printf("Refresh to %s:%u:\n",
	    inet_ntoa(current_named()), ntohs(named_port));
	dns_tell(0, &udp.dns, dlen);
    }
    ulen= offsetof(udp_dns_t, dns) + dlen;
    if (write(udp_fd, &udp, ulen) < 0) fatal(udp_device);
}

static int job_read_udp(void *data, int expired)
/* Read UDP queries and replies. */
{
    ssize_t ulen;
    size_t dlen;
    static udp_dns_t udp;
    u16_t id, port;
    ipaddr_t ip;
    time_t dtime;

    assert(!expired);

    /* Try to read a packet. */
    ulen= asyn_read(&asyn, udp_fd, &udp, sizeof(udp));
    dlen= ulen - offsetof(udp_dns_t, dns);

    if (ulen == -1) {
	if (errno == EINPROGRESS && !expired) return 0;
	if (errno == EIO) fatal(udp_device);

	if (debug >= 2) {
	    printf("%s: UDP read: %s\n", nowgmt(), strerror(errno));
	}
    } else {
	if (debug >= 2) {
	    printf("%s: UDP read, %d bytes\n", nowgmt(), (int) ulen);
	}
    }

    /* Restart this job no matter what. */
    newjob(job_read_udp, NEVER, nil);

    if (ulen < (ssize_t) (sizeof(udp_io_hdr_t) + sizeof(HEADER))) return 1;

    if (debug >= 1) {
	printf("%s:%u UDP ", inet_ntoa(udp.hdr.uih_src_addr),
				ntohs(udp.hdr.uih_src_port));
	dns_tell(0, &udp.dns, dlen);
    }

    /* Check, and if necessary reinitialize my configuration. */
    init_config(udp.hdr.uih_dst_addr);

    if (udp.dns.hdr.qr) {
	/* This is a remote named reply, not a query. */

	/* Response to a query previously relayed? */
	if (!old_id(udp.dns.hdr.dh_id, &id, &port, &ip)) return 1;

	if (ip == ID_IPSELF && id == ID_PROBE) {
	    if (searching()) {
		/* We have found a name server! */
		int i;

		/* In my list? */
		for (i= 0; i < n_nameds; i++) {
		    if (named[i] == udp.hdr.uih_src_addr) {
			i_named= i;
			if (debug >= 1) {
			    printf("Current named = %s\n",
				inet_ntoa(current_named()));
			}
			stop_searching();
			force_expire(job_find_named);
		    }
		}
	    }
	}

	/* We got an answer, so stop worrying. */
	if (expecting()) {
	    stop_expecting();
	    force_expire(job_expect_named);
	}

	/* Put the information in the cache. */
	cache_reply(&udp.dns, dlen);

	/* Refresh a cached entry that was used when stale. */
	refresh_cache();

	/* Discard reply to myself. */
	if (ip == ID_IPSELF) return 1;

	/* Send the reply to the process that asked for it. */
	udp.dns.hdr.dh_id= id;
	udp.hdr.uih_dst_addr= ip;
	udp.hdr.uih_dst_port= port;
	if (debug >= 1) printf("To client %s:%u\n", inet_ntoa(ip), ntohs(port));
    } else {
	/* A query. */
	if (udp.dns.hdr.dh_qdcount != HTONS(1)) return 1;

	if(localonly) {
		/* Check if it's a local query. */
		if(ntohl(udp.hdr.uih_src_addr) != LOCALHOST) {
		   	syslog(LOG_WARNING, "nonamed: dropped query from %s",
		   		inet_ntoa(udp.hdr.uih_src_addr));
		   	return 1;
		}
	}

	/* Try to compose a reply from local data. */
	if (compose_reply(&udp.dns, &dlen)) {
	    udp.hdr.uih_dst_addr= udp.hdr.uih_src_addr;
	    udp.hdr.uih_dst_port= udp.hdr.uih_src_port;
	    udp.hdr.uih_ip_opt_len= 0;
	    udp.hdr.uih_data_len= dlen;
	    ulen= offsetof(udp_dns_t, dns) + dlen;

	    /* Send an UDP DNS reply. */
	    if (debug >= 1) {
		printf("%s:%u UDP ", inet_ntoa(udp.hdr.uih_dst_addr),
					    ntohs(udp.hdr.uih_dst_port));
		dns_tell(0, &udp.dns, dlen);
	    }
	} else {
	    /* Let a real name daemon handle the query. */
	    udp.dns.hdr.dh_id= new_id(udp.dns.hdr.dh_id,
				udp.hdr.uih_src_port, udp.hdr.uih_src_addr);
	    udp.hdr.uih_dst_addr= current_named();
	    udp.hdr.uih_dst_port= named_port;
	    if (!expecting()) {
		start_expecting();
		newjob(job_expect_named, now + MEDIUM_TIMEOUT, nil);
	    }
	    if (debug >= 1) {
		printf("To named %s:%u\n",
		    inet_ntoa(current_named()), ntohs(named_port));
	    }
	}
    }
    if (write(udp_fd, &udp, ulen) < 0) fatal(udp_device);
    return 1;
}

#if DO_TCP

typedef struct data_cl {	/* Data for connect or listen jobs. */
	int		fd;		/* Open TCP channel. */
	int		dn_fd;		/* TCP channel to the name daemon. */
	int		retry;		/* Retrying a connect? */
	nwio_tcpcl_t	tcpcl;		/* Flags. */
} data_cl_t;

typedef struct data_rw {	/* Data for TCP read or write jobs. */
	int		r_fd;		/* Read from this TCP channel. */
	int		w_fd;		/* And write to this TCP channel. */
	struct data_rw	*rev;		/* Optional reverse TCP channel. */
	u8_t		*buf;		/* Buffer for bytes to transfer. */
	ssize_t		offset;		/* Offset in buf to r/w at. */
	size_t		size;		/* Size of buf. */
} data_rw_t;

static int job_setup_listen(void *data, int expired)
/* Set up a listening channel for TCP DNS queries. */
{
    data_cl_t *data_cl= data;
    nwio_tcpconf_t tcpconf;
    nwio_tcpopt_t tcpopt;
    int fd;

    if (!expired) return 0;
    if (debug >= 2) printf("%s: Setup listen\n", nowgmt());

    if (data_cl == nil) {
	if ((fd= open(tcp_device, O_RDWR)) < 0) {
	    if (errno != EMFILE) report(tcp_device);
	    newjob(job_setup_listen, now + SHORT_TIMEOUT, nil);
	    return 1;
	}

	tcpconf.nwtc_flags= NWTC_SHARED | NWTC_LP_SET | NWTC_UNSET_RA
							| NWTC_UNSET_RP;
	tcpconf.nwtc_locport= my_port;
	if (ioctl(fd, NWIOSTCPCONF, &tcpconf) == -1) fatal(tcp_device);

	tcpopt.nwto_flags= NWTO_DEL_RST;
	if (ioctl(fd, NWIOSTCPOPT, &tcpopt) == -1) fatal(tcp_device);

	data_cl= allocate(nil, sizeof(*data_cl));
	data_cl->fd= fd;
	data_cl->tcpcl.nwtcl_flags= 0;
    }
    /* And listen. */
    newjob(job_listen, NEVER, data_cl);
    return 1;
}

static int job_listen(void *data, int expired)
/* A connection on the TCP DNS query channel. */
{
    data_cl_t *data_cl= data;

    /* Wait for a client. */
    if (asyn_ioctl(&asyn, data_cl->fd, NWIOTCPLISTEN, &data_cl->tcpcl) < 0) {
	if (errno == EINPROGRESS) return 0;
	report(tcp_device);

	/* Try again after a short time. */
	newjob(job_setup_listen, now + SHORT_TIMEOUT, data_cl);
	return 1;
    }
    if (debug >= 2) printf("%s: Listen\n", nowgmt());

    /* Immediately resume listening. */
    newjob(job_setup_listen, IMMEDIATE, nil);

    /* Set up a connect to the real name daemon. */
    data_cl->retry= 0;
    newjob(job_setup_connect, IMMEDIATE, data_cl);
    return 1;
}

static void start_relay(int fd, int dn_fd)
/* Start one or two read jobs after job_setup_connect() or job_connect(). */
{
    data_rw_t *query;	/* Client to DNS daemon relay. */
    data_rw_t *reply;	/* DNS daemon to client relay. */

    query= allocate(nil, sizeof(*query));
    query->r_fd= fd;
    query->buf= allocate(nil, sizeof(u16_t));
    query->offset= 0;
    query->size= sizeof(u16_t);
    if (dn_fd == NO_FD) {
	/* Answer mode. */
	query->w_fd= fd;
	query->rev= nil;
    } else {
	/* Relay mode. */
	reply= allocate(nil, sizeof(*reply));
	reply->r_fd= dn_fd;
	reply->w_fd= fd;
	reply->buf= allocate(nil, sizeof(u16_t));
	reply->offset= 0;
	reply->size= sizeof(u16_t);
	reply->rev= query;
	query->w_fd= dn_fd;
	query->rev= reply;
	newjob(job_read_reply, now + LONG_TIMEOUT, reply);
    }
    newjob(job_read_query, now + LONG_TIMEOUT, query);
}

static void close_relay(data_rw_t *data_rw)
/* Close a relay channel. */
{
    if (data_rw->rev != nil) {
	/* Other end still active, signal EOF. */
	(void) ioctl(data_rw->w_fd, NWIOTCPSHUTDOWN, nil);
	data_rw->rev->rev= nil;
    } else {
	/* Close both ends down. */
	asyn_close(&asyn, data_rw->r_fd);
	close(data_rw->r_fd);
	if (data_rw->w_fd != data_rw->r_fd) {
	    asyn_close(&asyn, data_rw->w_fd);
	    close(data_rw->w_fd);
	}
    }
    deallocate(data_rw->buf);
    deallocate(data_rw);
}

static int job_setup_connect(void *data, int expired)
/* Set up a connect for a TCP channel to the real name daemon. */
{
    nwio_tcpconf_t tcpconf;
    int dn_fd;
    data_cl_t *data_cl= data;

    if (!expired) return 0;
    if (debug >= 2) printf("%s: Setup connect\n", nowgmt());

    if (n_nameds == 0) {
	/* No name daemons to relay to, answer myself. */
	start_relay(data_cl->fd, NO_FD);
	deallocate(data_cl);
	return 1;
    }

    if ((dn_fd= open(tcp_device, O_RDWR)) < 0) {
	if (errno != EMFILE) report(tcp_device);
	if (++data_cl->retry < 5) {
	    /* Retry. */
	    newjob(job_setup_connect, now + SHORT_TIMEOUT, data_cl);
	} else {
	    /* Reply myself (bound to fail). */
	    start_relay(data_cl->fd, NO_FD);
	    deallocate(data_cl);
	}
	return 1;
    }

    tcpconf.nwtc_flags= NWTC_LP_SEL | NWTC_SET_RA | NWTC_SET_RP;
    tcpconf.nwtc_remaddr= current_named();
    tcpconf.nwtc_remport= named_port;
    if (ioctl(dn_fd, NWIOSTCPCONF, &tcpconf) == -1) fatal(tcp_device);

    /* And connect. */
    data_cl->dn_fd= dn_fd;
    data_cl->tcpcl.nwtcl_flags= 0;
    newjob(job_connect, NEVER, data_cl);
    return 1;
}

static int job_connect(void *data, int expired)
/* Connect to a TCP DNS query channel. */
{
    data_cl_t *data_cl= data;

    /* Try to connect. */
    if (asyn_ioctl(&asyn, data_cl->dn_fd, NWIOTCPCONN, &data_cl->tcpcl) < 0) {
	if (errno == EINPROGRESS) return 0;
	if (errno == EIO) fatal(tcp_device);

	/* Connection refused. */
	if (debug >= 2) printf("%s: Connect: %s\n", nowgmt(), strerror(errno));
	asyn_close(&asyn, data_cl->dn_fd);
	close(data_cl->dn_fd);
	data_cl->dn_fd= NO_FD;
	if (++data_cl->retry < 5) {
	    /* Search a new name daemon. */
	    if (!searching()) {
		start_searching();
		force_expire(job_find_named);
	    }
	    newjob(job_setup_connect, NEVER, data_cl);
	    return 1;
	}
	/* Reply with a failure eventually. */
    }
    if (debug >= 2) printf("%s: Connect\n", nowgmt());

    /* Read the query from the user, send on to the name daemon, etc. */
    start_relay(data_cl->fd, data_cl->dn_fd);
    deallocate(data_cl);
    return 1;
}

static void tcp_dns_tell(int fd, u8_t *buf)
/* Tell about a DNS packet on a TCP channel. */
{
    nwio_tcpconf_t tcpconf;

    if (ioctl(fd, NWIOGTCPCONF, &tcpconf) < 0) {
	printf("??\?:?? TCP ");
    } else {
	printf("%s:%u TCP ", inet_ntoa(tcpconf.nwtc_remaddr),
				ntohs(tcpconf.nwtc_remport));
    }
    dns_tell(0, oct2dns(buf + sizeof(u16_t)), ntohs(upack16(buf)));
}

static int job_read_query(void *data, int expired)
/* Read TCP queries from the client. */
{
    data_rw_t *data_rw= data;
    ssize_t count;

    /* Try to read count bytes. */
    count= asyn_read(&asyn, data_rw->r_fd,
				data_rw->buf + data_rw->offset,
				data_rw->size - data_rw->offset);

    if (count < 0) {
	if (errno == EINPROGRESS && !expired) return 0;
	if (errno == EIO) fatal(tcp_device);

	/* Remote end is late, or an error occurred. */
	if (debug >= 2) {
	    printf("%s: TCP read query: %s\n", nowgmt(), strerror(errno));
	}
	close_relay(data_rw);
	return 1;
    }

    if (debug >= 2) {
	printf("%s: TCP read query, %d/%u bytes\n",
	    nowgmt(), data_rw->offset + count, data_rw->size);
    }
    if (count == 0) {
	/* EOF. */
	close_relay(data_rw);
	return 1;
    }
    data_rw->offset += count;
    if (data_rw->offset == data_rw->size) {
	data_rw->size= sizeof(u16_t) + ntohs(upack16(data_rw->buf));
	if (data_rw->size < sizeof(u16_t)) {
	    /* Malformed. */
	    close_relay(data_rw);
	    return 1;
	}
	if (data_rw->offset < data_rw->size) {
	    /* Query not complete, read more. */
	    data_rw->buf= allocate(data_rw->buf, data_rw->size);
	    newjob(job_read_query, now + LONG_TIMEOUT, data_rw);
	    return 1;
	}
    }

    if (data_rw->size < sizeof(u16_t) + sizeof(dns_hdr_t)) {
	close_relay(data_rw);
	return 1;
    }
    if (debug >= 1) tcp_dns_tell(data_rw->r_fd, data_rw->buf);

    /* Relay or reply. */
    if (data_rw->w_fd != data_rw->r_fd) {
	/* We have a real name daemon to do the work. */
	data_rw->offset= 0;
	newjob(job_write_query, now + LONG_TIMEOUT, data_rw);
    } else {
	/* No real name daemons or none reachable, so use the hosts file. */
	dns_t *dp;
	size_t dlen;

	if (data_rw->size < sizeof(u16_t) + PACKETSZ) {
	    data_rw->buf= allocate(data_rw->buf, sizeof(u16_t) + PACKETSZ);
	}

	/* Build a reply packet. */
	dp= oct2dns(data_rw->buf + sizeof(u16_t));
	dlen= data_rw->size - sizeof(u16_t);
	if (!compose_reply(dp, &dlen)) {
	    /* We're told to ask a name daemon, but that won't work. */
	    close_relay(data_rw);
	    return 1;
	}

	/* Start a reply write. */
	pack16(data_rw->buf, htons(dlen));
	data_rw->size= sizeof(u16_t) + dlen;
	data_rw->buf= allocate(data_rw->buf, data_rw->size);
	data_rw->offset= 0;
	newjob(job_write_reply, now + LONG_TIMEOUT, data_rw);
    }
    return 1;
}

static int job_write_query(void *data, int expired)
/* Relay a TCP query to the name daemon. */
{
    data_rw_t *data_rw= data;
    ssize_t count;

    /* Try to write count bytes to the name daemon. */
    count= asyn_write(&asyn, data_rw->w_fd,
				data_rw->buf + data_rw->offset,
				data_rw->size - data_rw->offset);

    if (count <= 0) {
	if (errno == EINPROGRESS && !expired) return 0;
	if (errno == EIO) fatal(tcp_device);

	/* A write expired or failed (usually a broken connection.) */
	if (debug >= 2) {
	    printf("%s: TCP write query: %s\n", nowgmt(), strerror(errno));
	}
	close_relay(data_rw);
	return 1;
    }

    if (debug >= 2) {
	printf("%s: TCP write query, %d/%u bytes\n",
	    nowgmt(), data_rw->offset + count, data_rw->size);
    }
    data_rw->offset += count;
    if (data_rw->offset < data_rw->size) {
	/* Partial write, continue. */
	newjob(job_write_query, now + LONG_TIMEOUT, data_rw);
	return 1;
    }
    if (debug >= 1) tcp_dns_tell(data_rw->w_fd, data_rw->buf);

    /* Query fully send on, go read more queries. */
    data_rw->offset= 0;
    data_rw->size= sizeof(u16_t);
    newjob(job_read_query, now + LONG_TIMEOUT, data_rw);
    return 1;
}

static int job_read_reply(void *data, int expired)
/* Read a TCP reply from the real name daemon. */
{
    data_rw_t *data_rw= data;
    ssize_t count;

    /* Try to read count bytes. */
    count= asyn_read(&asyn, data_rw->r_fd,
				data_rw->buf + data_rw->offset,
				data_rw->size - data_rw->offset);

    if (count < 0) {
	if (errno == EINPROGRESS && !expired) return 0;
	if (errno == EIO) fatal(tcp_device);

	/* Remote end is late, or an error occurred. */
	if (debug >= 2) {
	    printf("%s: TCP read reply: %s\n", nowgmt(), strerror(errno));
	}
	close_relay(data_rw);
	return 1;
    }

    if (debug >= 2) {
	printf("%s: TCP read reply, %d/%u bytes\n",
	    nowgmt(), data_rw->offset + count, data_rw->size);
    }
    if (count == 0) {
	/* EOF. */
	close_relay(data_rw);
	return 1;
    }
    data_rw->offset += count;
    if (data_rw->offset == data_rw->size) {
	data_rw->size= sizeof(u16_t) + ntohs(upack16(data_rw->buf));
	if (data_rw->size < sizeof(u16_t)) {
	    /* Malformed. */
	    close_relay(data_rw);
	    return 1;
	}
	if (data_rw->offset < data_rw->size) {
	    /* Reply not complete, read more. */
	    data_rw->buf= allocate(data_rw->buf, data_rw->size);
	    newjob(job_read_reply, now + LONG_TIMEOUT, data_rw);
	    return 1;
	}
    }
    if (debug >= 1) tcp_dns_tell(data_rw->r_fd, data_rw->buf);

    /* Reply fully read, send it on. */
    data_rw->offset= 0;
    newjob(job_write_reply, now + LONG_TIMEOUT, data_rw);
    return 1;
}

static int job_write_reply(void *data, int expired)
/* Send a TCP reply to the client. */
{
    data_rw_t *data_rw= data;
    ssize_t count;

    /* Try to write count bytes to the client. */
    count= asyn_write(&asyn, data_rw->w_fd,
				data_rw->buf + data_rw->offset,
				data_rw->size - data_rw->offset);

    if (count <= 0) {
	if (errno == EINPROGRESS && !expired) return 0;
	if (errno == EIO) fatal(tcp_device);

	/* A write expired or failed (usually a broken connection.) */
	if (debug >= 2) {
	    printf("%s: TCP write reply: %s\n", nowgmt(), strerror(errno));
	}
	close_relay(data_rw);
	return 1;
    }

    if (debug >= 2) {
	printf("%s: TCP write reply, %d/%u bytes\n",
	    nowgmt(), data_rw->offset + count, data_rw->size);
    }
    data_rw->offset += count;
    if (data_rw->offset < data_rw->size) {
	/* Partial write, continue. */
	newjob(job_write_reply, now + LONG_TIMEOUT, data_rw);
	return 1;
    }
    if (debug >= 1) tcp_dns_tell(data_rw->w_fd, data_rw->buf);

    /* Reply fully send on, go read more replies (or queries). */
    data_rw->offset= 0;
    data_rw->size= sizeof(u16_t);
    newjob(data_rw->w_fd != data_rw->r_fd ? job_read_reply : job_read_query,
		now + LONG_TIMEOUT, data_rw);
    return 1;
}
#else /* !DO_TCP */

static int job_dummy(void *data, int expired)
{
    return 1;
}
#define job_setup_listen	job_dummy
#define job_setup_connect	job_dummy
#endif /* !DO_TCP */

static void named_probe(ipaddr_t ip)
/* Send a probe to a name daemon, like 'host -r -t ns . <ip>'. */
{
    udp_dns_t udp;
#   define dlen (offsetof(dns_t, data) + 5)
#   define ulen (offsetof(udp_dns_t, dns) + dlen)

    /* Send a simple DNS query that all name servers can answer easily:
     * "What are the name servers for the root domain?"
     */
    udp.dns.hdr.dh_id= new_id(ID_PROBE, my_port, ID_IPSELF);
    udp.dns.hdr.qr = 0;
    udp.dns.hdr.opcode = 0;
    udp.dns.hdr.aa = 0;
    udp.dns.hdr.tc = 0;
    udp.dns.hdr.rd = 0;
    udp.dns.hdr.ra = 0;
    udp.dns.hdr.unused = 0;
    udp.dns.hdr.ad = 0;
    udp.dns.hdr.cd = 0;
    udp.dns.hdr.rcode = 0;
    udp.dns.hdr.dh_qdcount= HTONS(1);
    udp.dns.hdr.dh_ancount= HTONS(0);
    udp.dns.hdr.dh_nscount= HTONS(0);
    udp.dns.hdr.dh_arcount= HTONS(0);

    udp.dns.data[0] = 0;	/* Null name. */
    pack16(udp.dns.data+1, HTONS(T_NS));
    pack16(udp.dns.data+3, HTONS(C_IN));
    if (debug >= 1) {
	printf("PROBE %s ", inet_ntoa(ip));
	dns_tell(0, &udp.dns, dlen);
    }

    udp.hdr.uih_dst_addr= ip;
    udp.hdr.uih_dst_port= named_port;
    udp.hdr.uih_ip_opt_len= 0;
    udp.hdr.uih_data_len= dlen;

    if (write(udp_fd, &udp, ulen) < 0) fatal(udp_device);
#undef dlen
#undef ulen
}

static int job_find_named(void *data, int expired)
/* Look for a real name daemon to answer real DNS queries. */
{
    if (!expired) return 0;
    if (debug >= 2) printf("%s: Find named\n", nowgmt());

    /* New search? */
    if (search_ct < 0) {
	search_ct= n_nameds;
	i_named= -1;
    }

    if (--search_ct < 0) {
	/* Forced end of search (named response!), or end of search with
	 * nothing found.  Search again after a long time.
	 */
	newjob(job_find_named,
	    (stale > 0 || i_named > 0) ? now + LONG_TIMEOUT : NEVER, nil);
	force_expire(job_setup_connect);
	return 1;
    }

    /* Send a named probe. */
    i_named= (i_named+1) % n_nameds;
    named_probe(current_named());

    /* Schedule the next call. */
    newjob(job_find_named, now + SHORT_TIMEOUT, nil);
    return 1;
}

static int job_expect_named(void *data, int expired)
/* The real name server is expected to answer by now. */
{
    if (!expired) return 0;
    if (debug >= 2) printf("%s: Expect named\n", nowgmt());

    if (expecting() && !searching()) {
	/* No answer yet, start searching. */
	start_searching();
	force_expire(job_find_named);
    }
    return 1;
}

static void sig_handler(int sig)
/* A signal forces a search for a real name daemon, etc. */
{
    switch (sig) {
    case SIGINT:
    case SIGTERM:	done= 1;		break;
    case SIGHUP:	reinit= 1;		break;
    case SIGUSR1:	debug++;		break;
    case SIGUSR2:	debug= 0;		break;
    }
}

static void usage(void)
{
    fprintf(stderr, "Usage: nonamed [-qs] [-d[level]] [-p port]\n");
    exit(1);
}

int main(int argc, char **argv)
{
    job_t *job;
    nwio_udpopt_t udpopt;
    int i;
    struct servent *servent;
    struct sigaction sa;
    FILE *fp;
    int quit= 0;

    /* Debug output must be line buffered. */
    setvbuf(stdout, nil, _IOLBF, 0);

    /* DNS service port number? */
    if ((servent= getservbyname("domain", nil)) == nil) {
	fprintf(stderr, "nonamed: \"domain\": unknown service\n");
	exit(1);
    }
    my_port= servent->s_port;
    named_port= servent->s_port;

    i= 1;
    while (i < argc && argv[i][0] == '-') {
	char *opt= argv[i++] + 1, *end;

	if (opt[0] == '-' && opt[1] == 0) break;

	switch (*opt++) {
	case 'd':		/* Debug level. */
	    debug= 1;
	    if (between('0', *opt, '9')) debug= strtoul(opt, &opt, 10);
	    break;
	case 'p':		/* Port to listen to (for testing.) */
	    if (*opt == 0) {
		if (i == argc) usage();
		opt= argv[i++];
	    }
	    my_port= htons(strtoul(opt, &end, 0));
	    if (opt == end || *end != 0) usage();
	    opt= end;
	    break;
	case 's':
	    single= 1;
	    break;
	case 'q':		/* Quit after printing cache contents. */
	    quit= 1;
	    break;
	case 'L':
	    localonly= 1;
	    break;
	default:
	    usage();
	}
    }
    if (i != argc) usage();

    if (quit) {
	/* Oops, just having a look at the cache. */
	debug= 2;
	now= time(nil);
	n_datamax= -1;
	file2cache();
	return 0;
    }

    /* Don't die on broken pipes, reinitialize on hangup, etc. */
    sa.sa_handler= SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags= 0;
    sigaction(SIGPIPE, &sa, nil);
    sa.sa_handler= sig_handler;
    sigaction(SIGINT, &sa, nil);
    sigaction(SIGHUP, &sa, nil);
    sigaction(SIGUSR1, &sa, nil);
    sigaction(SIGUSR2, &sa, nil);
    sigaction(SIGTERM, &sa, nil);

    /* TCP and UDP device names. */
    if ((tcp_device= getenv("TCP_DEVICE")) == nil) tcp_device= TCP_DEVICE;
    if ((udp_device= getenv("UDP_DEVICE")) == nil) udp_device= UDP_DEVICE;

    /* Open an UDP channel for incoming DNS queries. */
    if ((udp_fd= open(udp_device, O_RDWR)) < 0) fatal(udp_device);

    udpopt.nwuo_flags= NWUO_EXCL | NWUO_LP_SET | NWUO_EN_LOC | NWUO_DI_BROAD
		| NWUO_RP_ANY | NWUO_RA_ANY | NWUO_RWDATALL | NWUO_DI_IPOPT;
    udpopt.nwuo_locport= my_port;
    if (ioctl(udp_fd, NWIOSUDPOPT, &udpopt) == -1
	|| ioctl(udp_fd, NWIOGUDPOPT, &udpopt) == -1
    ) {
	fatal(udp_device);
    }

    /* The current time is... */
    now= time(nil);

    /* Read configuration and data cached by the previous nonamed. */
    init_config(udpopt.nwuo_locaddr);
    file2cache();

    if (!single) {
	/* Save process id. */
	if ((fp= fopen(PIDFILE, "w")) != nil) {
	    fprintf(fp, "%u\n", (unsigned) getpid());
	    fclose(fp);
	}
    }

    /* Jobs that start the ball rolling. */
    newjob(job_read_udp, NEVER, nil);
    newjob(job_setup_listen, IMMEDIATE, nil);
    newjob(job_find_named, IMMEDIATE, nil);

    /* Open syslog. */
    openlog("nonamed", LOG_PID, LOG_DAEMON);

    while (!done) {
	/* There is always something in the queue. */
	assert(queue != nil);

	/* Any expired jobs? */
	while (queue->timeout <= now) {
	    (void) execjob(queue, 1);
	    assert(queue != nil);
	}

	/* Check I/O jobs. */
	for (job= queue; job != nil; job= job->next) {
	    if (execjob(job, 0)) break;
	}

	if (queue->timeout != IMMEDIATE) {
	    struct timeval tv, *tvp;

	    if (debug >= 2) printf("%s: I/O wait", nowgmt());

	    if (queue->timeout != NEVER) {
		tv.tv_sec= queue->timeout;
		tv.tv_usec= 0;
		tvp= &tv;
		if (debug >= 2) printf(" (expires %s)\n", timegmt(tv.tv_sec));
	    } else {
		tvp= nil;
		if (debug >= 2) fputc('\n', stdout);
	    }
	    fflush(stdout);

	    if (asyn_wait(&asyn, 0, tvp) < 0) {
		if (errno != EINTR && errno != EAGAIN) fatal("fwait()");
	    }
	    now= time(nil);
	}

	if (reinit) {
	    /* A hangup makes us go back to square one. */
	    reinit= 0;
	    if (ioctl(udp_fd, NWIOGUDPOPT, &udpopt) == -1) fatal(udp_device);
	    init_config(udpopt.nwuo_locaddr);
	    start_searching();
	    force_expire(job_find_named);
	}
    }
    cache2file();
    (void) unlink(PIDFILE);
    if (debug >= 2) printf("sbrk(0) = %u\n", (unsigned) sbrk(0));
    return 0;
}
