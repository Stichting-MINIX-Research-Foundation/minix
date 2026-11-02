/*	minix/evdev.h - Linux evdev compatible input devices.
 *
 * The input server exports, next to its native devices (/dev/kbdmux,
 * /dev/mousemux, ...), Linux "evdev" compatible event devices:
 *
 *	/dev/input/event0	all keyboards	(input major, minor 128)
 *	/dev/input/event1	all mice	(input major, minor 129)
 *
 * They speak the Linux evdev ABI as compiled on MINIX: a read returns whole
 * evdev_event records (the layout of Linux' struct input_event, using the
 * MINIX struct timeval), grouped into frames that end with SYN_REPORT, with
 * timestamps from CLOCK_MONOTONIC (or CLOCK_REALTIME, see EVDEV_IOCSCLOCKID).
 * The usual capability and state ioctls are supported.  Ported software
 * (libevdev, libinput) uses its own copy of Linux <linux/input.h>, which on
 * MINIX must yield the same ioctl numbers as below; in particular EVIOCGRAB
 * takes a pointer to an int here (the BSD ioctl encoding cannot pass an
 * immediate int argument through MINIX' VFS).
 *
 * While an evdev device is open, it receives all input of its kind: neither
 * the multiplexers nor the console (TTY) see keyboard or mouse input then,
 * so EVIOCGRAB is implied.  Each device can be opened by one process at a
 * time (EBUSY otherwise).
 *
 * Only the definitions that MINIX itself needs are here; event codes are
 * the Linux ones (see Linux' input-event-codes.h).
 *
 * $MINIX$
 */

#ifndef _MINIX_EVDEV_H
#define _MINIX_EVDEV_H

#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioccom.h>

/* Minor numbers of the evdev devices of the input server. */
#define EVDEV_KBD_MINOR		128	/* /dev/input/event0: keyboards */
#define EVDEV_MOUSE_MINOR	129	/* /dev/input/event1: mice */

/* One event, laid out as Linux' struct input_event. */
struct evdev_event {
	struct timeval time;		/* timestamp */
	uint16_t type;			/* EVDEV_EV_ */
	uint16_t code;			/* type-specific code */
	int32_t value;			/* type-specific value */
};

/* Device identification, laid out as Linux' struct input_id. */
struct evdev_id {
	uint16_t bustype;
	uint16_t vendor;
	uint16_t product;
	uint16_t version;
};

#define EVDEV_VERSION		0x010001	/* protocol version */

/* Event types. */
#define EVDEV_EV_SYN		0x00
#define EVDEV_EV_KEY		0x01
#define EVDEV_EV_REL		0x02
#define EVDEV_EV_MSC		0x04
#define EVDEV_EV_LED		0x11
#define EVDEV_EV_MAX		0x1f

/* EV_SYN codes. */
#define EVDEV_SYN_REPORT	0
#define EVDEV_SYN_DROPPED	3	/* events were lost; resynchronize */

/* EV_KEY: key codes 0..EVDEV_KEY_MAX; mouse buttons start at BTN_LEFT. */
#define EVDEV_BTN_LEFT		0x110
#define EVDEV_BTN_RIGHT		0x111
#define EVDEV_BTN_MIDDLE	0x112
#define EVDEV_BTN_SIDE		0x113
#define EVDEV_BTN_EXTRA		0x114
#define EVDEV_KEY_MAX		0x2ff

/* EV_REL codes. */
#define EVDEV_REL_X		0x00
#define EVDEV_REL_Y		0x01
#define EVDEV_REL_HWHEEL	0x06
#define EVDEV_REL_WHEEL		0x08
#define EVDEV_REL_MAX		0x0f

/* EV_LED codes. */
#define EVDEV_LED_NUML		0x00
#define EVDEV_LED_CAPSL		0x01
#define EVDEV_LED_SCROLLL	0x02
#define EVDEV_LED_MAX		0x0f

/* Bus type of the PS/2 controller devices. */
#define EVDEV_BUS_I8042		0x11

/* IOCTLs (group 'E'), numbered as in Linux. */
#define EVDEV_IOCGVERSION	_IOR('E', 0x01, int)
#define EVDEV_IOCGID		_IOR('E', 0x02, struct evdev_id)
#define EVDEV_IOCGNAME(len)	_IOC(IOC_OUT, 'E', 0x06, len)
#define EVDEV_IOCGPHYS(len)	_IOC(IOC_OUT, 'E', 0x07, len)
#define EVDEV_IOCGUNIQ(len)	_IOC(IOC_OUT, 'E', 0x08, len)
#define EVDEV_IOCGPROP(len)	_IOC(IOC_OUT, 'E', 0x09, len)
#define EVDEV_IOCGKEY(len)	_IOC(IOC_OUT, 'E', 0x18, len)
#define EVDEV_IOCGLED(len)	_IOC(IOC_OUT, 'E', 0x19, len)
#define EVDEV_IOCGSND(len)	_IOC(IOC_OUT, 'E', 0x1a, len)
#define EVDEV_IOCGSW(len)	_IOC(IOC_OUT, 'E', 0x1b, len)
#define EVDEV_IOCGBIT(ev, len)	_IOC(IOC_OUT, 'E', 0x20 + (ev), len)
#define EVDEV_IOCGRAB		_IOW('E', 0x90, int)
#define EVDEV_IOCSCLOCKID	_IOW('E', 0xa0, int)

/* Ioctl number fields, for the variable-length requests above. */
#define EVDEV_IOC_NR(req)	((req) & 0xff)

#endif /* !_MINIX_EVDEV_H */
