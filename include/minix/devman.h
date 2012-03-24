#ifndef MINIX_LIBDEVMAN_H
#define MINIX_LIBDEVMAN_H
#include <minix/com.h>
#include <minix/ipc.h>
#include <minix/usb_ch9.h>

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

#ifndef DEVMAN_SERVER
struct devman_usb_bind_cb_data {
	int dev_id;
	int interface;
};

struct devman_usb_interface {
	struct devman_dev *dev;
	struct devman_usb_dev *usb_dev;
	usb_interface_descriptor_t *desc;
	/* used by the lib */
	struct devman_usb_bind_cb_data cb_data;
};

struct devman_usb_dev {
	struct devman_dev *dev;
	int    dev_id;            /* The ID identifying the device 
									 on server side */
	usb_device_descriptor_t *desc;

	int    configuration;        /* the configuration used for this
	                                device */
	
	char   *manufacturer;
	char   *product;
	char   *serial;

	int    intf_count;          /* the number of interfaces in the current
	                               configuration */

	struct devman_usb_interface interfaces[32];
	/* used by the lib */
	struct devman_usb_bind_cb_data cb_data;
};

typedef int (*devman_usb_bind_cb_t)(struct devman_usb_bind_cb_data *data, endpoint_t ep);

int devman_add_device(struct devman_dev *dev);
int devman_del_device(struct devman_dev *dev);
int devman_init(void);
struct devman_usb_dev* devman_usb_device_new(int dev_id);
int devman_usb_device_add(struct devman_usb_dev *dev);
int devman_usb_device_remove(struct devman_usb_dev *dev);
void devman_usb_device_delete(struct devman_usb_dev *udev);
int devman_handle_msg(message *m);
void devman_usb_init(devman_usb_bind_cb_t bind_cb, devman_usb_bind_cb_t
	unbind_cb);

#endif

#endif
