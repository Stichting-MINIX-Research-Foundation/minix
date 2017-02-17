/*	$NetBSD: tcpd.h,v 1.14 2012/03/22 22:59:43 joerg Exp $	*/
 /*
  * @(#) tcpd.h 1.5 96/03/19 16:22:24
  * 
  * Author: Wietse Venema, Eindhoven University of Technology, The Netherlands.
  */

#include <sys/cdefs.h>
#include <stdio.h>

/* Structure to describe one communications endpoint. */

#define STRING_LENGTH	128		/* hosts, users, processes */

struct host_info {
    char    name[STRING_LENGTH];	/* access via eval_hostname(host) */
    char    addr[STRING_LENGTH];	/* access via eval_hostaddr(host) */
    struct sockaddr *sin;		/* socket address or 0 */
    struct t_unitdata *unit;		/* TLI transport address or 0 */
    struct request_info *request;	/* for shared information */
};

/* Structure to describe what we know about a service request. */

struct request_info {
    int     fd;				/* socket handle */
    char    user[STRING_LENGTH];	/* access via eval_user(request) */
    char    daemon[STRING_LENGTH];	/* access via eval_daemon(request) */
    char    pid[10];			/* access via eval_pid(request) */
    struct host_info client[1];		/* client endpoint info */
    struct host_info server[1];		/* server endpoint info */
    void (*sink)(int);			/* datagram sink function or 0 */
    void (*hostname)(struct host_info *); /* address to printable hostname */
    void (*hostaddr)(struct host_info *); /* address to printable address */
    void (*cleanup)(void);		/* cleanup function or 0 */
    struct netconfig *config;		/* netdir handle */
};

/* Common string operations. Less clutter should be more readable. */

#define STRN_CPY(d,s,l)	{ strncpy((d),(s),(l)); (d)[(l)-1] = 0; }

#define STRN_EQ(x,y,l)	(strncasecmp((x),(y),(l)) == 0)
#define STRN_NE(x,y,l)	(strncasecmp((x),(y),(l)) != 0)
#define STR_EQ(x,y)	(strcasecmp((x),(y)) == 0)
#define STR_NE(x,y)	(strcasecmp((x),(y)) != 0)

 /*
  * Initially, all above strings have the empty value. Information that
  * cannot be determined at runtime is set to "unknown", so that we can
  * distinguish between `unavailable' and `not yet looked up'. A hostname
  * that we do not believe in is set to "paranoid".
  */

#define STRING_UNKNOWN	"unknown"	/* lookup failed */
#define STRING_PARANOID	"paranoid"	/* hostname conflict */

__BEGIN_DECLS
extern char unknown[];
extern char paranoid[];
__END_DECLS

#define HOSTNAME_KNOWN(s) (STR_NE((s),unknown) && STR_NE((s),paranoid))

#define NOT_INADDR(s) (s[strspn(s,"01234567890./")] != 0)

/* Global functions. */

__BEGIN_DECLS
#define fromhost sock_host		/* no TLI support needed */

extern int hosts_access			/* access control */
		(struct request_info *);
extern int hosts_ctl			/* limited interface to hosts_access */
		(char *, char *, char *, char *);
extern void shell_cmd			/* execute shell command */
		(char *);
extern char *percent_x			/* do %<char> expansion */
		(char *, int, char *, struct request_info *);
extern void rfc931			/* client name from RFC 931 daemon */
		(struct sockaddr *, struct sockaddr *, char *);
__dead extern void clean_exit		/* clean up and exit */
		(struct request_info *);
__dead extern void refuse		/* clean up and exit */
		(struct request_info *);
extern char *xgets			/* fgets() on steroids */
		(char *, int, FILE *);
extern char *split_at			/* strchr() and split */
		(char *, int);
extern int dot_quad_addr	/* restricted inet_aton() */
		(char *, unsigned long *);

/* Global variables. */

extern int allow_severity;		/* for connection logging */
extern int deny_severity;		/* for connection logging */
extern const char *hosts_allow_table;	/* for verification mode redirection */
extern const char *hosts_deny_table;	/* for verification mode redirection */
extern int hosts_access_verbose;	/* for verbose matching mode */
extern int rfc931_timeout;		/* user lookup timeout */
extern int resident;			/* > 0 if resident process */

 /*
  * Routines for controlled initialization and update of request structure
  * attributes. Each attribute has its own key.
  */

extern struct request_info *request_init	/* initialize request */
		(struct request_info *,...);
extern struct request_info *request_set		/* update request structure */
		(struct request_info *,...);

#define RQ_FILE		1		/* file descriptor */
#define RQ_DAEMON	2		/* server process (argv[0]) */
#define RQ_USER		3		/* client user name */
#define RQ_CLIENT_NAME	4		/* client host name */
#define RQ_CLIENT_ADDR	5		/* client host address */
#define RQ_CLIENT_SIN	6		/* client endpoint (internal) */
#define RQ_SERVER_NAME	7		/* server host name */
#define RQ_SERVER_ADDR	8		/* server host address */
#define RQ_SERVER_SIN	9		/* server endpoint (internal) */

 /*
  * Routines for delayed evaluation of request attributes. Each attribute
  * type has its own access method. The trivial ones are implemented by
  * macros. The other ones are wrappers around the transport-specific host
  * name, address, and client user lookup methods. The request_info and
  * host_info structures serve as caches for the lookup results.
  */

extern char *eval_user			/* client user */
		(struct request_info *);
extern char *eval_hostname		/* printable hostname */
		(struct host_info *);
extern char *eval_hostaddr		/* printable host address */
		(struct host_info *);
extern char *eval_hostinfo		/* host name or address */
		(struct host_info *);
extern char *eval_client		/* whatever is available */
		(struct request_info *);
extern char *eval_server		/* whatever is available */
		(struct request_info *);
#define eval_daemon(r)	((r)->daemon)	/* daemon process name */
#define eval_pid(r)	((r)->pid)	/* process id */

/* Socket-specific methods, including DNS hostname lookups. */

extern void sock_host			/* look up endpoint addresses */
		(struct request_info *);
extern void sock_hostname		/* translate address to hostname */
		(struct host_info *);
extern void sock_hostaddr		/* address to printable address */
		(struct host_info *);
#define sock_methods(r) \
	{ (r)->hostname = sock_hostname; (r)->hostaddr = sock_hostaddr; }

/* The System V Transport-Level Interface (TLI) interface. */

#if defined(TLI) || defined(PTX) || defined(TLI_SEQUENT)
extern void tli_host			/* look up endpoint addresses etc. */
		(struct request_info *);
#endif

 /*
  * Problem reporting interface. Additional file/line context is reported
  * when available. The jump buffer (tcpd_buf) is not declared here, or
  * everyone would have to include <setjmp.h>.
  */

/* Report problem and proceed */
void tcpd_warn(const char *, ...) __printflike(1, 2);

/* Report problem and jump */
void tcpd_jump(const char *, ...) __dead __printflike(1, 2);
__END_DECLS

struct tcpd_context {
    const char *file;			/* current file */
    int     line;			/* current line */
};
__BEGIN_DECLS
extern struct tcpd_context tcpd_context;
__END_DECLS

 /*
  * While processing access control rules, error conditions are handled by
  * jumping back into the hosts_access() routine. This is cleaner than
  * checking the return value of each and every silly little function. The
  * (-1) returns are here because zero is already taken by longjmp().
  */

#define AC_PERMIT	1		/* permit access */
#define AC_DENY		(-1)		/* deny_access */
#define AC_ERROR	AC_DENY		/* XXX */

 /*
  * In verification mode an option function should just say what it would do,
  * instead of really doing it. An option function that would not return
  * should clear the dry_run flag to inform the caller of this unusual
  * behavior.
  */

__BEGIN_DECLS
extern void process_options		/* execute options */
		(char *, struct request_info *);
extern int dry_run;			/* verification flag */
extern void fix_options			/* get rid of IP-level socket options */
		(struct request_info *);
__END_DECLS
