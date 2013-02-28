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

/* max connection backlog for incoming connections */
#define UDS_SOMAXCONN 64

typedef void* filp_id_t;

/* ancillary data to be sent */
struct ancillary {
	filp_id_t filps[OPEN_MAX];
	int fds[OPEN_MAX];
	int nfiledes;
	struct ucred cred;
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

	/* endpoint for suspend/resume */
	endpoint_t endpoint;

/* Pipe Housekeeping */

	/* inode number on PFS -- each descriptor is backed by 1
	 * PIPE which is allocated in uds_open() and freed in
	 * uds_close(). Data is sent/written to a peer's PIPE.
	 * Data is recv/read from this PIPE.
	 */
	ino_t inode_nr;


	/* position in the PIPE where the data starts */
	off_t pos;

	/* size of data in the PIPE */
	size_t size;

	/* control read/write, set by uds_open() and shutdown(2).
	 * Can be set to S_IRUSR|S_IWUSR, S_IRUSR, S_IWUSR, or 0
	 * for read and write, read only, write only, or neither.
	 * default is S_IRUSR|S_IWUSR.
	 */
	mode_t mode;

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

	/* Flag (1 or 0) - thing socket was waiting for is ready.
	 * If 1, then uds_status() will attempt the operation that
	 * the socket was blocked on.
	 */
	int ready_to_revive;

	/* i/o grant, saved for later use by suspended procs */
	cp_grant_id_t io_gr;

	/* is of i/o grant, saved for later use by suspended procs */
	size_t io_gr_size;

	/* Save the call number so that uds_cancel() can unwind the
	 * call properly.
	 */
	int call_nr;

	/* Save the IOCTL so uds_cancel() knows what got cancelled. */
	int ioctl;

	/* Flag (1 or 0) - the system call completed.
	 * A doc I read said DEV_CANCEL might be called even though
	 * the operation is finished. We use this variable to
	 * determine if we should rollback the changes or not.
	 */
	int syscall_done;

/* select() */

	/* Flag (1 or 0) - the process blocked on select(2). When
	 * selecting is 1 and I/O happens on this socket, then
	 * select_proc should be notified.
	 */
	int selecting;

	/* when a select is in progress, we notify() this endpoint
	 * of new data.
	 */
	endpoint_t select_proc;

	/* Options (SEL_RD, SEL_WR, SEL_ERR) that are requested. */
	int sel_ops_in;

	/* Options that are available for this socket. */
	int sel_ops_out;

	/* Flag (1 or 0) to be set to one before calling notify().
	 * uds_status() will use the flag to locate this descriptor.
	 */
	int status_updated;
};

typedef struct uds_fd uds_fd_t;

/* File Descriptor Table -- Defined in uds.c */
EXTERN uds_fd_t uds_fd_table[NR_FDS];

/*
 * Take message m and get the index in uds_fd_table.
 */
#define uds_minor(m)		(minor((dev_t) m->DEVICE))

/*
 * Fill in a reply message.
 */
#define uds_set_reply(msg,type,endpoint,io_gr,status)	\
	do {						\
		(msg)->m_type = type;			\
		(msg)->REP_ENDPT = endpoint;		\
		(msg)->REP_IO_GRANT = io_gr;		\
		(msg)->REP_STATUS = status;		\
	} while (0)

#define uds_sel_reply(msg,type,minor,ops)		\
	do {						\
		(msg)->m_type = type;			\
		(msg)->DEV_MINOR = minor;			\
		(msg)->DEV_SEL_OPS = ops;			\
	} while (0)




#endif
