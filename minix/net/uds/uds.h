#ifndef MINIX_NET_UDS_UDS_H
#define MINIX_NET_UDS_UDS_H

#include <minix/drivers.h>
#include <minix/sockevent.h>
#include <minix/rmib.h>
#include <sys/un.h>

/*
 * Maximum number of UNIX domain sockets.  The control structures for all of
 * these are allocated statically, although each socket's receive buffer is
 * allocated only when the socket is in use.  If this constant is increased
 * beyond 65535, a few field sizes need to be changed.
 */
#define NR_UDSSOCK	256

/* Number of slots in the <dev,ino>-to-udssock hash table. */
#define UDSHASH_SLOTS	64

/* UDS has no protocols, so we accept only an "any protocol" value. */
#define UDSPROTO_UDS	0

/*
 * The size of each socket's receive buffer.  This size is currently a global
 * setting which cannot be changed per socket at run time, and it would be
 * rather tricky to change that.  In order not to waste resources, this size
 * should be a multiple of the page size.  Due to the fact that data and
 * metadata (such as lengths, source addresses and sender credentials) are
 * intermixed in the same buffer, the actual amount of data that can be in
 * transit at once is typically less than this value.  If this constant is
 * increased beyond 65535, several fields and field sizes need to be changed.
 */
#define UDS_BUF		32768

/* Maximum size of control data that can be sent or received at once. */
#define UDS_CTL_MAX	4096

/*
 * We allow longer path names than the size of struct sockaddr_un's sun_path
 * field.  The actual limit is determined by the maximum value of the sun_len
 * field, which is 255 and includes the first two fields of the structure (one
 * byte each) but not the null terminator of the path.  Thus, the maximum
 * length of the path minus null terminator is 253; with terminator it is 254.
 */
#define UDS_PATH_MAX	(UINT8_MAX - sizeof(uint8_t) - sizeof(sa_family_t) + 1)

/* Output debugging information? */
#define DEBUG		0

#if DEBUG
#define dprintf(x)	printf x
#else
#define dprintf(x)
#endif

/*
 * We declare this structure only for the static assert right below it.  We
 * have no need for the structure otherwise, as we use "struct sockaddr"
 * directly instead.
 */
struct sockaddr_unx {
	uint8_t sunx_len;
	sa_family_t sunx_family;
	char sunx_path[UDS_PATH_MAX];
};
STATIC_SOCKADDR_MAX_ASSERT(sockaddr_unx);

/*
 * In-flight file descriptor object.  Each in-use object is part of a socket's
 * file descriptor queue, and the file descriptor is for a file open by this
 * service.  For each set of in-flight file descriptors associated with a
 * particular segment, the first object's count field contains the number of
 * file descriptors in that set.  For all other objects in that set, the count
 * field is zero.  TODO: the count should be stored in the segment itself.
 */
struct uds_fd {
	SIMPLEQ_ENTRY(uds_fd) ufd_next;	/* next FD object for this socket */
	int ufd_fd;			/* local file descriptor number */
	unsigned int ufd_count;		/* number of FDs for this segment */
};

/*
 * Connection-type sockets (SOCK_STREAM, SOCK_SEQPACKET) are always in one of
 * the following five states, each with unique characteristics:
 *
 * - Unconnected: this socket is not in any of the other states, usually
 *   because it either has just been created, or because it has failed a
 *   connection attempt.  This socket has no connected peer and does not have
 *   the SO_ACCEPTCONN socket option set.
 * - Listening: this socket is in listening mode.  It has a queue with sockets
 *   that are connecting or connected to it but have not yet been accepted on
 *   it.  This socket has no connected peer.  It has the SO_ACCEPTCONN socket
 *   option set.
 * - Connecting: this socket is on a listening socket's queue.  While in this
 *   state, the socket has the listening socket as its linked peer, and it has
 *   no connected peer.
 * - Connected: this socket is connected to another socket, which is its
 *   connected peer socket.  It has the UDSF_CONNECTED flag set.  A socket may
 *   be connected and still be involved with a listening socket; see below.
 * - Disconnected: this socket was connected to another socket, but that other
 *   socket has been closed.  As a result, this socket has no peer.  It does
 *   have the UDSF_CONNECTED flag set.
 *
 * The UDS service supports two different type of connect behaviors, depending
 * on what the LOCAL_CONNWAIT option is set to on either the connecting or the
 * listening socket.  If LOCAL_CONNWAIT is not set on either (the default), the
 * connecting socket socket (let's call it "A") enters the connected state
 * right away, even if the connection is not immediately accepted through
 * accept(2).  In that case, a new limbo socket "B" is allocated as its
 * connection peer.  Limbo socket B is also in connected state, and either
 * returned from accept(2) later, or freed when socket A leaves the connected
 * state.  Socket A can leave the connected state either by being closed or
 * when the listening socket is closed.  If LOCAL_CONNWAIT is set, socket A
 * stays in the connecting state until it is accepted through accept(2).
 * Importantly, in both cases, it is socket A, and (in the first case) *not*
 * socket B, that is on the queue of the listening socket!
 *
 * Connected peers (uds_conn) are always symmetric: if one socket is connected
 * to another socket, that other socket is connected to it.  Any socket that is
 * on the queue of another socket, is said to be "linked" to that other socket
 * (uds_link). This is an asymmetric, one-to-many relationship: many sockets
 * may be linked to one other socket, which keeps all those sockets on its
 * queue. From the above story it should now be clear that for connection-type
 * sockets, only listening sockets may have sockets on its queue, and while
 * connecting sockets are always on a listening socket's queue, connected
 * sockets may or may not be.  Sockets in other states never are.
 *
 * UNIX domain sockets are generally reusable.  This means that the listening
 * state is the only final state; all other socket states allow the socket to
 * enter another state, although not necessarily every other state.  For
 * example, a disconnected socket may be reconnected to another target; if that
 * connection fails, the socket will enter the unconnected state.  As a result,
 * a socket in any state (even the listening state) may still have incoming
 * data pending from a previous connection.  However, EOF is currently produced
 * only for disconnected sockets.  To be sure: connecting and connected sockets
 * must first enter the unconnected or disconnected state, respectively, before
 * possibly being reconnected.
 *
 * For connectionless (i.e., SOCK_DGRAM) sockets, there are no separate states.
 * However, a connectionless socket may have been connected to another socket.
 * We maintain these links not with uds_conn but with uds_link, because such
 * connections are not symmetric, and there is an interest in keeping track of
 * which datagram sockets are connected to a particular socket (namely, to
 * break the connection on close without doing an exhaustive search).  For that
 * reason, when a datagram socket connects to another socket, it is linked to
 * that other socket, and the other socket has this socket on its queue.  As a
 * strange corner case, a connectionless socket may be connected to itself, in
 * which case it is its own linked peer and it is also on its own queue.  For
 * datagram sockets, uds_conn is always NULL and UDSF_CONNECTED is never set.
 *
 * For the purposes of sending and receiving, we generally refer to the
 * communication partner of a socket as its "peer".  As should now be clear,
 * for connection-type sockets, the socket's peer is identified with uds_conn;
 * for connectionless sockets, the socket's peer is identified with uds_link.
 */
struct udssock {
	struct sock uds_sock;		/* sock object */
	struct udssock *uds_conn;	/* connected socket, or NULL if none */
	struct udssock *uds_link;	/* linked socket, or NULL if none */
	unsigned char *uds_buf;		/* receive buffer (memory-mapped) */
	unsigned short uds_tail;	/* tail of data in receive buffer */
	unsigned short uds_len;		/* length of data in receive buffer */
	unsigned short uds_last;	/* offset to last header in buffer */
	unsigned short uds_queued;	/* current nr of sockets on queue */
	unsigned short uds_backlog;	/* maximum nr of connecting sockets */
	unsigned char uds_flags;	/* UDS-specific flags (UDSF_) */
	unsigned char uds_pathlen;	/* socket file path length (w/o nul) */
	char uds_path[UDS_PATH_MAX - 1];/* socket file path (not terminated) */
	dev_t uds_dev;			/* socket file device number */
	ino_t uds_ino;			/* socket file inode number */
	struct unpcbid uds_cred;	/* bind/connect-time credentials */
	SLIST_ENTRY(udssock) uds_hash;	/* next in hash chain */
	TAILQ_ENTRY(udssock) uds_next;	/* next in free list or queue */
	SIMPLEQ_HEAD(, uds_fd) uds_fds;	/* in-flight file descriptors */
	TAILQ_HEAD(, udssock) uds_queue;/* queue of linked sockets */
};

#define UDSF_IN_USE		0x01	/* in use (for enumeration only) */
#define UDSF_CONNECTED		0x02	/* connected or disconnected */
#define UDSF_CONNWAIT		0x04	/* leave connecting until accept */
#define UDSF_PASSCRED		0x08	/* pass credentials when receiving */

/* Macros. */
#define uds_get_type(uds)	sockevent_get_type(&(uds)->uds_sock)

/*
 * A socket that can be found through hash table lookups always has a non-empty
 * path as well as a valid <dev,ino> pair identifying the socket file that is,
 * or once was, identified by that path.  However, a socket that is bound, even
 * though it will still have an associated path, is not necessarily hashed.
 * The reason for the difference is <dev,ino> pair reuse.  This case is
 * elaborated on in uds_bind().
 */
#define uds_is_bound(uds)	((uds)->uds_pathlen != 0)
#define uds_is_hashed(uds)	((uds)->uds_dev != NO_DEV)

/*
 * These macros may be used on all socket types.  However, the uds_is_connected
 * macro returns TRUE only for connection-oriented sockets.  To see if a
 * datagram socket is connected to a target, use uds_has_link instead.
 */
#define uds_has_conn(uds)	((uds)->uds_conn != NULL)
#define uds_has_link(uds)	((uds)->uds_link != NULL)
#define uds_get_peer(uds)	\
	((uds_get_type(uds) != SOCK_DGRAM) ? (uds)->uds_conn : (uds)->uds_link)
#define uds_is_listening(uds)	sockevent_is_listening(&(uds)->uds_sock)
#define uds_is_connecting(uds)						\
	(uds_has_link(uds) && !((uds)->uds_flags & UDSF_CONNECTED) &&	\
	uds_get_type(uds) != SOCK_DGRAM)
#define uds_is_connected(uds)	\
	(((uds)->uds_flags & UDSF_CONNECTED) && uds_has_conn(uds))
#define uds_is_disconnected(uds)	\
	(((uds)->uds_flags & UDSF_CONNECTED) && !uds_has_conn(uds))

#define uds_is_shutdown(uds, mask)	\
	sockevent_is_shutdown(&(uds)->uds_sock, (mask))

/* Function prototypes. */

/* uds.c */
sockid_t uds_get_id(struct udssock * uds);
struct udssock *uds_enum(struct udssock * prev, int type);
void uds_make_addr(const char * path, size_t len, struct sockaddr * addr,
	socklen_t * addr_len);
int uds_lookup(struct udssock * uds, const struct sockaddr * addr,
	socklen_t addr_len, endpoint_t user_endpt, struct udssock ** peerp);

/* io.c */
void uds_io_init(void);
int uds_io_setup(struct udssock * uds);
void uds_io_cleanup(struct udssock * uds);
void uds_io_reset(struct udssock * uds);
size_t uds_io_buflen(void);
int uds_pre_send(struct sock * sock, size_t len, socklen_t ctl_len,
	const struct sockaddr * addr, socklen_t addr_len,
	endpoint_t user_endpt, int flags);
int uds_send(struct sock * sock, const struct sockdriver_data * data,
	size_t len, size_t * off, const struct sockdriver_data * ctl,
	socklen_t ctl_len, socklen_t * ctl_off, const struct sockaddr * addr,
	socklen_t addr_len, endpoint_t user_endpt, int flags, size_t min);
int uds_test_send(struct sock * sock, size_t min);
int uds_pre_recv(struct sock * sock, endpoint_t user_endpt, int flags);
int uds_recv(struct sock * sock, const struct sockdriver_data * data,
	size_t len, size_t * off, const struct sockdriver_data * ctl,
	socklen_t ctl_len, socklen_t * ctl_off, struct sockaddr * addr,
	socklen_t * addr_len, endpoint_t user_endpt, int flags, size_t min,
	int * rflags);
int uds_test_recv(struct sock * sock, size_t min, size_t * size);

/* stat.c */
void uds_stat_init(void);
void uds_stat_cleanup(void);

#endif /* !MINIX_NET_UDS_UDS_H */
