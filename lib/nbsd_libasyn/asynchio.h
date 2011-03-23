/*	asynchio.h - Asynchronous I/O			Author: Kees J. Bot
 *								7 Jul 1997
 * Minix-vmd compatible asynchio(3) using BSD select(2).
 */
#ifndef _SYS__ASYNCHIO_H
#define _SYS__ASYNCHIO_H

#include <sys/select.h>			/* for FD_SETSIZE */

#define SEL_READ	0		/* Code for a read. */
#define SEL_WRITE	1		/* Code for a write. */
#define SEL_EXCEPT	2		/* Code for some exception. */
#define SEL_NR		3		/* Number of codes. */

struct _asynfd {
	int		afd_seen;	/* Set if we manage this descriptor. */
	int		afd_flags;	/* File flags by fcntl(fd, F_GETFL). */
	int		afd_state[SEL_NR];  /* Operation state. */
};

typedef struct {
	int		asyn_more;	/* Set if more to do before blocking. */
	struct _asynfd	asyn_afd[FD_SETSIZE];
	fd_set		asyn_fdset[SEL_NR];	/* Select() fd sets. */
} asynchio_t;

#define ASYN_INPROGRESS	EAGAIN		/* Errno code telling "nothing yet." */
#define ASYN_NONBLOCK	0x01		/* If asyn_wait() mustn't block. */

struct timeval;

void asyn_init(asynchio_t *_asyn);
ssize_t asyn_read(asynchio_t *_asyn, int _fd, void *_buf, size_t _len);
ssize_t asyn_write(asynchio_t *_asyn, int _fd, const void *_buf, size_t _len);
int asyn_special(asynchio_t *_asyn, int _fd, int _op);
int asyn_result(asynchio_t *_asyn, int _fd, int _op, int _result);
int asyn_wait(asynchio_t *_asyn, int _flags, struct timeval *to);
int asyn_cancel(asynchio_t *_asyn, int _fd, int _op);
int asyn_pending(asynchio_t *_asyn, int _fd, int _op);
int asyn_synch(asynchio_t *_asyn, int _fd);
int asyn_close(asynchio_t *_asyn, int _fd);

#endif /* _SYS__ASYNCHIO_H */
