#ifndef _MINIX_SOCKDRIVER_H
#define _MINIX_SOCKDRIVER_H

#include <sys/socket.h>

/*
 * The maximum sockaddr structure size.  All provided address buffers are of
 * this size.  The socket driver writer must ensure that all the driver's
 * sockaddr structures fit in this size, increasing this variable as necessary.
 */
#define SOCKADDR_MAX	(UINT8_MAX + 1)

/*
 * Convenience macro to perform static testing of the above assumption.  Usage:
 *   STATIC_SOCKADDR_MAX_ASSERT(sockaddr_un);
 */
#define STATIC_SOCKADDR_MAX_ASSERT(t) \
	typedef int _STATIC_SOCKADDR_MAX_ASSERT_##t[/* CONSTCOND */ \
		(sizeof(struct t) <= SOCKADDR_MAX) ? 1 : -1]

/*
 * The maximum number of I/O vector elements that can be passed to the
 * sockdriver_vcopy functions.
 */
#define SOCKDRIVER_IOV_MAX	SCPVEC_NR

/* Socket identifier.  May also be a negative error code upon call return. */
typedef int32_t sockid_t;

/* Socket request identifier.  To be used in struct sockdriver_call only. */
typedef int32_t sockreq_t;

/*
 * The following structures are all identity transfer (ixfer) safe, meaning
 * that they are guaranteed not to contain pointers.
 */

/*
 * Data structure with call information for later call resumption.  The socket
 * driver may use the sc_endpt and sc_req fields to index and find suspended
 * calls for the purpose of cancellation.  A provided sc_endpt value will never
 * be NONE, so this value may be used to mark the structure as unused, if
 * needed.  Otherwise, the structure should be copied around as is.  Upon
 * cancellation, the original call structure must be used to resume the call,
 * and not the call structure passed to sdr_cancel.
 */
struct sockdriver_call {
	endpoint_t	sc_endpt;	/* endpoint of caller */
	sockreq_t	sc_req;		/* request identifier */
	cp_grant_id_t	_sc_grant;	/* address storage grant (private) */
	size_t		_sc_len;	/* size of address storage (private) */
};

/*
 * Data structure for the requesting party of select requests.  The socket
 * driver may use the ss_endpt field to index and find suspended select calls.
 * A provided ss_endpt value will never be NONE, so this value may be used to
 * mark the structure as unused, if needed.  For future compatibility, the
 * structure should be copied around in its entirety.
 */
struct sockdriver_select {
	endpoint_t	ss_endpt;	/* endpoint of caller */
};

/* Opaque data structure for copying in and out data. */
struct sockdriver_data {
	endpoint_t	_sd_endpt;	/* endpoint of grant owner (private) */
	cp_grant_id_t	_sd_grant;	/* safecopy grant (private) */
	size_t		_sd_len;	/* size of granted area (private) */
};

/*
 * Opaque data structure that may store the contents of sockdriver_data more
 * compactly in cases where some of its fields are available through other
 * means.  Practically, this can save memory when storing suspended calls.
 */
struct sockdriver_packed_data {
	cp_grant_id_t	_spd_grant;	/* safecopy grant (private) */
};

/* Function call table for socket drivers. */
struct sockdriver {
	sockid_t (* sdr_socket)(int domain, int type, int protocol,
	    endpoint_t user_endpt);
	int (* sdr_socketpair)(int domain, int type, int protocol,
	    endpoint_t user_endpt, sockid_t id[2]);
	int (* sdr_bind)(sockid_t id, const struct sockaddr * __restrict addr,
	    socklen_t addr_len, endpoint_t user_endpt,
	    const struct sockdriver_call * __restrict call);
	int (* sdr_connect)(sockid_t id,
	    const struct sockaddr * __restrict addr, socklen_t addr_len,
	    endpoint_t user_endpt,
	    const struct sockdriver_call * __restrict call);
	int (* sdr_listen)(sockid_t id, int backlog);
	sockid_t (* sdr_accept)(sockid_t id, struct sockaddr * __restrict addr,
	    socklen_t * __restrict addr_len, endpoint_t user_endpt,
	    const struct sockdriver_call * __restrict call);
	int (* sdr_send)(sockid_t id,
	    const struct sockdriver_data * __restrict data, size_t len,
	    const struct sockdriver_data * __restrict ctl_data,
	    socklen_t ctl_len, const struct sockaddr * __restrict addr,
	    socklen_t addr_len, endpoint_t user_endpt, int flags,
	    const struct sockdriver_call * __restrict call);
	int (* sdr_recv)(sockid_t id,
	    const struct sockdriver_data * __restrict data, size_t len,
	    const struct sockdriver_data * __restrict ctl_data,
	    socklen_t * __restrict ctl_len, struct sockaddr * __restrict addr,
	    socklen_t * __restrict addr_len, endpoint_t user_endpt,
	    int * __restrict flags,
	    const struct sockdriver_call * __restrict call);
	int (* sdr_ioctl)(sockid_t id, unsigned long request,
	    const struct sockdriver_data * __restrict data,
	    endpoint_t user_endpt,
	    const struct sockdriver_call * __restrict call);
	int (* sdr_setsockopt)(sockid_t id, int level, int name,
	    const struct sockdriver_data * data, socklen_t len);
	int (* sdr_getsockopt)(sockid_t id, int level, int name,
	    const struct sockdriver_data * __restrict data,
	    socklen_t * __restrict len);
	int (* sdr_getsockname)(sockid_t id, struct sockaddr * __restrict addr,
	    socklen_t * __restrict addr_len);
	int (* sdr_getpeername)(sockid_t id, struct sockaddr * __restrict addr,
	    socklen_t * __restrict addr_len);
	int (* sdr_shutdown)(sockid_t id, int how);
	int (* sdr_close)(sockid_t id, const struct sockdriver_call * call);
	void (* sdr_cancel)(sockid_t id, const struct sockdriver_call * call);
	int (* sdr_select)(sockid_t id, unsigned int ops,
	    const struct sockdriver_select * sel);
	void (* sdr_alarm)(clock_t stamp);
	void (* sdr_other)(const message * m_ptr, int ipc_status);
};

/* Functions defined by libsockdriver. */
void sockdriver_announce(void);
void sockdriver_process(const struct sockdriver * __restrict sdp,
	const message * __restrict m_ptr, int ipc_status);
void sockdriver_terminate(void);
void sockdriver_task(const struct sockdriver * sdp);

void sockdriver_reply_generic(const struct sockdriver_call * call, int reply);
void sockdriver_reply_accept(const struct sockdriver_call * __restrict call,
	sockid_t reply, struct sockaddr * __restrict addr, socklen_t addr_len);
void sockdriver_reply_recv(const struct sockdriver_call * __restrict call,
	int reply, socklen_t ctl_len, struct sockaddr * __restrict addr,
	socklen_t addr_len, int flags);
void sockdriver_reply_select(const struct sockdriver_select * sel, sockid_t id,
	int ops);

int sockdriver_copyin(const struct sockdriver_data * __restrict data,
	size_t off, void * __restrict ptr, size_t len);
int sockdriver_copyout(const struct sockdriver_data * __restrict data,
	size_t off, const void * __restrict ptr, size_t len);

int sockdriver_vcopyin(const struct sockdriver_data * __restrict data,
	size_t off, const iovec_t * iov, unsigned int iovcnt);
int sockdriver_vcopyout(const struct sockdriver_data * __restrict data,
	size_t off, const iovec_t * iov, unsigned int iovcnt);

int sockdriver_copyin_opt(const struct sockdriver_data * __restrict data,
	void * __restrict ptr, size_t len, socklen_t optlen);
int sockdriver_copyout_opt(const struct sockdriver_data * __restrict data,
	const void * __restrict ptr, size_t len,
	socklen_t * __restrict optlen);

int sockdriver_pack_data(struct sockdriver_packed_data * pack,
	const struct sockdriver_call * call,
	const struct sockdriver_data * data, size_t len);
void sockdriver_unpack_data(struct sockdriver_data * data,
	const struct sockdriver_call * call,
	const struct sockdriver_packed_data * pack, size_t len);

#endif /* !_MINIX_SOCKDRIVER_H */
