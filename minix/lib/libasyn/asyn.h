/*	asyn.h - async I/O
 *							Author: Kees J. Bot
 *								7 Jul 1997
 * Minix-vmd compatible asynchio(3) using BSD select(2).
 */
#define nil 0
#include <sys/types.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/asynchio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

typedef struct _asynfd asynfd_t;

#undef IDLE

typedef enum state { IDLE, WAITING, PENDING } state_t;
