#ifndef _SERVERS_INPUT_H
#define _SERVERS_INPUT_H

#include <minix/input.h>

/* Configuration. */
#define EVENTBUF_SIZE		32

#define KBDMUX_MINOR		0
#define KBD0_MINOR		1
#define KBD_MINORS		4

#define MOUSEMUX_MINOR		64
#define MOUSE0_MINOR		65
#define MOUSE_MINORS		4

/* Constants. */
#define KBDMUX_DEV		0
#define FIRST_KBD_DEV		1
#define LAST_KBD_DEV		(FIRST_KBD_DEV + KBD_MINORS - 1)

#define MOUSEMUX_DEV		(LAST_KBD_DEV + 1)
#define FIRST_MOUSE_DEV		(MOUSEMUX_DEV + 1)
#define LAST_MOUSE_DEV		(FIRST_MOUSE_DEV + MOUSE_MINORS - 1)

#define INPUT_DEV_MAX		(1 + KBD_MINORS + 1 + MOUSE_MINORS)

/* Input device state structure. */
struct input_dev {
	devminor_t minor;		/* minor number of this device */
	endpoint_t owner;		/* owning driver endpoint, or NONE */
	char label[DS_MAX_KEYLEN];	/* label of owning driver */
	struct input_event eventbuf[EVENTBUF_SIZE];	/* event ring buffer */
	unsigned int tail;		/* tail into ring buffer */
	unsigned int count;		/* number of elements in ring buffer */
	int opened;			/* has a process opened the device? */
	int suspended;			/* is a process suspended on a read? */
	endpoint_t caller;		/* endpoint for suspended read */
	cp_grant_id_t grant;		/* grant for suspended read */
	cdev_id_t req_id;		/* request ID for suspended read */
	endpoint_t selector;		/* read-selecting endpoint, or NONE */
	unsigned int leds;		/* LED mask - saved across connects */
};

#endif /* !_SERVERS_INPUT_H */
