#ifndef __NET_SERVER_SOCKET_H__
#define __NET_SERVER_SOCKET_H__

#include <stdlib.h>

#include <minix/ipc.h>
#include <minix/endpoint.h>
#include <minix/chardriver.h>

/*
 * User can set this variable to make the debugging output differ between
 * various users, e.g. "TCP" or "UDP"
 */
extern char * netsock_user_name;

#define SOCK_TYPE_IP	0
#define SOCK_TYPE_TCP	1
#define SOCK_TYPE_UDP	2
#define SOCK_TYPES	3

struct socket;
struct sock_req;

typedef int (* sock_op_opcl_t)(struct socket *);
typedef int (* sock_op_io_t)(struct socket *, struct sock_req *, int blk);
typedef int (* sock_op_select_t)(struct socket *, unsigned int);
typedef int (* sock_op_select_reply_t)(struct socket *);

struct sock_ops {
	sock_op_opcl_t		open;
	sock_op_opcl_t		close;
	sock_op_io_t		read;
	sock_op_io_t		write;
	sock_op_io_t		ioctl;
	sock_op_select_t	select;
	sock_op_select_reply_t	select_reply;
};

struct recv_q {
	struct recv_q *	next;
	void *		data;
};

#define SOCK_FLG_OP_PENDING	0x1
#define SOCK_FLG_OP_IOCTL	0x10
#define SOCK_FLG_OP_LISTENING	0x100	/* tcp socket is in a listening mode */
#define	SOCK_FLG_OP_CONNECTING	0x200	/* set when waiting for a connect */
#define SOCK_FLG_OP_READING	0x400	/* reading operation in progress */
#define SOCK_FLG_OP_WRITING	0x800	/* writing operation in progress */
#define SOCK_FLG_CLOSED		0x1000	/* tcp socket has been closed do not
					   expect any more data */
/* select() flags - they say what action do we monitor */
#define SOCK_FLG_SEL_WRITE	0x100000
#define SOCK_FLG_SEL_READ	0x200000
#define SOCK_FLG_SEL_ERROR	0x400000

#define sock_select_set(sock)	((sock)->flags & (SOCK_FLG_SEL_WRITE |	\
				SOCK_FLG_SEL_READ | SOCK_FLG_SEL_ERROR))
#define sock_select_read_set(sock)	((sock)->flags & SOCK_FLG_SEL_READ)
#define sock_select_write_set(sock)	((sock)->flags & SOCK_FLG_SEL_WRITE)
#define sock_select_rw_set(sock)	((sock)->flags & (SOCK_FLG_SEL_READ | \
							SOCK_FLG_SEL_WRITE))
#define sock_select_error_set(sock)	((sock)->flags & SOCK_FLG_SEL_ERROR)
#define sock_clear_select(sock)	do {					\
	(sock)->flags &= ~(SOCK_FLG_SEL_READ | SOCK_FLG_SEL_WRITE |	\
						SOCK_FLG_SEL_ERROR);	\
} while (0)

struct sock_req {
	enum {
		SOCK_REQ_READ,
		SOCK_REQ_WRITE,
		SOCK_REQ_IOCTL
	}			type;
	devminor_t		minor;
	endpoint_t		endpt;
	cp_grant_id_t		grant;
	union {
		size_t		size;	/* for SOCK_REQ_READ, SOCK_REQ_WRITE */
		unsigned long	req;	/* for SOCK_REQ_IOCTL */
	};
	int			flags;
	cdev_id_t		id;
};

struct socket {
	int			type;
	u32_t			flags;
	unsigned long		usr_flags;
	void *			pcb;
	struct sock_ops *	ops;
	void *			buf;
	size_t			buf_size;
	struct sock_req		req;
	void *			shm;
	size_t			shm_size;
	endpoint_t		select_ep;
	struct recv_q *		recv_head;
	struct recv_q *		recv_tail;
	unsigned		recv_data_size; /* sum of data enqueued */
	void *			data;
};

/*
 * Each component needs to provide a method how to initially open a socket.
 * The rest is handled byt the socket library.
 */
int socket_open(devminor_t minor);

#define get_sock_num(x) ((long int) ((x) - socket))
#define is_valid_sock_num(x) (x < MAX_SOCKETS)
#define get_sock(x) &socket[x]

#define MAX_SOCKETS 255 /* FIXME as log as the sockets are identified by the
			   minor device number 255 is ok */
#define MAX_DEVS 5
#define RESERVED (SOCK_TYPES + MAX_DEVS) /* rounded to 8 */

extern struct socket socket[MAX_SOCKETS];

void socket_request(message * m, int ipc_status);
void mq_process(void);

struct socket * get_unused_sock(void);
struct socket * get_nic_sock(unsigned dev);

void send_req_reply(struct sock_req * req, int status);

typedef void (* recv_data_free_fn)(void *);

int sock_enqueue_data(struct socket * sock, void * data, unsigned size);
void * sock_dequeue_data(struct socket * sock);
void sock_dequeue_data_all(struct socket * sock,
				recv_data_free_fn data_free);

void sock_select_notify(struct socket * sock);

static inline void * debug_malloc(size_t s)
{
	void * ret;

	ret = malloc(s);
	// printf("allocated %p size %d\n", ret, s);
	return ret;
}

#define debug_free(x) do {							\
	if (0)									\
		printf("free called from %s:%d %s freeing %p\n", __FILE__,	\
						__LINE__, __func__, (x));	\
	free(x);								\
} while(0)

int generic_op_select(struct socket * sock, unsigned int sel);
int generic_op_select_reply(struct socket * sock);

/* a function thr user has to provide to reply to the posix server */
void posix_reply(endpoint_t ep, message * m);

#endif /* __NET_SERVER_SOCKET_H__ */
