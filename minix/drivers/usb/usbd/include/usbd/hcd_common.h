/*
 * Contains commonly used types and procedures, for HCD handling/initialization
 * If possible, everything OS specific (IPC, virtual memory...) should be here
 */

#ifndef _HCD_COMMON_H_
#define _HCD_COMMON_H_

#include <ddekit/thread.h>
#include <ddekit/semaphore.h>
#include <ddekit/usb.h>

/* TODO: usb.h is for DDEKit's IPC and should not be used here */
#include <minix/usb.h>				/* for setup structures */
#include <minix/usb_ch9.h>			/* for descriptor structures */

#include <sys/cdefs.h>				/* for __aligned() */


/*===========================================================================*
 *    USB register handling defines                                          *
 *===========================================================================*/
/* Helper type used for register bitwise access */
#define HCD_BIT(num)				(0x01u << (num))

/* Unsigned type that can hold all possible addresses */
typedef unsigned long				hcd_addr;

/* Register types */
typedef unsigned long				hcd_reg4;
typedef unsigned short				hcd_reg2;
typedef unsigned char				hcd_reg1;

/* For register dereferencing */
#define _HCD_REG4 				volatile hcd_reg4 *
#define _HCD_REG2 				volatile hcd_reg2 *
#define _HCD_REG1 				volatile hcd_reg1 *

/* Scalar address to dereference */
#define _HCD_ADDR(base, off)			(((hcd_addr)(base))+(off))

/* Defines for fixed size register access
 * May cause unaligned memory access */
#define HCD_WR4(base, off, val)	(*((_HCD_REG4)_HCD_ADDR(base, off)) = (val))
#define HCD_WR2(base, off, val)	(*((_HCD_REG2)_HCD_ADDR(base, off)) = (val))
#define HCD_WR1(base, off, val)	(*((_HCD_REG1)_HCD_ADDR(base, off)) = (val))
#define HCD_RD4(base, off)	(*((_HCD_REG4)_HCD_ADDR(base, off)))
#define HCD_RD2(base, off)	(*((_HCD_REG2)_HCD_ADDR(base, off)))
#define HCD_RD1(base, off)	(*((_HCD_REG1)_HCD_ADDR(base, off)))

/* Other useful defines */
#define HCD_SET(val, bits)	((val)|=(bits))
#define HCD_CLR(val, bits)	((val)&=~(bits))

/* Alignment safe conversion from 'bytes' array to a word */
#define HCD_8TO32(bytes)	(((bytes)[0])		|		\
				(((bytes)[1])<<8)	|		\
				(((bytes)[2])<<16)	|		\
				(((bytes)[3])<<24))

/* Convert type's 'sizeof' to 4-byte words count */
#define HCD_SIZEOF_TO_4(type)	((sizeof(type)+3)/4)


/*===========================================================================*
 *    USB descriptor types                                                   *
 *===========================================================================*/
typedef usb_descriptor_t			hcd_descriptor;
typedef usb_device_descriptor_t			hcd_device_descriptor;
typedef usb_config_descriptor_t			hcd_config_descriptor;
typedef usb_interface_descriptor_t		hcd_interface_descriptor;
typedef usb_endpoint_descriptor_t		hcd_endpoint_descriptor;
typedef usb_string_descriptor_t			hcd_string_descriptor;


/*===========================================================================*
 *    HCD descriptor tree types                                              *
 *===========================================================================*/
typedef struct hcd_endpoint {

	hcd_endpoint_descriptor descriptor;
}
hcd_endpoint;

typedef struct hcd_interface {

	hcd_interface_descriptor descriptor;
	hcd_endpoint * endpoint;
	int num_endpoints;
}
hcd_interface;

typedef struct hcd_configuration {

	hcd_config_descriptor descriptor;
	hcd_interface * interface;
	int num_interfaces;
}
hcd_configuration;


/*===========================================================================*
 *    HCD enumerations                                                       *
 *===========================================================================*/
/* Possible USB transfer types */
typedef enum {

	HCD_TRANSFER_CONTROL		= UE_CONTROL,
	HCD_TRANSFER_ISOCHRONOUS	= UE_ISOCHRONOUS,
	HCD_TRANSFER_BULK		= UE_BULK,
	HCD_TRANSFER_INTERRUPT		= UE_INTERRUPT
}
hcd_transfer;

/* Possible USB transfer directions */
typedef enum {

	HCD_DIRECTION_OUT		= 0,
	HCD_DIRECTION_IN		= 1,
	HCD_DIRECTION_UNUSED		= 0xFF
}
hcd_direction;

/* Possible asynchronous HCD events */
typedef enum {

	HCD_EVENT_CONNECTED = 0,	/* Device connected directly to root */
	HCD_EVENT_DISCONNECTED,		/* Directly connected device removed */
	HCD_EVENT_PORT_LS_CONNECTED,	/* Low speed device connected to hub */
	HCD_EVENT_PORT_FS_CONNECTED,	/* Full speed device connected to hub */
	HCD_EVENT_PORT_HS_CONNECTED,	/* High speed device connected to hub */
	HCD_EVENT_PORT_DISCONNECTED,	/* Device disconnected from hub */
	HCD_EVENT_ENDPOINT,		/* Something happened at endpoint */
	HCD_EVENT_URB,			/* URB was submitted by device driver */
	HCD_EVENT_INVALID = 0xFF
}
hcd_event;

/* Possible device states */
typedef enum {

	HCD_STATE_DISCONNECTED = 0,		/* default for initialization */
	HCD_STATE_CONNECTION_PENDING,
	HCD_STATE_CONNECTED
}
hcd_state;

/* USB speeds */
typedef enum {

	HCD_SPEED_LOW,
	HCD_SPEED_FULL,
	HCD_SPEED_HIGH,
}
hcd_speed;

/* Possible data toggle values (at least for bulk transfer) */
typedef enum {

	HCD_DATATOG_DATA0 = 0,
	HCD_DATATOG_DATA1 = 1
}
hcd_datatog;


/*===========================================================================*
 *    HCD threading/device/URB types                                         *
 *===========================================================================*/
typedef					void (*hcd_thread_function)(void *);
typedef ddekit_thread_t			hcd_thread;
typedef ddekit_sem_t			hcd_lock;
typedef struct hcd_driver_state		hcd_driver_state;
typedef struct usb_ctrlrequest		hcd_ctrlrequest;

/* Largest value that can be transfered by this driver at a time
 * see MAXPAYLOAD in TXMAXP/RXMAXP */
#define MAX_WTOTALLENGTH 		1024u

/* TODO: This has corresponding redefinition in hub driver */
/* Limit of child devices for each parent */
#define HCD_CHILDREN			8u

/* Total number of endpoints available in USB 2.0 */
#define HCD_TOTAL_EP			16u

/* Forward declarations */
typedef struct hcd_datarequest		hcd_datarequest;
typedef struct hcd_urb			hcd_urb;
typedef struct hcd_device_state		hcd_device_state;

/* Non-control transfer request structure */
struct hcd_datarequest {

	hcd_reg1 * data;		/* RX/TX buffer */
	int data_left;			/* Amount of data to transfer (may
					 * become negative in case of error
					 * thus 'int') */
	hcd_reg2 max_packet_size;	/* Read from EP descriptor */
	hcd_reg1 endpoint;		/* EP number */
	hcd_reg1 interval;		/* Should match one in EP descriptor */
	hcd_direction direction;	/* Should match one in EP descriptor */
	hcd_speed speed;		/* Decided during device reset */
	hcd_transfer type;		/* Should match one in EP descriptor */
};

/* HCD's URB structure */
struct hcd_urb {

	/* Basic */
	void * original_urb;
	hcd_device_state * target_device;
	void (*handled)(hcd_urb *);	/* URB handled callback */

	/* Transfer (in/out signifies what may be overwritten by HCD) */
	hcd_ctrlrequest * in_setup;
	hcd_reg1 * inout_data;
	hcd_reg4 in_size;
	hcd_reg4 out_size;
	int inout_status;	/* URB submission/validity status */

	/* Transfer control */
	hcd_transfer type;
	hcd_direction direction;
	hcd_reg1 endpoint;
	hcd_reg1 interval;
};

/* Current state of attached device */
struct hcd_device_state {

	hcd_device_state * parent;		/* In case of hub attachment */
	hcd_device_state * child[HCD_CHILDREN];	/* In case of being hub */
	hcd_device_state * _next;		/* To allow device lists */
	hcd_driver_state * driver;		/* Specific HCD driver object */
	hcd_thread * thread;
	hcd_lock * lock;
	void * data;

	hcd_urb * urb;				/* URB to be used by device */
	hcd_event wait_event;			/* Expected event */
	hcd_reg1 wait_ep;			/* Expected event's endpoint */
	hcd_device_descriptor device_desc;
	hcd_configuration config_tree;
	hcd_reg1 max_packet_size;
	hcd_speed speed;
	hcd_state state;
	hcd_reg1 reserved_address;
	hcd_reg1 current_address;
	hcd_datatog ep_tx_tog[HCD_TOTAL_EP];
	hcd_datatog ep_rx_tog[HCD_TOTAL_EP];

	/*
	 * Control transfer's local data:
	 */

	/* Number of bytes received/transmitted in last control transfer (may
	 * become negative in case of error thus 'int') */
	int control_len;

	/* Word aligned buffer for each device to hold transfered data */
	hcd_reg1 control_data[MAX_WTOTALLENGTH] __aligned(sizeof(hcd_reg4));
};


/*===========================================================================*
 *    Other definitions                                                      *
 *===========================================================================*/
#define HCD_MILI			1000
#define HCD_MICRO			1000000
#define HCD_NANO			1000000000
#define HCD_NANOSLEEP_SEC(sec)		((sec)  * HCD_NANO)
#define HCD_NANOSLEEP_MSEC(msec)	((msec) * HCD_MICRO)
#define HCD_NANOSLEEP_USEC(usec)	((usec) * HCD_MILI)

/* Default USB communication parameters */
#define HCD_DEFAULT_EP			0x00u
#define HCD_DEFAULT_ADDR		0x00u
#define HCD_DEFAULT_CONFIG		0x00u
#define HCD_FIRST_ADDR			0x01u
#define HCD_LAST_ADDR			0x7Fu
#define HCD_TOTAL_ADDR			0x80u
#define HCD_LAST_EP			0x0Fu
#define HCD_UNUSED_VAL			0xFFu	/* When number not needed */
#define HCD_DEFAULT_NAKLIMIT		0x10u


/* Legal interval values */
#define HCD_LOWEST_INTERVAL		0x00u
#define HCD_HIGHEST_INTERVAL		0xFFu

/* Translates configuration number for 'set configuration' */
#define HCD_SET_CONFIG_NUM(num)		((num)+0x01u)

/* Default MaxPacketSize for control transfer */
#define HCD_LS_MAXPACKETSIZE		8u	/* Low-speed, Full-speed */
#define HCD_HS_MAXPACKETSIZE		64u	/* High-speed */
#define HCD_MAX_MAXPACKETSIZE		1024u


/*===========================================================================*
 *    Operating system specific                                              *
 *===========================================================================*/
/* Generic method for registering interrupts */
int hcd_os_interrupt_attach(int irq, void (*init)(void *),
				void (*isr)(void *), void *priv);

/* Generic method for unregistering interrupts */
void hcd_os_interrupt_detach(int);

/* Generic method for enabling interrupts */
void hcd_os_interrupt_enable(int);

/* Generic method for disabling interrupts */
void hcd_os_interrupt_disable(int);

/* Returns pointer to memory mapped for given arguments */
void * hcd_os_regs_init(hcd_addr, unsigned long);

/* Unregisters mapped memory */
int hcd_os_regs_deinit(hcd_addr, unsigned long);

/* Configure clocking */
int hcd_os_clkconf(unsigned long, unsigned long, unsigned long);

/* Release clocking */
int hcd_os_clkconf_release(void);

/* OS's sleep wrapper */
void hcd_os_nanosleep(int);


/*===========================================================================*
 *    Device handling calls                                                  *
 *===========================================================================*/
/* Initializes device threading on connection */
int hcd_connect_device(hcd_device_state *, hcd_thread_function);

/* Cleans after device disconnection */
void hcd_disconnect_device(hcd_device_state *);

/* Locks device thread until 'hcd_device_continue' */
void hcd_device_wait(hcd_device_state *, hcd_event, hcd_reg1);

/* Unlocks device thread halted by 'hcd_device_wait' */
void hcd_device_continue(hcd_device_state *, hcd_event, hcd_reg1);

/* Allocation/deallocation of device structures */
hcd_device_state * hcd_new_device(void);
void hcd_delete_device(hcd_device_state *);
void hcd_dump_devices(void);
int hcd_check_device(hcd_device_state *);


/*===========================================================================*
 *    Descriptor tree calls                                                  *
 *===========================================================================*/
/* Creates descriptor tree based on given buffer */
int hcd_buffer_to_tree(hcd_reg1 *, int, hcd_configuration *);

/* Frees descriptor tree */
void hcd_tree_cleanup(hcd_configuration *);

/* Find EP in a tree */
hcd_endpoint * hcd_tree_find_ep(hcd_configuration *, hcd_reg1);


#endif /* !_HCD_COMMON_H_ */
