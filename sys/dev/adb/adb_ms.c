/*	$NetBSD: adb_ms.c,v 1.16 2014/10/29 00:48:12 macallan Exp $	*/

/*
 * Copyright (C) 1998	Colin Wood
 * Copyright (C) 2006, 2007 Michael Lorenz
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Colin Wood.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: adb_ms.c,v 1.16 2014/10/29 00:48:12 macallan Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/fcntl.h>
#include <sys/poll.h>
#include <sys/select.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>

#include <machine/autoconf.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsmousevar.h>

#include <machine/adbsys.h>
#include <dev/adb/adbvar.h>

#include "adbdebug.h"

#ifdef ADBMS_DEBUG
#define DPRINTF printf
#else
#define DPRINTF while (0) printf
#endif

/*
 * State info, per mouse instance.
 */
struct adbms_softc {
	device_t	sc_dev;
	struct adb_device *sc_adbdev;
	struct adb_bus_accessops *sc_ops;

	/* Extended Mouse Protocol info, faked for non-EMP mice */
	u_int8_t	sc_class;	/* mouse class (mouse, trackball) */
	u_int8_t	sc_buttons;	/* number of buttons */
	u_int32_t	sc_res;		/* mouse resolution (dpi) */
	char		sc_devid[5];	/* device indentifier */
	uint8_t		sc_us;		/* cmd to watch for */
	int		sc_mb;		/* current button state */
	device_t	sc_wsmousedev;
	/* helpers for trackpads */
	int		sc_down;
	/*
	 * trackpad protocol variant. Known so far:
	 * 2 buttons - PowerBook 3400, single events on button 3 and 4 indicate
	 *             finger down and up
	 * 4 buttons - iBook G4, button 6 indicates finger down, button 4 is
	 *             always down
	 */
	int		sc_x, sc_y;
	int		sc_tapping;
	/* buffers */
	int		sc_poll;
	int		sc_msg_len;
	int		sc_event;
	uint8_t		sc_buffer[16];
};

/* EMP device classes */
#define MSCLASS_TABLET		0
#define MSCLASS_MOUSE		1
#define MSCLASS_TRACKBALL	2
#define MSCLASS_TRACKPAD	3

/*
 * Function declarations.
 */
static int	adbms_match(device_t, cfdata_t, void *);
static void	adbms_attach(device_t, device_t, void *);
static void	ems_init(struct adbms_softc *);
static void	init_trackpad(struct adbms_softc *);
static void	adbms_init_mouse(struct adbms_softc *);
static void	adbms_init_turbo(struct adbms_softc *);
static void	adbms_init_uspeed(struct adbms_softc *);
static void	adbms_process_event(struct adbms_softc *, int, uint8_t *);
static int	adbms_send_sync(struct adbms_softc *, uint8_t, int, uint8_t *);

/* Driver definition. */
CFATTACH_DECL_NEW(adbms, sizeof(struct adbms_softc),
    adbms_match, adbms_attach, NULL, NULL);

static int adbms_enable(void *);
static int adbms_ioctl(void *, u_long, void *, int, struct lwp *);
static void adbms_disable(void *);

/*
 * handle tapping the trackpad
 * different pads report different button counts and use slightly different
 * protocols
 */
static void adbms_mangle_2(struct adbms_softc *, int);
static void adbms_mangle_4(struct adbms_softc *, int);
static void adbms_handler(void *, int, uint8_t *);
static int  adbms_wait(struct adbms_softc *, int);
static int  sysctl_adbms_tap(SYSCTLFN_ARGS);

const struct wsmouse_accessops adbms_accessops = {
	adbms_enable,
	adbms_ioctl,
	adbms_disable,
};

static int
adbms_match(device_t parent, cfdata_t cf, void *aux)
{
	struct adb_attach_args *aaa = aux;

	if (aaa->dev->original_addr == ADBADDR_MS)
		return 1;
	else
		return 0;
}

static void
adbms_attach(device_t parent, device_t self, void *aux)
{
	struct adbms_softc *sc = device_private(self);
	struct adb_attach_args *aaa = aux;
	struct wsmousedev_attach_args a;

	sc->sc_dev = self;
	sc->sc_ops = aaa->ops;
	sc->sc_adbdev = aaa->dev;
	sc->sc_adbdev->cookie = sc;
	sc->sc_adbdev->handler = adbms_handler;
	sc->sc_us = ADBTALK(sc->sc_adbdev->current_addr, 0);
	printf(" addr %d: ", sc->sc_adbdev->current_addr);

	sc->sc_class = MSCLASS_MOUSE;
	sc->sc_buttons = 1;
	sc->sc_res = 100;
	sc->sc_devid[0] = 0;
	sc->sc_devid[4] = 0;
	sc->sc_poll = 0;
	sc->sc_msg_len = 0;
	sc->sc_tapping = 1;

	ems_init(sc);

	/* print out the type of mouse we have */
	switch (sc->sc_adbdev->handler_id) {
	case ADBMS_100DPI:
		printf("%d-button, %u dpi mouse\n", sc->sc_buttons,
		    sc->sc_res);
		break;
	case ADBMS_200DPI:
		sc->sc_res = 200;
		printf("%d-button, %u dpi mouse\n", sc->sc_buttons,
		    sc->sc_res);
		break;
	case ADBMS_MSA3:
		printf("Mouse Systems A3 mouse, %d-button, %u dpi\n",
		    sc->sc_buttons, sc->sc_res);
		break;
	case ADBMS_USPEED:
		printf("MicroSpeed mouse, default parameters\n");
		break;
	case ADBMS_UCONTOUR:
		printf("Contour mouse, default parameters\n");
		break;
	case ADBMS_TURBO:
		printf("Kensington Turbo Mouse\n");
		break;
	case ADBMS_EXTENDED:
		if (sc->sc_devid[0] == '\0') {
			printf("Logitech ");
			switch (sc->sc_class) {
			case MSCLASS_MOUSE:
				printf("MouseMan (non-EMP) mouse");
				break;
			case MSCLASS_TRACKBALL:
				printf("TrackMan (non-EMP) trackball");
				break;
			default:
				printf("non-EMP relative positioning device");
				break;
			}
			printf("\n");
		} else {
			printf("EMP ");
			switch (sc->sc_class) {
			case MSCLASS_TABLET:
				printf("tablet");
				break;
			case MSCLASS_MOUSE:
				printf("mouse");
				break;
			case MSCLASS_TRACKBALL:
				printf("trackball");
				break;
			case MSCLASS_TRACKPAD:
				printf("trackpad");
				init_trackpad(sc);
				break;
			default:
				printf("unknown device");
				break;
			}
			printf(" <%s> %d-button, %u dpi\n", sc->sc_devid,
			    sc->sc_buttons, sc->sc_res);
		}
		break;
	default:
		printf("relative positioning device (mouse?) (%d)\n",
			sc->sc_adbdev->handler_id);
		break;
	}

	a.accessops = &adbms_accessops;
	a.accesscookie = sc;
	sc->sc_wsmousedev = config_found(self, &a, wsmousedevprint);
}


/*
 * Initialize extended mouse support -- probes devices as described
 * in Inside Macintosh: Devices, Chapter 5 "ADB Manager".
 *
 * Extended Mouse Protocol is documented in TechNote HW1:
 * 	"ADB - The Untold Story:  Space Aliens Ate My Mouse"
 *
 * Supports: Extended Mouse Protocol, MicroSpeed Mouse Deluxe,
 * 	     Mouse Systems A^3 Mouse, Logitech non-EMP MouseMan
 */
void
ems_init(struct adbms_softc *sc)
{
	
	DPRINTF("ems_init %d\n", sc->sc_adbdev->handler_id);

	switch (sc->sc_adbdev->handler_id) {
		case ADBMS_USPEED:
		case ADBMS_UCONTOUR:
			adbms_init_uspeed(sc);
			return;
		case ADBMS_TURBO:
			adbms_init_turbo(sc);
			return;
		case ADBMS_100DPI:
		case ADBMS_200DPI:
			adbms_init_mouse(sc);
	}
}

static void
adbms_init_uspeed(struct adbms_softc *sc)
{
	uint8_t cmd, addr, buffer[4];

	addr = sc->sc_adbdev->current_addr;

	/* Found MicroSpeed Mouse Deluxe Mac or Contour Mouse */
	cmd = ADBLISTEN(addr, 1);

	/*
	 * To setup the MicroSpeed or the Contour, it appears
	 * that we can send the following command to the mouse
	 * and then expect data back in the form:
	 *  buffer[0] = 4 (bytes)
	 *  buffer[1], buffer[2] as std. mouse
	 *  buffer[3] = buffer[4] = 0xff when no buttons
	 *   are down.  When button N down, bit N is clear.
	 * buffer[4]'s locking mask enables a
	 * click to toggle the button down state--sort of
	 * like the "Easy Access" shift/control/etc. keys.
	 * buffer[3]'s alternative speed mask enables using
	 * different speed when the corr. button is down
	 */
	buffer[0] = 0x00;	/* Alternative speed */
	buffer[1] = 0x00;	/* speed = maximum */
	buffer[2] = 0x10;	/* enable extended protocol,
				 * lower bits = alt. speed mask
				 *            = 0000b
				 */
	buffer[3] = 0x07;	/* Locking mask = 0000b,
				 * enable buttons = 0111b
				 */
	adbms_send_sync(sc, cmd, 4, buffer);

	sc->sc_buttons = 3;
	sc->sc_res = 200;
}

static void
adbms_init_turbo(struct adbms_softc *sc)
{
	uint8_t addr;

	/* Found Kensington Turbo Mouse */
	static u_char data1[] =
		{ 0xe7, 0x8c, 0, 0, 0, 0xff, 0xff, 0x94 };
	static u_char data2[] =
		{ 0xa5, 0x14, 0, 0, 0x69, 0xff, 0xff, 0x27 };

	addr = sc->sc_adbdev->current_addr;

	adbms_send_sync(sc, ADBFLUSH(addr), 0, NULL);
	adbms_send_sync(sc, ADBLISTEN(addr, 2), 8, data1);
	adbms_send_sync(sc, ADBFLUSH(addr), 0, NULL);
	adbms_send_sync(sc, ADBLISTEN(addr, 2), 8, data2);
}

static void
adbms_init_mouse(struct adbms_softc *sc)
{
	int len;
	uint8_t cmd, addr, buffer[16];

	addr = sc->sc_adbdev->current_addr;
	/* found a mouse */
	cmd = ADBTALK(addr, 3);
	if (!adbms_send_sync(sc, cmd, 0, NULL)) {
#ifdef ADBMS_DEBUG
		printf("adb: ems_init timed out\n");
#endif
		return;
	}

	/* Attempt to initialize Extended Mouse Protocol */
	len = sc->sc_msg_len;
	memcpy(buffer, sc->sc_buffer, len);
	DPRINTF("buffer: %02x %02x\n", buffer[0], buffer[1]);
	buffer[1] = 4; /* make handler ID 4 */
	cmd = ADBLISTEN(addr, 3);
	if (!adbms_send_sync(sc, cmd, len, buffer)) {
#ifdef ADBMS_DEBUG
		printf("adb: ems_init timed out\n");
#endif
		return;
	}

	/*
	 * Check to see if successful, if not
	 * try to initialize it as other types
	 */
	cmd = ADBTALK(addr, 3);
	if (!adbms_send_sync(sc, cmd, 0, NULL)) {
		DPRINTF("timeout checking for EMP switch\n");
		return;
	}
	DPRINTF("new handler ID: %02x\n", sc->sc_buffer[1]);
	if (sc->sc_buffer[1] == ADBMS_EXTENDED) {
		sc->sc_adbdev->handler_id = ADBMS_EXTENDED;
		cmd = ADBTALK(addr, 1);
		if(!adbms_send_sync(sc, cmd, 0, NULL)) {
			DPRINTF("adb: ems_init timed out\n");
			return;
		}

		len = sc->sc_msg_len;
		memcpy(buffer, sc->sc_buffer, len);

		if (sc->sc_msg_len == 8) {
			uint16_t res;
			/* we have a true EMP device */
#ifdef ADB_PRINT_EMP
		
			printf("EMP: %02x %02x %02x %02x %02x %02x %02x %02x\n",
			    buffer[0], buffer[1], buffer[2], buffer[3],
			    buffer[4], buffer[5], buffer[6], buffer[7]);
#endif
			memcpy(sc->sc_devid, &buffer[0], 4);
			memcpy(&res, &buffer[4], sizeof(res));
			sc->sc_res = res;
			sc->sc_class = buffer[6];
			sc->sc_buttons = buffer[7];
		} else if (buffer[0] == 0x9a &&
		    ((buffer[1] == 0x20) || (buffer[1] == 0x21))) {
			/*
			 * Set up non-EMP Mouseman/Trackman to put
			 * button bits in 3rd byte instead of sending
			 * via pseudo keyboard device.
			 */
			if (buffer[1] == 0x21)
				sc->sc_class = MSCLASS_TRACKBALL;
			else
				sc->sc_class = MSCLASS_MOUSE;

			cmd = ADBLISTEN(addr, 1);
			buffer[0]=0x00;
			buffer[1]=0x81;
			adbms_send_sync(sc, cmd, 2, buffer);

			cmd = ADBLISTEN(addr, 1);
			buffer[0]=0x01;
			buffer[1]=0x81;
			adbms_send_sync(sc, cmd, 2, buffer);

			cmd = ADBLISTEN(addr, 1);
			buffer[0]=0x02;
			buffer[1]=0x81;
			adbms_send_sync(sc, cmd, 2, buffer);

			cmd = ADBLISTEN(addr, 1);
			buffer[0]=0x03;
			buffer[1]=0x38;
			adbms_send_sync(sc, cmd, 2, buffer);

			sc->sc_buttons = 3;
			sc->sc_res = 400;
		}
	} else {
		/* Attempt to initialize as an A3 mouse */
		buffer[1] = 0x03; /* make handler ID 3 */
		cmd = ADBLISTEN(addr, 3);
		if (!adbms_send_sync(sc, cmd, len, buffer)) {
#ifdef ADBMS_DEBUG
			printf("adb: ems_init timed out\n");
#endif
			return;
		}

		/*
		 * Check to see if successful, if not
		 * try to initialize it as other types
		 */
		cmd = ADBTALK(addr, 3);
		if(adbms_send_sync(sc, cmd, 0, NULL)) {
			len = sc->sc_msg_len;
			memcpy(buffer, sc->sc_buffer, len);
			if (buffer[1] == ADBMS_MSA3) {
				sc->sc_adbdev->handler_id = ADBMS_MSA3;
				/* Initialize as above */
				cmd = ADBLISTEN(addr, 2);
				/* listen 2 */
				buffer[0] = 0x00;
				/* Irrelevant, buffer has 0x77 */
				buffer[2] = 0x07;
				/*
				 * enable 3 button mode = 0111b,
				 * speed = normal
				 */
				adbms_send_sync(sc, cmd, 3, buffer);
				sc->sc_buttons = 3;
				sc->sc_res = 300;
			}
		}
	}
}

static void
adbms_handler(void *cookie, int len, uint8_t *data)
{
	struct adbms_softc *sc = cookie;

#ifdef ADBMS_DEBUG
	int i;
	printf("%s: %02x - ", device_xname(sc->sc_dev), sc->sc_us);
	for (i = 0; i < len; i++) {
		printf(" %02x", data[i]);
	}
	printf("\n");
#endif
	if (len >= 2) {
		memcpy(sc->sc_buffer, &data[2], len - 2);
		sc->sc_msg_len = len - 2;
		if (data[1] == sc->sc_us) {
			/* make sense of the mouse message */
			adbms_process_event(sc, sc->sc_msg_len, sc->sc_buffer);
			return;
		}
		wakeup(&sc->sc_event);
	} else {
		DPRINTF("bogus message\n");
	}
}

static void
adbms_process_event(struct adbms_softc *sc, int len, uint8_t *buffer)
{
	int buttons = 0, mask, dx, dy, i;
	int button_bit = 1;

	if ((sc->sc_adbdev->handler_id == ADBMS_EXTENDED) && (sc->sc_devid[0] == 0)) {
		/* massage the data to look like EMP data */
		if ((buffer[2] & 0x04) == 0x04)
			buffer[0] &= 0x7f;
		else
			buffer[0] |= 0x80;
		if ((buffer[2] & 0x02) == 0x02)
			buffer[1] &= 0x7f;
		else
			buffer[1] |= 0x80;
		if ((buffer[2] & 0x01) == 0x01)
			buffer[2] = 0x00;
		else
			buffer[2] = 0x80;
	}

	switch (sc->sc_adbdev->handler_id) {
		case ADBMS_USPEED:
		case ADBMS_UCONTOUR:
			/* MicroSpeed mouse and Contour mouse */
			if (len == 4)
				buttons = (~buffer[3]) & 0xff;
			else
				buttons = (buffer[1] & 0x80) ? 0 : 1;
			break;
		case ADBMS_MSA3:
			/* Mouse Systems A3 mouse */
			if (len == 3)
				buttons = (~buffer[2]) & 0x07;
			else
				buttons = (buffer[0] & 0x80) ? 0 : 1;
			break;
		default:	
			/* Classic Mouse Protocol (up to 2 buttons) */
			for (i = 0; i < 2; i++, button_bit <<= 1)
				/* 0 when button down */
				if (!(buffer[i] & 0x80))
					buttons |= button_bit;
				else
					buttons &= ~button_bit;
			/* Extended Protocol (up to 6 more buttons) */
			for (mask = 0x80; i < len;
			     i += (mask == 0x80), button_bit <<= 1) {
				/* 0 when button down */
				if (!(buffer[i] & mask))
					buttons |= button_bit;
				else
					buttons &= ~button_bit;
				mask = ((mask >> 4) & 0xf)
					| ((mask & 0xf) << 4);
			}				
			break;
	}

	if (sc->sc_adbdev->handler_id != ADBMS_EXTENDED) {
		dx = ((int)(buffer[1] & 0x3f)) - ((buffer[1] & 0x40) ? 64 : 0);
		dy = ((int)(buffer[0] & 0x3f)) - ((buffer[0] & 0x40) ? 64 : 0);
	} else {
		/* EMP crap, additional motion bits */
		int shift = 7, ddx, ddy, sign, smask;

#ifdef ADBMS_DEBUG
		printf("EMP packet:");
		for (i = 0; i < len; i++)
			printf(" %02x", buffer[i]);
		printf("\n");
#endif
		dx = (int)buffer[1] & 0x7f;
		dy = (int)buffer[0] & 0x7f;
		for (i = 2; i < len; i++) {
			ddx = (buffer[i] & 0x07);
			ddy = (buffer[i] & 0x70) >> 4;
			dx |= (ddx << shift);
			dy |= (ddy << shift);
			shift += 3;
		}
		sign = 1 << (shift - 1);
		smask = 0xffffffff << shift;
		if (dx & sign)
			dx |= smask;
		if (dy & sign)
			dy |= smask;
#ifdef ADBMS_DEBUG
		printf("%d %d %08x %d\n", dx, dy, smask, shift);
#endif
	}

	if (sc->sc_class == MSCLASS_TRACKPAD) {

		if (sc->sc_tapping == 1) {
			if (sc->sc_down) {
				/* finger is down - collect motion data */
				sc->sc_x += dx;
				sc->sc_y += dy;
			}
			DPRINTF("buttons: %02x\n", buttons);
			switch (sc->sc_buttons) {
				case 2:
					buttons |= ((buttons & 2) >> 1);
					adbms_mangle_2(sc, buttons);
					break;
				case 4:
					adbms_mangle_4(sc, buttons);
					break;
			}
		}
		/* filter the pseudo-buttons out */
		buttons &= 1;
	}

	if (sc->sc_wsmousedev)
		wsmouse_input(sc->sc_wsmousedev, sc->sc_mb | buttons,
			      dx, -dy, 0, 0,
			      WSMOUSE_INPUT_DELTA);
#if NAED > 0
	aed_input(&new_event);
#endif
}

static void
adbms_mangle_2(struct adbms_softc *sc, int buttons)
{

	if (buttons & 4) {
		/* finger down on pad */
		if (sc->sc_down == 0) {
			sc->sc_down = 1;
			sc->sc_x = 0;
			sc->sc_y = 0;
		}
	}
	if (buttons & 8) {
		/* finger up */
		if (sc->sc_down) {
			if (((sc->sc_x * sc->sc_x + 
			    sc->sc_y * sc->sc_y) < 3) && 
			    (sc->sc_wsmousedev)) {
				/* 
				 * if there wasn't much movement between
				 * finger down and up again we assume
				 * someone tapped the pad and we just
				 * send a mouse button event
				 */
				wsmouse_input(sc->sc_wsmousedev,
				    1, 0, 0, 0, 0, WSMOUSE_INPUT_DELTA);
			}
			sc->sc_down = 0;
		}
	}
}

static void
adbms_mangle_4(struct adbms_softc *sc, int buttons)
{

	if (buttons & 0x20) {
		/* finger down on pad */
		if (sc->sc_down == 0) {
			sc->sc_down = 1;
			sc->sc_x = 0;
			sc->sc_y = 0;
		}
	}
	if ((buttons & 0x20) == 0) {
		/* finger up */
		if (sc->sc_down) {
			if (((sc->sc_x * sc->sc_x + 
			    sc->sc_y * sc->sc_y) < 3) && 
			    (sc->sc_wsmousedev)) {
				/* 
				 * if there wasn't much movement between
				 * finger down and up again we assume
				 * someone tapped the pad and we just
				 * send a mouse button event
				 */
				wsmouse_input(sc->sc_wsmousedev,
				    1, 0, 0, 0, 0, WSMOUSE_INPUT_DELTA);
			}
			sc->sc_down = 0;
		}
	}
}

static int
adbms_enable(void *v)
{
	return 0;
}

static int
adbms_ioctl(void *v, u_long cmd, void *data, int flag, struct lwp *l)
{

	switch (cmd) {
	case WSMOUSEIO_GTYPE:
		*(u_int *)data = WSMOUSE_TYPE_ADB;
		break;

	default:
		return (EPASSTHROUGH);
	}
	return (0);
}

static void
adbms_disable(void *v)
{
}

static void
init_trackpad(struct adbms_softc *sc)
{
	const struct sysctlnode *me = NULL, *node = NULL;
	int cmd, addr, ret;
	uint8_t buffer[16];
	uint8_t b2[] = {0x99, 0x94, 0x19, 0xff, 0xb2, 0x8a, 0x1b, 0x50};
	
	addr = sc->sc_adbdev->current_addr;
	cmd = ADBTALK(addr, 1);
	if (!adbms_send_sync(sc, cmd, 0, NULL))
		return;

	if (sc->sc_msg_len != 8)
		return;

	memcpy(buffer, sc->sc_buffer, 8);

	/* now whack the pad */
	cmd = ADBLISTEN(addr, 1);
	buffer[6] = 0x0d;
	adbms_send_sync(sc, cmd, 8, buffer);

	delay(1000);
	cmd = ADBLISTEN(addr, 2);
	adbms_send_sync(sc, cmd, 8, b2);

	delay(1000);
	cmd = ADBLISTEN(addr, 1);
	buffer[6] = 0x03;
	adbms_send_sync(sc, cmd, 8, buffer);

	cmd = ADBFLUSH(addr);
	adbms_send_sync(sc, cmd, 0, NULL);
	delay(1000);

	/*
	 * setup a sysctl node to control whether tapping the pad should
	 * trigger mouse button events
	 */

	sc->sc_tapping = 1;
	
	ret = sysctl_createv(NULL, 0, NULL, &me,
	    CTLFLAG_READWRITE,
	    CTLTYPE_NODE, device_xname(sc->sc_dev), NULL,
	    NULL, 0, NULL, 0,
	    CTL_MACHDEP, CTL_CREATE, CTL_EOL);

	ret = sysctl_createv(NULL, 0, NULL, &node,
	    CTLFLAG_READWRITE | CTLFLAG_OWNDESC,
	    CTLTYPE_INT, "tapping", "tapping the pad causes button events",
	    sysctl_adbms_tap, 1, (void *)sc, 0,
	    CTL_MACHDEP, me->sysctl_num, CTL_CREATE, CTL_EOL);

	(void)ret;
}

static int
adbms_wait(struct adbms_softc *sc, int timeout)
{
	int cnt = 0;
	
	if (sc->sc_poll) {
		while (sc->sc_msg_len == -1) {
			sc->sc_ops->poll(sc->sc_ops->cookie);
		}
	} else {
		while ((sc->sc_msg_len == -1) && (cnt < timeout)) {
			tsleep(&sc->sc_event, 0, "adbmsio", hz);
			cnt++;
		}
	}
	return (sc->sc_msg_len > 0);
}

static int
adbms_send_sync(struct adbms_softc *sc, uint8_t cmd, int len, uint8_t *msg)
{
	int i;

	sc->sc_msg_len = -1;
	DPRINTF("send: %02x", cmd);
	for (i = 0; i < len; i++)
		DPRINTF(" %02x", msg[i]);
	DPRINTF("\n");
	sc->sc_ops->send(sc->sc_ops->cookie, sc->sc_poll, cmd, len, msg);
	adbms_wait(sc, 1000);
	return (sc->sc_msg_len != -1);
}

static int
sysctl_adbms_tap(SYSCTLFN_ARGS)
{
	struct sysctlnode node = *rnode;
	struct adbms_softc *sc = node.sysctl_data;

	node.sysctl_idata = sc->sc_tapping;

	if (newp) {

		/* we're asked to write */	
		node.sysctl_data = &sc->sc_tapping;
		if (sysctl_lookup(SYSCTLFN_CALL(&node)) == 0) {

			sc->sc_tapping = (*(int *)node.sysctl_data == 0) ? 0 : 1;
			return 0;
		}
		return EINVAL;
	} else {

		node.sysctl_data = &sc->sc_tapping;
		node.sysctl_size = 4;
		return (sysctl_lookup(SYSCTLFN_CALL(&node)));
	}

	return 0;
}

SYSCTL_SETUP(sysctl_ams_setup, "sysctl ams subtree setup")
{

	sysctl_createv(NULL, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "machdep", NULL,
		       NULL, 0, NULL, 0,
		       CTL_MACHDEP, CTL_EOL);
}
