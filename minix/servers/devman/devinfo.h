#ifndef DEVMAN_DEVINFO_H
#define DEVMAN_DEVINFO_H 1


struct devman_dev {
	int dev_id;
	int parent_dev_id;
	char *name;
	char *subsys;
	void *data;
	TAILQ_HEAD(static_attribute_head, devman_static_attribute) attrs;
};

struct devman_static_attribute {
	char *name;
	char *data;
	TAILQ_ENTRY(devman_static_attribute) list;
};

/* used for serializing */
struct devman_device_info {
	int count;
	int parent_dev_id;
	unsigned name_offset;
	unsigned subsystem_offset;
};

struct devman_device_info_entry {
	unsigned type;
	unsigned name_offset;
	unsigned data_offset;
	unsigned req_nr;
};

#endif
