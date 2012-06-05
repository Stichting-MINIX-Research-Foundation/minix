#ifndef DEVMAN_USB_DRIVER
#define DEVMAN_USB_DRIVER

#include <minix/usb.h>
#include <sys/queue.h>

#define USB_MATCH_ID_VENDOR          (1 << 0)
#define USB_MATCH_ID_PRODUCT         (1 << 1)
#define USB_MATCH_BCD_DEVICE         (1 << 2)
#define USB_MATCH_DEVICE_CLASS       (1 << 3)
#define USB_MATCH_DEVICE_SUBCLASS    (1 << 4)
#define USB_MATCH_DEVICE_PROTOCOL    (1 << 5)
#define USB_MATCH_INTERFACE_CLASS    (1 << 6)
#define USB_MATCH_INTERFACE_SUBCLASS (1 << 7)
#define USB_MATCH_INTERFACE_PROTOCOL (1 << 8)

enum devmand_device_type {
	char_dev,
	block_dev
};

struct devmand_usb_match_id {
	unsigned match_flags;
	struct usb_device_id match_id;
	LIST_ENTRY(devmand_usb_match_id) list;
};

#define DEVMAND_DRIVER_LABEL_LEN 32

struct devmand_driver_instance {
	int dev_id;
	int major;
	char label[DEVMAND_DRIVER_LABEL_LEN];
	struct devmand_usb_driver *drv;
	LIST_ENTRY(devmand_driver_instance) list;
};

struct devmand_usb_driver {
	char *name;
	char *devprefix;
	char *binary;
	char *upscript;
	char *downscript;
	enum devmand_device_type dev_type;
	LIST_HEAD(devid_head, devmand_usb_match_id) ids;
	LIST_ENTRY(devmand_usb_driver) list;
};

struct devmand_usb_driver * add_usb_driver(char *name);
struct devmand_usb_match_id *add_usb_match_id(struct devmand_usb_driver *drv);

#endif
