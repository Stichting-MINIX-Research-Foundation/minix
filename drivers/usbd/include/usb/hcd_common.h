/*
 * Contains commonly used types and procedures, for HCD handling/initialization
 * If possible, everything OS specific (IPC, virtual memory...) should be here
 */

#ifndef _HCD_COMMON_H_
#define _HCD_COMMON_H_

#include <ddekit/thread.h>
#include <ddekit/semaphore.h>
#include <ddekit/usb.h>

#include <minix/usb.h>				/* for setup structures */
#include <minix/usb_ch9.h>			/* for descriptor structures */


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
#define _HCD_REG4 				hcd_reg4 * volatile
#define _HCD_REG2 				hcd_reg2 * volatile
#define _HCD_REG1 				hcd_reg1 * volatile

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
 *    HCD device helper types                                                *
 *===========================================================================*/
typedef					void (*hcd_thread_function)(void *);
typedef ddekit_thread_t			hcd_thread;
typedef ddekit_sem_t			hcd_lock;
typedef struct hcd_driver_state		hcd_driver_state;
typedef struct ddekit_usb_urb		hcd_urb;

typedef enum {

	HCD_STATE_DISCONNECTED = 0,		/* default for initialization */
	HCD_STATE_CONNECTION_PENDING,
	HCD_STATE_CONNECTED
}
hcd_state;

typedef enum {

	HCD_SPEED_LOW,
	HCD_SPEED_FULL,
	HCD_SPEED_HIGH,
}
hcd_speed;

/* Largest value that can be transfered by this driver at a time
 * see MAXPAYLOAD in TXMAXP/RXMAXP */
#define MAX_WTOTALLENGTH 1024

typedef struct hcd_device_state {

	hcd_driver_state * driver;	/* Specific HCD driver object */
	hcd_thread * thread;
	hcd_lock * lock;
	hcd_urb * urb;
	void * data;

	hcd_device_descriptor device_desc;
	hcd_configuration config_tree;
	hcd_reg1 max_packet_size;
	hcd_speed speed;
	hcd_state state;
	int address;

	/* Number of bytes received/transmitted in last transfer */
	int data_len;

	/* TODO: forcefully align buffer to make things clear? */
	/* Buffer for each device to hold transfered data */
	hcd_reg1 buffer[MAX_WTOTALLENGTH];
}
hcd_device_state;


/*===========================================================================*
 *    HCD transfer requests                                                  *
 *===========================================================================*/
struct hcd_bulkrequest {

	char * data;
	int size;
	int endpoint;
	unsigned int max_packet_size;
	hcd_speed speed;
};

typedef struct usb_ctrlrequest		hcd_ctrlrequest;
typedef struct hcd_bulkrequest		hcd_bulkrequest;


/*===========================================================================*
 *    HCD event handling                                                     *
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

	HCD_EVENT_CONNECTED,
	HCD_EVENT_DISCONNECTED,
	HCD_EVENT_ENDPOINT,
	HCD_EVENT_URB
}
hcd_event;

/* EP event constants */
#define HCD_NO_ENDPOINT			-1
#define HCD_ENDPOINT_0			0


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
#define HCD_DEFAULT_EP		0x00
#define HCD_DEFAULT_ADDR	0x00

/* TODO: one device */
#define HCD_ATTACHED_ADDR	0x01


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
void * hcd_os_regs_init(unsigned long, unsigned long);

/* Unregisters mapped memory */
int hcd_os_regs_deinit(unsigned long, unsigned long);

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
void hcd_device_wait(hcd_device_state *, hcd_event, int);

/* Unlocks device thread halted by 'hcd_device_wait' */
void hcd_device_continue(hcd_device_state *);


/*===========================================================================*
 *    Descriptor tree calls                                                  *
 *===========================================================================*/
/* Creates descriptor tree based on given buffer */
int hcd_buffer_to_tree(hcd_reg1 *, int, hcd_configuration *);

/* Frees descriptor tree */
void hcd_tree_cleanup(hcd_configuration *);

/* Find EP in a tree */
hcd_endpoint * hcd_tree_find_ep(hcd_configuration *, int);


#endif /* !_HCD_COMMON_H_ */
