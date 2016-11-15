/* $NetBSD: atppc.c,v 1.32 2014/07/13 17:12:23 dholland Exp $ */

/*
 * Copyright (c) 2001 Alcove - Nicolas Souchu
 * Copyright (c) 2003, 2004 Gary Thorpe <gathorpe@users.sourceforge.net>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * FreeBSD: src/sys/isa/ppc.c,v 1.26.2.5 2001/10/02 05:21:45 nsouch Exp
 *
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: atppc.c,v 1.32 2014/07/13 17:12:23 dholland Exp $");

#include "opt_atppc.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/kmem.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/syslog.h>

#include <sys/bus.h>
/*#include <sys/intr.h>*/

#include <dev/isa/isareg.h>

#include <dev/ic/atppcreg.h>
#include <dev/ic/atppcvar.h>

#include <dev/ppbus/ppbus_conf.h>
#include <dev/ppbus/ppbus_msq.h>
#include <dev/ppbus/ppbus_io.h>
#include <dev/ppbus/ppbus_var.h>

#ifdef ATPPC_DEBUG
int atppc_debug = 1;
#endif

#ifdef ATPPC_VERBOSE
int atppc_verbose = 1;
#endif

/* List of supported chipsets detection routines */
static int (*chipset_detect[])(struct atppc_softc *) = {
/* XXX Add these LATER: maybe as separate devices?
		atppc_pc873xx_detect,
		atppc_smc37c66xgt_detect,
		atppc_w83877f_detect,
		atppc_smc37c935_detect,
*/
		NULL
};


/* Prototypes for functions. */

/* Print function for config_found() */
static int atppc_print(void *, const char *);

/* Detection routines */
static int atppc_detect_fifo(struct atppc_softc *);
static int atppc_detect_chipset(struct atppc_softc *);
static int atppc_detect_generic(struct atppc_softc *);

/* Routines for ppbus interface (bus + device) */
static int atppc_read(device_t, char *, int, int, size_t *);
static int atppc_write(device_t, char *, int, int, size_t *);
static int atppc_setmode(device_t, int);
static int atppc_getmode(device_t);
static int atppc_check_epp_timeout(device_t);
static void atppc_reset_epp_timeout(device_t);
static void atppc_ecp_sync(device_t);
static int atppc_exec_microseq(device_t, struct ppbus_microseq * *);
static u_int8_t atppc_io(device_t, int, u_char *, int, u_char);
static int atppc_read_ivar(device_t, int, unsigned int *);
static int atppc_write_ivar(device_t, int, unsigned int *);
static int atppc_add_handler(device_t, void (*)(void *), void *);
static int atppc_remove_handler(device_t, void (*)(void *));

/* Utility functions */

/* Functions to read bytes into device's input buffer */
static void atppc_nibble_read(struct atppc_softc * const);
static void atppc_byte_read(struct atppc_softc * const);
static void atppc_epp_read(struct atppc_softc * const);
static void atppc_ecp_read(struct atppc_softc * const);
static void atppc_ecp_read_dma(struct atppc_softc *, unsigned int *,
	unsigned char);
static void atppc_ecp_read_pio(struct atppc_softc *, unsigned int *,
	unsigned char);
static void atppc_ecp_read_error(struct atppc_softc *);


/* Functions to write bytes to device's output buffer */
static void atppc_std_write(struct atppc_softc * const);
static void atppc_epp_write(struct atppc_softc * const);
static void atppc_fifo_write(struct atppc_softc * const);
static void atppc_fifo_write_dma(struct atppc_softc * const, unsigned char,
	unsigned char);
static void atppc_fifo_write_pio(struct atppc_softc * const, unsigned char,
	unsigned char);
static void atppc_fifo_write_error(struct atppc_softc * const,
	const unsigned int);

/* Miscellaneous */
static int atppc_poll_str(const struct atppc_softc * const, const u_int8_t,
	const u_int8_t);
static int atppc_wait_interrupt(struct atppc_softc * const, kcondvar_t *,
	const u_int8_t);


/*
 * Generic attach and detach functions for atppc device. If sc_dev_ok in soft
 * configuration data is not ATPPC_ATTACHED, these should be skipped altogether.
 */

/* Soft configuration attach for atppc */
void
atppc_sc_attach(struct atppc_softc *lsc)
{
	/* Adapter used to configure ppbus device */
	struct parport_adapter sc_parport_adapter;
	char buf[64];

	mutex_init(&lsc->sc_lock, MUTEX_DEFAULT, IPL_TTY);
	cv_init(&lsc->sc_out_cv, "atppcout");
	cv_init(&lsc->sc_in_cv, "atppcin");

	/* Probe and set up chipset */
	if (atppc_detect_chipset(lsc) != 0) {
		if (atppc_detect_generic(lsc) != 0) {
			ATPPC_DPRINTF(("%s: Error detecting chipset\n",
				device_xname(lsc->sc_dev)));
		}
	}

	/* Probe and setup FIFO queue */
	if (atppc_detect_fifo(lsc) == 0) {
		printf("%s: FIFO <depth,wthr,rthr>=<%d,%d,%d>\n",
			device_xname(lsc->sc_dev), lsc->sc_fifo, lsc->sc_wthr,
			lsc->sc_rthr);
	}

        /* Print out chipset capabilities */
	snprintb(buf, sizeof(buf), "\20\1INTR\2DMA\3FIFO\4PS2\5ECP\6EPP",
	    lsc->sc_has);
	printf("%s: capabilities=%s\n", device_xname(lsc->sc_dev), buf);

	/* Initialize device's buffer pointers */
	lsc->sc_outb = lsc->sc_outbstart = lsc->sc_inb = lsc->sc_inbstart
		= NULL;
	lsc->sc_inb_nbytes = lsc->sc_outb_nbytes = 0;

	/* Last configuration step: set mode to standard mode */
	if (atppc_setmode(lsc->sc_dev, PPBUS_COMPATIBLE) != 0) {
		ATPPC_DPRINTF(("%s: unable to initialize mode.\n",
			device_xname(lsc->sc_dev)));
	}

	/* Set up parport_adapter structure */

	/* Set capabilites */
	sc_parport_adapter.capabilities = 0;
	if (lsc->sc_has & ATPPC_HAS_INTR) {
		sc_parport_adapter.capabilities |= PPBUS_HAS_INTR;
	}
	if (lsc->sc_has & ATPPC_HAS_DMA) {
		sc_parport_adapter.capabilities |= PPBUS_HAS_DMA;
	}
	if (lsc->sc_has & ATPPC_HAS_FIFO) {
		sc_parport_adapter.capabilities |= PPBUS_HAS_FIFO;
	}
	if (lsc->sc_has & ATPPC_HAS_PS2) {
		sc_parport_adapter.capabilities |= PPBUS_HAS_PS2;
	}
	if (lsc->sc_has & ATPPC_HAS_EPP) {
		sc_parport_adapter.capabilities |= PPBUS_HAS_EPP;
	}
	if (lsc->sc_has & ATPPC_HAS_ECP) {
		sc_parport_adapter.capabilities |= PPBUS_HAS_ECP;
	}

	/* Set function pointers */
	sc_parport_adapter.parport_io = atppc_io;
	sc_parport_adapter.parport_exec_microseq = atppc_exec_microseq;
	sc_parport_adapter.parport_reset_epp_timeout =
		atppc_reset_epp_timeout;
	sc_parport_adapter.parport_setmode = atppc_setmode;
	sc_parport_adapter.parport_getmode = atppc_getmode;
	sc_parport_adapter.parport_ecp_sync = atppc_ecp_sync;
	sc_parport_adapter.parport_read = atppc_read;
	sc_parport_adapter.parport_write = atppc_write;
	sc_parport_adapter.parport_read_ivar = atppc_read_ivar;
	sc_parport_adapter.parport_write_ivar = atppc_write_ivar;
	sc_parport_adapter.parport_dma_malloc = lsc->sc_dma_malloc;
	sc_parport_adapter.parport_dma_free = lsc->sc_dma_free;
	sc_parport_adapter.parport_add_handler = atppc_add_handler;
	sc_parport_adapter.parport_remove_handler = atppc_remove_handler;

	/* Initialize handler list, may be added to by grandchildren */
	SLIST_INIT(&(lsc->sc_handler_listhead));

	/* Initialize interrupt state */
	lsc->sc_irqstat = ATPPC_IRQ_NONE;
	lsc->sc_ecr_intr = lsc->sc_ctr_intr = lsc->sc_str_intr = 0;

	/* Disable DMA/interrupts (each ppbus driver selects usage itself) */
	lsc->sc_use = 0;

	/* Configure child of the device. */
	lsc->child = config_found(lsc->sc_dev, &(sc_parport_adapter),
		atppc_print);

	return;
}

/* Soft configuration detach */
int
atppc_sc_detach(struct atppc_softc *lsc, int flag)
{
	device_t dev = lsc->sc_dev;

	/* Detach children devices */
	if (config_detach(lsc->child, flag) && !(flag & DETACH_QUIET)) {
		aprint_error_dev(dev, "not able to detach child device, ");

		if (!(flag & DETACH_FORCE)) {
			printf("cannot detach\n");
			return 1;
		} else {
			printf("continuing (DETACH_FORCE)\n");
		}
	}

	if (!(flag & DETACH_QUIET))
		printf("%s detached", device_xname(dev));

	return 0;
}

/* Used by config_found() to print out device information */
static int
atppc_print(void *aux, const char *name)
{
	/* Print out something on failure. */
	if (name != NULL) {
		printf("%s: child devices", name);
		return UNCONF;
	}

	return QUIET;
}

/*
 * Machine independent detection routines for atppc driver.
 */

/* Detect parallel port I/O port: taken from FreeBSD code directly. */
int
atppc_detect_port(bus_space_tag_t iot, bus_space_handle_t ioh)
{
        /*
	 * Much shorter than scheme used by lpt_isa_probe() and lpt_port_test()
	 * in original lpt driver.
	 * Write to data register common to all controllers and read back the
	 * values. Also tests control and status registers.
	 */

	/*
	 * Cannot use convenient macros because the device's config structure
	 * may not have been created yet: major change from FreeBSD code.
	 */

	int rval;
	u_int8_t ctr_sav, dtr_sav, str_sav;

	/* Store writtable registers' values and test if they can be read */
	str_sav = bus_space_read_1(iot, ioh, ATPPC_SPP_STR);
	ctr_sav = bus_space_read_1(iot, ioh, ATPPC_SPP_CTR);
	dtr_sav = bus_space_read_1(iot, ioh, ATPPC_SPP_DTR);
	bus_space_barrier(iot, ioh, 0, IO_LPTSIZE,
		BUS_SPACE_BARRIER_READ);

        /*
	 * Ensure PS2 ports in output mode, also read back value of control
	 * register.
	 */
	bus_space_write_1(iot, ioh, ATPPC_SPP_CTR, 0x0c);
	bus_space_barrier(iot, ioh, 0, IO_LPTSIZE,
		BUS_SPACE_BARRIER_WRITE);

	if (bus_space_read_1(iot, ioh, ATPPC_SPP_CTR) != 0x0c) {
		rval = 0;
	} else {
		/*
		 * Test if two values can be written and read from the data
		 * register.
		 */
		bus_space_barrier(iot, ioh, 0, IO_LPTSIZE,
			BUS_SPACE_BARRIER_READ);
		bus_space_write_1(iot, ioh, ATPPC_SPP_DTR, 0xaa);
		bus_space_barrier(iot, ioh, 0, IO_LPTSIZE,
			BUS_SPACE_BARRIER_WRITE);
		if (bus_space_read_1(iot, ioh, ATPPC_SPP_DTR) != 0xaa) {
			rval = 1;
		} else {
			/* Second value to test */
			bus_space_barrier(iot, ioh, 0, IO_LPTSIZE,
				BUS_SPACE_BARRIER_READ);
			bus_space_write_1(iot, ioh, ATPPC_SPP_DTR, 0x55);
			bus_space_barrier(iot, ioh, 0, IO_LPTSIZE,
				BUS_SPACE_BARRIER_WRITE);
			if (bus_space_read_1(iot, ioh, ATPPC_SPP_DTR) != 0x55) {
				rval = 1;
			} else {
				rval = 0;
			}
		}

	}

	/* Restore registers */
	bus_space_barrier(iot, ioh, 0, IO_LPTSIZE,
		BUS_SPACE_BARRIER_READ);
	bus_space_write_1(iot, ioh, ATPPC_SPP_CTR, ctr_sav);
	bus_space_write_1(iot, ioh, ATPPC_SPP_DTR, dtr_sav);
	bus_space_write_1(iot, ioh, ATPPC_SPP_STR, str_sav);
	bus_space_barrier(iot, ioh, 0, IO_LPTSIZE,
		BUS_SPACE_BARRIER_WRITE);

	return rval;
}

/* Detect parallel port chipset. */
static int
atppc_detect_chipset(struct atppc_softc *atppc)
{
	/* Try each detection routine. */
	int i, mode;
	for (i = 0; chipset_detect[i] != NULL; i++) {
		if ((mode = chipset_detect[i](atppc)) != -1) {
			atppc->sc_mode = mode;
			return 0;
		}
	}

	return 1;
}

/* Detect generic capabilities. */
static int
atppc_detect_generic(struct atppc_softc *atppc)
{
	u_int8_t ecr_sav = atppc_r_ecr(atppc);
	u_int8_t ctr_sav = atppc_r_ctr(atppc);
	u_int8_t str_sav = atppc_r_str(atppc);
	u_int8_t tmp;
	atppc_barrier_r(atppc);

	/* Default to generic */
	atppc->sc_type = ATPPC_TYPE_GENERIC;
	atppc->sc_model = GENERIC;

	/* Check for ECP */
	tmp = atppc_r_ecr(atppc);
	atppc_barrier_r(atppc);
	if ((tmp & ATPPC_FIFO_EMPTY) && !(tmp & ATPPC_FIFO_FULL)) {
		atppc_w_ecr(atppc, 0x34);
		atppc_barrier_w(atppc);
		tmp = atppc_r_ecr(atppc);
		atppc_barrier_r(atppc);
		if (tmp == 0x35) {
			atppc->sc_has |= ATPPC_HAS_ECP;
		}
	}

	/* Allow search for SMC style ECP+EPP mode */
	if (atppc->sc_has & ATPPC_HAS_ECP) {
		atppc_w_ecr(atppc, ATPPC_ECR_EPP);
		atppc_barrier_w(atppc);
	}
	/* Check for EPP by checking for timeout bit */
	if (atppc_check_epp_timeout(atppc->sc_dev) != 0) {
		atppc->sc_has |= ATPPC_HAS_EPP;
		atppc->sc_epp = ATPPC_EPP_1_9;
		if (atppc->sc_has & ATPPC_HAS_ECP) {
			/* SMC like chipset found */
			atppc->sc_model = SMC_LIKE;
			atppc->sc_type = ATPPC_TYPE_SMCLIKE;
		}
	}

	/* Detect PS2 mode */
	if (atppc->sc_has & ATPPC_HAS_ECP) {
		/* Put ECP port into PS2 mode */
		atppc_w_ecr(atppc, ATPPC_ECR_PS2);
		atppc_barrier_w(atppc);
	}
	/* Put PS2 port in input mode: writes should not be readable */
	atppc_w_ctr(atppc, 0x20);
	atppc_barrier_w(atppc);
	/*
	 * Write two values to data port: if neither are read back,
	 * bidirectional mode is functional.
	 */
	atppc_w_dtr(atppc, 0xaa);
	atppc_barrier_w(atppc);
	tmp = atppc_r_dtr(atppc);
	atppc_barrier_r(atppc);
	if (tmp != 0xaa) {
		atppc_w_dtr(atppc, 0x55);
		atppc_barrier_w(atppc);
		tmp = atppc_r_dtr(atppc);
		atppc_barrier_r(atppc);
		if (tmp != 0x55) {
			atppc->sc_has |= ATPPC_HAS_PS2;
		}
	}

	/* Restore to previous state */
	atppc_w_ecr(atppc, ecr_sav);
	atppc_w_ctr(atppc, ctr_sav);
	atppc_w_str(atppc, str_sav);
	atppc_barrier_w(atppc);

	return 0;
}

/*
 * Detect parallel port FIFO: taken from FreeBSD code directly.
 */
static int
atppc_detect_fifo(struct atppc_softc *atppc)
{
#ifdef ATPPC_DEBUG
	device_t dev = atppc->sc_dev;
#endif
	u_int8_t ecr_sav;
	u_int8_t ctr_sav;
	u_int8_t str_sav;
	u_int8_t cc;
	short i;

	/* If there is no ECP mode, we cannot config a FIFO */
	if (!(atppc->sc_has & ATPPC_HAS_ECP)) {
		return (EINVAL);
	}

	/* save registers */
	ecr_sav = atppc_r_ecr(atppc);
	ctr_sav = atppc_r_ctr(atppc);
	str_sav = atppc_r_str(atppc);
	atppc_barrier_r(atppc);

	/* Enter ECP configuration mode, no interrupt, no DMA */
	atppc_w_ecr(atppc, (ATPPC_ECR_CFG | ATPPC_SERVICE_INTR) &
		~ATPPC_ENABLE_DMA);
	atppc_barrier_w(atppc);

	/* read PWord size - transfers in FIFO mode must be PWord aligned */
	atppc->sc_pword = (atppc_r_cnfgA(atppc) & ATPPC_PWORD_MASK);
	atppc_barrier_r(atppc);

	/* XXX 16 and 32 bits implementations not supported */
	if (atppc->sc_pword != ATPPC_PWORD_8) {
		ATPPC_DPRINTF(("%s(%s): FIFO PWord(%d) not supported.\n",
			__func__, device_xname(dev), atppc->sc_pword));
		goto error;
	}

	/* Byte mode, reverse direction, no interrupt, no DMA */
	atppc_w_ecr(atppc, ATPPC_ECR_PS2 | ATPPC_SERVICE_INTR);
	atppc_w_ctr(atppc, (ctr_sav & ~IRQENABLE) | PCD);
	/* enter ECP test mode, no interrupt, no DMA */
	atppc_w_ecr(atppc, ATPPC_ECR_TST | ATPPC_SERVICE_INTR);
	atppc_barrier_w(atppc);

	/* flush the FIFO */
	for (i = 0; i < 1024; i++) {
		atppc_r_fifo(atppc);
		atppc_barrier_r(atppc);
		cc = atppc_r_ecr(atppc);
		atppc_barrier_r(atppc);
		if (cc & ATPPC_FIFO_EMPTY)
			break;
	}
	if (i >= 1024) {
		ATPPC_DPRINTF(("%s(%s): cannot flush FIFO.\n", __func__,
			device_xname(dev)));
		goto error;
	}

	/* Test mode, enable interrupts, no DMA */
	atppc_w_ecr(atppc, ATPPC_ECR_TST);
	atppc_barrier_w(atppc);

	/* Determine readIntrThreshold - fill FIFO until serviceIntr is set */
	for (i = atppc->sc_rthr = atppc->sc_fifo = 0; i < 1024; i++) {
		atppc_w_fifo(atppc, (char)i);
		atppc_barrier_w(atppc);
		cc = atppc_r_ecr(atppc);
		atppc_barrier_r(atppc);
		if ((atppc->sc_rthr == 0) && (cc & ATPPC_SERVICE_INTR)) {
			/* readThreshold reached */
			atppc->sc_rthr = i + 1;
		}
		if (cc & ATPPC_FIFO_FULL) {
			atppc->sc_fifo = i + 1;
			break;
		}
	}
	if (i >= 1024) {
		ATPPC_DPRINTF(("%s(%s): cannot fill FIFO.\n", __func__,
			device_xname(dev)));
		goto error;
	}

	/* Change direction */
	atppc_w_ctr(atppc, (ctr_sav & ~IRQENABLE) & ~PCD);
	atppc_barrier_w(atppc);

	/* Clear the serviceIntr bit we've already set in the above loop */
	atppc_w_ecr(atppc, ATPPC_ECR_TST);
	atppc_barrier_w(atppc);

	/* Determine writeIntrThreshold - empty FIFO until serviceIntr is set */
	for (atppc->sc_wthr = 0; i > -1; i--) {
		cc = atppc_r_fifo(atppc);
		atppc_barrier_r(atppc);
		if (cc != (char)(atppc->sc_fifo - i - 1)) {
			ATPPC_DPRINTF(("%s(%s): invalid data in FIFO.\n",
				__func__, device_xname(dev)));
			goto error;
		}

		cc = atppc_r_ecr(atppc);
		atppc_barrier_r(atppc);
		if ((atppc->sc_wthr == 0) && (cc & ATPPC_SERVICE_INTR)) {
			/* writeIntrThreshold reached */
			atppc->sc_wthr = atppc->sc_fifo - i;
		}

		if (i > 0 && (cc & ATPPC_FIFO_EMPTY)) {
			/* If FIFO empty before the last byte, error */
			ATPPC_DPRINTF(("%s(%s): data lost in FIFO.\n", __func__,
				device_xname(dev)));
			goto error;
		}
	}

	/* FIFO must be empty after the last byte */
	cc = atppc_r_ecr(atppc);
	atppc_barrier_r(atppc);
	if (!(cc & ATPPC_FIFO_EMPTY)) {
		ATPPC_DPRINTF(("%s(%s): cannot empty the FIFO.\n", __func__,
			device_xname(dev)));
		goto error;
	}

	/* Restore original registers */
	atppc_w_ctr(atppc, ctr_sav);
	atppc_w_str(atppc, str_sav);
	atppc_w_ecr(atppc, ecr_sav);
	atppc_barrier_w(atppc);

	/* Update capabilities */
	atppc->sc_has |= ATPPC_HAS_FIFO;

	return 0;

error:
	/* Restore original registers */
	atppc_w_ctr(atppc, ctr_sav);
	atppc_w_str(atppc, str_sav);
	atppc_w_ecr(atppc, ecr_sav);
	atppc_barrier_w(atppc);

	return (EINVAL);
}

/* Interrupt handler for atppc device: wakes up read/write functions */
int
atppcintr(void *arg)
{
	device_t dev = arg;
	struct atppc_softc *atppc = device_private(dev);
	int claim = 1;
	enum { NONE, READER, WRITER } wake_up = NONE;

	mutex_enter(&atppc->sc_lock);

	/* Record registers' status */
	atppc->sc_str_intr = atppc_r_str(atppc);
	atppc->sc_ctr_intr = atppc_r_ctr(atppc);
	atppc->sc_ecr_intr = atppc_r_ecr(atppc);
	atppc_barrier_r(atppc);

	/* Determine cause of interrupt and wake up top half */
	switch (atppc->sc_mode) {
	case ATPPC_MODE_STD:
		/* nAck pulsed for 5 usec, too fast to check reliably, assume */
		atppc->sc_irqstat = ATPPC_IRQ_nACK;
		if (atppc->sc_outb)
			wake_up = WRITER;
		else
			claim = 0;
		break;

	case ATPPC_MODE_NIBBLE:
	case ATPPC_MODE_PS2:
		/* nAck is set low by device and then high on ack */
		if (!(atppc->sc_str_intr & nACK)) {
			claim = 0;
			break;
		}
		atppc->sc_irqstat = ATPPC_IRQ_nACK;
		if (atppc->sc_inb)
			wake_up = READER;
		break;

	case ATPPC_MODE_ECP:
	case ATPPC_MODE_FAST:
		/* Confirm interrupt cause: these are not pulsed as in nAck. */
		if (atppc->sc_ecr_intr & ATPPC_SERVICE_INTR) {
			if (atppc->sc_ecr_intr & ATPPC_ENABLE_DMA)
				atppc->sc_irqstat |= ATPPC_IRQ_DMA;
			else
				atppc->sc_irqstat |= ATPPC_IRQ_FIFO;

			/* Decide where top half will be waiting */
			if (atppc->sc_mode & ATPPC_MODE_ECP) {
				if (atppc->sc_ctr_intr & PCD) {
					if (atppc->sc_inb)
						wake_up = READER;
					else
						claim = 0;
				} else {
					if (atppc->sc_outb)
						wake_up = WRITER;
					else
						claim = 0;
				}
			} else {
				if (atppc->sc_outb)
					wake_up = WRITER;
				else
					claim = 0;
			}
		}
		/* Determine if nFault has occurred */
		if ((atppc->sc_mode & ATPPC_MODE_ECP) &&
			(atppc->sc_ecr_intr & ATPPC_nFAULT_INTR) &&
			!(atppc->sc_str_intr & nFAULT)) {

			/* Device is requesting the channel */
			atppc->sc_irqstat |= ATPPC_IRQ_nFAULT;
			claim = 1;
		}
		break;

	case ATPPC_MODE_EPP:
		/* nAck pulsed for 5 usec, too fast to check reliably */
		atppc->sc_irqstat = ATPPC_IRQ_nACK;
		if (atppc->sc_inb)
			wake_up = WRITER;
		else if (atppc->sc_outb)
			wake_up = READER;
		else
			claim = 0;
		break;

	default:
		panic("%s: chipset is in invalid mode.", device_xname(dev));
	}

	if (claim) {
		switch (wake_up) {
		case NONE:
			break;

		case READER:
			cv_broadcast(&atppc->sc_in_cv);
			break;

		case WRITER:
			cv_broadcast(&atppc->sc_out_cv);
			break;
		}
	}

	/* Call all of the installed handlers */
	if (claim) {
		struct atppc_handler_node * callback;
		SLIST_FOREACH(callback, &(atppc->sc_handler_listhead),
			entries) {
				(*callback->func)(callback->arg);
		}
	}

	mutex_exit(&atppc->sc_lock);

	return claim;
}


/* Functions which support ppbus interface */


/* Check EPP mode timeout */
static int
atppc_check_epp_timeout(device_t dev)
{
	struct atppc_softc *atppc = device_private(dev);
	int error;

	mutex_enter(&atppc->sc_lock);

	atppc_reset_epp_timeout(dev);
	error = !(atppc_r_str(atppc) & TIMEOUT);
	atppc_barrier_r(atppc);

	mutex_exit(&atppc->sc_lock);

	return (error);
}

/*
 * EPP timeout, according to the PC87332 manual
 * Semantics of clearing EPP timeout bit.
 * PC87332	- reading SPP_STR does it...
 * SMC		- write 1 to EPP timeout bit			XXX
 * Others	- (?) write 0 to EPP timeout bit
 */
static void
atppc_reset_epp_timeout(device_t dev)
{
	struct atppc_softc *atppc = device_private(dev);
	register unsigned char r;

	r = atppc_r_str(atppc);
	atppc_barrier_r(atppc);
	atppc_w_str(atppc, r | 0x1);
	atppc_barrier_w(atppc);
	atppc_w_str(atppc, r & 0xfe);
	atppc_barrier_w(atppc);

	return;
}


/* Read from atppc device: returns 0 on success. */
static int
atppc_read(device_t dev, char *buf, int len, int ioflag,
	size_t *cnt)
{
	struct atppc_softc *atppc = device_private(dev);
	int error = 0;

	mutex_enter(&atppc->sc_lock);

	*cnt = 0;

	/* Initialize buffer */
	atppc->sc_inb = atppc->sc_inbstart = buf;
	atppc->sc_inb_nbytes = len;

	/* Initialize device input error state for new operation */
	atppc->sc_inerr = 0;

	/* Call appropriate function to read bytes */
	switch(atppc->sc_mode) {
	case ATPPC_MODE_STD:
	case ATPPC_MODE_FAST:
		error = ENODEV;
		break;

	case ATPPC_MODE_NIBBLE:
		atppc_nibble_read(atppc);
		break;

	case ATPPC_MODE_PS2:
		atppc_byte_read(atppc);
		break;

	case ATPPC_MODE_ECP:
		atppc_ecp_read(atppc);
		break;

	case ATPPC_MODE_EPP:
		atppc_epp_read(atppc);
		break;

	default:
		panic("%s(%s): chipset in invalid mode.\n", __func__,
			device_xname(dev));
	}

	/* Update counter*/
	*cnt = (atppc->sc_inbstart - atppc->sc_inb);

	/* Reset buffer */
	atppc->sc_inb = atppc->sc_inbstart = NULL;
	atppc->sc_inb_nbytes = 0;

	if (!(error))
		error = atppc->sc_inerr;

	mutex_exit(&atppc->sc_lock);

	return (error);
}

/* Write to atppc device: returns 0 on success. */
static int
atppc_write(device_t dev, char *buf, int len, int ioflag, size_t *cnt)
{
	struct atppc_softc * const atppc = device_private(dev);
	int error = 0;

	*cnt = 0;

	mutex_enter(&atppc->sc_lock);

	/* Set up line buffer */
	atppc->sc_outb = atppc->sc_outbstart = buf;
	atppc->sc_outb_nbytes = len;

	/* Initialize device output error state for new operation */
	atppc->sc_outerr = 0;

	/* Call appropriate function to write bytes */
	switch (atppc->sc_mode) {
	case ATPPC_MODE_STD:
		atppc_std_write(atppc);
		break;

	case ATPPC_MODE_NIBBLE:
	case ATPPC_MODE_PS2:
		error = ENODEV;
		break;

	case ATPPC_MODE_FAST:
	case ATPPC_MODE_ECP:
		atppc_fifo_write(atppc);
		break;

	case ATPPC_MODE_EPP:
		atppc_epp_write(atppc);
		break;

	default:
		panic("%s(%s): chipset in invalid mode.\n", __func__,
			device_xname(dev));
	}

	/* Update counter*/
	*cnt = (atppc->sc_outbstart - atppc->sc_outb);

	/* Reset output buffer */
	atppc->sc_outb = atppc->sc_outbstart = NULL;
	atppc->sc_outb_nbytes = 0;

	if (!(error))
		error = atppc->sc_outerr;

	mutex_exit(&atppc->sc_lock);

	return (error);
}

/*
 * Set mode of chipset to mode argument. Modes not supported are ignored. If
 * multiple modes are flagged, the mode is not changed. Mode's are those
 * defined for ppbus_softc.sc_mode in ppbus_conf.h. Only ECP-capable chipsets
 * can change their mode of operation. However, ALL operation modes support
 * centronics mode and nibble mode. Modes determine both hardware AND software
 * behaviour.
 * NOTE: the mode for ECP should only be changed when the channel is in
 * forward idle mode. This function does not make sure FIFO's have flushed or
 * any consistency checks.
 */
static int
atppc_setmode(device_t dev, int mode)
{
	struct atppc_softc *atppc = device_private(dev);
	u_int8_t ecr;
	u_int8_t chipset_mode;
	int rval = 0;

	mutex_enter(&atppc->sc_lock);

	/* If ECP capable, configure ecr register */
	if (atppc->sc_has & ATPPC_HAS_ECP) {
		/* Read ECR with mode masked out */
		ecr = (atppc_r_ecr(atppc) & 0x1f);
		atppc_barrier_r(atppc);

		switch (mode) {
		case PPBUS_ECP:
			/* Set ECP mode */
			ecr |= ATPPC_ECR_ECP;
			chipset_mode = ATPPC_MODE_ECP;
			break;

		case PPBUS_EPP:
			/* Set EPP mode */
			if (atppc->sc_has & ATPPC_HAS_EPP) {
				ecr |= ATPPC_ECR_EPP;
				chipset_mode = ATPPC_MODE_EPP;
			} else {
				rval = ENODEV;
				goto end;
			}
			break;

		case PPBUS_FAST:
			/* Set fast centronics mode */
			ecr |= ATPPC_ECR_FIFO;
			chipset_mode = ATPPC_MODE_FAST;
			break;

		case PPBUS_PS2:
			/* Set PS2 mode */
			ecr |= ATPPC_ECR_PS2;
			chipset_mode = ATPPC_MODE_PS2;
			break;

		case PPBUS_COMPATIBLE:
			/* Set standard mode */
			ecr |= ATPPC_ECR_STD;
			chipset_mode = ATPPC_MODE_STD;
			break;

		case PPBUS_NIBBLE:
			/* Set nibble mode: uses chipset standard mode */
			ecr |= ATPPC_ECR_STD;
			chipset_mode = ATPPC_MODE_NIBBLE;
			break;

		default:
			/* Invalid mode specified for ECP chip */
			ATPPC_DPRINTF(("%s(%s): invalid mode passed as "
				"argument.\n", __func__, device_xname(dev)));
			rval = ENODEV;
			goto end;
		}

		/* Switch to byte mode to be able to change modes. */
		atppc_w_ecr(atppc, ATPPC_ECR_PS2);
		atppc_barrier_w(atppc);

		/* Update mode */
		atppc_w_ecr(atppc, ecr);
		atppc_barrier_w(atppc);
	} else {
		switch (mode) {
		case PPBUS_EPP:
			if (atppc->sc_has & ATPPC_HAS_EPP) {
				chipset_mode = ATPPC_MODE_EPP;
			} else {
				rval = ENODEV;
				goto end;
			}
			break;

		case PPBUS_PS2:
			if (atppc->sc_has & ATPPC_HAS_PS2) {
				chipset_mode = ATPPC_MODE_PS2;
			} else {
				rval = ENODEV;
				goto end;
			}
			break;

		case PPBUS_NIBBLE:
			/* Set nibble mode (virtual) */
			chipset_mode = ATPPC_MODE_NIBBLE;
			break;

		case PPBUS_COMPATIBLE:
			chipset_mode = ATPPC_MODE_STD;
			break;

		case PPBUS_ECP:
			rval = ENODEV;
			goto end;

		default:
			ATPPC_DPRINTF(("%s(%s): invalid mode passed as "
				"argument.\n", __func__, device_xname(dev)));
			rval = ENODEV;
			goto end;
		}
	}

	atppc->sc_mode = chipset_mode;
	if (chipset_mode == ATPPC_MODE_PS2) {
		/* Set direction bit to reverse */
		ecr = atppc_r_ctr(atppc);
		atppc_barrier_r(atppc);
		ecr |= PCD;
		atppc_w_ctr(atppc, ecr);
		atppc_barrier_w(atppc);
	}

end:
	mutex_exit(&atppc->sc_lock);

	return rval;
}

/* Get the current mode of chipset */
static int
atppc_getmode(device_t dev)
{
	struct atppc_softc *atppc = device_private(dev);
	int mode;

	mutex_enter(&atppc->sc_lock);

	/* The chipset can only be in one mode at a time logically */
	switch (atppc->sc_mode) {
	case ATPPC_MODE_ECP:
		mode = PPBUS_ECP;
		break;

	case ATPPC_MODE_EPP:
		mode = PPBUS_EPP;
		break;

	case ATPPC_MODE_PS2:
		mode = PPBUS_PS2;
		break;

	case ATPPC_MODE_STD:
		mode = PPBUS_COMPATIBLE;
		break;

	case ATPPC_MODE_NIBBLE:
		mode = PPBUS_NIBBLE;
		break;

	case ATPPC_MODE_FAST:
		mode = PPBUS_FAST;
		break;

	default:
		panic("%s(%s): device is in invalid mode!", __func__,
			device_xname(dev));
		break;
	}

	mutex_exit(&atppc->sc_lock);

	return mode;
}


/* Wait for FIFO buffer to empty for ECP-capable chipset */
static void
atppc_ecp_sync(device_t dev)
{
	struct atppc_softc *atppc = device_private(dev);
	int i;
	u_int8_t r;

	mutex_enter(&atppc->sc_lock);

	/*
	 * Only wait for FIFO to empty if mode is chipset is ECP-capable AND
	 * the mode is either ECP or Fast Centronics.
	 */
	r = atppc_r_ecr(atppc);
	atppc_barrier_r(atppc);
	r &= 0xe0;
	if (!(atppc->sc_has & ATPPC_HAS_ECP) || ((r != ATPPC_ECR_ECP)
		&& (r != ATPPC_ECR_FIFO))) {
		goto end;
	}

	/* Wait for FIFO to empty */
	for (i = 0; i < ((MAXBUSYWAIT/hz) * 1000000); i += 100) {
		r = atppc_r_ecr(atppc);
		atppc_barrier_r(atppc);
		if (r & ATPPC_FIFO_EMPTY) {
			goto end;
		}
		delay(100); /* Supposed to be a 100 usec delay */
	}

	ATPPC_DPRINTF(("%s: ECP sync failed, data still in FIFO.\n",
		device_xname(dev)));

end:
	mutex_exit(&atppc->sc_lock);

	return;
}

/* Execute a microsequence to handle fast I/O operations. */
static int
atppc_exec_microseq(device_t dev, struct ppbus_microseq **p_msq)
{
	struct atppc_softc *atppc = device_private(dev);
	struct ppbus_microseq *mi = *p_msq;
	char cc, *p;
	int i, iter, len;
	int error;
	register int reg;
	register unsigned char mask;
	register int accum = 0;
	register char *ptr = NULL;
	struct ppbus_microseq *stack = NULL;

	mutex_enter(&atppc->sc_lock);

/* microsequence registers are equivalent to PC-like port registers */

#define r_reg(register,atppc) bus_space_read_1((atppc)->sc_iot, \
	(atppc)->sc_ioh, (register))
#define w_reg(register, atppc, byte) bus_space_write_1((atppc)->sc_iot, \
	(atppc)->sc_ioh, (register), (byte))

	/* Loop until microsequence execution finishes (ending op code) */
	for (;;) {
		switch (mi->opcode) {
		case MS_OP_RSET:
			cc = r_reg(mi->arg[0].i, atppc);
			atppc_barrier_r(atppc);
			cc &= (char)mi->arg[2].i;	/* clear mask */
			cc |= (char)mi->arg[1].i;	/* assert mask */
			w_reg(mi->arg[0].i, atppc, cc);
			atppc_barrier_w(atppc);
			mi++;
                       	break;

		case MS_OP_RASSERT_P:
			reg = mi->arg[1].i;
			ptr = atppc->sc_ptr;

			if ((len = mi->arg[0].i) == MS_ACCUM) {
				accum = atppc->sc_accum;
				for (; accum; accum--) {
					w_reg(reg, atppc, *ptr++);
					atppc_barrier_w(atppc);
				}
				atppc->sc_accum = accum;
			} else {
				for (i = 0; i < len; i++) {
					w_reg(reg, atppc, *ptr++);
					atppc_barrier_w(atppc);
				}
			}

			atppc->sc_ptr = ptr;
			mi++;
			break;

       	        case MS_OP_RFETCH_P:
			reg = mi->arg[1].i;
			mask = (char)mi->arg[2].i;
			ptr = atppc->sc_ptr;

			if ((len = mi->arg[0].i) == MS_ACCUM) {
				accum = atppc->sc_accum;
				for (; accum; accum--) {
					*ptr++ = r_reg(reg, atppc) & mask;
					atppc_barrier_r(atppc);
				}
				atppc->sc_accum = accum;
			} else {
				for (i = 0; i < len; i++) {
					*ptr++ = r_reg(reg, atppc) & mask;
					atppc_barrier_r(atppc);
				}
			}

			atppc->sc_ptr = ptr;
			mi++;
			break;

                case MS_OP_RFETCH:
			*((char *)mi->arg[2].p) = r_reg(mi->arg[0].i, atppc) &
				(char)mi->arg[1].i;
			atppc_barrier_r(atppc);
			mi++;
       	                break;

		case MS_OP_RASSERT:
                case MS_OP_DELAY:
			/* let's suppose the next instr. is the same */
			do {
				for (;mi->opcode == MS_OP_RASSERT; mi++) {
					w_reg(mi->arg[0].i, atppc,
						(char)mi->arg[1].i);
					atppc_barrier_w(atppc);
				}

				for (;mi->opcode == MS_OP_DELAY; mi++) {
					delay(mi->arg[0].i);
				}
			} while (mi->opcode == MS_OP_RASSERT);
			break;

		case MS_OP_ADELAY:
			if (mi->arg[0].i) {
				tsleep(atppc, PPBUSPRI, "atppcdelay",
					mi->arg[0].i * (hz/1000));
			}
			mi++;
			break;

		case MS_OP_TRIG:
			reg = mi->arg[0].i;
			iter = mi->arg[1].i;
			p = (char *)mi->arg[2].p;

			/* XXX delay limited to 255 us */
			for (i = 0; i < iter; i++) {
				w_reg(reg, atppc, *p++);
				atppc_barrier_w(atppc);
				delay((unsigned char)*p++);
			}

			mi++;
			break;

		case MS_OP_SET:
                        atppc->sc_accum = mi->arg[0].i;
			mi++;
                       	break;

		case MS_OP_DBRA:
                       	if (--atppc->sc_accum > 0) {
                               	mi += mi->arg[0].i;
			}

			mi++;
			break;

		case MS_OP_BRSET:
			cc = atppc_r_str(atppc);
			atppc_barrier_r(atppc);
			if ((cc & (char)mi->arg[0].i) == (char)mi->arg[0].i) {
				mi += mi->arg[1].i;
			}
			mi++;
			break;

		case MS_OP_BRCLEAR:
			cc = atppc_r_str(atppc);
			atppc_barrier_r(atppc);
			if ((cc & (char)mi->arg[0].i) == 0) {
				mi += mi->arg[1].i;
			}
			mi++;
			break;

		case MS_OP_BRSTAT:
			cc = atppc_r_str(atppc);
			atppc_barrier_r(atppc);
			if ((cc & ((char)mi->arg[0].i | (char)mi->arg[1].i)) ==
				(char)mi->arg[0].i) {
				mi += mi->arg[2].i;
			}
			mi++;
			break;

		case MS_OP_C_CALL:
			/*
			 * If the C call returns !0 then end the microseq.
			 * The current state of ptr is passed to the C function
			 */
			if ((error = mi->arg[0].f(mi->arg[1].p,
				atppc->sc_ptr))) {
				mutex_exit(&atppc->sc_lock);
				return (error);
			}
			mi++;
			break;

		case MS_OP_PTR:
			atppc->sc_ptr = (char *)mi->arg[0].p;
			mi++;
			break;

		case MS_OP_CALL:
			if (stack) {
				panic("%s - %s: too much calls", device_xname(dev),
					__func__);
			}

			if (mi->arg[0].p) {
				/* store state of the actual microsequence */
				stack = mi;

				/* jump to the new microsequence */
				mi = (struct ppbus_microseq *)mi->arg[0].p;
			} else {
				mi++;
			}
			break;

		case MS_OP_SUBRET:
			/* retrieve microseq and pc state before the call */
			mi = stack;

			/* reset the stack */
			stack = 0;

			/* XXX return code */

			mi++;
			break;

		case MS_OP_PUT:
		case MS_OP_GET:
		case MS_OP_RET:
			/*
			 * Can't return to atppc level during the execution
			 * of a submicrosequence.
			 */
			if (stack) {
				panic("%s: cannot return to atppc level",
					__func__);
			}
			/* update pc for atppc level of execution */
			*p_msq = mi;

			mutex_exit(&atppc->sc_lock);
			return (0);
			break;

		default:
			panic("%s: unknown microsequence "
				"opcode 0x%x", __func__, mi->opcode);
			break;
		}
	}

	/* Should not be reached! */
#ifdef ATPPC_DEBUG
	panic("%s: unexpected code reached!\n", __func__);
#endif
}

/* General I/O routine */
static u_int8_t
atppc_io(device_t dev, int iop, u_char *addr, int cnt, u_char byte)
{
	struct atppc_softc *atppc = device_private(dev);
	u_int8_t val = 0;

	mutex_enter(&atppc->sc_lock);

	switch (iop) {
	case PPBUS_OUTSB_EPP:
		bus_space_write_multi_1(atppc->sc_iot, atppc->sc_ioh,
			ATPPC_EPP_DATA, addr, cnt);
		break;
	case PPBUS_OUTSW_EPP:
		bus_space_write_multi_2(atppc->sc_iot, atppc->sc_ioh,
			ATPPC_EPP_DATA, (u_int16_t *)addr, cnt);
		break;
	case PPBUS_OUTSL_EPP:
		bus_space_write_multi_4(atppc->sc_iot, atppc->sc_ioh,
			ATPPC_EPP_DATA, (u_int32_t *)addr, cnt);
		break;
	case PPBUS_INSB_EPP:
		bus_space_read_multi_1(atppc->sc_iot, atppc->sc_ioh,
			ATPPC_EPP_DATA, addr, cnt);
		break;
	case PPBUS_INSW_EPP:
		bus_space_read_multi_2(atppc->sc_iot, atppc->sc_ioh,
			ATPPC_EPP_DATA, (u_int16_t *)addr, cnt);
		break;
	case PPBUS_INSL_EPP:
		bus_space_read_multi_4(atppc->sc_iot, atppc->sc_ioh,
			ATPPC_EPP_DATA, (u_int32_t *)addr, cnt);
		break;
	case PPBUS_RDTR:
		val = (atppc_r_dtr(atppc));
		break;
	case PPBUS_RSTR:
		val = (atppc_r_str(atppc));
		break;
	case PPBUS_RCTR:
		val = (atppc_r_ctr(atppc));
		break;
	case PPBUS_REPP_A:
		val = (atppc_r_eppA(atppc));
		break;
	case PPBUS_REPP_D:
		val = (atppc_r_eppD(atppc));
		break;
	case PPBUS_RECR:
		val = (atppc_r_ecr(atppc));
		break;
	case PPBUS_RFIFO:
		val = (atppc_r_fifo(atppc));
		break;
	case PPBUS_WDTR:
		atppc_w_dtr(atppc, byte);
		break;
	case PPBUS_WSTR:
		atppc_w_str(atppc, byte);
		break;
	case PPBUS_WCTR:
		atppc_w_ctr(atppc, byte);
		break;
	case PPBUS_WEPP_A:
		atppc_w_eppA(atppc, byte);
		break;
	case PPBUS_WEPP_D:
		atppc_w_eppD(atppc, byte);
		break;
	case PPBUS_WECR:
		atppc_w_ecr(atppc, byte);
		break;
	case PPBUS_WFIFO:
		atppc_w_fifo(atppc, byte);
		break;
	default:
		panic("%s(%s): unknown I/O operation", device_xname(dev),
			__func__);
		break;
	}

	atppc_barrier(atppc);

	mutex_exit(&atppc->sc_lock);

	return val;
}

/* Read "instance variables" of atppc device */
static int
atppc_read_ivar(device_t dev, int index, unsigned int *val)
{
	struct atppc_softc *atppc = device_private(dev);
	int rval = 0;

	mutex_enter(&atppc->sc_lock);

	switch(index) {
	case PPBUS_IVAR_EPP_PROTO:
		if (atppc->sc_epp == ATPPC_EPP_1_9)
			*val = PPBUS_EPP_1_9;
		else if (atppc->sc_epp == ATPPC_EPP_1_7)
			*val = PPBUS_EPP_1_7;
		/* XXX what if not using EPP ? */
		break;

	case PPBUS_IVAR_INTR:
		*val = ((atppc->sc_use & ATPPC_USE_INTR) != 0);
		break;

	case PPBUS_IVAR_DMA:
		*val = ((atppc->sc_use & ATPPC_USE_DMA) != 0);
		break;

	default:
		rval = ENODEV;
	}

	mutex_exit(&atppc->sc_lock);

	return rval;
}

/* Write "instance varaibles" of atppc device */
static int
atppc_write_ivar(device_t dev, int index, unsigned int *val)
{
	struct atppc_softc *atppc = device_private(dev);
	int rval = 0;

	mutex_enter(&atppc->sc_lock);

	switch(index) {
	case PPBUS_IVAR_EPP_PROTO:
		if (*val == PPBUS_EPP_1_9 || *val == PPBUS_EPP_1_7)
			atppc->sc_epp = *val;
		else
			rval = EINVAL;
		break;

	case PPBUS_IVAR_INTR:
		if (*val == 0)
			atppc->sc_use &= ~ATPPC_USE_INTR;
		else if (atppc->sc_has & ATPPC_HAS_INTR)
			atppc->sc_use |= ATPPC_USE_INTR;
		else
			rval = ENODEV;
		break;

	case PPBUS_IVAR_DMA:
		if (*val == 0)
			atppc->sc_use &= ~ATPPC_USE_DMA;
		else if (atppc->sc_has & ATPPC_HAS_DMA)
			atppc->sc_use |= ATPPC_USE_DMA;
		else
			rval = ENODEV;
		break;

	default:
		rval = ENODEV;
	}

	mutex_exit(&atppc->sc_lock);

	return rval;
}

/* Add a handler routine to be called by the interrupt handler */
static int
atppc_add_handler(device_t dev, void (*handler)(void *), void *arg)
{
	struct atppc_softc *atppc = device_private(dev);
	struct atppc_handler_node *callback;
	int rval = 0;

	if (handler == NULL) {
		ATPPC_DPRINTF(("%s(%s): attempt to register NULL handler.\n",
			__func__, device_xname(dev)));
		rval = EINVAL;
	} else {
		callback = kmem_alloc(sizeof(*callback), KM_SLEEP);
		if (callback) {
			callback->func = handler;
			callback->arg = arg;
			mutex_enter(&atppc->sc_lock);
			SLIST_INSERT_HEAD(&(atppc->sc_handler_listhead),
				callback, entries);
			mutex_exit(&atppc->sc_lock);
		} else {
			rval = ENOMEM;
		}
	}

	return rval;
}

/* Remove a handler added by atppc_add_handler() */
static int
atppc_remove_handler(device_t dev, void (*handler)(void *))
{
	struct atppc_softc *atppc = device_private(dev);
	struct atppc_handler_node *callback;
	int rval = EINVAL;

	mutex_enter(&atppc->sc_lock);

	if (SLIST_EMPTY(&(atppc->sc_handler_listhead)))
		panic("%s(%s): attempt to remove handler from empty list.\n",
			__func__, device_xname(dev));

	/* Search list for handler */
	SLIST_FOREACH(callback, &(atppc->sc_handler_listhead), entries) {
		if (callback->func == handler) {
			SLIST_REMOVE(&(atppc->sc_handler_listhead), callback,
				atppc_handler_node, entries);
			rval = 0;
			break;
		}
	}

	mutex_exit(&atppc->sc_lock);

	if (rval == 0) {
		kmem_free(callback, sizeof(*callback));
	}

	return rval;
}

/* Utility functions */


/*
 * Functions that read bytes from port into buffer: called from interrupt
 * handler depending on current chipset mode and cause of interrupt. Return
 * value: number of bytes moved.
 */

/* Only the lower 4 bits of the final value are valid */
#define nibble2char(s) ((((s) & ~nACK) >> 3) | (~(s) & nBUSY) >> 4)

/* Read bytes in nibble mode */
static void
atppc_nibble_read(struct atppc_softc *atppc)
{
	int i;
	u_int8_t nibble[2];
	u_int8_t ctr;
	u_int8_t str;

	/* Enable interrupts if needed */
	if (atppc->sc_use & ATPPC_USE_INTR) {
		ctr = atppc_r_ctr(atppc);
		atppc_barrier_r(atppc);
		if (!(ctr & IRQENABLE)) {
			ctr |= IRQENABLE;
			atppc_w_ctr(atppc, ctr);
			atppc_barrier_w(atppc);
		}
	}

	while (atppc->sc_inbstart < (atppc->sc_inb + atppc->sc_inb_nbytes)) {
		/* Check if device has data to send in idle phase */
		str = atppc_r_str(atppc);
		atppc_barrier_r(atppc);
		if (str & nDATAVAIL) {
			return;
		}

		/* Nibble-mode handshake transfer */
		for (i = 0; i < 2; i++) {
			/* Event 7 - ready to take data (HOSTBUSY low) */
			ctr = atppc_r_ctr(atppc);
			atppc_barrier_r(atppc);
			ctr |= HOSTBUSY;
			atppc_w_ctr(atppc, ctr);
			atppc_barrier_w(atppc);

			/* Event 8 - peripheral writes the first nibble */

			/* Event 9 - peripheral set nAck low */
			atppc->sc_inerr = atppc_poll_str(atppc, 0, PTRCLK);
			if (atppc->sc_inerr)
				return;

			/* read nibble */
			nibble[i] = atppc_r_str(atppc);

			/* Event 10 - ack, nibble received */
			ctr &= ~HOSTBUSY;
			atppc_w_ctr(atppc, ctr);

			/* Event 11 - wait ack from peripheral */
			if (atppc->sc_use & ATPPC_USE_INTR)
				atppc->sc_inerr = atppc_wait_interrupt(atppc,
					&atppc->sc_in_cv, ATPPC_IRQ_nACK);
			else
				atppc->sc_inerr = atppc_poll_str(atppc, PTRCLK,
					PTRCLK);
			if (atppc->sc_inerr)
				return;
		}

		/* Store byte transfered */
		*(atppc->sc_inbstart) = ((nibble2char(nibble[1]) << 4) & 0xf0) |
			(nibble2char(nibble[0]) & 0x0f);
		atppc->sc_inbstart++;
	}
}

/* Read bytes in bidirectional mode */
static void
atppc_byte_read(struct atppc_softc * const atppc)
{
	u_int8_t ctr;
	u_int8_t str;

	/* Check direction bit */
	ctr = atppc_r_ctr(atppc);
	atppc_barrier_r(atppc);
	if (!(ctr & PCD)) {
		ATPPC_DPRINTF(("%s: byte-mode read attempted without direction "
			"bit set.", device_xname(atppc->sc_dev)));
		atppc->sc_inerr = ENODEV;
		return;
	}
	/* Enable interrupts if needed */
	if (atppc->sc_use & ATPPC_USE_INTR) {
		if (!(ctr & IRQENABLE)) {
			ctr |= IRQENABLE;
			atppc_w_ctr(atppc, ctr);
			atppc_barrier_w(atppc);
		}
	}

	/* Byte-mode handshake transfer */
	while (atppc->sc_inbstart < (atppc->sc_inb + atppc->sc_inb_nbytes)) {
		/* Check if device has data to send */
		str = atppc_r_str(atppc);
		atppc_barrier_r(atppc);
		if (str & nDATAVAIL) {
			return;
		}

		/* Event 7 - ready to take data (nAUTO low) */
		ctr |= HOSTBUSY;
		atppc_w_ctr(atppc, ctr);
		atppc_barrier_w(atppc);

		/* Event 9 - peripheral set nAck low */
		atppc->sc_inerr = atppc_poll_str(atppc, 0, PTRCLK);
		if (atppc->sc_inerr)
			return;

		/* Store byte transfered */
		*(atppc->sc_inbstart) = atppc_r_dtr(atppc);
		atppc_barrier_r(atppc);

		/* Event 10 - data received, can't accept more */
		ctr &= ~HOSTBUSY;
		atppc_w_ctr(atppc, ctr);
		atppc_barrier_w(atppc);

		/* Event 11 - peripheral ack */
		if (atppc->sc_use & ATPPC_USE_INTR)
			atppc->sc_inerr = atppc_wait_interrupt(atppc,
				&atppc->sc_in_cv, ATPPC_IRQ_nACK);
		else
			atppc->sc_inerr = atppc_poll_str(atppc, PTRCLK, PTRCLK);
		if (atppc->sc_inerr)
			return;

		/* Event 16 - strobe */
		str |= HOSTCLK;
		atppc_w_str(atppc, str);
		atppc_barrier_w(atppc);
		DELAY(1);
		str &= ~HOSTCLK;
		atppc_w_str(atppc, str);
		atppc_barrier_w(atppc);

		/* Update counter */
		atppc->sc_inbstart++;
	}
}

/* Read bytes in EPP mode */
static void
atppc_epp_read(struct atppc_softc * atppc)
{
	if (atppc->sc_epp == ATPPC_EPP_1_9) {
		{
			uint8_t str;
			int i;

			atppc_reset_epp_timeout(atppc->sc_dev);
			for (i = 0; i < atppc->sc_inb_nbytes; i++) {
				 *(atppc->sc_inbstart) = atppc_r_eppD(atppc);
				atppc_barrier_r(atppc);
				str = atppc_r_str(atppc);
				atppc_barrier_r(atppc);
				if (str & TIMEOUT) {
					atppc->sc_inerr = EIO;
					break;
				}
				atppc->sc_inbstart++;
			}
		}
	} else {
		/* Read data block from EPP data register */
		atppc_r_eppD_multi(atppc, atppc->sc_inbstart,
			atppc->sc_inb_nbytes);
		atppc_barrier_r(atppc);
		/* Update buffer position, byte count and counter */
		atppc->sc_inbstart += atppc->sc_inb_nbytes;
	}

	return;
}

/* Read bytes in ECP mode */
static void
atppc_ecp_read(struct atppc_softc *atppc)
{
	u_int8_t ecr;
	u_int8_t ctr;
	u_int8_t str;
	const unsigned char ctr_sav = atppc_r_ctr(atppc);
	const unsigned char ecr_sav = atppc_r_ecr(atppc);
	unsigned int worklen;

	/* Check direction bit */
	ctr = ctr_sav;
	atppc_barrier_r(atppc);
	if (!(ctr & PCD)) {
		ATPPC_DPRINTF(("%s: ecp-mode read attempted without direction "
			"bit set.", device_xname(atppc->sc_dev)));
		atppc->sc_inerr = ENODEV;
		goto end;
	}

	/* Clear device request if any */
	if (atppc->sc_use & ATPPC_USE_INTR)
		atppc->sc_irqstat &= ~ATPPC_IRQ_nFAULT;

	while (atppc->sc_inbstart < (atppc->sc_inb + atppc->sc_inb_nbytes)) {
		ecr = atppc_r_ecr(atppc);
		atppc_barrier_r(atppc);
		if (ecr & ATPPC_FIFO_EMPTY) {
			/* Check for invalid state */
			if (ecr & ATPPC_FIFO_FULL) {
				atppc_ecp_read_error(atppc);
				break;
			}

			/* Check if device has data to send */
			str = atppc_r_str(atppc);
			atppc_barrier_r(atppc);
			if (str & nDATAVAIL) {
				break;
			}

			if (atppc->sc_use & ATPPC_USE_INTR) {
				/* Enable interrupts */
				ecr &= ~ATPPC_SERVICE_INTR;
				atppc_w_ecr(atppc, ecr);
				atppc_barrier_w(atppc);
				/* Wait for FIFO to fill */
				atppc->sc_inerr = atppc_wait_interrupt(atppc,
					&atppc->sc_in_cv, ATPPC_IRQ_FIFO);
				if (atppc->sc_inerr)
					break;
			} else {
				DELAY(1);
			}
			continue;
		}
		else if (ecr & ATPPC_FIFO_FULL) {
			/* Transfer sc_fifo bytes */
			worklen = atppc->sc_fifo;
		}
		else if (ecr & ATPPC_SERVICE_INTR) {
			/* Transfer sc_rthr bytes */
			worklen = atppc->sc_rthr;
		} else {
			/* At least one byte is in the FIFO */
			worklen = 1;
		}

		if ((atppc->sc_use & ATPPC_USE_INTR) &&
			(atppc->sc_use & ATPPC_USE_DMA)) {

			atppc_ecp_read_dma(atppc, &worklen, ecr);
		} else {
			atppc_ecp_read_pio(atppc, &worklen, ecr);
		}

		if (atppc->sc_inerr) {
			atppc_ecp_read_error(atppc);
			break;
		}

		/* Update counter */
		atppc->sc_inbstart += worklen;
	}
end:
	atppc_w_ctr(atppc, ctr_sav);
	atppc_w_ecr(atppc, ecr_sav);
	atppc_barrier_w(atppc);
}

/* Read bytes in ECP mode using DMA transfers */
static void
atppc_ecp_read_dma(struct atppc_softc *atppc, unsigned int *length,
	unsigned char ecr)
{
	/* Limit transfer to maximum DMA size and start it */
	*length = min(*length, atppc->sc_dma_maxsize);
	atppc->sc_dmastat = ATPPC_DMA_INIT;
	atppc->sc_dma_start(atppc, atppc->sc_inbstart, *length,
		ATPPC_DMA_MODE_READ);

	atppc->sc_dmastat = ATPPC_DMA_STARTED;

	/* Enable interrupts, DMA */
	ecr &= ~ATPPC_SERVICE_INTR;
	ecr |= ATPPC_ENABLE_DMA;
	atppc_w_ecr(atppc, ecr);
	atppc_barrier_w(atppc);

	/* Wait for DMA completion */
	atppc->sc_inerr = atppc_wait_interrupt(atppc, &atppc->sc_in_cv,
		ATPPC_IRQ_DMA);
	if (atppc->sc_inerr)
		return;

	/* Get register value recorded by interrupt handler */
	ecr = atppc->sc_ecr_intr;
	/* Clear DMA programming */
	atppc->sc_dma_finish(atppc);
	atppc->sc_dmastat = ATPPC_DMA_COMPLETE;
	/* Disable DMA */
	ecr &= ~ATPPC_ENABLE_DMA;
	atppc_w_ecr(atppc, ecr);
	atppc_barrier_w(atppc);
}

/* Read bytes in ECP mode using PIO transfers */
static void
atppc_ecp_read_pio(struct atppc_softc *atppc, unsigned int *length,
	unsigned char ecr)
{
	/* Disable DMA */
	ecr &= ~ATPPC_ENABLE_DMA;
	atppc_w_ecr(atppc, ecr);
	atppc_barrier_w(atppc);

	/* Read from FIFO */
	atppc_r_fifo_multi(atppc, atppc->sc_inbstart, *length);
}

/* Handle errors for ECP reads */
static void
atppc_ecp_read_error(struct atppc_softc *atppc)
{
	unsigned char ecr = atppc_r_ecr(atppc);

	/* Abort DMA if not finished */
	if (atppc->sc_dmastat == ATPPC_DMA_STARTED) {
		atppc->sc_dma_abort(atppc);
		ATPPC_DPRINTF(("%s: DMA interrupted.\n", __func__));
	}

	/* Check for invalid states */
	if ((ecr & ATPPC_FIFO_EMPTY) && (ecr & ATPPC_FIFO_FULL)) {
		ATPPC_DPRINTF(("%s: FIFO full+empty bits set.\n", __func__));
		ATPPC_DPRINTF(("%s: reseting FIFO.\n", __func__));
		atppc_w_ecr(atppc, ATPPC_ECR_PS2);
		atppc_barrier_w(atppc);
	}
}

/*
 * Functions that write bytes to port from buffer: called from atppc_write()
 * function depending on current chipset mode. Returns number of bytes moved.
 */

/* Write bytes in std/bidirectional mode */
static void
atppc_std_write(struct atppc_softc * const atppc)
{
	unsigned char ctr;

	ctr = atppc_r_ctr(atppc);
	atppc_barrier_r(atppc);
	/* Enable interrupts if needed */
	if (atppc->sc_use & ATPPC_USE_INTR) {
		if (!(ctr & IRQENABLE)) {
			ctr |= IRQENABLE;
			atppc_w_ctr(atppc, ctr);
			atppc_barrier_w(atppc);
		}
	}

	while (atppc->sc_outbstart < (atppc->sc_outb + atppc->sc_outb_nbytes)) {
		/* Wait for peripheral to become ready for MAXBUSYWAIT */
		atppc->sc_outerr = atppc_poll_str(atppc, SPP_READY, SPP_MASK);
		if (atppc->sc_outerr)
			return;

		/* Put data in data register */
		atppc_w_dtr(atppc, *(atppc->sc_outbstart));
		atppc_barrier_w(atppc);
		DELAY(1);

		/* Pulse strobe to indicate valid data on lines */
		ctr |= STROBE;
		atppc_w_ctr(atppc, ctr);
		atppc_barrier_w(atppc);
		DELAY(1);
		ctr &= ~STROBE;
		atppc_w_ctr(atppc, ctr);
		atppc_barrier_w(atppc);

		/* Wait for nACK for MAXBUSYWAIT */
		if (atppc->sc_use & ATPPC_USE_INTR) {
			atppc->sc_outerr = atppc_wait_interrupt(atppc,
				&atppc->sc_out_cv, ATPPC_IRQ_nACK);
			if (atppc->sc_outerr)
				return;
		} else {
			/* Try to catch the pulsed acknowledgement */
			atppc->sc_outerr = atppc_poll_str(atppc, 0, nACK);
			if (atppc->sc_outerr)
				return;
			atppc->sc_outerr = atppc_poll_str(atppc, nACK, nACK);
			if (atppc->sc_outerr)
				return;
		}

		/* Update buffer position, byte count and counter */
		atppc->sc_outbstart++;
	}
}


/* Write bytes in EPP mode */
static void
atppc_epp_write(struct atppc_softc *atppc)
{
	if (atppc->sc_epp == ATPPC_EPP_1_9) {
		{
			uint8_t str;
			int i;

			atppc_reset_epp_timeout(atppc->sc_dev);
			for (i = 0; i < atppc->sc_outb_nbytes; i++) {
				atppc_w_eppD(atppc, *(atppc->sc_outbstart));
				atppc_barrier_w(atppc);
				str = atppc_r_str(atppc);
				atppc_barrier_r(atppc);
				if (str & TIMEOUT) {
					atppc->sc_outerr = EIO;
					break;
				}
				atppc->sc_outbstart++;
			}
		}
	} else {
		/* Write data block to EPP data register */
		atppc_w_eppD_multi(atppc, atppc->sc_outbstart,
			atppc->sc_outb_nbytes);
		atppc_barrier_w(atppc);
		/* Update buffer position, byte count and counter */
		atppc->sc_outbstart += atppc->sc_outb_nbytes;
	}

	return;
}


/* Write bytes in ECP/Fast Centronics mode */
static void
atppc_fifo_write(struct atppc_softc * const atppc)
{
	unsigned char ctr;
	unsigned char ecr;
	const unsigned char ctr_sav = atppc_r_ctr(atppc);
	const unsigned char ecr_sav = atppc_r_ecr(atppc);

	ctr = ctr_sav;
	ecr = ecr_sav;
	atppc_barrier_r(atppc);

	/* Reset and flush FIFO */
	atppc_w_ecr(atppc, ATPPC_ECR_PS2);
	atppc_barrier_w(atppc);
	/* Disable nAck interrupts and initialize port bits */
	ctr &= ~(IRQENABLE | STROBE | AUTOFEED);
	atppc_w_ctr(atppc, ctr);
	atppc_barrier_w(atppc);
	/* Restore mode */
	atppc_w_ecr(atppc, ecr);
	atppc_barrier_w(atppc);

	/* DMA or Programmed IO */
	if ((atppc->sc_use & ATPPC_USE_DMA) &&
		(atppc->sc_use & ATPPC_USE_INTR)) {

		atppc_fifo_write_dma(atppc, ecr, ctr);
	} else {
		atppc_fifo_write_pio(atppc, ecr, ctr);
	}

	/* Restore original register values */
	atppc_w_ctr(atppc, ctr_sav);
	atppc_w_ecr(atppc, ecr_sav);
	atppc_barrier_w(atppc);
}

static void
atppc_fifo_write_dma(struct atppc_softc * const atppc, unsigned char ecr,
	unsigned char ctr)
{
	unsigned int len;
	unsigned int worklen;

	for (len = (atppc->sc_outb + atppc->sc_outb_nbytes) -
		atppc->sc_outbstart; len > 0; len = (atppc->sc_outb +
		atppc->sc_outb_nbytes) - atppc->sc_outbstart) {

		/* Wait for device to become ready */
		atppc->sc_outerr = atppc_poll_str(atppc, SPP_READY, SPP_MASK);
		if (atppc->sc_outerr)
			return;

		/* Reset chipset for next DMA transfer */
		atppc_w_ecr(atppc, ATPPC_ECR_PS2);
		atppc_barrier_w(atppc);
		atppc_w_ecr(atppc, ecr);
		atppc_barrier_w(atppc);

		/* Limit transfer to maximum DMA size and start it */
		worklen = min(len, atppc->sc_dma_maxsize);
		atppc->sc_dmastat = ATPPC_DMA_INIT;
		atppc->sc_dma_start(atppc, atppc->sc_outbstart,
			worklen, ATPPC_DMA_MODE_WRITE);
		atppc->sc_dmastat = ATPPC_DMA_STARTED;

		/* Enable interrupts, DMA */
		ecr &= ~ATPPC_SERVICE_INTR;
		ecr |= ATPPC_ENABLE_DMA;
		atppc_w_ecr(atppc, ecr);
		atppc_barrier_w(atppc);

		/* Wait for DMA completion */
		atppc->sc_outerr = atppc_wait_interrupt(atppc,
			&atppc->sc_out_cv, ATPPC_IRQ_DMA);
		if (atppc->sc_outerr) {
			atppc_fifo_write_error(atppc, worklen);
			return;
		}
		/* Get register value recorded by interrupt handler */
		ecr = atppc->sc_ecr_intr;
		/* Clear DMA programming */
		atppc->sc_dma_finish(atppc);
		atppc->sc_dmastat = ATPPC_DMA_COMPLETE;
		/* Disable DMA */
		ecr &= ~ATPPC_ENABLE_DMA;
		atppc_w_ecr(atppc, ecr);
		atppc_barrier_w(atppc);

		/* Wait for FIFO to empty */
		for (;;) {
			if (ecr & ATPPC_FIFO_EMPTY) {
				if (ecr & ATPPC_FIFO_FULL) {
					atppc->sc_outerr = EIO;
					atppc_fifo_write_error(atppc, worklen);
					return;
				} else {
					break;
				}
			}

			/* Enable service interrupt */
			ecr &= ~ATPPC_SERVICE_INTR;
			atppc_w_ecr(atppc, ecr);
			atppc_barrier_w(atppc);

			atppc->sc_outerr = atppc_wait_interrupt(atppc,
				&atppc->sc_out_cv, ATPPC_IRQ_FIFO);
			if (atppc->sc_outerr) {
				atppc_fifo_write_error(atppc, worklen);
				return;
			}

			/* Get register value recorded by interrupt handler */
			ecr = atppc->sc_ecr_intr;
		}

		/* Update pointer */
		atppc->sc_outbstart += worklen;
	}
}

static void
atppc_fifo_write_pio(struct atppc_softc * const atppc, unsigned char ecr,
	unsigned char ctr)
{
	unsigned int len;
	unsigned int worklen;
	unsigned int timecount;

	/* Disable DMA */
	ecr &= ~ATPPC_ENABLE_DMA;
	atppc_w_ecr(atppc, ecr);
	atppc_barrier_w(atppc);

	for (len = (atppc->sc_outb + atppc->sc_outb_nbytes) -
		atppc->sc_outbstart; len > 0; len = (atppc->sc_outb +
		atppc->sc_outb_nbytes) - atppc->sc_outbstart) {

		/* Wait for device to become ready */
		atppc->sc_outerr = atppc_poll_str(atppc, SPP_READY, SPP_MASK);
		if (atppc->sc_outerr)
			return;

		/* Limit transfer to minimum of space in FIFO and buffer */
		worklen = min(len, atppc->sc_fifo);

		/* Write to FIFO */
		atppc_w_fifo_multi(atppc, atppc->sc_outbstart, worklen);

		timecount = 0;
		if (atppc->sc_use & ATPPC_USE_INTR) {
			ecr = atppc_r_ecr(atppc);
			atppc_barrier_w(atppc);

			/* Wait for interrupt */
			for (;;) {
				if (ecr & ATPPC_FIFO_EMPTY) {
					if (ecr & ATPPC_FIFO_FULL) {
						atppc->sc_outerr = EIO;
						atppc_fifo_write_error(atppc,
							worklen);
						return;
					} else {
						break;
					}
				}

				/* Enable service interrupt */
				ecr &= ~ATPPC_SERVICE_INTR;
				atppc_w_ecr(atppc, ecr);
				atppc_barrier_w(atppc);

				atppc->sc_outerr = atppc_wait_interrupt(atppc,
					&atppc->sc_out_cv, ATPPC_IRQ_FIFO);
				if (atppc->sc_outerr) {
					atppc_fifo_write_error(atppc, worklen);
					return;
				}

				/* Get ECR value saved by interrupt handler */
				ecr = atppc->sc_ecr_intr;
			}
		} else {
			for (; timecount < ((MAXBUSYWAIT/hz)*1000000);
				timecount++) {

				ecr = atppc_r_ecr(atppc);
				atppc_barrier_r(atppc);
				if (ecr & ATPPC_FIFO_EMPTY) {
					if (ecr & ATPPC_FIFO_FULL) {
						atppc->sc_outerr = EIO;
						atppc_fifo_write_error(atppc,
							worklen);
						return;
					} else {
						break;
					}
				}
				DELAY(1);
			}

			if (((timecount*hz)/1000000) >= MAXBUSYWAIT) {
				atppc->sc_outerr = EIO;
				atppc_fifo_write_error(atppc, worklen);
				return;
			}
		}

		/* Update pointer */
		atppc->sc_outbstart += worklen;
	}
}

static void
atppc_fifo_write_error(struct atppc_softc * const atppc,
	const unsigned int worklen)
{
	unsigned char ecr = atppc_r_ecr(atppc);

	/* Abort DMA if not finished */
	if (atppc->sc_dmastat == ATPPC_DMA_STARTED) {
		atppc->sc_dma_abort(atppc);
		ATPPC_DPRINTF(("%s: DMA interrupted.\n", __func__));
	}

	/* Check for invalid states */
	if ((ecr & ATPPC_FIFO_EMPTY) && (ecr & ATPPC_FIFO_FULL)) {
		ATPPC_DPRINTF(("%s: FIFO full+empty bits set.\n", __func__));
	} else if (!(ecr & ATPPC_FIFO_EMPTY)) {
		unsigned char ctr = atppc_r_ctr(atppc);
		int bytes_left;
		int i;

		ATPPC_DPRINTF(("%s(%s): FIFO not empty.\n", __func__,
			device_xname(atppc->sc_dev)));

		/* Drive strobe low to stop data transfer */
		ctr &= ~STROBE;
		atppc_w_ctr(atppc, ctr);
		atppc_barrier_w(atppc);

		/* Determine how many bytes remain in FIFO */
		for (i = 0; i < atppc->sc_fifo; i++) {
			atppc_w_fifo(atppc, (unsigned char)i);
			ecr = atppc_r_ecr(atppc);
			atppc_barrier_r(atppc);
			if (ecr & ATPPC_FIFO_FULL)
				break;
		}
		bytes_left = (atppc->sc_fifo) - (i + 1);
		ATPPC_DPRINTF(("%s: %d bytes left in FIFO.\n", 	__func__,
			bytes_left));

		/* Update counter */
		atppc->sc_outbstart += (worklen - bytes_left);
	} else {
		/* Update counter */
		atppc->sc_outbstart += worklen;
	}

	ATPPC_DPRINTF(("%s: reseting FIFO.\n", __func__));
	atppc_w_ecr(atppc, ATPPC_ECR_PS2);
	atppc_barrier_w(atppc);
}

/*
 * Poll status register using mask and status for MAXBUSYWAIT.
 * Returns 0 if device ready, error value otherwise.
 */
static int
atppc_poll_str(const struct atppc_softc * const atppc, const u_int8_t status,
	const u_int8_t mask)
{
	unsigned int timecount;
	u_int8_t str;
	int error = EIO;

	/* Wait for str to have status for MAXBUSYWAIT */
	for (timecount = 0; timecount < ((MAXBUSYWAIT/hz)*1000000);
		timecount++) {

		str = atppc_r_str(atppc);
		atppc_barrier_r(atppc);
		if ((str & mask) == status) {
			error = 0;
			break;
		}
		DELAY(1);
	}

	return error;
}

/* Wait for interrupt for MAXBUSYWAIT: returns 0 if acknowledge received. */
static int
atppc_wait_interrupt(struct atppc_softc * const atppc, kcondvar_t *cv,
	const u_int8_t irqstat)
{
	int error = EIO;

	atppc->sc_irqstat &= ~irqstat;

	/* Wait for interrupt for MAXBUSYWAIT */
	error = cv_timedwait_sig(cv, &atppc->sc_lock, MAXBUSYWAIT);

	if (!(error) && (atppc->sc_irqstat & irqstat)) {
		atppc->sc_irqstat &= ~irqstat;
		error = 0;
	}

	return error;
}
