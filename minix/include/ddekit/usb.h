#ifndef _DDEKIT_USB_H
#define _DDEKIT_USB_H

#include <ddekit/ddekit.h>
#include <ddekit/types.h>

/** isochronous transfer */
#define  DDEKIT_USB_TRANSFER_ISO 0
/** interrupt transfer */
#define  DDEKIT_USB_TRANSFER_INT 1
 /** control transfer */
#define  DDEKIT_USB_TRANSFER_CTL 2
/** bulk transfer */
#define  DDEKIT_USB_TRANSFER_BLK 3

#define DDEKIT_USB_IN  1
#define DDEKIT_USB_OUT 0

struct ddekit_usb_dev;
struct ddekit_usb_urb;

struct ddekit_usb_device_id {
	ddekit_uint16_t idVendor;
	ddekit_uint16_t idProduct;
	ddekit_uint32_t bcdDevice;

	ddekit_uint8_t  bDeviceClass;
	ddekit_uint8_t  bDeviceSubClass;
	ddekit_uint8_t  bDeviceProtocol;

	ddekit_uint8_t  bInterfaceClass;
	ddekit_uint8_t  bInterfaceSubClass;
	ddekit_uint8_t  bInterfaceProtocol;

};

struct ddekit_usb_iso_packet_desc {
	ddekit_int32_t offset;
	ddekit_int32_t length;		/* expected length */
	ddekit_int32_t actual_length;
	ddekit_int32_t status;
};

typedef void (*ddekit_usb_completion_cb)(void* priv);

typedef void (*ddekit_usb_connect_cb)(struct ddekit_usb_dev *dev, 
                                      unsigned int interfaces);

typedef void (*ddekit_usb_disconnect_cb)(struct ddekit_usb_dev *dev);

typedef void *(*ddekit_usb_malloc_fn)(unsigned size);
typedef void (*ddekit_usb_free_fn)(void *ptr);

struct ddekit_usb_driver {
	ddekit_usb_completion_cb completion;
	ddekit_usb_connect_cb    connect;
	ddekit_usb_disconnect_cb    disconnect;
};


struct ddekit_usb_urb {
	struct ddekit_usb_dev *dev;
	ddekit_int32_t type;
	ddekit_int32_t endpoint;
	ddekit_int32_t direction;
	ddekit_int32_t status;
	ddekit_int32_t interval;
	ddekit_uint32_t transfer_flags;
	ddekit_uint32_t size;
	ddekit_uint32_t actual_length;
	ddekit_int32_t number_of_packets;
	ddekit_int32_t error_count;
	ddekit_int32_t start_frame;
	char *setup_packet;
	char *data;
	struct ddekit_usb_iso_packet_desc *iso_desc;
	void *priv;
	void *ddekit_priv;
};

/* USB message types */
typedef enum {

	DDEKIT_HUB_PORT_LS_CONN,	/* Low speed device connected */
	DDEKIT_HUB_PORT_FS_CONN,	/* Full speed device connected */
	DDEKIT_HUB_PORT_HS_CONN,	/* High speed device connected */
	DDEKIT_HUB_PORT_DISCONN		/* Device disconnected */
}
ddekit_msg_type_t;

int ddekit_usb_dev_set_data(struct ddekit_usb_dev *dev, void *data);
void *ddekit_usb_dev_get_data(struct ddekit_usb_dev *dev);
void ddekit_usb_get_device_id(struct ddekit_usb_dev *dev, struct
	ddekit_usb_device_id *id);
int ddekit_usb_submit_urb(struct ddekit_usb_urb *d_urb);
int ddekit_usb_cancle_urb(struct ddekit_usb_urb *d_urb);
long ddekit_usb_info(struct ddekit_usb_dev *, long, long);

/*
 * This one is only implemented for the client side. For the server side is
 * has to be implemented in the DDELinux/FBSD part.
 */
int ddekit_usb_init(struct ddekit_usb_driver *drv, ddekit_usb_malloc_fn
	*_m, ddekit_usb_free_fn *_f);

#endif
