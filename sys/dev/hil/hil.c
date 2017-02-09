/*	$NetBSD: hil.c,v 1.2 2011/02/15 11:05:51 tsutsui Exp $	*/
/*	$OpenBSD: hil.c,v 1.24 2010/11/20 16:45:46 miod Exp $	*/
/*
 * Copyright (c) 2003, 2004, Miodrag Vallat.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: Utah $Hdr: hil.c 1.38 92/01/21$
 *
 *	@(#)hil.c	8.2 (Berkeley) 1/12/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/kthread.h>
#include <sys/bus.h>
#include <sys/cpu.h>

#include <machine/autoconf.h>

#include <dev/hil/hilreg.h>
#include <dev/hil/hilvar.h>
#include <dev/hil/hildevs.h>
#include <dev/hil/hildevs_data.h>

#include "hilkbd.h"

static void	hilconfig(struct hil_softc *, u_int);
static void	hilempty(struct hil_softc *);
static int	hilsubmatch(device_t, cfdata_t, const int *, void *);
static void	hil_process_int(struct hil_softc *, uint8_t, uint8_t);
static int	hil_process_poll(struct hil_softc *, uint8_t, uint8_t);
static void	hil_thread(void *);
static int	send_device_cmd(struct hil_softc *sc, u_int device, u_int cmd);
static void	polloff(struct hil_softc *);
static void	pollon(struct hil_softc *);

static int hilwait(struct hil_softc *);
static int hildatawait(struct hil_softc *);

#define	hil_process_pending(sc)	wakeup(&(sc)->sc_pending)

static __inline int
hilwait(struct hil_softc *sc)
{
	int cnt;

	for (cnt = 50000; cnt != 0; cnt--) {
		DELAY(1);
		if ((bus_space_read_1(sc->sc_bst, sc->sc_bsh, HILP_STAT) &
		    HIL_BUSY) == 0)
			break;
	}

	return cnt;
}

static __inline int
hildatawait(struct hil_softc *sc)
{
	int cnt;

	for (cnt = 50000; cnt != 0; cnt--) {
		DELAY(1);
		if ((bus_space_read_1(sc->sc_bst, sc->sc_bsh, HILP_STAT) &
		    HIL_DATA_RDY) != 0)
			break;
	}

	return cnt;
}

/*
 * Common HIL bus attachment
 */

void
hil_attach(struct hil_softc *sc, int *hil_is_console)
{

	aprint_normal("\n");

	/*
	 * Initialize loop information
	 */
	sc->sc_cmdending = 0;
	sc->sc_actdev = sc->sc_cmddev = 0;
	sc->sc_cmddone = 0;
	sc->sc_cmdbp = sc->sc_cmdbuf;
	sc->sc_pollbp = sc->sc_pollbuf;
	sc->sc_console = hil_is_console;
}

/*
 * HIL subdevice attachment
 */

int
hildevprint(void *aux, const char *pnp)
{
	struct hil_attach_args *ha = aux;

	if (pnp != NULL) {
		aprint_normal("\"%s\" at %s id %x",
		    ha->ha_descr, pnp, ha->ha_id);
	}
	aprint_normal(" code %d", ha->ha_code);
	if (pnp == NULL) {
		aprint_normal(": %s", ha->ha_descr);
	}

	return UNCONF;
}

int
hilsubmatch(device_t parent, cfdata_t cf, const int *ldesc, void *aux)
{
	struct hil_attach_args *ha = aux;

	if (cf->cf_loc[0] != -1 &&
	    cf->cf_loc[0] != ha->ha_code)
		return 0;

	return config_match(parent, cf, aux);
}

void
hil_attach_deferred(device_t self)
{
	struct hil_softc *sc = device_private(self);
	int tries;
	uint8_t db;

	sc->sc_status = HIL_STATUS_BUSY;

	/*
	 * Initialize the loop: reconfigure, don't report errors,
	 * put keyboard in cooked mode, and enable autopolling.
	 */
	db = LPC_RECONF | LPC_KBDCOOK | LPC_NOERROR | LPC_AUTOPOLL;
	send_hil_cmd(sc, HIL_WRITELPCTRL, &db, 1, NULL);

	/*
	 * Delay one second for reconfiguration and then read the
	 * data to clear the interrupt (if the loop reconfigured).
	 */
	DELAY(1000000);
	if (bus_space_read_1(sc->sc_bst, sc->sc_bsh, HILP_STAT) &
	    HIL_DATA_RDY) {
		db = bus_space_read_1(sc->sc_bst, sc->sc_bsh, HILP_DATA);
		DELAY(1);
	}

	/*
	 * The HIL loop may have reconfigured.  If so we proceed on,
	 * if not we loop a few times until a successful reconfiguration
	 * is reported back to us. If the HIL loop is still lost after a
	 * few seconds, give up.
	 */
	for (tries = 10; tries != 0; tries--) {
		if (send_hil_cmd(sc, HIL_READLPSTAT, NULL, 0, &db) == 0) {
			if (db & (LPS_CONFFAIL | LPS_CONFGOOD))
				break;
		}

#ifdef HILDEBUG
		aprint_debug(self, "%s: loop not ready, retrying...\n");
#endif

		DELAY(1000000);
        }

	if (tries == 0 || (db & LPS_CONFFAIL)) {
		aprint_normal_dev(self, "no devices\n");
		sc->sc_pending = 0;
		if (tries == 0)
			return;
	}

	/*
	 * Create asynchronous loop event handler thread.
	 */
	if (kthread_create(PRI_NONE, 0, NULL, hil_thread, sc, &sc->sc_thread,
	    "%s", device_xname(sc->sc_dev)) != 0) {
		aprint_error_dev(self, "unable to create event thread\n");
		return;
	}

	/*
	 * Enable loop interrupts.
	 */
	send_hil_cmd(sc, HIL_INTON, NULL, 0, NULL);

	/*
	 * Reconfigure if necessary
	 */
	sc->sc_status = HIL_STATUS_READY;
	hil_process_pending(sc);
}

/*
 * Asynchronous event processing
 */

int
hil_intr(void *v)
{
	struct hil_softc *sc = v;
	uint8_t c, stat;

	if (cold)
		return 0;

	stat = bus_space_read_1(sc->sc_bst, sc->sc_bsh, HILP_STAT);

	/*
	 * This should never happen if the interrupt comes from the
	 * loop.
	 */
	if ((stat & HIL_DATA_RDY) == 0)
		return 0;	/* not for us */

	c = bus_space_read_1(sc->sc_bst, sc->sc_bsh,
	    HILP_DATA);	/* clears interrupt */
	DELAY(1);

	hil_process_int(sc, stat, c);

	if (sc->sc_status != HIL_STATUS_BUSY)
		hil_process_pending(sc);

	return 1;
}

void
hil_process_int(struct hil_softc *sc, uint8_t stat, uint8_t c)
{
	device_t child;
	struct hildev_softc *hdsc;

	switch ((stat >> HIL_SSHIFT) & HIL_SMASK) {
	case HIL_STATUS:
		if (c & HIL_ERROR) {
		  	sc->sc_cmddone = 1;
			switch (c) {
			case HIL_RECONFIG:
				sc->sc_pending = HIL_PENDING_RECONFIG;
				break;
			case HIL_UNPLUGGED:
				sc->sc_pending = HIL_PENDING_UNPLUGGED;
				break;
			}
			break;
		}
		if (c & HIL_COMMAND) {
		  	if (c & HIL_POLLDATA) {	/* End of data */
				child = sc->sc_devices[sc->sc_actdev];
				if (child != NULL) {
					hdsc = device_private(child);
					if (hdsc->sc_fn != NULL)
						(*hdsc->sc_fn)(hdsc,
						    sc->sc_pollbp
						    - sc->sc_pollbuf,
						    sc->sc_pollbuf);
				}
			} else {		/* End of command */
			  	sc->sc_cmdending = 1;
			}
			sc->sc_actdev = 0;
		} else {
		  	if (c & HIL_POLLDATA) {	/* Start of polled data */
				sc->sc_actdev = (c & HIL_DEVMASK);
				sc->sc_pollbp = sc->sc_pollbuf;
			} else {		/* Start of command */
				if (sc->sc_cmddev == (c & HIL_DEVMASK)) {
					sc->sc_cmdbp = sc->sc_cmdbuf;
					sc->sc_actdev = 0;
				}
			}
		}
	        break;
	case HIL_DATA:
		if (sc->sc_actdev != 0)	/* Collecting poll data */
			*sc->sc_pollbp++ = c;
		else {
			if (sc->sc_cmddev != 0) {  /* Collecting cmd data */
				if (sc->sc_cmdending) {
					sc->sc_cmddone = 1;
					sc->sc_cmdending = 0;
				} else  
					*sc->sc_cmdbp++ = c;
		        }
		}
		break;
	}
}

/*
 * Same as above, but in polled mode: return data as it gets seen, instead
 * of buffering it.
 */
int
hil_process_poll(struct hil_softc *sc, uint8_t stat, uint8_t c)
{
	uint8_t db;

	switch ((stat >> HIL_SSHIFT) & HIL_SMASK) {
	case HIL_STATUS:
		if (c & HIL_ERROR) {
		  	sc->sc_cmddone = 1;
			switch (c) {
			case HIL_RECONFIG:
				/*
				 * Remember that a configuration event
				 * occurred; it will be processed upon
				 * leaving polled mode...
				 */
				sc->sc_pending = HIL_PENDING_RECONFIG;
				/*
				 * However, the keyboard will come back as
				 * cooked, and we rely on it being in raw
				 * mode. So, put it back in raw mode right
				 * now.
				 */
				db = 0;
				send_hil_cmd(sc, HIL_WRITEKBDSADR, &db,
				    1, NULL);
				break;
			case HIL_UNPLUGGED:
				/*
				 * Remember that an unplugged event
				 * occured; it will be processed upon
				 * leaving polled mode...
				 */
				sc->sc_pending = HIL_PENDING_UNPLUGGED;
				break;
			}
			break;
		}
		if (c & HIL_COMMAND) {
		  	if (!(c & HIL_POLLDATA)) {
				/* End of command */
			  	sc->sc_cmdending = 1;
			}
			sc->sc_actdev = 0;
		} else {
		  	if (c & HIL_POLLDATA) {
				/* Start of polled data */
				sc->sc_actdev = (c & HIL_DEVMASK);
				sc->sc_pollbp = sc->sc_pollbuf;
			} else {
				/* Start of command - should not happen */
				if (sc->sc_cmddev == (c & HIL_DEVMASK)) {
					sc->sc_cmdbp = sc->sc_cmdbuf;
					sc->sc_actdev = 0;
				}
			}
		}
	        break;
	case HIL_DATA:
		if (sc->sc_actdev != 0)	/* Collecting poll data */
			return 1;
		else {
			if (sc->sc_cmddev != 0) {  /* Discarding cmd data */
				if (sc->sc_cmdending) {
					sc->sc_cmddone = 1;
					sc->sc_cmdending = 0;
				}
		        }
		}
		break;
	}

	return 0;
}

void
hil_thread(void *arg)
{
	struct hil_softc *sc = arg;
	int s;

	for (;;) {
		s = splhil();
		if (sc->sc_pending == 0) {
			splx(s);
			(void)tsleep(&sc->sc_pending, PWAIT, "hil_event", 0);
			continue;
		}

		switch (sc->sc_pending) {
		case HIL_PENDING_RECONFIG:
			sc->sc_pending = 0;
			hilconfig(sc, sc->sc_maxdev);
			break;
		case HIL_PENDING_UNPLUGGED:
			sc->sc_pending = 0;
			hilempty(sc);
			break;
		}
		splx(s);
	}
}

/*
 * Called after the loop has reconfigured.  Here we need to:
 *	- determine how many devices are on the loop
 *	  (some may have been added or removed)
 *	- make sure all keyboards are in raw mode
 *
 * Note that our device state is now potentially invalid as
 * devices may no longer be where they were.  What we should
 * do here is either track where the devices went and move
 * state around accordingly...
 *
 * Note that it is necessary that we operate the loop with the keyboards
 * in raw mode: they won't cause the loop to generate an NMI if the
 * ``reset'' key combination is pressed, and we do not handle the hil
 * NMI interrupt...
 */
void
hilconfig(struct hil_softc *sc, u_int knowndevs)
{
	struct hil_attach_args ha;
	uint8_t db;
	int id, s;

	s = splhil();

	/*
	 * Determine how many devices are on the loop.
	 */
	db = 0;
	send_hil_cmd(sc, HIL_READLPSTAT, NULL, 0, &db);
	sc->sc_maxdev = db & LPS_DEVMASK;
#ifdef HILDEBUG
	printf("%s: %d device(s)\n", device_xname(sc->sc_dev), sc->sc_maxdev);
#endif

	/*
	 * Put all keyboards in raw mode now.
	 */
	db = 0;
	send_hil_cmd(sc, HIL_WRITEKBDSADR, &db, 1, NULL);

	/*
	 * If the loop grew, attach new devices.
	 */
	for (id = knowndevs + 1; id <= sc->sc_maxdev; id++) {
		int len;
		const struct hildevice *hd;
		
		if (send_device_cmd(sc, id, HIL_IDENTIFY) != 0) {
			aprint_normal_dev(sc->sc_dev,
			    "no answer from device %d\n", id);
			continue;
		}

		len = sc->sc_cmdbp - sc->sc_cmdbuf;
		if (len == 0) {
#ifdef HILDEBUG
			printf("%s: no device at code %d\n",
			    device_xname(sc->sc_dev), id);
#endif
			continue;
		}

		/* Identify and attach device */
		for (hd = hildevs; hd->minid >= 0; hd++)
			if (sc->sc_cmdbuf[0] >= hd->minid &&
			    sc->sc_cmdbuf[0] <= hd->maxid) {

			ha.ha_console = *sc->sc_console;
			ha.ha_code = id;
			ha.ha_type = hd->type;
			ha.ha_descr = hd->descr;
			ha.ha_infolen = len;
			memcpy(ha.ha_info, sc->sc_cmdbuf, len);

			sc->sc_devices[id] =
			    config_found_sm_loc(sc->sc_dev, "hil", NULL,
			    &ha, hildevprint, hilsubmatch);

#if NHILKBD > 0
			/*
			 * If we just attached a keyboard as console,
			 * console choice is not indeterminate anymore.
			 */
			if (sc->sc_devices[id] != NULL &&
			    ha.ha_type == HIL_DEVICE_KEYBOARD &&
			    ha.ha_console != 0)
				*sc->sc_console = 1;
#endif
		}
	}

	/*
	 * Detach remaining devices, if the loop has shrunk.
	 */
	for (id = sc->sc_maxdev + 1; id < NHILD; id++) {
		if (sc->sc_devices[id] != NULL)
			config_detach(sc->sc_devices[id],
			    DETACH_FORCE);
		sc->sc_devices[id] = NULL;
	}

	sc->sc_cmdbp = sc->sc_cmdbuf;

	splx(s);
}

/*
 * Called after the loop has been unplugged. We simply force detach of
 * all our children.
 */
void
hilempty(struct hil_softc *sc)
{
	uint8_t db;
	int id, s;
	u_int oldmaxdev;

	s = splhil();

	/*
	 * Wait for the loop to be stable.
	 */
	for (;;) {
		if (send_hil_cmd(sc, HIL_READLPSTAT, NULL, 0, &db) == 0) {
			if (db & (LPS_CONFFAIL | LPS_CONFGOOD))
				break;
		} else {
			db = LPS_CONFFAIL;
			break;
		}
	}

	if (db & LPS_CONFFAIL) {
		sc->sc_maxdev = 0;
	} else {
		db = 0;
		send_hil_cmd(sc, HIL_READLPSTAT, NULL, 0, &db);
		oldmaxdev = sc->sc_maxdev;
		sc->sc_maxdev = db & LPS_DEVMASK;

		if (sc->sc_maxdev != 0) {
			/*
			 * The loop was not unplugged after all, but its
			 * configuration has changed.
			 */
			hilconfig(sc, oldmaxdev);
			return;
		}
	}

	/*
	 * Now detach all hil devices.
	 */
	for (id = sc->sc_maxdev + 1; id < NHILD; id++) {
		if (sc->sc_devices[id] != NULL)
			config_detach(sc->sc_devices[id],
			    DETACH_FORCE);
		sc->sc_devices[id] = NULL;
	}

	sc->sc_cmdbp = sc->sc_cmdbuf;

	splx(s);
}

/*
 * Low level routines which actually talk to the 8042 chip.
 */

/*
 * Send a command to the 8042 with zero or more bytes of data.
 * If rdata is non-null, wait for and return a byte of data.
 */
int
send_hil_cmd(struct hil_softc *sc, u_int cmd, uint8_t *data, u_int dlen,
    uint8_t *rdata)
{
	uint8_t status;
	int s;
	
	s = splhil();

	if (hilwait(sc) == 0) {
#ifdef HILDEBUG
		printf("%s: no answer from the loop\n",
		    device_xname(sc->sc_dev));
#endif
		splx(s);
		return EBUSY;
	}

	bus_space_write_1(sc->sc_bst, sc->sc_bsh, HILP_CMD, cmd);
	while (dlen--) {
	  	hilwait(sc);
		bus_space_write_1(sc->sc_bst, sc->sc_bsh, HILP_DATA, *data++);
		DELAY(1);
	}
	if (rdata) {
		do {
			if (hildatawait(sc) == 0) {
#ifdef HILDEBUG
				printf("%s: no answer from the loop\n",
				    device_xname(sc->sc_dev));
#endif
				break;
			}
			status = bus_space_read_1(sc->sc_bst, sc->sc_bsh,
			    HILP_STAT);
			*rdata = bus_space_read_1(sc->sc_bst, sc->sc_bsh,
			    HILP_DATA);
			DELAY(1);
		} while (((status >> HIL_SSHIFT) & HIL_SMASK) != HIL_68K);
	}
	splx(s);
	return 0;
}

/*
 * Send a command to a device on the loop.
 * Since only one command can be active on the loop at any time,
 * we must ensure that we are not interrupted during this process.
 * Hence we mask interrupts to prevent potential access from most
 * interrupt routines and turn off auto-polling to disable the
 * internally generated poll commands.
 * Needs to be called at splhil().
 */
int
send_device_cmd(struct hil_softc *sc, u_int device, u_int cmd)
{
	uint8_t status, c;
	int rc = 0;

	polloff(sc);

	sc->sc_cmdbp = sc->sc_cmdbuf;
	sc->sc_cmddev = device;

	if (hilwait(sc) == 0) {
#ifdef HILDEBUG
		printf("%s: no answer from device %d\n",
		    device_xname(sc->sc_dev), device);
#endif
		rc = EBUSY;
		goto out;
	}

	/*
	 * Transfer the command and device info to the chip
	 */
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, HILP_CMD, HIL_STARTCMD);
  	hilwait(sc);
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, HILP_DATA, 8 + device);
  	hilwait(sc);
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, HILP_DATA, cmd);
  	hilwait(sc);
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, HILP_DATA, HIL_TIMEOUT);

	/*
	 * Trigger the command and wait for completion
	 */
	hilwait(sc);
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, HILP_CMD, HIL_TRIGGER);
	sc->sc_cmddone = 0;
	do {
		if (hildatawait(sc) == 0) {
#ifdef HILDEBUG
			printf("%s: no answer from device %d\n",
			    device_xname(sc->sc_dev), device);
#endif
			rc = EBUSY;
			break;
		}
		status = bus_space_read_1(sc->sc_bst, sc->sc_bsh, HILP_STAT);
		c = bus_space_read_1(sc->sc_bst, sc->sc_bsh, HILP_DATA);
		DELAY(1);
		hil_process_int(sc, status, c);
	} while (sc->sc_cmddone == 0);
out:
	sc->sc_cmddev = 0;

	pollon(sc);
	return rc;
}

int
send_hildev_cmd(struct hildev_softc *hdsc, u_int cmd,
    uint8_t *outbuf, u_int *outlen)
{
	struct hil_softc *sc = device_private(device_parent(hdsc->sc_dev));
	int s, rc;
       
	s = splhil();

	if ((rc = send_device_cmd(sc, hdsc->sc_code, cmd)) == 0) {
		/*
		 * Return the command response in the buffer if necessary
	 	*/
		if (outbuf != NULL && outlen != NULL) {
			*outlen = min(*outlen, sc->sc_cmdbp - sc->sc_cmdbuf);
			memcpy(outbuf, sc->sc_cmdbuf, *outlen);
		}
	}

	splx(s);
	return rc;
}

/*
 * Turn auto-polling off and on.
 */
void
polloff(struct hil_softc *sc)
{
	uint8_t db;

	if (hilwait(sc) == 0)
		return;

	/*
	 * Turn off auto repeat
	 */
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, HILP_CMD, HIL_SETARR);
	hilwait(sc);
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, HILP_DATA, 0);

	/*
	 * Turn off auto-polling
	 */
	hilwait(sc);
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, HILP_CMD, HIL_READLPCTRL);
	hildatawait(sc);
	db = bus_space_read_1(sc->sc_bst, sc->sc_bsh, HILP_DATA);
	db &= ~LPC_AUTOPOLL;
	hilwait(sc);
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, HILP_CMD, HIL_WRITELPCTRL);
	hilwait(sc);
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, HILP_DATA, db);

	/*
	 * Must wait until polling is really stopped
	 */
	do {	
		hilwait(sc);
		bus_space_write_1(sc->sc_bst, sc->sc_bsh, HILP_CMD, HIL_READBUSY);
		hildatawait(sc);
		db = bus_space_read_1(sc->sc_bst, sc->sc_bsh, HILP_DATA);
	} while (db & BSY_LOOPBUSY);

	sc->sc_cmddone = 0;
	sc->sc_cmddev = 0;
}

void
pollon(struct hil_softc *sc)
{
	uint8_t db;

	if (hilwait(sc) == 0)
		return;

	/*
	 * Turn on auto polling
	 */
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, HILP_CMD, HIL_READLPCTRL);
	hildatawait(sc);
	db = bus_space_read_1(sc->sc_bst, sc->sc_bsh, HILP_DATA);
	db |= LPC_AUTOPOLL;
	hilwait(sc);
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, HILP_CMD, HIL_WRITELPCTRL);
	hilwait(sc);
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, HILP_DATA, db);

	/*
	 * Turn off auto repeat - we emulate this through wscons
	 */
	hilwait(sc);
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, HILP_CMD, HIL_SETARR);
	hilwait(sc);
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, HILP_DATA, 0);
	DELAY(1);
}

void
hil_set_poll(struct hil_softc *sc, int on)
{
	if (on) {
		pollon(sc);
	} else {
		hil_process_pending(sc);
		send_hil_cmd(sc, HIL_INTON, NULL, 0, NULL);
	}
}

int
hil_poll_data(struct hildev_softc *hdsc, uint8_t *stat, uint8_t *data)
{
	struct hil_softc *sc = device_private(device_parent(hdsc->sc_dev));
	uint8_t s, c;

	s = bus_space_read_1(sc->sc_bst, sc->sc_bsh, HILP_STAT);
	if ((s & HIL_DATA_RDY) == 0)
		return -1;

	c = bus_space_read_1(sc->sc_bst, sc->sc_bsh, HILP_DATA);
	DELAY(1);

	if (hil_process_poll(sc, s, c)) {
		/* Discard any data not for us */
		if (sc->sc_actdev == hdsc->sc_code) {
			*stat = s;
			*data = c;
			return 0;
		}
	}

	return -1;
}
