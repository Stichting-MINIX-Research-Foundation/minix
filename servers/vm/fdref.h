
#ifndef _FDREF_H
#define _FDREF_H 1

#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/config.h>
#include <minix/const.h>
#include <minix/ds.h>
#include <minix/endpoint.h>
#include <minix/minlib.h>
#include <minix/type.h>
#include <minix/ipc.h>
#include <minix/sysutil.h>
#include <minix/syslib.h>
#include <minix/const.h>

struct fdref {
	int             fd;
	int             refcount;
	dev_t   dev;
	ino_t   ino;
	struct fdref	*next;
	int counting;	/* sanity check */
};

#endif

