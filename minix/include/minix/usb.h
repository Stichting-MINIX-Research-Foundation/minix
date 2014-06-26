#ifndef _MINIX_USB_H
#define _MINIX_USB_H

#include <sys/types.h>
#include <minix/com.h>
#include <minix/ipc.h>
#include <stdio.h>

#define USB_URBSIZE(data_size, iso_count) \
	(data_size + sizeof(struct usb_urb) + iso_count * \
	sizeof(struct usb_iso_packet_desc))

#define USB_PREPARE_URB(urb, data_size, iso_count) \
	do { \
		if(iso_count)\
			urb->iso_data.iso_desc = data_size;\
			urb->urb_size = data_size+sizeof(struct usb_urb)+iso_count * \
			sizeof(struct usb_iso_packet_desc); \
	} while (0)


struct usb_urb;

struct usb_driver {
	void (*urb_completion)(struct usb_urb *urb);
	void (*connect_device)(unsigned dev_id, unsigned int interfaces);
	void (*disconnect_device)(unsigned dev_id);
};

struct usb_device_id {
	u16_t idVendor;
	u16_t idProduct;
	u32_t bcdDevice;

	u8_t  bDeviceClass;
	u8_t  bDeviceSubClass;
	u8_t  bDeviceProtocol;

	u8_t  bInterfaceClass;
	u8_t  bInterfaceSubClass;
	u8_t  bInterfaceProtocol;
};

struct usb_iso_packet_desc {
	unsigned int offset;
	unsigned int length;		/* expected length */
	unsigned int actual_length;
	unsigned int status;
};
	
/** isochronous transfer */
#define USB_TRANSFER_ISO 0
/** interrupt transfer */
#define USB_TRANSFER_INT 1
/** control transfer */
#define USB_TRANSFER_CTL 2
/** bulk transfer */
#define USB_TRANSFER_BLK 3

#define USB_IN  0
#define USB_OUT 1

#define USB_INVALID_URB_ID 0

struct usb_urb {
	/* private */
	struct usb_urb *next;

	/** ID identifying the device on HCD side */
	int dev_id;
	int type;
	int endpoint;
	int direction;
	int status;
	int error_count;
	size_t size;
	size_t actual_length;
	void *priv;
	int interval;

	unsigned long transfer_flags;

	
	/* housekeeping information needed by usb library */
	unsigned urb_id;
	size_t urb_size;
	cp_grant_id_t gid;

	size_t iso_desc_offset;
	int number_of_packets;
	int start_frame;
	char setup_packet[8];

	/* data allways starts here */
	char buffer[1];
};

struct usb_ctrlrequest {
        u8_t bRequestType; 
        u8_t bRequest;
        u16_t wValue;
		u16_t wIndex;
        u16_t wLength;
} __attribute__ ((packed));

#ifdef DEBUG
static void dump_urb(struct usb_urb *urb) {
	printf("================\n");
	printf("DUMP: urb (0x%p)\n", urb);
	printf("================\n");
	printf("= dev_id: %d\n", urb->dev_id);
	printf("= type: %d\n", urb->type);
	printf("= endpoint: %d\n", urb->endpoint);
	printf("= direction: %d\n", urb->direction);
	printf("= status: %d\n", urb->status);
	printf("= error_count: %d\n", urb->error_count);
	printf("= size: %d\n", urb->size);
	printf("= actual_length: %d\n", urb->actual_length);
	printf("= interval %d\n", urb->interval);
	printf("= transfer_flags %x\n", urb->transfer_flags);
	printf("= urb_id = %d\n", urb->urb_id);
	printf("= urb_size = 0x%x\n", urb->urb_size);
	printf("= setup_packet: \n");
	printf("=   bRequestType: 0x%x \n",
	    ((struct usb_ctrlrequest *)urb->setup_packet)->bRequestType);
	printf("=   bRequest 0x%x \n",
	    ((struct usb_ctrlrequest *)urb->setup_packet)->bRequest);
	printf("=   wValue: 0x%x \n",
	    ((struct usb_ctrlrequest *)urb->setup_packet)->wValue);
	printf("=   wIndex: 0x%x \n",
	    ((struct usb_ctrlrequest *)urb->setup_packet)->wIndex);
	printf("=   wLength: 0x%x \n",
	    ((struct usb_ctrlrequest *)urb->setup_packet)->wLength);
	printf("===============\n");
}
#else
#define dumb_urb(x) 
#endif

/** Submit a URB */
int usb_send_urb(struct usb_urb* urb);

/** Cancels an URB */ 
int usb_cancle_urb(struct usb_urb* urb);

/** Gets the USB device ID of an USB device **/
int usb_get_device_id(int dev_id, struct usb_device_id *usb_device_id);

/* this initializes a session with the HCD */
int usb_init(char *name);

/** This functions handles a message from the HCD */
int usb_handle_msg(struct usb_driver *ubd, message *msg);

/** Lets device driver send HCD various information */
int usb_send_info(long, long);

#endif /* _MINIX_USB_H */
