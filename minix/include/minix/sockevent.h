#ifndef _MINIX_SOCKEVENT_H
#define _MINIX_SOCKEVENT_H

#include <minix/sockdriver.h>

/* Socket events. */
#define SEV_BIND	0x01	/* a pending bind operation has ended */
#define SEV_CONNECT	0x02	/* a pending connect operation has ended */
#define SEV_ACCEPT	0x04	/* pending accept operations may be resumed */
#define SEV_SEND	0x08	/* pending send operations may be resumed */
#define SEV_RECV	0x10	/* pending receive operations may be resumed */
#define SEV_CLOSE	0x20	/* a pending close operation has ended */

/* Socket flags. */
#define SFL_SHUT_RD	0x01	/* socket has been shut down for reading */
#define SFL_SHUT_WR	0x02	/* socket has been shut down for writing */
#define SFL_CLOSING	0x04	/* socket close operation in progress */
#define SFL_CLONED	0x08	/* socket has been cloned but not accepted */
#define SFL_TIMER	0x10	/* socket is on list of timers */

/*
 * Special return value from sop_recv callback functions.  This pseudo-value
 * is used to differentiate between zero-sized packets and actual EOF.
 */
#define SOCKEVENT_EOF	1

struct sockevent_ops;
struct sockevent_proc;

/* Socket structure.  None of its fields must ever be accessed directly. */
struct sock {
	sockid_t sock_id;		/* socket identifier */
	unsigned char sock_events;	/* pending events (SEV_) */
	unsigned char sock_flags;	/* internal flags (SFL_) */
	unsigned char sock_domain;	/* domain, address family (PF_, AF_) */
	int sock_type;			/* type: stream, datagram.. (SOCK_) */
	int sock_err;			/* pending error code < 0, 0 if none */
	unsigned int sock_opt;		/* generic option flags (SO_) */
	clock_t sock_linger;		/* SO_LINGER value, in ticks or time */
	clock_t sock_stimeo;		/* SO_SNDTIMEO value, in clock ticks */
	clock_t sock_rtimeo;		/* SO_RCVTIMEO value, in clock ticks */
	size_t sock_slowat;		/* SO_SNDLOWAT value, in bytes */
	size_t sock_rlowat;		/* SO_RCVLOWAT value, in bytes */
	const struct sockevent_ops *sock_ops;	/* socket operations table */
	SIMPLEQ_ENTRY(sock) sock_next;		/* list for pending events */
	SLIST_ENTRY(sock) sock_hash;		/* list for hash table */
	SLIST_ENTRY(sock) sock_timer;		/* list of socks with timers */
	struct sockevent_proc *sock_proc;	/* list of suspended calls */
	struct sockdriver_select sock_select;	/* pending select query */
	unsigned int sock_selops;	/* pending select operations, or 0 */
};

/* Socket operations table. */
struct sockevent_ops {
	int (* sop_pair)(struct sock * sock1, struct sock * sock2,
	    endpoint_t user_endpt);
	int (* sop_bind)(struct sock * sock, const struct sockaddr * addr,
	    socklen_t addr_len, endpoint_t user_endpt);
	int (* sop_connect)(struct sock * sock, const struct sockaddr * addr,
	    socklen_t addr_len, endpoint_t user_endpt);
	int (* sop_listen)(struct sock * sock, int backlog);
	sockid_t (* sop_accept)(struct sock * sock, struct sockaddr * addr,
	    socklen_t * addr_len, endpoint_t user_endpt,
	    struct sock ** newsockp);
	int (* sop_test_accept)(struct sock * sock);
	int (* sop_pre_send)(struct sock * sock, size_t len, socklen_t ctl_len,
	    const struct sockaddr * addr, socklen_t addr_len,
	    endpoint_t user_endpt, int flags);
	int (* sop_send)(struct sock * sock,
	    const struct sockdriver_data * data, size_t len, size_t * off,
	    const struct sockdriver_data * ctl, socklen_t ctl_len,
	    socklen_t * ctl_off, const struct sockaddr * addr,
	    socklen_t addr_len, endpoint_t user_endpt, int flags, size_t min);
	int (* sop_test_send)(struct sock * sock, size_t min);
	int (* sop_pre_recv)(struct sock * sock, endpoint_t user_endpt,
	    int flags);
	int (* sop_recv)(struct sock * sock,
	    const struct sockdriver_data * data, size_t len, size_t * off,
	    const struct sockdriver_data * ctl, socklen_t ctl_len,
	    socklen_t * ctl_off, struct sockaddr * addr, socklen_t * addr_len,
	    endpoint_t user_endpt, int flags, size_t min, int * rflags);
	int (* sop_test_recv)(struct sock * sock, size_t min, size_t * size);
	int (* sop_ioctl)(struct sock * sock, unsigned long request,
	    const struct sockdriver_data * data, endpoint_t user_endpt);
	void (* sop_setsockmask)(struct sock * sock, unsigned int mask);
	int (* sop_setsockopt)(struct sock * sock, int level, int name,
	    const struct sockdriver_data * data, socklen_t len);
	int (* sop_getsockopt)(struct sock * sock, int level, int name,
	    const struct sockdriver_data * data, socklen_t * len);
	int (* sop_getsockname)(struct sock * sock, struct sockaddr * addr,
	    socklen_t * addr_len);
	int (* sop_getpeername)(struct sock * sock, struct sockaddr * addr,
	    socklen_t * addr_len);
	int (* sop_shutdown)(struct sock * sock, unsigned int flags);
	int (* sop_close)(struct sock * sock, int force);
	void (* sop_free)(struct sock * sock);
};

typedef sockid_t (* sockevent_socket_cb_t)(int domain, int type, int protocol,
	endpoint_t user_endpt, struct sock ** sock,
	const struct sockevent_ops ** ops);

void sockevent_init(sockevent_socket_cb_t socket_cb);
void sockevent_process(const message * m_ptr, int ipc_status);

void sockevent_clone(struct sock * sock, struct sock * newsock,
	sockid_t newid);

void sockevent_raise(struct sock * sock, unsigned int mask);
void sockevent_set_error(struct sock * sock, int err);
void sockevent_set_shutdown(struct sock * sock, unsigned int flags);

#define sockevent_get_domain(sock)	((int)((sock)->sock_domain))
#define sockevent_get_type(sock)	((sock)->sock_type)
#define sockevent_get_opt(sock)		((sock)->sock_opt)
#define sockevent_is_listening(sock)	(!!((sock)->sock_opt & SO_ACCEPTCONN))
#define sockevent_is_shutdown(sock, mask)	((sock)->sock_flags & (mask))
#define sockevent_is_closing(sock)	(!!((sock)->sock_flags & SFL_CLOSING))

#endif /* !_MINIX_SOCKEVENT_H */
