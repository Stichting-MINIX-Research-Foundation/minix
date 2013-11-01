#ifndef __UDS_UDS_H
#define __UDS_UDS_H

#include <minix/drivers.h>
#include <minix/chardriver.h>
#undef send
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/ucred.h>
#include <sys/un.h>
#include <sys/mman.h>

/* Maximum number of UNIX domain sockets. */
#define NR_FDS		256

/* Connection backlog size for incoming connections. */
#define UDS_SOMAXCONN	64

/* Maximum UDS socket buffer size. */
#define UDS_BUF		PIPE_BUF

/* Output debugging information? */
#define DEBUG		0

#if DEBUG
#define dprintf(x)	printf x
#else
#define dprintf(x)
#endif

/* ancillary data to be sent */
struct ancillary {
	int fds[OPEN_MAX];
	int nfiledes;
	struct uucred cred;
};

#define UDS_R	0x1
#define UDS_W	0x2

/*
 * Internal State Information for a socket descriptor.
 */
struct uds_fd {

/* Flags */

	enum UDS_STATE {
		/* This file descriptor is UDS_FREE and can be allocated. */
		UDS_FREE  = 0,

		/* OR it is UDS_INUSE and can't be allocated. */
		UDS_INUSE = 1

	/* state is set to UDS_INUSE in uds_open(). state is Set to
	 * UDS_FREE in uds_init() and uds_close(). state should be
	 * checked prior to all operations.
	 */
	} state;

/* Owner Info */

	/* Socket Owner */
	endpoint_t owner;

/* Pipe Housekeeping */

	char *buf;			/* ring buffer */
	size_t pos;			/* tail position into ring buffer */
	size_t size;			/* size of used part of ring buffer */

	/* control read/write, set by uds_open() and shutdown(2).
	 * Can be set to UDS_R|UDS_W, UDS_R, UDS_W, or 0
	 * for read and write, read only, write only, or neither.
	 * default is UDS_R|UDS_W.
	 */
	int mode;

/* Socket Info */

	/* socket type - SOCK_STREAM, SOCK_DGRAM, or SOCK_SEQPACKET
	 * Set by uds_ioctl(NWIOSUDSTYPE). It defaults to -1 in
	 * uds_open(). Any action on a socket with type -1 besides
	 * uds_ioctl(NWIOSUDSTYPE) and uds_close() will result in
	 * an error.
	 */
	int type;

	/* queue of pending connections for server sockets.
	 * connect(2) inserts and accept(2) removes from the queue
	 */
	int backlog[UDS_SOMAXCONN];

	/* requested connection backlog size. Set by listen(2)
	 * Bounds (0 <= backlog_size <= UDS_SOMAXCONN)
	 * Defaults to UDS_SOMAXCONN which is defined above.
	 */
	unsigned char backlog_size;

	/* index of peer in uds_fd_table for connected sockets.
	 * -1 is used to mean no peer. Assumptions: peer != -1 means
	 * connected.
	 */
	int peer;

	/* index of child (client sd returned by accept(2))
	 * -1 is used to mean no child.
	 */
	int child;

	/* address -- the address the socket is bound to.
	 * Assumptions: addr.sun_family == AF_UNIX means its bound.
	 */
	struct sockaddr_un addr;

	/* target -- where DGRAMs are sent to on the next uds_write(). */
	struct sockaddr_un target;

	/* source -- address where DGRAMs are from. used to fill in the
	 * from address in recvfrom(2) and recvmsg(2).
	 */
	struct sockaddr_un source;

	/* Flag (1 or 0) - listening for incoming connections.
	 * Default to 0. Set to 1 by do_listen()
	 */
	int listening;

	/* stores file pointers and credentials being sent between
	 * processes with sendmsg(2) and recvmsg(2).
	 */
	struct ancillary ancillary_data;

	/* Holds an errno. This is set when a connected socket is
	 * closed and we need to pass ECONNRESET on to a suspended
	 * peer.
	 */
	int err;

/* Suspend/Revive Housekeeping */

	/* SUSPEND State Flags */
	enum UDS_SUSPENDED {

		/* Socket isn't blocked. */
		UDS_NOT_SUSPENDED     = 0,

		/* Socket is blocked on read(2) waiting for data to read. */
		UDS_SUSPENDED_READ    = 1,

		/* Socket is blocked on write(2) for space to write data. */
		UDS_SUSPENDED_WRITE   = 2,

		/* Socket is blocked on connect(2) waiting for the server. */
		UDS_SUSPENDED_CONNECT = 4,

		/* Socket is blocked on accept(2) waiting for clients. */
		UDS_SUSPENDED_ACCEPT  = 8
	} suspended;

	/* source endpoint, saved for later use by suspended procs */
	endpoint_t susp_endpt;

	/* i/o grant, saved for later use by suspended procs */
	cp_grant_id_t susp_grant;

	/* size of request, saved for later use by suspended procs */
	size_t susp_size;

	/* request ID, saved for later use by suspended procs */
	cdev_id_t susp_id;

/* select() */

	/* when a select is in progress, we notify this endpoint
	 * of new data.
	 */
	endpoint_t sel_endpt;

	/* Options (CDEV_OP_RD,WR,ERR) that are requested. */
	unsigned int sel_ops;
};

typedef struct uds_fd uds_fd_t;

/* File Descriptor Table -- Defined in uds.c */
EXTERN uds_fd_t uds_fd_table[NR_FDS];

/* Function prototypes. */

/* ioc_uds.c */
int uds_clear_fds(devminor_t minor, struct ancillary *data);
int uds_do_ioctl(devminor_t minor, unsigned long request, endpoint_t endpt,
	cp_grant_id_t grant);

/* uds.c */
ssize_t uds_perform_read(devminor_t minor, endpoint_t endpt,
	cp_grant_id_t grant, size_t size, int pretend);
void uds_unsuspend(devminor_t minor);

#endif /* !__UDS_UDS_H */
