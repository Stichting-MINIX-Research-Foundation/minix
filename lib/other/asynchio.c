/*	asyn_init(), asyn_read(), asyn_write(), asyn_ioctl(),
 *	asyn_wait(), asyn_synch(), asyn_close()
 *							Author: Kees J. Bot
 *								26 Jan 1995
 * Thise are just stub routines that are call compatible with
 * the asynchio(3) library of Minix-vmd.  See asynchio.h.
 */
#define nil 0
#define alarm	_alarm
#define ioctl	_ioctl
#define read	_read
#define sigaction _sigaction
#define sigfillset _sigfillset
#define time	_time
#define write	_write
#include <lib.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/asynchio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#define IO_IDLE		0
#define IO_INPROGRESS	1
#define IO_RESULT	2

#define OP_NOOP		0
#define OP_READ		1
#define OP_WRITE	2
#define OP_IOCTL	3

static asynchio_t *asyn_current;

void asyn_init(asynchio_t *asyn)
{
	asyn->state= IO_IDLE;
	asyn->op= OP_NOOP;
}

static ssize_t operation(int op, asynchio_t *asyn, int fd, int req,
						void *data, ssize_t count)
{
	switch (asyn->state) {
	case IO_INPROGRESS:
		if (asyn_current != asyn && asyn->op != op) abort();
		/*FALL THROUGH*/
	case IO_IDLE:
		asyn_current= asyn;
		asyn->op= op;
		asyn->fd= fd;
		asyn->req= req;
		asyn->data= data;
		asyn->count= count;
		asyn->state= IO_INPROGRESS;
		errno= EINPROGRESS;
		return -1;
	case IO_RESULT:
		if (asyn_current != asyn && asyn->op != op) abort();
		errno= asyn->errno;
		return asyn->count;
	}
}

ssize_t asyn_read(asynchio_t *asyn, int fd, void *buf, size_t len)
{
	return operation(OP_READ, asyn, fd, 0, buf, len);
}

ssize_t asyn_write(asynchio_t *asyn, int fd, const void *buf, size_t len)
{
	return operation(OP_WRITE, asyn, fd, 0, (void *) buf, len);
}

int asyn_ioctl(asynchio_t *asyn, int fd, unsigned long request, void *data)
{
	return operation(OP_IOCTL, asyn, fd, request, data, 0);
}

static void time_out(int sig)
{
	alarm(1);
}

int asyn_wait(asynchio_t *asyn, int flags, struct timeval *to)
{
	time_t now;
	unsigned old_timer, new_timer;
	struct sigaction old_sa, new_sa;

	if (asyn_current != asyn) abort();
	if (flags & ASYN_NONBLOCK) abort();

	if (asyn->state == IO_RESULT) {
		asyn->state= IO_IDLE;
		asyn->op= OP_NOOP;
		return 0;
	}

	if (to != nil) {
		now= time(nil);
		if (to->tv_sec <= now) { errno= EINTR; return -1; }
		old_timer= alarm(0);
		new_sa.sa_handler= time_out;
		sigfillset(&new_sa.sa_mask);
		new_sa.sa_flags= 0;
		sigaction(SIGALRM, &new_sa, &old_sa);
		new_timer= to->tv_sec - now;
		if (new_timer < old_timer) {
			new_timer= old_timer;
		}
		alarm(new_timer);
	}
	switch (asyn->op) {
	case OP_NOOP:
		asyn->count= pause();
		asyn->errno= errno;
	case OP_READ:
		asyn->count= read(asyn->fd, asyn->data, asyn->count);
		asyn->errno= errno;
		break;
	case OP_WRITE:
		asyn->count= write(asyn->fd, asyn->data, asyn->count);
		asyn->errno= errno;
		break;
	case OP_IOCTL:
		asyn->count= ioctl(asyn->fd, asyn->req, asyn->data);
		asyn->errno= errno;
		break;
	}
	if (to != nil) {
		alarm(0);
		sigaction(SIGALRM, &old_sa, (struct sigaction *)0);
		alarm(old_timer);
	}

	if (asyn->count == -1 && asyn->errno == EINTR) {
		errno= EINTR;
		return -1;
	} else {
		asyn->state= IO_RESULT;
		return 0;
	}
}

int asyn_synch(asynchio_t *asyn, int fd)
{
}

int asyn_close(asynchio_t *asyn, int fd)
{
	asyn_init(asyn);
}
