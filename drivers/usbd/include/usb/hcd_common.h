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
typedef struct usb_ctrlrequest			hcd_ctrlrequest;
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

/* Largest value that can be transfered by this driver at a time
 * see MAXPAYLOAD in TXMAXP/RXMAXP */
#define MAX_WTOTALLENGTH 1024

typedef struct hcd_device_state {

	hcd_driver_state * driver;
	hcd_thread * thread;
	hcd_lock * lock;
	hcd_urb * urb;
	void * data;

	hcd_device_descriptor device_desc;
	hcd_configuration config_tree;
	hcd_reg1 max_packet_size;

	hcd_state state;

	/* Number of bytes received/transmitted in last transfer */
	int data_len;
	/* TODO: forcefully align buffer to make things clear? */
	/* Buffer for each device to hold transfered data */
	hcd_reg1 buffer[MAX_WTOTALLENGTH];
}
hcd_device_state;


/*===========================================================================*
 *    Other definitions                                                      *
 *===========================================================================*/
#define HCD_NANOSLEEP_SEC(sec)		((sec)  * 1000000000)
#define HCD_NANOSLEEP_MSEC(msec)	((msec) * 1000000)
#define HCD_NANOSLEEP_USEC(usec)	((usec) * 1000)

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


/*===========================================================================*
 *    Device handling calls                                                  *
 *===========================================================================*/
/* Initializes device threading on connection */
int hcd_connect_device(hcd_device_state *, hcd_thread_function);

/* Cleans after device disconnection */
void hcd_disconnect_device(hcd_device_state *);

/* Locks device thread until 'hcd_device_continue' */
void hcd_device_wait(hcd_device_state *);

/* Unlocks device thread halted by 'hcd_device_wait' */
void hcd_device_continue(hcd_device_state *);


/*===========================================================================*
 *    Descriptor tree calls                                                  *
 *===========================================================================*/
/* Creates descriptor tree based on given buffer */
int hcd_buffer_to_tree(hcd_reg1 *, int, hcd_configuration *);

/* Frees descriptor tree */
void hcd_tree_cleanup(hcd_configuration *);


#endif /* !_HCD_COMMON_H_ */
