#ifndef __PFS_UDS_H__
#define __PFS_UDS_H__

/*
 * Unix Domain Sockets Implementation (PF_UNIX, PF_LOCAL)
 *
 * Also See...
 *
 *   dev_uds.c, table.c, uds.c
 */

#include <limits.h>
#include <sys/types.h>
#include <sys/ucred.h>
#include <sys/un.h>

#include <minix/endpoint.h>
#include <minix/chardriver.h>

/* max connection backlog for incoming connections */
#define UDS_SOMAXCONN 64

typedef void* filp_id_t;

/* ancillary data to be sent */
struct ancillary {
	filp_id_t filps[OPEN_MAX];
	int fds[OPEN_MAX];
	int nfiledes;
	struct uucred cred;
};

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

	/* inode number on PFS -- each descriptor is backed by 1
	 * PIPE which is allocated in uds_open() and freed in
	 * uds_close(). Data is sent/written to a peer's PIPE.
	 * Data is recv/read from this PIPE.
	 */
	pino_t inode_nr;


	/* position in the PIPE where the data starts */
	off_t pos;

	/* size of data in the PIPE */
	size_t size;

	/* control read/write, set by uds_open() and shutdown(2).
	 * Can be set to S_IRUSR|S_IWUSR, S_IRUSR, S_IWUSR, or 0
	 * for read and write, read only, write only, or neither.
	 * default is S_IRUSR|S_IWUSR.
	 */
	pmode_t mode;

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

	/* when a select is in progress, we notify() this endpoint
	 * of new data.
	 */
	endpoint_t sel_endpt;

	/* Options (SEL_RD, SEL_WR, SEL_ERR) that are requested. */
	unsigned int sel_ops;
};

typedef struct uds_fd uds_fd_t;

/* File Descriptor Table -- Defined in uds.c */
EXTERN uds_fd_t uds_fd_table[NR_FDS];

#endif
