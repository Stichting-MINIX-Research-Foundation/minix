#ifndef _LIB_LIBDEVMAN_LOCAL
#define _LIB_LIBDEVMAN_LOCAL

#include <sys/queue.h>
#include <minix/com.h>

#define DEVMAN_DEV_NAME_LEN 32

struct devman_dev {
	int dev_id;
	int parent_dev_id;
	char name[DEVMAN_DEV_NAME_LEN];
	char *subsys;
	void *data;
	int (*bind_cb)  (void *data, endpoint_t ep);
	int (*unbind_cb)(void *data, endpoint_t ep);
	TAILQ_HEAD(static_attribute_head, devman_static_attribute) attrs;
	TAILQ_ENTRY(devman_dev) dev_list;
};

struct devman_static_attribute {
	char *name;
	char *data;
	TAILQ_ENTRY(devman_static_attribute) list;
};

#endif
