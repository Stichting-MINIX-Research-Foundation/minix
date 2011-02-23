#ifndef _SERVERS_DEVMAN_DEVMAN_H
#define _SERVERS_DEVMAN_DEVMAN_H
#define _POSIX_SOURCE      1	/* tell headers to include POSIX stuff */
#define _MINIX             1	/* tell headers to include MINIX stuff */
#define _SYSTEM            1	/* tell headers that this is the kernel */
#define DEVMAN_SERVER      1

#include <minix/config.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <lib.h>
#include <timers.h>

#include <minix/callnr.h>
#include <minix/type.h>
#include <minix/const.h>
#include <minix/com.h>
#include <minix/syslib.h>
#include <minix/sysutil.h>
#include <minix/vfsif.h>
#include <minix/endpoint.h>
#include <minix/sysinfo.h>
#include <minix/u64.h>
#include <minix/sysinfo.h>
#include <minix/type.h>
#include <minix/ipc.h>

#include <sys/time.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <minix/vtreefs.h>

#include <minix/devman.h>
#include <sys/queue.h>

#define DEVMAN_DEFAULT_MODE   (S_IRUSR | S_IRGRP | S_IROTH)
#define DEVMAN_STRING_LEN 128

#define ADD_STRING "ADD "
#define REMOVE_STRING "REMOVE "

enum devman_inode_type {
	DEVMAN_DEVINFO_STATIC,
	DEVMAN_DEVINFO_DYNAMIC,
	DEVMAN_DEVICE
};

typedef int (*devman_read_fn)
    (char **ptr, size_t *len, off_t offset, void *data);

struct devman_device_file {
	int minor;
	int type;
};

struct devman_static_info_inode {
	struct devman_device *dev;
	char data[DEVMAN_STRING_LEN];
};

struct devman_event {
	char data[DEVMAN_STRING_LEN];
	TAILQ_ENTRY(devman_event) events;
};

struct devman_event_inode {
	TAILQ_HEAD(event_head, devman_event) event_queue;
};

struct devman_inode {
	struct inode *inode;
	devman_read_fn read_fn;
	void *data;
	TAILQ_ENTRY(devman_inode) inode_list;
};

struct devman_device {
	int dev_id;
	char * name;

	int ref_count; 

	int major;	
#define DEVMAN_DEVICE_ZOMBIE 2
#define DEVMAN_DEVICE_BOUND 1
#define DEVMAN_DEVICE_UNBOUND 0
	int state;

	endpoint_t owner;

	struct devman_inode inode;
	struct devman_device *parent;

	/* the serialized information on this device */
	struct devman_device_info *info;

	TAILQ_ENTRY(devman_device) siblings;
	
	/* devices attached to the this device */
	TAILQ_HEAD(children_head, devman_device) children;
	TAILQ_HEAD(info_head, devman_inode) infos;
};
#endif
