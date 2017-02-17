/*	$NetBSD: syslogd.h,v 1.7 2015/09/08 18:33:12 plunky Exp $	*/

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Martin Schütte.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef SYSLOGD_H_
#define SYSLOGD_H_
/*
 * hold common data structures and prototypes
 * for syslogd.c and tls.c
 *
 */

#include <sys/cdefs.h>
#define MAXLINE		1024		/* maximum line length */
#define MAXSVLINE	120		/* maximum saved line length */
#define DEFUPRI		(LOG_USER|LOG_NOTICE)
#define DEFSPRI		(LOG_KERN|LOG_NOTICE)
#define TIMERINTVL	30		/* interval for checking flush, mark */
#define TTYMSGTIME	1		/* timeout passed to ttymsg */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/queue.h>
#include <netinet/in.h>
#include <sys/event.h>
#include <event.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <locale.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <utmp.h>
#ifdef __NetBSD_Version__
#include <util.h>
#include "utmpentry.h"
#endif /* __NetBSD_Version__ */
#ifdef __FreeBSD_version
#include <libutil.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <limits.h>
#endif /* __FreeBSD_version */

#ifndef DISABLE_TLS
#include <netinet/tcp.h>
#include <openssl/ssl.h>
#endif /* !DISABLE_TLS */

#include <sys/stdint.h>
#include <sys/resource.h>

#include "pathnames.h"
#include <sys/syslog.h>

/* some differences between the BSDs  */
#ifdef __FreeBSD_version
#undef _PATH_UNIX
#define _PATH_UNIX "kernel"
#define HAVE_STRNDUP 0
#endif /* __FreeBSD_version */

#ifdef __NetBSD_Version__
#define HAVE_STRNDUP 1
#define HAVE_DEHUMANIZE_NUMBER 1
#endif /* __NetBSD_Version__ */

#if defined(__minix)
#undef _PATH_UNIX
#define _PATH_UNIX "kernel"
#endif /* defined(__minix) */

#ifndef HAVE_DEHUMANIZE_NUMBER	/* not in my 4.0-STABLE yet */
extern int dehumanize_number(const char *str, int64_t *size);
#endif /* !HAVE_DEHUMANIZE_NUMBER */

#if !HAVE_STRNDUP
char *strndup(const char *str, size_t n);
#endif /* !HAVE_STRNDUP */

#ifdef LIBWRAP
#include <tcpd.h>
#endif

#define FDMASK(fd)	(1 << (fd))

#define A_CNT(x)	(sizeof((x)) / sizeof((x)[0]))

/* debug messages with categories */
#define D_NONE	   0
#define D_CALL	   1	/* function calls */
#define D_DATA	   2	/* syslog message reading/formatting */
#define D_NET	   4	/* sockets/network */
#define D_FILE	   8	/* local files */
#define D_TLS	  16	/* TLS */
#define D_PARSE	  32	/* configuration/parsing */
#define D_EVENT	  64	/* libevent */
#define D_BUFFER 128	/* message queues */
#define D_MEM	 256	/* malloc/free */
#define D_MEM2	1024	/* every single malloc/free */
#define D_SIGN	2048	/* -sign */
#define D_MISC	4096	/* everything else */
#define D_ALL	(D_CALL | D_DATA | D_NET | D_FILE | D_TLS | D_PARSE |  \
		 D_EVENT | D_BUFFER | D_MEM | D_MEM2 | D_SIGN | D_MISC)
#define D_DEFAULT (D_CALL | D_NET | D_FILE | D_TLS | D_MISC)


/* build with -DNDEBUG to remove all assert()s and DPRINTF()s */
#ifdef NDEBUG
#define DPRINTF(x, ...) (void)0
#else
void dbprintf(const char *, const char *, size_t, const char *, ...)
    __printflike(4, 5);
#define DPRINTF(x, ...) /*LINTED null effect */(void)(Debug & (x) \
    ? dbprintf(__FILE__, __func__, __LINE__, __VA_ARGS__) : ((void)0))
#endif

/* shortcuts for libevent */
#define EVENT_ADD(x) do {						\
	DPRINTF(D_EVENT, "event_add(%s@%p)\n", #x, x);			\
	if (event_add(x, NULL) == -1) {					\
		DPRINTF(D_EVENT, "Failure in event_add()\n");		\
	}								\
} while (/*CONSTCOND*/0)
#define RETRYEVENT_ADD(x) do {						\
	struct timeval _tv;						\
	_tv.tv_sec = 0;							\
	_tv.tv_usec = TLS_RETRY_EVENT_USEC;				\
	DPRINTF(D_EVENT, "retryevent_add(%s@%p)\n", #x, x);		\
	if (event_add(x, &_tv) == -1) {					\
		DPRINTF(D_EVENT, "Failure in event_add()\n");		\
	}								\
} while (/*CONSTCOND*/0)
#define DEL_EVENT(x) do {						\
	DPRINTF(D_MEM2, "DEL_EVENT(%s@%p)\n", #x, x);			\
	if ((x) && (event_del(x) == -1)) {				\
		DPRINTF(D_EVENT, "Failure in event_del()\n");		\
	}								\
} while (/*CONSTCOND*/0)

/* safe calls to free() */
#define FREEPTR(x)	if (x) {					\
		DPRINTF(D_MEM2, "free(%s@%p)\n", #x, x);		\
		free(x);	 x = NULL; }
#define FREE_SSL(x)	if (x) {					\
		DPRINTF(D_MEM2, "SSL_free(%s@%p)\n", #x, x);		\
		SSL_free(x);	 x = NULL; }
#define FREE_SSL_CTX(x) if (x) {					\
		DPRINTF(D_MEM2, "SSL_CTX_free(%s@%p)\n", #x, x);	\
		SSL_CTX_free(x); x = NULL; }

/* reference counting macros for buffers */
#define NEWREF(x) ((x) ? (DPRINTF(D_BUFFER, "inc refcount of " #x \
			" @ %p: %zu --> %zu\n", (x), (x)->refcount, \
			(x)->refcount + 1), (x)->refcount++, (x))\
		       : (DPRINTF(D_BUFFER, "inc refcount of NULL!\n"), NULL))
#define DELREF(x) /*LINTED null effect*/(void)((x) ? (DPRINTF(D_BUFFER, "dec refcount of " #x \
			" @ %p: %zu --> %zu\n", (x), (x)->refcount, \
			(x)->refcount - 1), buf_msg_free(x), NULL) \
		       : (DPRINTF(D_BUFFER, "dec refcount of NULL!\n"), NULL))

/* assumption:
 * - malloc()/calloc() only fails if not enough memory available
 * - once init() has set up all global variables etc.
 *   the bulk of available memory is used for buffers
 *   and can be freed if necessary
 */
#define MALLOC(ptr, size) do {						\
	while(!(ptr = malloc(size))) {					\
		DPRINTF(D_MEM, "Unable to allocate memory");		\
		message_allqueues_purge();				\
	}								\
	DPRINTF(D_MEM2, "MALLOC(%s@%p, %zu)\n", #ptr, ptr, size);	\
} while (/*CONSTCOND*/0)

#define CALLOC(ptr, size) do {						\
	while(!(ptr = calloc(1, size))) {				\
		DPRINTF(D_MEM, "Unable to allocate memory");		\
		message_allqueues_purge();				\
	}								\
	DPRINTF(D_MEM2, "CALLOC(%s@%p, %zu)\n", #ptr, ptr, size);	\
} while (/*CONSTCOND*/0)

/* define strlen(NULL) to be 0 */
#define SAFEstrlen(x) ((x) ? strlen(x) : 0)

/* shorthand to block/restore signals for the duration of one function */
#define BLOCK_SIGNALS(omask, newmask) do {				\
	sigemptyset(&newmask);						\
	sigaddset(&newmask, SIGHUP);					\
	sigaddset(&newmask, SIGALRM);					\
	sigprocmask(SIG_BLOCK, &newmask, &omask);			\
} while (/*CONSTCOND*/0)

#define RESTORE_SIGNALS(omask) sigprocmask(SIG_SETMASK, &omask, NULL)

/* small optimization to call send_queue() only if queue has elements */
#define SEND_QUEUE(f) do {						\
	if ((f)->f_qelements)						\
		send_queue(0, 0, f);	      				\
} while (/*CONSTCOND*/0)

#define MAXUNAMES		20	/* maximum number of user names */
#define BSD_TIMESTAMPLEN	14+1
#define MAX_TIMESTAMPLEN	31+1

/* maximum field lengths in syslog-protocol */
#define PRI_MAX	      5
#define HOST_MAX    255
#define APPNAME_MAX  48
#define PROCID_MAX  128
#define MSGID_MAX    32
/* longest possible header length */
#define HEADER_LEN_MAX (PRI_MAX + 1 + 1 + MAX_TIMESTAMPLEN + 1 + HOST_MAX \
			+ 1 + APPNAME_MAX + 1 + PROCID_MAX + 1 + MSGID_MAX)

/* allowed number of priorities by IETF standards */
#define IETF_NUM_PRIVALUES  192

/* check if message with fac/sev belogs to a destination f */
#define MATCH_PRI(f, fac, sev) \
	   (  (((f)->f_pcmp[fac] & PRI_EQ) && ((f)->f_pmask[fac] == (sev))) \
	    ||(((f)->f_pcmp[fac] & PRI_LT) && ((f)->f_pmask[fac]  < (sev)))  \
	    ||(((f)->f_pcmp[fac] & PRI_GT) && ((f)->f_pmask[fac]  > (sev)))  \
	   )

/* shorthand to test Byte Order Mark which indicates UTF-8 content */
#define IS_BOM(p) ( \
    (p)[0] != '\0' && (unsigned char)(p)[0] == (unsigned char)0xEF && \
    (p)[1] != '\0' && (unsigned char)(p)[1] == (unsigned char)0xBB && \
    (p)[2] != '\0' && (unsigned char)(p)[2] == (unsigned char)0xBF)

/* message buffer container used for processing, formatting, and queueing */
struct buf_msg {
	size_t	 refcount;
	int	 pri;
	int	 flags;
	char	*timestamp;
	char	*recvhost;
	char	*host;
	char	*prog;
	char	*pid;
	char	*msgid;
	char	*sd;	    /* structured data */
	char	*msg;	    /* message content */
	char	*msgorig;   /* in case we advance *msg beyond header fields
			       we still want to free() the original ptr  */
	size_t	 msglen;    /* strlen(msg) */
	size_t	 msgsize;   /* allocated memory size   */
	size_t	 tlsprefixlen; /* bytes for the TLS length prefix */
	size_t	 prilen;       /* bytes for priority and version  */
};

/* queue of messages */
struct buf_queue {
	struct buf_msg* msg;
	STAILQ_ENTRY(buf_queue) entries;
};
STAILQ_HEAD(buf_queue_head, buf_queue);

/* a pair of a socket and an associated event object */
struct socketEvent {
	int fd;
	int af;
	struct event *ev;
};

/*
 * Flags to logmsg().
 */
#define IGN_CONS	0x001	/* don't print on console */
#define SYNC_FILE	0x002	/* do fsync on file after printing */
#define ADDDATE		0x004	/* add a date to the message */
#define MARK		0x008	/* this message is a mark */
#define ISKERNEL	0x010	/* kernel generated message */
#define BSDSYSLOG	0x020	/* line in traditional BSD Syslog format */
#define SIGN_MSG	0x040	/* syslog-sign data, not signed again */

/* strategies for message_queue_purge() */
#define PURGE_OLDEST		1
#define PURGE_BY_PRIORITY	2

/*
 * This structure represents the files that will have log
 * copies printed.
 * We require f_file to be valid if f_type is F_FILE, F_CONSOLE, F_TTY,
 * or if f_type is F_PIPE and f_pid > 0.
 */

struct filed {
	struct	filed *f_next;		/* next in linked list */
	short	f_type;			/* entry type, see below */
	short	f_file;			/* file descriptor */
	time_t	f_time;			/* time this was last written */
	char	*f_host;		/* host from which to record */
	u_char	f_pmask[LOG_NFACILITIES+1];	/* priority mask */
	u_char	f_pcmp[LOG_NFACILITIES+1];	/* compare priority */
#define PRI_LT	0x1
#define PRI_EQ	0x2
#define PRI_GT	0x4
	char	*f_program;		/* program this applies to */
	union {
		char	f_uname[MAXUNAMES][UT_NAMESIZE+1];
		struct {
			char	f_hname[MAXHOSTNAMELEN];
			struct	addrinfo *f_addr;
		} f_forw;		/* UDP forwarding address */
#ifndef DISABLE_TLS
		struct {
			SSL	*ssl;			/* SSL object  */
			struct tls_conn_settings *tls_conn;  /* certificate info */
		} f_tls;		/* TLS forwarding address */
#endif /* !DISABLE_TLS */
		char	f_fname[MAXPATHLEN];
		struct {
			char	f_pname[MAXPATHLEN];
			pid_t	f_pid;
		} f_pipe;
	} f_un;
#ifndef DISABLE_SIGN
	struct signature_group_t *f_sg;	     /* one signature group */
#endif /* !DISABLE_SIGN */
	struct buf_queue_head f_qhead;	     /* undelivered msgs queue */
	size_t	      	      f_qelements;   /* elements in queue */
	size_t		      f_qsize;	     /* size of queue in bytes */
	struct buf_msg	     *f_prevmsg;     /* last message logged */
	struct event	     *f_sq_event;    /* timer for send_queue() */
	int		      f_prevcount;   /* repetition cnt of prevmsg */
	int		      f_repeatcount; /* number of "repeated" msgs */
	int		      f_lasterror;   /* last error on writev() */
	int		      f_flags;	     /* file-specific flags */
#define FFLAG_SYNC	0x01	/* for F_FILE: fsync after every msg */
#define FFLAG_FULL	0x02	/* for F_FILE | F_PIPE: write PRI header */
#define FFLAG_SIGN	0x04	/* for syslog-sign with SG="3":
				 * sign the messages to this destination */
};

#ifndef DISABLE_TLS

/* linked list for allowed TLS peer credentials
 * (one for fingerprint, one for cert-files)
 */
SLIST_HEAD(peer_cred_head, peer_cred);
struct peer_cred {
	SLIST_ENTRY(peer_cred) entries;
	char *data;
};

/* config options for TLS server-side */
struct tls_global_options_t {
	SSL_CTX *global_TLS_CTX;
	struct peer_cred_head fprint_head;  /* trusted client fingerprints */
	struct peer_cred_head cert_head;    /* trusted client cert files   */
	char *keyfile;	    /* file with private key	 */
	char *certfile;	    /* file with own certificate */
	char *CAfile;	    /* file with CA certificate	 */
	char *CAdir;	    /* alternative: path to directory with CA certs */
	char *x509verify;   /* level of peer verification */
	char *bindhost;	    /* hostname/IP to bind to	  */
	char *bindport;	    /* port/service to bind to	  */
	char *server;	    /* if !NULL: do not listen to incoming TLS	  */
	char *gen_cert;	    /* if !NULL: generate self-signed certificate */
};

/* TLS needs three sets of sockets:
 * - listening sockets: a fixed size array TLS_Listen_Set, just like finet for UDP.
 * - outgoing connections: managed as part of struct filed.
 * - incoming connections: variable sized, thus a linked list TLS_Incoming.
 */
/* every connection has its own input buffer with status
 * variables for message reading */
SLIST_HEAD(TLS_Incoming, TLS_Incoming_Conn);

struct TLS_Incoming_Conn {
	SLIST_ENTRY(TLS_Incoming_Conn) entries;
	struct tls_conn_settings *tls_conn;
	int socket;
	char *inbuf;		    /* input buffer */
	size_t inbuflen;
	size_t cur_msg_len;	    /* length of current msg */
	size_t cur_msg_start;	    /* beginning of current msg */
	size_t read_pos;	    /* ring buffer position to write to */
	size_t errorcount;	    /* to close faulty connections */
	bool closenow;		    /* close connection as soon as buffer processed */
	bool dontsave;		    /* for receiving oversized messages w/o saving them */
};

#endif /* !DISABLE_TLS */

#endif /*SYSLOGD_H_*/
