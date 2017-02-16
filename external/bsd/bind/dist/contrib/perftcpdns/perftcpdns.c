/*	$NetBSD: perftcpdns.c,v 1.3 2014/12/10 04:37:56 christos Exp $	*/

/*
 * Copyright (C) 2013, 2014  Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * TCP DNS perf tool
 *
 * main parameters are -r<rate> and <server>
 * standard options are 4|6 (IPv4|IPv6), rate computations, terminaisons,
 * EDNS0, NOERROR|NXDOMAIN, template (for your own query), diags,
 * alternate server port and UDP version.
 *
 * To help to crush kernels (unfortunately both client and server :-)
 * this version of the tool is multi-threaded:
 *  - the master thread inits, monitors the activity each millisecond,
 *   and report results when finished
 *  - the connecting thread computes the date of the next connection,
 *   creates a socket, makes it non blocking, binds it if wanted,
 *   connects it and pushes it on the output epoll queue
 *  - the sending thread gets by epoll connected sockets, timeouts
 *   embryonic connections, sends queries and pushes sockets on
 *   the input epoll queue
 *  - the receiving thread gets by epoll sockets with a pending
 *   response, receives responses, timeouts unanswered queries,
 *   and recycles (by closing them) all sockets.
 *
 * Rate computation details:
 *  - the target rate is in query+response per second.
 *  - rating is done by the connecting thread.
 *  - of course the tool is always late so the target rate is never
 *   reached. BTW there is no attempt to internally adjust the
 *   effective rate to the target one: this must be by tuning
 *   the rate related parameters, first the -r<rate> itself.
 *  - at the beginning of the connecting thread iteration loop
 *   (second "loops" counter) the date of the due (aka next) connect()
 *   call is computed from the last one with 101% of the rate.
 *  - the due date is compared with the current date (aka now).
 *  - if the due is before, lateconn counter is incremented, else
 *   the thread sleeps for the difference,
 *  - the next step is to reget the current date, if it is still
 *   before the due date (e.g., because the sleep was interrupted)
 *   the first shortwait counter is incremented.
 *  - if it is after (common case) the number of connect calls is
 *   computed from the difference between now and due divided by rate,
 *   rounded to the next number,
 *  - this number of connect() calls is bounded by the -a<aggressiveness>
 *   parameter to avoid too many back to back new connection attempts.
 *  - the compconn counter is incremented, errors (other than EINPROGRESS
 *   from not blocking connect()) are printed. When an error is
 *   related to a local limit (e.g., EMFILE, EADDRNOTAVAIL or the
 *   internal ENOMEM) the locallimit counter is incremented.
 */

#ifdef __linux__
#define _GNU_SOURCE
#endif

#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/prctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* DNS defines */

#define NS_TYPE_A		1
#define NS_TYPE_NS		2
#define NS_TYPE_CNAME		5
#define NS_TYPE_SOA		6
#define NS_TYPE_NULL		10
#define NS_TYPE_PTR		12
#define NS_TYPE_MX		15
#define NS_TYPE_TXT		16
#define NS_TYPE_AAAA		28
#define NS_TYPE_OPT		41
#define NS_TYPE_DS		43
#define NS_TYPE_RRSIG		46
#define NS_TYPE_NSEC		47
#define NS_TYPE_DNSKEY		48
#define NS_TYPE_NSEC3		50
#define NS_TYPE_NSEC3PARAM	51
#define NS_TYPE_TSIG		250
#define NS_TYPE_IXFR		251
#define NS_TYPE_AXFR		252
#define NS_TYPE_ANY		255

#define NS_CLASS_IN		1
#define NS_CLASS_ANY		255

#define NS_OFF_ID		0
#define NS_OFF_FLAGS		2
#define NS_OFF_QDCOUNT		4
#define NS_OFF_ANCOUNT		6
#define NS_OFF_NSCOUNT		8
#define NS_OFF_ARCOUNT		10
#define NS_OFF_QUESTION		12

#define NS_FLAG_QR		0x8000U
#define NS_FLAG_AA		0x0400U
#define NS_FLAG_TC		0x0200U
#define NS_FLAG_RD		0x0100U
#define NS_FLAG_RA		0x0080U
#define NS_FLAG_AD		0x0020U
#define NS_FLAG_CD		0x0010U

#define NS_XFLAG_DO		0x8000U

#define NS_OPCODE_MASK		0x7000U
#define NS_OPCODE_QUERY		0

#define NS_RCODE_MASK		0x000fU
#define NS_RCODE_NOERROR	0
#define NS_RCODE_FORMERR	1
#define NS_RCODE_SERVFAIL	2
#define NS_RCODE_NXDOMAIN	3
#define NS_RCODE_NOIMP		4
#define NS_RCODE_REFUSED	5
#define NS_RCODE_LAST		6

/* chaining macros */

#define ISC_INIT(head, headl)		do { \
	(head) = -1; \
	(headl) = &(head); \
} while (/*CONSTCOND*/0)

#define ISC_INSERT(head, headl, elm)	do { \
	(elm)->next = -1; \
	(elm)->prev = (headl); \
	*(headl) = (elm) - xlist; \
	(headl) = &((elm)->next); \
} while (/*CONSTCOND*/0)

#define ISC_REMOVE(headl, elm)		do { \
	if ((elm)->next != -1) \
		xlist[(elm)->next].prev = (elm)->prev; \
	else \
		(headl) = (elm)->prev; \
	*(elm)->prev = (elm)->next; \
} while (/*CONSTCOND*/0)

/*
 * Data structures
 */

/*
 * exchange:
 *    - per exchange values:
 *	* order (for debugging)
 *	* id
 *	* random (for debugging)
 *	* time-stamps
 *
 * sent/rcvd chain, "next to be received" on entry cache.
 */

struct exchange {				/* per exchange structure */
	int sock;				/* socket descriptor */
	int next, *prev;			/* chaining */
#define X_FREE	0
#define X_CONN	1
#define X_READY	2
#define X_SENT	3
	int state;				/* state */
	uint16_t id;				/* ID */
	uint64_t order;				/* number of this exchange */
	struct timespec ts0, ts1, ts2, ts3;	/* timespecs */
};
struct exchange *xlist;				/* exchange list */
int xlast;					/* number of exchanges */
int xconn, *xconnl;				/* connecting list */
int xready, *xreadyl;				/* connected list */
int xsent, *xsentl;				/* sent list */
int xfree, *xfreel;				/* free list */
int xused;					/* next to be used list */
pthread_mutex_t mtxconn, mtxsent, mtxfree;	/* mutexes */
uint64_t xccount;				/* connected counters */
uint64_t xscount;				/* sent counters */
uint64_t xrcount;				/* received counters */

/*
 * statictics counters and accumulators
 */

uint64_t recverr, tooshort, locallimit;		/* error counters */
uint64_t loops[4], shortwait[3];		/* rate stats */
uint64_t lateconn, compconn;			/* rate stats (cont) */
uint64_t badconn, collconn, badsent, collsent;	/* rate stats (cont) */
uint64_t badid, notresp;			/* bad response counters */
uint64_t rcodes[NS_RCODE_LAST + 1];		/* rcode counters */
double dmin = 999999999.;			/* minimum delay */
double dmax = 0.;				/* maximum delay */
double dsum = 0.;				/* delay sum */
double dsumsq = 0.;				/* square delay sum */

/*
 * command line parameters
 */

int edns0;				/* EDNS0 DO flag */
int ipversion = 0;			/* IP version */
int rate;				/* rate in connections per second */
int noreport;				/* disable auto reporting */
int report;				/* delay between two reports */
uint32_t range;				/* randomization range */
uint32_t maxrandom;			/* maximum random value */
int basecnt;				/* base count */
char *base[2];				/* bases */
int gotnumreq = -1;			/* numreq[0] was set */
int numreq[2];				/* number of exchanges */
int period;				/* test period */
int gotlosttime = -1;			/* losttime[0] was set */
double losttime[2] = {.5, 1.};		/* delay for a timeout  */
int gotmaxloss = -1;			/* max{p}loss[0] was set */
int maxloss[2];				/* maximum number of losses */
double maxploss[2] = {0., 0.};		/* maximum percentage */
char *localname;			/* local address or interface */
int aggressiveness = 1;			/* back to back connections */
int seeded;				/* is a seed provided */
unsigned int seed;			/* randomization seed */
char *templatefile;			/* template file name */
int rndoffset = -1;			/* template offset (random) */
char *diags;				/* diagnostic selectors */
char *servername;			/* server */
int ixann;				/* ixann NXDOMAIN */
int udp;				/* use UDP in place of TCP */
int minport, maxport, curport;		/* port range */

/*
 * global variables
 */

struct sockaddr_storage localaddr;	/* local socket address */
struct sockaddr_storage serveraddr;	/* server socket address */
in_port_t port = 53;			/* server socket port */

int epoll_ifd, epoll_ofd;		/* epoll file descriptors */
#ifndef EVENTS_CNT
#define EVENTS_CNT	16
#endif
struct epoll_event ievents[EVENTS_CNT];	/* polled input events */
struct epoll_event oevents[EVENTS_CNT];	/* polled output events */
int interrupted, fatal;			/* to finish flags */

uint8_t obuf[4098], ibuf[4098];		/* I/O buffers */
char tbuf[4098];			/* template buffer */

struct timespec boot;			/* the date of boot */
struct timespec last;			/* the date of last connect */
struct timespec due;			/* the date of next connect */
struct timespec dreport;		/* the date of next reporting */
struct timespec finished;		/* the date of finish */

/*
 * template
 */

size_t length_query;
uint8_t template_query[4096];
size_t random_query;

/*
 * threads
 */

pthread_t master, connector, sender, receiver;

/*
 * initialize data structures handling exchanges
 */

void
inits(void)
{
	int idx;

	ISC_INIT(xconn, xconnl);
	ISC_INIT(xready, xreadyl);
	ISC_INIT(xsent, xsentl);
	ISC_INIT(xfree, xfreel);

	if ((pthread_mutex_init(&mtxconn, NULL) != 0) ||
	    (pthread_mutex_init(&mtxsent, NULL) != 0) ||
	    (pthread_mutex_init(&mtxfree, NULL) != 0)) {
		fprintf(stderr, "pthread_mutex_init failed\n");
		exit(1);
	}

	epoll_ifd = epoll_create(EVENTS_CNT);
	if (epoll_ifd < 0) {
		perror("epoll_create(input)");
		exit(1);
	}
	epoll_ofd = epoll_create(EVENTS_CNT);
	if (epoll_ofd < 0) {
		perror("epoll_create(output)");
		exit(1);
	}

	xlist = (struct exchange *) malloc(xlast * sizeof(struct exchange));
	if (xlist == NULL) {
		perror("malloc(exchanges)");
		exit(1);
	}
	memset(xlist, 0, xlast * sizeof(struct exchange));

	for (idx = 0; idx < xlast; idx++)
		xlist[idx].sock = xlist[idx].next = -1;
}

/*
 * build a TCP DNS QUERY
 */

void
build_template_query(void)
{
	uint8_t *p = template_query;
	uint16_t v;

	/* flags */
	p += NS_OFF_FLAGS;
	v = NS_FLAG_RD;
	*p++ = v >> 8;
	*p++ = v & 0xff;
	/* qdcount */
	v = 1;
	*p++ = v >> 8;
	*p++ = v & 0xff;
	/* ancount */
	v = 0;
	*p++ = v >> 8;
	*p++ = v & 0xff;
	/* nscount */
	v = 0;
	*p++ = v >> 8;
	*p++ = v & 0xff;
	/* arcount */
	v = edns0;
	*p++ = v >> 8;
	*p++ = v & 0xff;
	/* icann.link (or ixann.link) */
	*p++ = 5;
	*p++ = 'i';
	if (ixann == 0)
		*p++ = 'c';
	else
		*p++ = 'x';
	*p++ = 'a';
	*p++ = 'n';
	*p++ = 'n';
	*p++ = 4;
	*p++ = 'l';
	*p++ = 'i';
	*p++ = 'n';
	*p++ = 'k';
	*p++ = 0;
	/* type A/AAAA */
	if (ipversion == 4)
		v = NS_TYPE_A;
	else
		v = NS_TYPE_AAAA;
	*p++ = v >> 8;
	*p++ = v & 0xff;
	/* class IN */
	v = NS_CLASS_IN;
	*p++ = v >> 8;
	*p++ = v & 0xff;
	/* EDNS0 OPT with DO */
	if (edns0) {
		/* root name */
		*p++ = 0;
		/* type OPT */
		v = NS_TYPE_OPT;
		*p++ = v >> 8;
		*p++ = v & 0xff;
		/* class UDP length */
		v = 4096;
		*p++ = v >> 8;
		*p++ = v & 0xff;
		/* extended rcode 0 */
		*p++ = 0;
		/* version 0 */
		*p++ = 0;
		/* extended flags DO */
		v = NS_XFLAG_DO;
		*p++ = v >> 8;
		*p++ = v & 0xff;
		/* rdlength */
		v = 0;
		*p++ = v >> 8;
		*p++ = v & 0xff;
	}
	/* length */
	length_query = p - template_query;
}

/*
 * get a TCP DNS client QUERY template
 * from the file given in the command line (-T<template-file>)
 * and rnd offset (-O<random-offset>)
 */

void
get_template_query(void)
{
	uint8_t *p = template_query;
	int fd, cc, i, j;

	fd = open(templatefile, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "open(%s): %s\n",
			templatefile, strerror(errno));
		exit(2);
	}
	cc = read(fd, tbuf, sizeof(tbuf));
	(void) close(fd);
	if (cc < 0) {
		fprintf(stderr, "read(%s): %s\n",
			templatefile, strerror(errno));
		exit(1);
	}
	if (cc < NS_OFF_QUESTION + 6) {
		fprintf(stderr,"file '%s' too small\n", templatefile);
		exit(2);
	}
	if (cc > 4096) {
		fprintf(stderr,"file '%s' too large\n", templatefile);
		exit(2);
	}
	j = 0;
	for (i = 0; i < cc; i++) {
		if (isspace((int) tbuf[i]))
			continue;
		if (!isxdigit((int) tbuf[i])) {
			fprintf(stderr,
				"illegal char[%d]='%c' in file '%s'\n",
				i, (int) tbuf[i], templatefile);
			exit(2);
		}
		tbuf[j] = tbuf[i];
		j++;
	}
	cc = j;
	if ((cc & 1) != 0) {
		fprintf(stderr,
			"odd number of hexadecimal digits in file '%s'\n",
			templatefile);
		exit(2);
	}
	length_query = cc >> 1;
	for (i = 0; i < cc; i += 2)
		(void) sscanf(tbuf + i, "%02hhx", &p[i >> 1]);
	if (rndoffset >= 0)
		random_query = (size_t) rndoffset;
	if (random_query > length_query) {
		fprintf(stderr,
			"random (at %zu) outside the template (length %zu)?\n",
			random_query, length_query);
		exit(2);
	}
}

#if 0
/*
 * randomize the value of the given field:
 *   - offset of the field
 *   - random seed (used as it when suitable)
 *   - returns the random value which was used
 */

uint32_t
randomize(size_t offset, uint32_t r)
{
	uint32_t v;

	if (range == 0)
		return 0;
	if (range == UINT32_MAX)
		return r;
	if (maxrandom != 0)
		while (r >= maxrandom)
			r = (uint32_t) random();
	r %= range + 1;
	v = r;
	v += obuf[offset];
	obuf[offset] = v;
	if (v < 256)
		return r;
	v >>= 8;
	v += obuf[offset - 1];
	obuf[offset - 1] = v;
	if (v < 256)
		return r;
	v >>= 8;
	v += obuf[offset - 2];
	obuf[offset - 2] = v;
	if (v < 256)
		return r;
	v >>= 8;
	v += obuf[offset - 3];
	obuf[offset - 3] = v;
	return r;
}
#endif

/*
 * flush/timeout connect
 */

void
flushconnect(void)
{
	struct exchange *x;
	struct timespec now;
	int idx = xconn;
	int cnt = 10;
	double waited;

	if (clock_gettime(CLOCK_REALTIME, &now) < 0) {
		perror("clock_gettime(flushconnect)");
		fatal = 1;
		(void) pthread_kill(master, SIGTERM);
		return;
	}

	while (--cnt >= 0) {
		if (idx < 0)
			return;
		x = xlist + idx;
		idx = x->next;
		if (x->state != X_CONN)
			abort();
		/* check for a timed-out connection */
		waited = now.tv_sec - x->ts0.tv_sec;
		waited += (now.tv_nsec - x->ts0.tv_nsec) / 1e9;
		if (waited < losttime[0])
			return;
		/* garbage collect timed-out connections */
		if (pthread_mutex_lock(&mtxconn) != 0) {
			fprintf(stderr, "pthread_mutex_lock(flushconnect)");
			fatal = 1;
			(void) pthread_kill(master, SIGTERM);
			return;
		}
		ISC_REMOVE(xconnl, x);
		if (pthread_mutex_unlock(&mtxconn) != 0) {
			fprintf(stderr, "pthread_mutex_unlock(flushconnect)");
			fatal = 1;
			(void) pthread_kill(master, SIGTERM);
			return;
		}
		(void) close(x->sock);
		x->sock = -1;
		collconn++;
		if (pthread_mutex_lock(&mtxfree) != 0) {
			fprintf(stderr, "pthread_mutex_lock(flushconnect)");
			fatal = 1;
			(void) pthread_kill(master, SIGTERM);
			return;
		}
		x->state = X_FREE;
		ISC_INSERT(xfree, xfreel, x);
		if (pthread_mutex_unlock(&mtxfree) != 0) {
			fprintf(stderr, "pthread_mutex_unlock(flushconnect)");
			fatal = 1;
			(void) pthread_kill(master, SIGTERM);
			return;
		}
	}
}

/*
 * poll connected
 */

void
pollconnect(int topoll)
{
	struct exchange *x;
	int evn, idx, err;
	socklen_t len = sizeof(int);

	for (evn = 0; evn < topoll; evn++) {
		idx = oevents[evn].data.fd;
		x = xlist + idx;
		if (x->state != X_CONN)
			continue;
		if (oevents[evn].events == 0)
			continue;
		if (pthread_mutex_lock(&mtxconn) != 0) {
			fprintf(stderr, "pthread_mutex_lock(pollconnect)");
			fatal = 1;
			(void) pthread_kill(master, SIGTERM);
			return;
		}
		ISC_REMOVE(xconnl, x);
		if (pthread_mutex_unlock(&mtxconn) != 0) {
			fprintf(stderr, "pthread_mutex_unlock(pollconnect)");
			fatal = 1;
			(void) pthread_kill(master, SIGTERM);
			return;
		}
		oevents[evn].events = 0;
		if ((getsockopt(x->sock, SOL_SOCKET, SO_ERROR,
				&err, &len) < 0) ||
		    (err != 0)) {
			(void) close(x->sock);
			x->sock = -1;
			badconn++;
			if (pthread_mutex_lock(&mtxfree) != 0) {
				fprintf(stderr,
					"pthread_mutex_lock(pollconnect)");
				fatal = 1;
				(void) pthread_kill(master, SIGTERM);
				return;
			}
			x->state = X_FREE;
			ISC_INSERT(xfree, xfreel, x);
			if (pthread_mutex_unlock(&mtxfree) != 0) {
				fprintf(stderr,
					"pthread_mutex_unlock(pollconnect)");
				fatal = 1;
				(void) pthread_kill(master, SIGTERM);
				return;
			}
			continue;
		}
		x->state = X_READY;
		ISC_INSERT(xready, xreadyl, x);
	}
}

/*
 * send the TCP DNS QUERY
 */

int
sendquery(struct exchange *x)
{
	ssize_t ret;
	size_t off;

	if (udp)
		off = 0;
	else {
		off = 2;
		/* message length */
		obuf[0] = length_query >> 8;
		obuf[1]= length_query & 0xff;
	}
	/* message from template */
	memcpy(obuf + off, template_query, length_query);
	/* ID */
	memcpy(obuf + off + NS_OFF_ID, &x->id, 2);
#if 0
	/* random */
	if (random_query > 0)
		x->rnd = randomize(random_query + off, x->rnd);
#endif
	/* timestamp */
	errno = 0;
	ret = clock_gettime(CLOCK_REALTIME, &x->ts2);
	if (ret < 0) {
		perror("clock_gettime(send)");
		fatal = 1;
		(void) pthread_kill(master, SIGTERM);
		return -errno;
	}
	ret = send(x->sock, obuf, length_query + off, 0);
	if ((size_t) ret == length_query + off)
		return 0;
	return -errno;
}

/*
 * poll ready and send
 */

void
pollsend(void)
{
	struct exchange *x;
	int idx = xready;
	struct epoll_event ev;

	memset(&ev, 0, sizeof(ev));
	ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
	for (;;) {
		if (idx < 0)
			return;
		x = xlist + idx;
		ev.data.fd = idx;
		idx = x->next;
		ISC_REMOVE(xreadyl, x);
		if (sendquery(x) < 0) {
			(void) close(x->sock);
			x->sock = -1;
			badsent++;
			if (pthread_mutex_lock(&mtxfree) != 0) {
				fprintf(stderr,
					"pthread_mutex_lock(pollsend)");
				fatal = 1;
				(void) pthread_kill(master, SIGTERM);
				return;
			}
			x->state = X_FREE;
			ISC_INSERT(xfree, xfreel, x);
			if (pthread_mutex_unlock(&mtxfree) != 0) {
				fprintf(stderr,
					"pthread_mutex_unlock(pollsend)");
				fatal = 1;
				(void) pthread_kill(master, SIGTERM);
				return;
			}
			continue;
		}
		xscount++;
		if (pthread_mutex_lock(&mtxsent) != 0) {
			fprintf(stderr, "pthread_mutex_lock(pollsend)");
			fatal = 1;
			(void) pthread_kill(master, SIGTERM);
			return;
		}
		x->state = X_SENT;
		ISC_INSERT(xsent, xsentl, x);
		if (pthread_mutex_unlock(&mtxsent) != 0) {
			fprintf(stderr, "pthread_mutex_unlock(pollsend)");
			fatal = 1;
			(void) pthread_kill(master, SIGTERM);
			return;
		}
		if (epoll_ctl(epoll_ifd, EPOLL_CTL_ADD, x->sock, &ev) < 0) {
			perror("epoll_ctl(add input)");
			fatal = 1;
			(void) pthread_kill(master, SIGTERM);
			return;
		}
	}
}

/*
 * receive a TCP DNS RESPONSE
 */

void
receiveresp(struct exchange *x)
{
	struct timespec now;
	ssize_t cc;
	size_t off;
	uint16_t v;
	double delta;

	cc = recv(x->sock, ibuf, sizeof(ibuf), 0);
	if (cc < 0) {
		if ((errno == EAGAIN) ||
		    (errno == EWOULDBLOCK) ||
		    (errno == EINTR) ||
		    (errno == ECONNRESET)) {
			recverr++;
			return;
		}
		perror("recv");
		fatal = 1;
		(void) pthread_kill(master, SIGTERM);
		return;
	}
	if (udp)
		off = 0;
	else
		off = 2;
	/* enforce a reasonable length */
	if ((size_t) cc < length_query + off) {
		tooshort++;
		return;
	}
	/* must match the ID */
	if (memcmp(ibuf + off + NS_OFF_ID, &x->id, 2) != 0) {
		badid++;
		return;
	}
	/* must be a response */
	memcpy(&v, ibuf + off + NS_OFF_FLAGS, 2);
	v = ntohs(v);
	if ((v & NS_FLAG_QR) == 0) {
		notresp++;
		return;
	}
	if (clock_gettime(CLOCK_REALTIME, &now) < 0) {
		perror("clock_gettime(receive)");
		fatal = 1;
		(void) pthread_kill(master, SIGTERM);
		return;
	}
	/* got it: update stats */
	xrcount++;
	x->ts3 = now;
	delta = x->ts3.tv_sec - x->ts2.tv_sec;
	delta += (x->ts3.tv_nsec - x->ts2.tv_nsec) / 1e9;
	if (delta < dmin)
		dmin = delta;
	if (delta > dmax)
		dmax = delta;
	dsum += delta;
	dsumsq += delta * delta;
	v &= NS_RCODE_MASK;
	if (v >= NS_RCODE_LAST)
		v = NS_RCODE_LAST;
	rcodes[v] += 1;
}

/*
 * flush/timeout receive
 */

void
flushrecv(void)
{
	struct exchange *x;
	struct timespec now;
	int idx = xsent;
	int cnt = 5;
	double waited;

	if (clock_gettime(CLOCK_REALTIME, &now) < 0) {
		perror("clock_gettime(receive)");
		fatal = 1;
		(void) pthread_kill(master, SIGTERM);
		return;
	}

	while (--cnt >= 0) {
		if (idx < 0)
			return;
		x = xlist + idx;
		idx = x->next;
		if (x->state != X_SENT)
			abort();
		/* check for a timed-out exchange */
		waited = now.tv_sec - x->ts2.tv_sec;
		waited += (now.tv_nsec - x->ts2.tv_nsec) / 1e9;
		if (waited < losttime[1])
			return;
		/* garbage collect timed-out exchange */
		if (pthread_mutex_lock(&mtxsent) != 0) {
			fprintf(stderr, "pthread_mutex_lock(flushrecv)");
			fatal = 1;
			(void) pthread_kill(master, SIGTERM);
			return;
		}
		ISC_REMOVE(xsentl, x);
		if (pthread_mutex_unlock(&mtxsent) != 0) {
			fprintf(stderr, "pthread_mutex_unlock(flushrecv)");
			fatal = 1;
			(void) pthread_kill(master, SIGTERM);
			return;
		}
		(void) close(x->sock);
		x->sock = -1;
		collsent++;
		if (pthread_mutex_lock(&mtxfree) != 0) {
			fprintf(stderr, "pthread_mutex_lock(flushrecv)");
			fatal = 1;
			(void) pthread_kill(master, SIGTERM);
			return;
		}
		x->state = X_FREE;
		ISC_INSERT(xfree, xfreel, x);
		if (pthread_mutex_unlock(&mtxfree) != 0) {
			fprintf(stderr, "pthread_mutex_unlock(flushrecv)");
			fatal = 1;
			(void) pthread_kill(master, SIGTERM);
			return;
		}
	}
}

/*
 * poll receive
 */

void
pollrecv(int topoll)
{
	struct exchange *x;
	int evn, idx;

	for (evn = 0; evn < topoll; evn++) {
		idx = ievents[evn].data.fd;
		x = xlist + idx;
		if (x->state != X_SENT)
			continue;
		if (ievents[evn].events == 0)
			continue;
		if (pthread_mutex_lock(&mtxsent) != 0) {
			fprintf(stderr, "pthread_mutex_lock(pollrecv)");
			fatal = 1;
			(void) pthread_kill(master, SIGTERM);
			return;
		}
		ISC_REMOVE(xsentl, x);
		if (pthread_mutex_unlock(&mtxsent) != 0) {
			fprintf(stderr, "pthread_mutex_unlock(pollrecv)");
			fatal = 1;
			(void) pthread_kill(master, SIGTERM);
			return;
		}
		receiveresp(x);
		ievents[evn].events = 0;
		(void) close(x->sock);
		x->sock = -1;
		if (pthread_mutex_lock(&mtxfree) != 0) {
			fprintf(stderr, "pthread_mutex_lock(pollrecv)");
			fatal = 1;
			(void) pthread_kill(master, SIGTERM);
			return;
		}
		x->state = X_FREE;
		ISC_INSERT(xfree, xfreel, x);
		if (pthread_mutex_unlock(&mtxfree) != 0) {
			fprintf(stderr, "pthread_mutex_unlock(pollrecv)");
			fatal = 1;
			(void) pthread_kill(master, SIGTERM);
			return;
		}
	}
}

/*
 * get the TCP DNS socket descriptor (IPv4)
 */

int
getsock4(void)
{
	int sock;
	int flags;

	errno = 0;
	if (udp)
		sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	else
		sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0)
		return -errno;

	/* make the socket descriptor not blocking */
	flags = fcntl(sock, F_GETFL, 0);
	if (flags == -1) {
		(void) close(sock);
		return -errno;
	}
	if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
		(void) close(sock);
		return -errno;
	}

	/* bind if wanted */
	if (localname != NULL) {
		if (curport) {
			struct sockaddr_in *l4;

			l4 = (struct sockaddr_in *) &localaddr;
			l4->sin_port = htons((uint16_t) curport);
			curport++;
			if (curport > maxport)
				curport = minport;
		}
		if (bind(sock,
			 (struct sockaddr *) &localaddr,
			 sizeof(struct sockaddr_in)) < 0) {
			(void) close(sock);
			return -errno;
		}
	}

	/* connect */
	if (connect(sock,
		    (struct sockaddr *) &serveraddr,
		    sizeof(struct sockaddr_in)) < 0) {
		if (errno != EINPROGRESS) {
			(void) close(sock);
			return -errno;
		}
	}
	return sock;
}

/*
 * connect the TCP DNS QUERY (IPv4)
 */

int
connect4(void)
{
	struct exchange *x;
	int ret;
	int idx;
	struct epoll_event ev;

	ret = clock_gettime(CLOCK_REALTIME, &last);
	if (ret < 0) {
		perror("clock_gettime(connect)");
		fatal = 1;
		(void) pthread_kill(master, SIGTERM);
		return -errno;
	}

	if (xfree >= 0) {
		idx = xfree;
		x = xlist + idx;
		ret = pthread_mutex_lock(&mtxfree);
		if (ret != 0) {
			fprintf(stderr, "pthread_mutex_lock(connect4)");
			fatal = 1;
			(void) pthread_kill(master, SIGTERM);
			return -ret;
		}
		ISC_REMOVE(xfreel, x);
		ret = pthread_mutex_unlock(&mtxfree);
		if (ret != 0) {
			fprintf(stderr, "pthread_mutex_unlock(connect4)");
			fatal = 1;
			(void) pthread_kill(master, SIGTERM);
			return -ret;
		}
	} else if (xused < xlast) {
		idx = xused;
		x = xlist + idx;
		xused++;
	} else
		return -ENOMEM;

	if ((x->state != X_FREE) || (x->sock != -1))
		abort();

	memset(x, 0, sizeof(*x));
	memset(&ev, 0, sizeof(ev));
	x->next = -1;
	x->prev = NULL;
	x->ts0 = last;
	x->sock = getsock4();
	if (x->sock < 0) {
		int result = x->sock;

		x->sock = -1;
		ret = pthread_mutex_lock(&mtxfree);
		if (ret != 0) {
			fprintf(stderr, "pthread_mutex_lock(connect4)");
			fatal = 1;
			(void) pthread_kill(master, SIGTERM);
			return -ret;
		}
		ISC_INSERT(xfree, xfreel, x);
		ret = pthread_mutex_unlock(&mtxfree);
		if (ret != 0) {
			fprintf(stderr, "pthread_mutex_unlock(connect4)");
			fatal = 1;
			(void) pthread_kill(master, SIGTERM);
			return -ret;
		}
		return result;
	}
	ret = pthread_mutex_lock(&mtxconn);
	if (ret != 0) {
		fprintf(stderr, "pthread_mutex_lock(connect4)");
		fatal = 1;
		(void) pthread_kill(master, SIGTERM);
		return -ret;
	}
	x->state = X_CONN;
	ISC_INSERT(xconn, xconnl, x);
	ret = pthread_mutex_unlock(&mtxconn);
	if (ret != 0) {
		fprintf(stderr, "pthread_mutex_unlock(connect4)");
		fatal = 1;
		(void) pthread_kill(master, SIGTERM);
		return -ret;
	}
	ev.events = EPOLLOUT | EPOLLET | EPOLLONESHOT;
	ev.data.fd = idx;
	if (epoll_ctl(epoll_ofd, EPOLL_CTL_ADD, x->sock, &ev) < 0) {
		perror("epoll_ctl(add output)");
		fatal = 1;
		(void) pthread_kill(master, SIGTERM);
		return -errno;
	}
	x->order = xccount++;
	x->id = (uint16_t) random();
#if 0
	if (random_query > 0)
		x->rnd = (uint32_t) random();
#endif
	return idx;
}

/*
 * get the TCP DNS socket descriptor (IPv6)
 */

int
getsock6(void)
{
	int sock;
	int flags;

	errno = 0;
	if (udp)
		sock = socket(PF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	else
		sock = socket(PF_INET6, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0)
		return -errno;

	/* make the socket descriptor not blocking */
	flags = fcntl(sock, F_GETFL, 0);
	if (flags == -1) {
		(void) close(sock);
		return -errno;
	}
	if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
		(void) close(sock);
		return -errno;
	}

	/* bind if wanted */
	if (localname != NULL) {
		if (curport) {
			struct sockaddr_in6 *l6;

			l6 = (struct sockaddr_in6 *) &localaddr;
			l6->sin6_port = htons((uint16_t) curport);
			curport++;
			if (curport > maxport)
				curport = minport;
		}
		if (bind(sock,
			 (struct sockaddr *) &localaddr,
			 sizeof(struct sockaddr_in6)) < 0) {
			(void) close(sock);
			return -errno;
		}
	}

	/* connect */
	if (connect(sock,
		    (struct sockaddr *) &serveraddr,
		    sizeof(struct sockaddr_in6)) < 0) {
		if (errno != EINPROGRESS) {
			(void) close(sock);
			return -errno;
		}
	}
	return sock;
}

/*
 * connect the TCP DNS QUERY (IPv6)
 */

int
connect6(void)
{
	struct exchange *x;
	int ret;
	int idx;
	struct epoll_event ev;

	ret = clock_gettime(CLOCK_REALTIME, &last);
	if (ret < 0) {
		perror("clock_gettime(connect)");
		fatal = 1;
		(void) pthread_kill(master, SIGTERM);
		return -errno;
	}

	if (xfree >= 0) {
		idx = xfree;
		x = xlist + idx;
		ret = pthread_mutex_lock(&mtxfree);
		if (ret != 0) {
			fprintf(stderr, "pthread_mutex_lock(connect6)");
			fatal = 1;
			(void) pthread_kill(master, SIGTERM);
			return -ret;
		}
		ISC_REMOVE(xfreel, x);
		ret = pthread_mutex_unlock(&mtxfree);
		if (ret != 0) {
			fprintf(stderr, "pthread_mutex_unlock(connect6)");
			fatal = 1;
			(void) pthread_kill(master, SIGTERM);
			return -ret;
		}
	} else if (xused < xlast) {
		idx = xused;
		x = xlist + idx;
		xused++;
	} else
		return -ENOMEM;

	memset(x, 0, sizeof(*x));
	memset(&ev, 0, sizeof(ev));
	x->next = -1;
	x->prev = NULL;
	x->ts0 = last;
	x->sock = getsock6();
	if (x->sock < 0) {
		int result = x->sock;

		x->sock = -1;
		ret = pthread_mutex_lock(&mtxfree);
		if (ret != 0) {
			fprintf(stderr, "pthread_mutex_lock(connect6)");
			fatal = 1;
			(void) pthread_kill(master, SIGTERM);
			return -ret;
		}
		ISC_INSERT(xfree, xfreel, x);
		ret = pthread_mutex_unlock(&mtxfree);
		if (ret != 0) {
			fprintf(stderr, "pthread_mutex_unlock(connect6)");
			fatal = 1;
			(void) pthread_kill(master, SIGTERM);
			return -ret;
		}
		return result;
	}
	ret = pthread_mutex_lock(&mtxconn);
	if (ret != 0) {
		fprintf(stderr, "pthread_mutex_lock(connect6)");
		fatal = 1;
		(void) pthread_kill(master, SIGTERM);
		return -ret;
	}
	x->state = X_CONN;
	ISC_INSERT(xconn, xconnl, x);
	ret = pthread_mutex_unlock(&mtxconn);
	if (ret != 0) {
		fprintf(stderr, "pthread_mutex_unlock(connect6)");
		fatal = 1;
		(void) pthread_kill(master, SIGTERM);
		return -ret;
	}
	ev.events = EPOLLOUT | EPOLLET | EPOLLONESHOT;
	ev.data.fd = idx;
	if (epoll_ctl(epoll_ofd, EPOLL_CTL_ADD, x->sock, &ev) < 0) {
		perror("epoll_ctl(add output)");
		fatal = 1;
		(void) pthread_kill(master, SIGTERM);
		return -errno;
	}
	x->order = xccount++;
	x->id = (uint16_t) random();
#if 0
	if (random_query > 0)
		x->rnd = (uint32_t) random();
#endif
	return idx;
}

/*
 * connector working routine
 */

void *
connecting(void *dummy)
{
	struct timespec now, ts;
	int ret;
	int i;
	char name[16];

	dummy = dummy;

	/* set conn-name */
	memset(name, 0, sizeof(name));
	ret = prctl(PR_GET_NAME, name, 0, 0, 0);
	if (ret < 0)
		perror("prctl(PR_GET_NAME)");
	else {
		memmove(name + 5, name, 11);
		memcpy(name, "conn-", 5);
		ret = prctl(PR_SET_NAME, name, 0, 0, 0);
		if (ret < 0)
			perror("prctl(PR_SET_NAME");
	}

	for (;;) {
		if (fatal)
			break;

		loops[1]++;

		/* compute the delay for the next connection */
		if (clock_gettime(CLOCK_REALTIME, &now) < 0) {
			perror("clock_gettime(connecting)");
			fatal = 1;
			(void) pthread_kill(master, SIGTERM);
			break;
		}

		due = last;
		if (rate == 1)
			due.tv_sec += 1;
		else
			due.tv_nsec += 1010000000 / rate;
		while (due.tv_nsec >= 1000000000) {
			due.tv_sec += 1;
			due.tv_nsec -= 1000000000;
		}
		ts = due;
		ts.tv_sec -= now.tv_sec;
		ts.tv_nsec -= now.tv_nsec;
		while (ts.tv_nsec < 0) {
			ts.tv_sec -= 1;
			ts.tv_nsec += 1000000000;
		}
		/* the connection was already due? */
		if (ts.tv_sec < 0) {
			ts.tv_sec = ts.tv_nsec = 0;
			lateconn++;
		} else {
			/* wait until */
			ret = clock_nanosleep(CLOCK_REALTIME, 0, &ts, NULL);
			if (ret < 0) {
				if (errno == EINTR)
					continue;
				perror("clock_nanosleep");
				fatal = 1;
				(void) pthread_kill(master, SIGTERM);
				break;
			}
		}

		/* compute how many connections to open */
		if (clock_gettime(CLOCK_REALTIME, &now) < 0) {
			perror("clock_gettime(connecting)");
			fatal = 1;
			(void) pthread_kill(master, SIGTERM);
			break;
		}

		if ((now.tv_sec > due.tv_sec) ||
		    ((now.tv_sec == due.tv_sec) &&
		     (now.tv_nsec >= due.tv_nsec))) {
			double toconnect;

			toconnect = (now.tv_nsec - due.tv_nsec) / 1e9;
			toconnect += now.tv_sec - due.tv_sec;
			toconnect *= rate;
			toconnect++;
			if (toconnect > (double) aggressiveness)
				i = aggressiveness;
			else
				i = (int) toconnect;
			compconn += i;
			/* open connections */
			while (i-- > 0) {
				if (ipversion == 4)
					ret = connect4();
				else
					ret = connect6();
				if (ret < 0) {
					if ((ret == -EAGAIN) ||
					    (ret == -EWOULDBLOCK) ||
					    (ret == -ENOBUFS) ||
					    (ret == -ENFILE) ||
					    (ret == -EMFILE) ||
					    (ret == -EADDRNOTAVAIL) ||
					    (ret == -ENOMEM))
						locallimit++;
					fprintf(stderr,
						"connect: %s\n",
						strerror(-ret));
					break;
				}
			}
		} else
			/* there was no connection to open */
			shortwait[0]++;
	}

	return NULL;
}

/*
 * sender working routine
 */

void *
sending(void *dummy)
{
	int ret;
	int nfds;
	char name[16];

	dummy = dummy;

	/* set send-name */
	memset(name, 0, sizeof(name));
	ret = prctl(PR_GET_NAME, name, 0, 0, 0);
	if (ret < 0)
		perror("prctl(PR_GET_NAME)");
	else {
		memmove(name + 5, name, 11);
		memcpy(name, "send-", 5);
		ret = prctl(PR_SET_NAME, name, 0, 0, 0);
		if (ret < 0)
			perror("prctl(PR_SET_NAME");
	}

	for (;;) {
		if (fatal)
			break;

		loops[2]++;

		/* epoll_wait() */
		memset(oevents, 0, sizeof(oevents));
		nfds = epoll_wait(epoll_ofd, oevents, EVENTS_CNT, 1);
		if (nfds < 0) {
			if (errno == EINTR)
				continue;
			perror("epoll_wait(output)");
			fatal = 1;
			(void) pthread_kill(master, SIGTERM);
			break;
		}

		/* connection(s) to finish */
		if (nfds == 0)
			shortwait[1]++;
		else
			pollconnect(nfds);
		if (fatal)
			break;
		flushconnect();
		if (fatal)
			break;

		/* packet(s) to send */
		pollsend();
		if (fatal)
			break;
	}

	return NULL;
}

/*
 * receiver working routine
 */

void *
receiving(void *dummy)
{
	int ret;
	int nfds;
	char name[16];

	dummy = dummy;

	/* set recv-name */
	memset(name, 0, sizeof(name));
	ret = prctl(PR_GET_NAME, name, 0, 0, 0);
	if (ret < 0)
		perror("prctl(PR_GET_NAME)");
	else {
		memmove(name + 5, name, 11);
		memcpy(name, "recv-", 5);
		ret = prctl(PR_SET_NAME, name, 0, 0, 0);
		if (ret < 0)
			perror("prctl(PR_SET_NAME");
	}

	for (;;) {
		if (fatal)
			break;

		loops[3]++;

		/* epoll_wait() */
		memset(ievents, 0, sizeof(ievents));
		nfds = epoll_wait(epoll_ifd, ievents, EVENTS_CNT, 1);
		if (nfds < 0) {
			if (errno == EINTR)
				continue;
			perror("epoll_wait(input)");
			fatal = 1;
			(void) pthread_kill(master, SIGTERM);
			break;
		}

		/* packet(s) to receive */
		if (nfds == 0)
			shortwait[2]++;
		else
			pollrecv(nfds);
		if (fatal)
			break;
		flushrecv();
		if (fatal)
			break;
	}

	return NULL;
}

/*
 * get the server socket address from the command line:
 *  - flags: inherited from main, 0 or AI_NUMERICHOST (for literals)
 */

void
getserveraddr(const int flags)
{
	struct addrinfo hints, *res;
	int ret;

	memset(&hints, 0, sizeof(hints));
	if (ipversion == 4)
		hints.ai_family = AF_INET;
	else
		hints.ai_family = AF_INET6;
	if (udp) {
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_protocol = IPPROTO_UDP;
	} else {
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
	}
	hints.ai_flags = AI_ADDRCONFIG | flags;

	ret = getaddrinfo(servername, NULL, &hints, &res);
	if (ret != 0) {
		fprintf(stderr, "bad server=%s: %s\n",
			servername, gai_strerror(ret));
		exit(2);
	}
	if (res->ai_next != NULL) {
		fprintf(stderr, "ambiguous server=%s\n", servername);
		exit(2);
	}
	memcpy(&serveraddr, res->ai_addr, res->ai_addrlen);
	freeaddrinfo(res);
	if (ipversion == 4)
		((struct sockaddr_in *)&serveraddr)->sin_port = htons(port);
	else
		((struct sockaddr_in6 *)&serveraddr)->sin6_port = htons(port);
}

/*
 * get the local socket address from the command line
 */

void
getlocaladdr(void)
{
	struct addrinfo hints, *res;
	int ret;

	memset(&hints, 0, sizeof(hints));
	if (ipversion == 4)
		hints.ai_family = AF_INET;
	else
		hints.ai_family = AF_INET6;
	if (udp) {
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_protocol = IPPROTO_UDP;
	} else {
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
	}
	hints.ai_flags = AI_ADDRCONFIG;

	ret = getaddrinfo(localname, NULL, &hints, &res);
	if (ret != 0) {
		fprintf(stderr,
			"bad -l<local-addr=%s>: %s\n",
			localname,
			gai_strerror(ret));
		exit(2);
	}
	/* refuse multiple addresses */
	if (res->ai_next != NULL) {
		fprintf(stderr,
			"ambiguous -l<local-addr=%s>\n",
			localname);
		exit(2);
	}
	memcpy(&localaddr, res->ai_addr, res->ai_addrlen);
	freeaddrinfo(res);
}

/*
 * intermediate reporting
 * (note: an in-transit packet can be reported as lost)
 */

void
reporting(void)
{
	dreport.tv_sec += report;

	if (xccount != 0) {
		printf("connect: %llu, sent: %llu, received: %llu "
		       "(embryonics: %lld, drops: %lld)",
		       (unsigned long long) xccount,
		       (unsigned long long) xscount,
		       (unsigned long long) xrcount,
		       (long long) (xccount - xscount),
		       (long long) (xscount - xrcount));
		if (xrcount != 0) {
			double avg;

			avg = dsum / xrcount;
			printf(" average: %.3f ms", avg * 1e3);
		}
	}
	printf("\n");
}

/*
 * SIGCHLD handler
 */

void
reapchild(int sig)
{
	int status;

	sig = sig;
	while (wait3(&status, WNOHANG, NULL) > 0)
		/* continue */;
}

/*
 * SIGINT handler
 */

void
interrupt(int sig)
{
	sig = sig;
	interrupted = 1;
}

/*
 * SIGTERM handler
 */

void
terminate(int sig)
{
	sig = sig;
	fatal = 1;
}

/*
 * '-v' handler
 */

void
version(void)
{
	fprintf(stderr, "version 0.01\n");
}

/*
 * usage (from the wiki)
 */

void
usage(void)
{
	fprintf(stderr, "%s",
"perftcpdns [-huvX0] [-4|-6] [-r<rate>] [-t<report>] [-p<test-period>]\n"
"    [-n<num-request>]* [-d<lost-time>]* [-D<max-loss>]* [-T<template-file>]\n"
"    [-l<local-addr>] [-L<local-port>]* [-a<aggressiveness>] [-s<seed>]\n"
"    [-M<memory>] [-x<diagnostic-selector>] [-P<port>] server\n"
"\f\n"
"The server argument is the name/address of the DNS server to contact.\n"
"\n"
"Options:\n"
"-0: Add EDNS0 option with DO flag.\n"
"-4: TCP/IPv4 operation (default). This is incompatible with the -6 option.\n"
"-6: TCP/IPv6 operation. This is incompatible with the -4 option.\n"
"-a<aggressiveness>: When the target sending rate is not yet reached,\n"
"    control how many connections are initiated before the next pause.\n"
"-d<lost-time>: Specify the time after which a connection or a query is\n"
"    treated as having been lost. The value is given in seconds and\n"
"    may contain a fractional component. The default is 1 second.\n"
"-h: Print this help.\n"
"-l<local-addr>: Specify the local hostname/address to use when\n"
"    communicating with the server.\n"
"-L<local-port>: Specify the (minimal and maximal) local port number\n"
"-M<memory>: Size of the tables (default 60000)\n"
"-P<port>: Specify an alternate (i.e., not 53) port\n"
"-r<rate>: Initiate <rate> TCP DNS connections per second.  A periodic\n"
"    report is generated showing the number of exchanges which were not\n"
"    completed, as well as the average response latency.  The program\n"
"    continues until interrupted, at which point a final report is\n"
"    generated.\n"
"-s<seed>: Specify the seed for randomization, making it repeatable.\n"
"-t<report>: Delay in seconds between two periodic reports.\n"
"-T<template-file>: The name of a file containing the template to use\n"
"    as a stream of hexadecimal digits.\n"
"-u: Use UDP in place of TCP.\n"
"-v: Report the version number of this program.\n"
"-X: change default template to get NXDOMAIN responses.\n"
"-x<diagnostic-selector>: Include extended diagnostics in the output.\n"
"    <diagnostic-selector> is a string of single-keywords specifying\n"
"    the operations for which verbose output is desired.  The selector\n"
"    keyletters are:\n"
"   * 'a': print the decoded command line arguments\n"
"   * 'e': print the exit reason\n"
"   * 'i': print rate processing details\n"
"   * 'T': when finished, print templates\n"
"\n"
"Stopping conditions:\n"
"-D<max-loss>: Abort the test if more than <max-loss> connections or\n"
"   queries have been lost.  If <max-loss> includes the suffix '%', it\n"
"   specifies a maximum percentage of losses before stopping.\n"
"   In this case, testing of the threshold begins after 10\n"
"   connections/responses have been expected to be accepted/received.\n"
"-n<num-request>: Initiate <num-request> transactions.  No report is\n"
"    generated until all transactions have been initiated/waited-for,\n"
"    after which a report is generated and the program terminates.\n"
"-p<test-period>: Send requests for the given test period, which is\n"
"    specified in the same manner as -d.  This can be used as an\n"
"    alternative to -n, or both options can be given, in which case the\n"
"    testing is completed when either limit is reached.\n"
"\n"
"Errors:\n"
"- locallimit: reached to local system limits when sending a message.\n"
"- badconn: connection failed (from getsockopt(SO_ERROR))\n"
"- collconn: connect() timed out\n"
"- badsent: send() failed\n"
"- callsent: timed out waiting from a response\n"
"- recverr: recv() system call failed\n"
"- tooshort: received a too short message\n"
"- badid: the id mismatches between the query and the response\n"
"- notresp: doesn't receive a response\n"
"Rate stats:\n"
"- loops: number of thread loop iterations\n"
"- shortwait: no direct activity in a thread iteration\n"
"- compconn: computed number of connect() calls\n"
"- lateconn: connect() already dued when computing delay to the next one\n"
"\n"
"Exit status:\n"
"The exit status is:\n"
"0 on complete success.\n"
"1 for a general error.\n"
"2 if an error is found in the command line arguments.\n"
"3 if there are no general failures in operation, but one or more\n"
"  exchanges are not successfully completed.\n");
}

/*
 * main function / entry point
 */

int
main(const int argc, char * const argv[])
{
	int opt, flags = 0, ret, i;
	long long r;
	char *pc;
	double d;
	extern char *optarg;
	extern int optind;

#define OPTIONS	"hv46u0XM:r:t:R:b:n:p:d:D:l:L:a:s:T:O:x:P:"

	/* decode options */
	while ((opt = getopt(argc, argv, OPTIONS)) != -1)
	switch (opt) {
	case 'h':
		usage();
		exit(0);

	case 'u':
		udp = 1;
		break;

	case 'v':
		version();
		exit(0);

	case '0':
		edns0 = 1;
		break;

	case '4':
		if (ipversion == 6) {
			fprintf(stderr, "IP version already set to 6\n");
			usage();
			exit(2);
		}
		ipversion = 4;
		break;

	case '6':
		if (ipversion == 4) {
			fprintf(stderr, "IP version already set to 4\n");
			usage();
			exit(2);
		}
		ipversion = 6;
		break;

	case 'X':
		ixann = 1;
		break;

	case 'M':
		xlast = atoi(optarg);
		if (xlast <= 1000) {
			fprintf(stderr, "memory must be greater than 1000\n");
			usage();
			exit(2);
		}
		break;

	case 'r':
		rate = atoi(optarg);
		if (rate <= 0) {
			fprintf(stderr, "rate must be a positive integer\n");
			usage();
			exit(2);
		}
		break;

	case 't':
		report = atoi(optarg);
		if (report <= 0) {
			fprintf(stderr, "report must be a positive integer\n");
			usage();
			exit(2);
		}
		break;

	case 'R':
		r = atoll(optarg);
		if (r < 0) {
			fprintf(stderr,
				"range must not be a negative integer\n");
			usage();
			exit(2);
		}
		range = (uint32_t) r;
		if ((range != 0) && (range != UINT32_MAX)) {
			uint32_t s = range + 1;
			uint64_t b = UINT32_MAX + 1, m;

			m = (b / s) * s;
			if (m == b)
				maxrandom = 0;
			else
				maxrandom = (uint32_t) m;
		}
		break;

	case 'b':
		if (basecnt > 1) {
			fprintf(stderr, "too many bases\n");
			usage();
			exit(2);
		}
		base[basecnt] = optarg;
		/* decodebase(); */
		basecnt++;
		break;

	case 'n':
		noreport = 1;
		gotnumreq++;
		if (gotnumreq > 1) {
			fprintf(stderr, "too many num-request's\n");
			usage();
			exit(2);
		}
		numreq[gotnumreq] = atoi(optarg);
		if ((numreq[gotnumreq] < 0) ||
		    ((numreq[gotnumreq] == 0) && (gotnumreq == 1))) {
			fprintf(stderr,
				"num-request must be a positive integer\n");
			usage();
			exit(2);
		}
		break;

	case 'p':
		noreport = 1;
		period = atoi(optarg);
		if (period <= 0) {
			fprintf(stderr,
				"test-period must be a positive integer\n");
			usage();
			exit(2);
		}
		break;

	case 'd':
		gotlosttime++;
		if (gotlosttime > 1) {
			fprintf(stderr, "too many lost-time's\n");
			usage();
			exit(2);
		}
		d = atof(optarg);
		if ((d < 0.) || ((d == 0.) && (gotlosttime == 1))) {
			fprintf(stderr,
				"lost-time must be a positive number\n");
			usage();
			exit(2);
		}
		if (d > 0.)
			losttime[gotlosttime] = d;
		break;

	case 'D':
		noreport = 1;
		gotmaxloss++;
		if (gotmaxloss > 1) {
			fprintf(stderr, "too many max-loss's\n");
			usage();
			exit(2);
		}
		pc = strchr(optarg, '%');
		if (pc != NULL) {
			*pc = '\0';
			maxploss[gotmaxloss] = atof(optarg);
			if ((maxploss[gotmaxloss] < 0) ||
			    (maxploss[gotmaxloss] >= 100)) {
				fprintf(stderr,
					"invalid max-loss percentage\n");
				usage();
				exit(2);
			}
		} else {
			maxloss[gotmaxloss] = atoi(optarg);
			if ((maxloss[gotmaxloss] < 0) ||
			    ((maxloss[gotmaxloss] == 0) &&
			     (gotmaxloss == 1))) {
				fprintf(stderr,
					"max-loss must be a "
					"positive integer\n");
				usage();
				exit(2);
			}
		}
		break;

	case 'l':
		localname = optarg;
		break;

	case 'L':
		i = atoi(optarg);
		if ((i <= 0) || (i >65535)) {
			fprintf(stderr,
				"local-port must be a small positive integer\n");
			usage();
			exit(2);
		}
		if (maxport != 0) {
			fprintf(stderr, "too many local-port's\n");
			usage();
			exit(2);
		}
		if (curport == 0)
			minport = curport = i;
		else
			maxport = i;
		break;

	case 'a':
		aggressiveness = atoi(optarg);
		if (aggressiveness <= 0) {
			fprintf(stderr,
				"aggressiveness must be a positive integer\n");
			usage();
			exit(2);
		}
		break;

	case 's':
		seeded = 1;
		seed = (unsigned int) atol(optarg);
		break;

	case 'T':
		if (templatefile != NULL) {
			fprintf(stderr, "template-file is already set\n");
			usage();
			exit(2);
		}
		templatefile = optarg;
		break;

	case 'O':
		rndoffset = atoi(optarg);
		if (rndoffset < 14) {
			fprintf(stderr,
				"random-offset must be greater than 14\n");
			usage();
			exit(2);
		}
		break;

	case 'x':
		diags = optarg;
		break;

	case 'P':
		i = atoi(optarg);
		if ((i <= 0) || (i > 65535)) {
			fprintf(stderr,
				"port must be a positive short integer\n");
			usage();
			exit(2);
		}
		port = (in_port_t) i;
		break;

	default:
		usage();
		exit(2);
	}

	/* adjust some global variables */
	if (ipversion == 0)
		ipversion = 4;
	if (rate == 0)
		rate = 100;
	if (xlast == 0)
		xlast = 60000;
	if (noreport == 0)
		report = 1;
	if ((curport != 0) && (maxport == 0))
		maxport = 65535;

	/* when required, print the internal view of the command line */
	if ((diags != NULL) && (strchr(diags, 'a') != NULL)) {
		if (udp)
			printf("UDP ");
		printf("IPv%d", ipversion);
		printf(" rate=%d", rate);
		if (edns0 != 0)
			printf(" EDNS0");
		if (report != 0)
			printf(" report=%d", report);
		if (range != 0) {
			if (strchr(diags, 'r') != NULL)
				printf(" range=0..%d [0x%x]",
				       range,
				       (unsigned int) maxrandom);
			else
				printf(" range=0..%d", range);
		}
		if (basecnt != 0)
			for (i = 0; i < basecnt; i++)
				printf(" base[%d]='%s'", i, base[i]);
		if (gotnumreq >= 0) {
			if ((numreq[0] == 0) && (numreq[1] != 0))
				printf(" num-request=*,%d", numreq[1]);
			if ((numreq[0] != 0) && (numreq[1] == 0))
				printf(" num-request=%d,*", numreq[0]);
			if ((numreq[0] != 0) && (numreq[1] != 0))
				printf(" num-request=%d,%d",
				       numreq[0], numreq[1]);
		}
		if (period != 0)
			printf(" test-period=%d", period);
		printf(" lost-time=%g,%g", losttime[0], losttime[1]);
		if (gotmaxloss == 0) {
			if (maxloss[0] != 0)
				printf(" max-loss=%d,*", maxloss[0]);
			if (maxploss[0] != 0.)
				printf(" max-loss=%2.2f%%,*", maxploss[0]);
		} else if (gotmaxloss == 1) {
			if (maxloss[0] != 0)
				printf(" max-loss=%d,", maxloss[0]);
			else if (maxploss[0] != 0.)
				printf(" max-loss=%2.2f%%,", maxploss[0]);
			else
				printf(" max-loss=*,");
			if (maxloss[1] != 0)
				printf("%d", maxloss[1]);
			else if (maxploss[1] != 0.)
				printf("%2.2f%%", maxploss[1]);
			else
				printf("*");
		}
		printf(" aggressiveness=%d", aggressiveness);
		if (seeded)
			printf(" seed=%u", seed);
		if (templatefile != NULL)
			printf(" template-file='%s'", templatefile);
		else if (ixann != 0)
			printf(" Xflag");
		if (rndoffset >= 0)
			printf(" rnd-offset=%d", rndoffset);
		printf(" diagnotic-selectors='%s'", diags);
		printf("\n");
	}

	/* check local address options */
	if ((localname == NULL) && (curport != 0)) {
		fprintf(stderr,
			"-l<local-addr> must be set to use -L<local-port>\n");
		usage();
		exit(2);
	}

	/* check template file options */
	if ((templatefile == NULL) && (rndoffset >= 0)) {
		fprintf(stderr,
			"-T<template-file> must be set to "
			"use -O<random-offset>\n");
		usage();
		exit(2);
	}

	/* check various template file(s) and other condition(s) options */
	if ((templatefile != NULL) && (range > 0) && (rndoffset < 0)) {
		fprintf(stderr,
			"-O<random-offset> must be set when "
			"-T<template-file> and -R<range> are used\n");
		usage();
		exit(2);
	}

	/* get the server argument */
	if (optind < argc - 1) {
		fprintf(stderr, "extra arguments?\n");
		usage();
		exit(2);
	}
	if (optind == argc - 1)
		servername = argv[optind];

	/* handle the local '-l' address/interface */
	if (localname != NULL) {
		/* given */
		getlocaladdr();
		if ((diags != NULL) && (strchr(diags, 'a') != NULL)) {
			printf("local-addr='%s'", localname);
			if (curport != 0)
				printf(" local-port='%d..%d'",
				       minport, maxport);
			printf("\n");
		}
	}

	/* get the server socket address */
	if (servername == NULL) {
		fprintf(stderr, "server is required\n");
		usage();
		exit(2);
	}
	getserveraddr(flags);

	/* finish local/server socket address stuff and print it */
	if ((diags != NULL) && (strchr(diags, 'a') != NULL))
		printf("server='%s'\n", servername);
	if ((localname != NULL) &&
	    (diags != NULL) && (strchr(diags, 'a') != NULL)) {
		char addr[NI_MAXHOST];

		ret = getnameinfo((struct sockaddr *) &localaddr,
				  sizeof(localaddr),
				  addr,
				  NI_MAXHOST,
				  NULL,
				  0,
				  NI_NUMERICHOST);
		if (ret != 0) {
			fprintf(stderr,
				"can't get the local address: %s\n",
				gai_strerror(ret));
			exit(1);
		}
		printf("local address='%s'\n", addr);
	}

	/* initialize exchange structures */
	inits();

	/* get the socket descriptor and template(s) */
	if (templatefile == NULL)
		build_template_query();
	else
		get_template_query();

	/* boot is done! */
	if (clock_gettime(CLOCK_REALTIME, &boot) < 0) {
		perror("clock_gettime(boot)");
		exit(1);
	}

	/* compute the next intermediate reporting date */
	if (report != 0) {
		dreport.tv_sec = boot.tv_sec + report;
		dreport.tv_nsec = boot.tv_nsec;
	}

	/* seed the random generator */
	if (seeded == 0)
		seed = (unsigned int) (boot.tv_sec + boot.tv_nsec);
	srandom(seed);

	/* required only before the interrupted flag check */
	(void) signal(SIGINT, interrupt);
	(void) signal(SIGTERM, terminate);

	/* threads */
	master = pthread_self();
	ret = pthread_create(&connector, NULL, connecting, NULL);
	if (ret != 0) {
		fprintf(stderr, "pthread_create: %s\n", strerror(ret));
		exit(1);
	}
	ret = pthread_create(&sender, NULL, sending, NULL);
	if (ret != 0) {
		fprintf(stderr, "pthread_create: %s\n", strerror(ret));
		exit(1);
	}
	ret = pthread_create(&receiver, NULL, receiving, NULL);
	if (ret != 0) {
		fprintf(stderr, "pthread_create: %s\n", strerror(ret));
		exit(1);
	}

	/* main loop */
	for (;;) {
		struct timespec now, ts;

		/* immediate loop exit conditions */
		if (interrupted) {
			if ((diags != NULL) && (strchr(diags, 'e') != NULL))
				printf("interrupted\n");
			break;
		}
		if (fatal) {
			if ((diags != NULL) && (strchr(diags, 'e') != NULL))
				printf("got a fatal error\n");
			break;
		}

		loops[0]++;

		/* get the date and use it */
		if (clock_gettime(CLOCK_REALTIME, &now) < 0) {
			perror("clock_gettime(now)");
			fatal = 1;
			continue;
		}
		if ((period != 0) &&
		    ((boot.tv_sec + period < now.tv_sec) ||
		     ((boot.tv_sec + period == now.tv_sec) &&
		      (boot.tv_nsec < now.tv_nsec)))) {
			if ((diags != NULL) && (strchr(diags, 'e') != NULL))
				printf("reached test-period\n");
			break;
		}
		if ((report != 0) &&
		    ((dreport.tv_sec < now.tv_sec) ||
		     ((dreport.tv_sec == now.tv_sec) &&
		      (dreport.tv_nsec < now.tv_nsec))))
			reporting();

		/* check receive loop exit conditions */
		if ((numreq[0] != 0) && ((int) xccount >= numreq[0])) {
			if ((diags != NULL) && (strchr(diags, 'e') != NULL))
				printf("reached num-connection\n");
			break;
		}
		if ((numreq[1] != 0) && ((int) xscount >= numreq[1])) {
			if ((diags != NULL) && (strchr(diags, 'e') != NULL))
				printf("reached num-query\n");
			break;
		}
		if ((maxloss[0] != 0) &&
		    ((int) (xccount - xscount) > maxloss[0])) {
			if ((diags != NULL) && (strchr(diags, 'e') != NULL))
				printf("reached max-loss "
				       "(connection/absolute)\n");
			break;
		}
		if ((maxloss[1] != 0) &&
		    ((int) (xscount - xrcount) > maxloss[1])) {
			if ((diags != NULL) && (strchr(diags, 'e') != NULL))
				printf("reached max-loss "
				       "(query/absolute)\n");
			break;
		}
		if ((maxploss[0] != 0.) &&
		    (xccount > 10) &&
		    (((100. * (xccount - xscount)) / xccount) > maxploss[1])) {
			if ((diags != NULL) && (strchr(diags, 'e') != NULL))
				printf("reached max-loss "
				       "(connection/percent)\n");
			break;
		}
		if ((maxploss[1] != 0.) &&
		    (xscount > 10) &&
		    (((100. * (xscount - xrcount)) / xscount) > maxploss[1])) {
			if ((diags != NULL) && (strchr(diags, 'e') != NULL))
				printf("reached max-loss "
				       "(query/percent)\n");
			break;
		}

		/* waiting 1ms */
		memset(&ts, 0, sizeof(ts));
		ts.tv_nsec = 1000000;
		(void) clock_nanosleep(CLOCK_REALTIME, 0, &ts, NULL);
	}

	/* after main loop: finished */
	if (clock_gettime(CLOCK_REALTIME, &finished) < 0)
		perror("clock_gettime(finished)");

	/* threads */
	(void) pthread_cancel(connector);
	(void) pthread_cancel(sender);
	(void) pthread_cancel(receiver);

	/* main statictics */
	printf("connect: %llu, sent: %llu, received: %llu\n",
	       (unsigned long long) xccount,
	       (unsigned long long) xscount,
	       (unsigned long long) xrcount);
	printf("embryonics: %lld (%.1f%%)\n",
	       (long long) (xccount - xscount),
	       (100. * (xccount - xscount)) / xccount);
	printf("drops: %lld (%.1f%%)\n",
	       (long long) (xscount - xrcount),
	       (100. * (xscount - xrcount)) / xscount);
	printf("total losses: %lld (%.1f%%)\n",
	       (long long) (xccount - xrcount),
	       (100. * (xccount - xrcount)) / xccount);
	printf("local limits: %llu, bad connects: %llu, "
	       "connect timeouts: %llu\n",
	       (unsigned long long) locallimit,
	       (unsigned long long) badconn,
	       (unsigned long long) collconn);
	printf("bad sends: %llu, bad recvs: %llu, recv timeouts: %llu\n",
	       (unsigned long long) badsent,
	       (unsigned long long) recverr,
	       (unsigned long long) collsent);
	printf("too shorts: %llu, bad IDs: %llu, not responses: %llu\n",
	       (unsigned long long) tooshort,
	       (unsigned long long) badid,
	       (unsigned long long) notresp);
	printf("rcode counters:\n noerror: %llu, formerr: %llu, "
	       "servfail: %llu\n "
	       "nxdomain: %llu, noimp: %llu, refused: %llu, others: %llu\n",
	       (unsigned long long) rcodes[NS_RCODE_NOERROR],
	       (unsigned long long) rcodes[NS_RCODE_FORMERR],
	       (unsigned long long) rcodes[NS_RCODE_SERVFAIL],
	       (unsigned long long) rcodes[NS_RCODE_NXDOMAIN],
	       (unsigned long long) rcodes[NS_RCODE_NOIMP],
	       (unsigned long long) rcodes[NS_RCODE_REFUSED],
	       (unsigned long long) rcodes[NS_RCODE_LAST]);

	/* print the rates */
	if (finished.tv_sec != 0) {
		double dall, erate[3];

		dall = (finished.tv_nsec - boot.tv_nsec) / 1e9;
		dall += finished.tv_sec - boot.tv_sec;
		erate[0] = xccount / dall;
		erate[1] = xscount / dall;
		erate[2] = xrcount / dall;
		printf("rates: %.0f,%.0f,%.0f (target %d)\n",
		       erate[0], erate[1], erate[2], rate);
	}

	/* rate processing instrumentation */
	if ((diags != NULL) && (strchr(diags, 'i') != NULL)) {
		printf("loops: %llu,%llu,%llu,%llu\n",
		       (unsigned long long) loops[0],
		       (unsigned long long) loops[1],
		       (unsigned long long) loops[2],
		       (unsigned long long) loops[3]);
		printf("shortwait: %llu,%llu,%llu\n",
		       (unsigned long long) shortwait[0],
		       (unsigned long long) shortwait[1],
		       (unsigned long long) shortwait[2]);
		printf("compconn: %llu, lateconn: %llu\n",
		       (unsigned long long) compconn,
		       (unsigned long long) lateconn);
		printf("badconn: %llu, collconn: %llu, "
		       "recverr: %llu, collsent: %llu\n",
		       (unsigned long long) badconn,
		       (unsigned long long) collconn,
		       (unsigned long long) recverr,
		       (unsigned long long) collsent);
		printf("memory: used(%d) / allocated(%d)\n",
		       xused, xlast);
	}

	/* round-time trip statistics */
	if (xrcount != 0) {
		double avg, stddev;

		avg = dsum / xrcount;
		stddev = sqrt(dsumsq / xrcount - avg * avg);
		printf("RTT: min/avg/max/stddev:  %.3f/%.3f/%.3f/%.3f ms\n",
		       dmin * 1e3, avg * 1e3, dmax * 1e3, stddev * 1e3);
	}
	printf("\n");

	/* template(s) */
	if ((diags != NULL) && (strchr(diags, 'T') != NULL)) {
		size_t n;

		printf("length = 0x%zx\n", length_query);
		if (random_query > 0)
			printf("random offset = %zu\n", random_query);
		printf("content:\n");
		for (n = 0; n < length_query; n++) {
			printf("%s%02hhx",
			       (n & 15) == 0 ? "" : " ",
			       template_query[n]);
			if ((n & 15) == 15)
				printf("\n");
		}
		if ((n & 15) != 15)
			printf("\n");
		printf("\n");
	}

	/* compute the exit code (and exit) */
	if (fatal)
		exit(1);
	else if ((xccount == xscount) && (xscount == xrcount))
		exit(0);
	else
		exit(3);
}
