/* $NetBSD: ppbus_base.c,v 1.20 2014/10/14 01:04:39 christos Exp $ */

/*-
 * Copyright (c) 1997, 1998, 1999 Nicolas Souchu
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
 * FreeBSD: src/sys/dev/ppbus/ppb_base.c,v 1.10.2.1 2000/08/01 23:26:26 n_hibma Exp
 *
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ppbus_base.c,v 1.20 2014/10/14 01:04:39 christos Exp $");

#include "opt_ppbus_1284.h"
#include "opt_ppbus.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/systm.h>

#include <dev/ppbus/ppbus_1284.h>
#include <dev/ppbus/ppbus_conf.h>
#include <dev/ppbus/ppbus_base.h>
#include <dev/ppbus/ppbus_device.h>
#include <dev/ppbus/ppbus_io.h>
#include <dev/ppbus/ppbus_var.h>

#ifndef DONTPROBE_1284
/* Utility functions */
static char * search_token(char *, int, const char *);
#endif

/* Perform general ppbus I/O request */
int
ppbus_io(device_t dev, int iop, u_char * addr, int cnt, u_char byte)
{
	struct ppbus_softc * bus = device_private(dev);
	return (bus->ppbus_io(device_parent(dev), iop, addr, cnt, byte));
}

/* Execute microsequence */
int
ppbus_exec_microseq(device_t dev, struct ppbus_microseq ** sequence)
{
	struct ppbus_softc * bus = device_private(dev);
	return (bus->ppbus_exec_microseq(device_parent(dev), sequence));
}

/* Read instance variables of ppbus */
int
ppbus_read_ivar(device_t dev, int index, unsigned int * val)

{
	struct ppbus_softc * bus = device_private(dev);

	switch (index) {
	case PPBUS_IVAR_INTR:
	case PPBUS_IVAR_EPP_PROTO:
	case PPBUS_IVAR_DMA:
		return (bus->ppbus_read_ivar(device_parent(dev), index, val));

	case PPBUS_IVAR_IEEE:
		*val = (bus->sc_use_ieee == PPBUS_ENABLE_IEEE) ? 1 : 0;
		break;

	default:
		return (ENOENT);
	}

	return 0;
}

/* Write an instance variable */
int
ppbus_write_ivar(device_t dev, int index, unsigned int * val)
{
	struct ppbus_softc * bus = device_private(dev);

	switch (index) {
	case PPBUS_IVAR_INTR:
	case PPBUS_IVAR_EPP_PROTO:
	case PPBUS_IVAR_DMA:
		return (bus->ppbus_write_ivar(device_parent(dev), index, val));

	case PPBUS_IVAR_IEEE:
		bus->sc_use_ieee = ((*val != 0) ? PPBUS_ENABLE_IEEE :
			PPBUS_DISABLE_IEEE);
		break;

	default:
		return (ENOENT);
	}

	return 0;
}

/* Polls the bus for a max of 10-milliseconds */
int
ppbus_poll_bus(device_t dev, int maxp, char mask, char status,
	int how)
{
	int i, j, error;
	char r;

	/* try at least up to 10ms */
	for (j = 0; j < ((how & PPBUS_POLL) ? maxp : 1); j++) {
		for (i = 0; i < 10000; i++) {
			r = ppbus_rstr(dev);
			DELAY(1);
			if ((r & mask) == status)
				return (0);
		}
	}

	if (!(how & PPBUS_POLL)) {
	   for (i = 0; maxp == PPBUS_FOREVER || i < maxp-1; i++) {
		if ((ppbus_rstr(dev) & mask) == status)
			return (0);

		switch (how) {
		case PPBUS_NOINTR:
			/* wait 10 ms */
			kpause("ppbuspoll", false, hz / 100, NULL);
			break;

		case PPBUS_INTR:
		default:
			/* wait 10 ms */
			error = kpause("ppbuspoll", true, hz / 100, NULL);
			if (error != EWOULDBLOCK) {
				return error;
			}
			break;
		}
	   }
	}

	return (EWOULDBLOCK);
}

/* Get operating mode of the chipset */
int
ppbus_get_mode(device_t dev)
{
	struct ppbus_softc * bus = device_private(dev);

	return (bus->sc_mode);
}

/* Set the operating mode of the chipset, return 0 on success. */
int
ppbus_set_mode(device_t dev, int mode, int options)
{
	struct ppbus_softc * bus = device_private(dev);
	int error = 0;

	/* If no mode change, do nothing */
	if(bus->sc_mode == mode)
		return error;

	/* Do necessary negotiations */
	if(bus->sc_use_ieee == PPBUS_ENABLE_IEEE) {
		/* Cannot negotiate standard mode */
		if(!(mode & (PPBUS_FAST | PPBUS_COMPATIBLE))) {
 			error = ppbus_1284_negotiate(dev, mode, options);
		}
		/* Termination is unnecessary for standard<->fast */
		else if(!(bus->sc_mode & (PPBUS_FAST | PPBUS_COMPATIBLE))) {
			ppbus_1284_terminate(dev);
		}
	}

	if(!error) {
		/* Set mode and update mode of ppbus to actual mode */
		error = bus->ppbus_setmode(device_parent(dev), mode);
		bus->sc_mode = bus->ppbus_getmode(device_parent(dev));
	}

	/* Update host state if necessary */
	if(!(error) && (bus->sc_use_ieee == PPBUS_ENABLE_IEEE)) {
		switch(mode) {
		case PPBUS_COMPATIBLE:
		case PPBUS_FAST:
		case PPBUS_EPP:
		case PPBUS_ECP:
			ppbus_1284_set_state(dev, PPBUS_FORWARD_IDLE);
			break;

		case PPBUS_NIBBLE:
		case PPBUS_PS2:
			ppbus_1284_set_state(dev, PPBUS_REVERSE_IDLE);
			break;
		}
	}

	return error;
}

/* Write charaters to the port */
int
ppbus_write(device_t dev, char * buf, int len, int how, size_t * cnt)
{
	struct ppbus_softc * bus = device_private(dev);

	if(bus->sc_use_ieee == PPBUS_ENABLE_IEEE) {
		if(bus->sc_1284_state != PPBUS_FORWARD_IDLE) {
			printf("%s(%s): bus not in forward idle mode.\n",
				__func__, device_xname(dev));
			return ENODEV;
		}
	}

	return (bus->ppbus_write(device_parent(bus->sc_dev), buf, len, how, cnt));
}

/* Read charaters from the port */
int
ppbus_read(device_t dev, char * buf, int len, int how, size_t * cnt)
{
	struct ppbus_softc * bus = device_private(dev);

	if(bus->sc_use_ieee == PPBUS_ENABLE_IEEE) {
		if(bus->sc_1284_state != PPBUS_REVERSE_IDLE) {
			printf("%s(%s): bus not in reverse idle mode.\n",
				__func__, device_xname(dev));
			return ENODEV;
		}
	}

	return (bus->ppbus_read(device_parent(dev), buf, len, how, cnt));
}

/* Reset the EPP timeout bit in the status register */
int
ppbus_reset_epp_timeout(device_t dev)
{
	struct ppbus_softc * bus = device_private(dev);

	if(bus->sc_capabilities & PPBUS_HAS_EPP) {
		bus->ppbus_reset_epp_timeout(device_parent(dev));
		return 0;
	}
	else {
		return ENODEV;
	}
}

/* Wait for the ECP FIFO to be empty */
int
ppbus_ecp_sync(device_t dev)
{
	struct ppbus_softc * bus = device_private(dev);

	if(bus->sc_capabilities & PPBUS_HAS_ECP) {
		bus->ppbus_ecp_sync(device_parent(dev));
		return 0;
	}
	else {
		return ENODEV;
	}
}

/* Allocate DMA for use with ppbus */
int
ppbus_dma_malloc(device_t dev, void ** buf, bus_addr_t * addr,
	bus_size_t size)
{
	struct ppbus_softc * ppbus = device_private(dev);

	if(ppbus->sc_capabilities & PPBUS_HAS_DMA)
		return (ppbus->ppbus_dma_malloc(device_parent(dev), buf, addr,
			size));
	else
		return ENODEV;
}

/* Free memory allocated with ppbus_dma_malloc() */
int
ppbus_dma_free(device_t dev, void ** buf, bus_addr_t * addr,
	bus_size_t size)
{
	struct ppbus_softc * ppbus = device_private(dev);

	if(ppbus->sc_capabilities & PPBUS_HAS_DMA) {
		ppbus->ppbus_dma_free(device_parent(dev), buf, addr, size);
		return 0;
	}
	else {
		return ENODEV;
	}
}

/* Install a handler to be called by hardware interrupt handler */
int ppbus_add_handler(device_t dev, void (*func)(void *), void *arg)
{
	struct ppbus_softc * bus = device_private(dev);

	if(bus->sc_capabilities & PPBUS_HAS_INTR)
		return bus->ppbus_add_handler(device_parent(dev), func, arg);
	else
		return ENODEV;
}

/* Remove a handler registered with ppbus_add_handler() */
int ppbus_remove_handler(device_t dev, void (*func)(void *))
{
	struct ppbus_softc * bus = device_private(dev);

	if(bus->sc_capabilities & PPBUS_HAS_INTR)
		return bus->ppbus_remove_handler(device_parent(dev), func);
	else
		return ENODEV;
}

/*
 * ppbus_get_status()
 *
 * Read the status register and update the status info
 */
int
ppbus_get_status(device_t dev, struct ppbus_status * status)
{
	register char r = status->status = ppbus_rstr(dev);

	status->timeout	= r & TIMEOUT;
	status->error	= !(r & nFAULT);
	status->select	= r & SELECT;
	status->paper_end = r & PERROR;
	status->ack	= !(r & nACK);
	status->busy	= !(r & nBUSY);

	return (0);
}

/* Allocate the device to perform transfers */
int
ppbus_request_bus(device_t dev, device_t busdev, int how,
	unsigned int timeout)
{
	struct ppbus_softc * bus = device_private(dev);
	unsigned int counter = timeout;
	bool intr = (how & PPBUS_INTR) != 0;
	int error;

	/* Loop until lock acquired (if PPBUS_WAIT) or an error occurs */
	for(;;) {
		if (mutex_tryenter(&(bus->sc_lock)))
			break;

		if(how & PPBUS_WAIT) {
			error = kpause("ppbusreq", intr, hz / 2, NULL);
			counter -= (hz/2);
			if(!(error))
				continue;
			else if(error != EWOULDBLOCK)
				goto end;
			if(counter == 0) {
				error = ETIMEDOUT;
				goto end;
			}
		}
		else {
			error = EWOULDBLOCK;
			goto end;
		}
	}

	/* Set bus owner or return error if bus is taken */
	if(bus->ppbus_owner == NULL) {
		bus->ppbus_owner = busdev;
		error = 0;
	}
	else {
		error = EBUSY;
	}

	/* Release lock */
	mutex_exit(&(bus->sc_lock));

end:
	return error;
}

/* Release the device allocated with ppbus_request_bus() */
int
ppbus_release_bus(device_t dev, device_t busdev, int how,
	unsigned int timeout)
{
	struct ppbus_softc * bus = device_private(dev);
	unsigned int counter = timeout;
	bool intr = (how & PPBUS_INTR) != 0;
	int error;

	/* Loop until lock acquired (if PPBUS_WAIT) or an error occurs */
	for(;;) {
		if (mutex_tryenter(&(bus->sc_lock)))
			break;

		if(how & PPBUS_WAIT) {
			error = kpause("ppbusrel", intr, hz / 2, NULL);
			counter -= (hz/2);
			if(!(error))
				continue;
			else if(error != EWOULDBLOCK)
				goto end;
			if(counter == 0) {
				error = ETIMEDOUT;
				goto end;
			}
		}
		else {
			error = EWOULDBLOCK;
			goto end;
		}
	}

	/* If the device is the owner, release bus */
	if(bus->ppbus_owner != busdev) {
		error = EINVAL;
	}
	else {
		bus->ppbus_owner = NULL;
		error = 0;
	}

	/* Release lock */
	mutex_exit(&(bus->sc_lock));

end:
	return error;
}


/* IEEE 1284-based probes */

#ifndef DONTPROBE_1284

static const char *pnp_tokens[] = {
	"PRINTER", "MODEM", "NET", "HDC", "PCMCIA", "MEDIA",
	"FDC", "PORTS", "SCANNER", "DIGICAM", "", NULL };

/* ??? */
#if 0
static char *pnp_classes[] = {
	"printer", "modem", "network device",
	"hard disk", "PCMCIA", "multimedia device",
	"floppy disk", "ports", "scanner",
	"digital camera", "unknown device", NULL };
#endif

/*
 * Search the first occurence of a token within a string
 * XXX should use strxxx() calls
 */
static char *
search_token(char *str, int slen, const char *token)
{
	const char *p;
	int tlen, i, j;

#define UNKNOWN_LENGTH  -1

	if (slen == UNKNOWN_LENGTH)
	       /* get string's length */
	       for (slen = 0, p = str; *p != '\0'; p++)
		       slen++;

       /* get token's length */
       for (tlen = 0, p = token; *p != '\0'; p++)
	       tlen++;

       if (tlen == 0)
	       return (str);

       for (i = 0; i <= slen-tlen; i++) {
	       for (j = 0; j < tlen; j++)
		       if (str[i+j] != token[j])
			       break;
	       if (j == tlen)
		       return (&str[i]);
       }

	return (NULL);
}

/* Stores the class ID of the peripheral in soft config data */
void
ppbus_pnp_detect(device_t dev)
{
	struct ppbus_softc * bus = device_private(dev);
	int i;
	int error;
	size_t len = 0;
	size_t str_sz = 0;
	char * str = NULL;
	char * class = NULL;
	char * token;

#ifdef PPBUS_VERBOSE
	printf("%s: Probing for PnP devices.\n", device_xname(dev));
#endif

	error = ppbus_1284_read_id(dev, PPBUS_NIBBLE, &str, &str_sz, &len);
	if(str_sz != len) {
#ifdef DEBUG_1284
		printf("%s(%s): device returned less characters than expected "
			"in device ID.\n", __func__, device_xname(dev));
#endif
	}
	if(error) {
		printf("%s: Error getting device ID (errno = %d)\n",
			device_xname(dev), error);
		goto end_detect;
	}

#ifdef DEBUG_1284
	printf("%s: <PnP> %zu characters: ", device_xname(dev), len);
	for (i = 0; i < len; i++)
		printf("%c(0x%x) ", str[i], str[i]);
	printf("\n");
#endif

	/* replace ';' characters by '\0' */
	for (i = 0; i < len; i++)
		if(str[i] == ';') str[i] = '\0';
		/* str[i] = (str[i] == ';') ? '\0' : str[i]; */

	if ((token = search_token(str, len, "MFG")) != NULL ||
		(token = search_token(str, len, "MANUFACTURER")) != NULL)
		printf("%s: <%s", device_xname(dev),
			search_token(token, UNKNOWN_LENGTH, ":") + 1);
	else
		printf("%s: <unknown", device_xname(dev));

	if ((token = search_token(str, len, "MDL")) != NULL ||
		(token = search_token(str, len, "MODEL")) != NULL)
		printf(" %s",
			search_token(token, UNKNOWN_LENGTH, ":") + 1);

	if ((token = search_token(str, len, "REV")) != NULL)
		printf(".%s",
			search_token(token, UNKNOWN_LENGTH, ":") + 1);

	printf(">");

	if ((token = search_token(str, len, "CLS")) != NULL) {
		class = search_token(token, UNKNOWN_LENGTH, ":") + 1;
		printf(" %s", class);
	}

	if ((token = search_token(str, len, "CMD")) != NULL ||
		(token = search_token(str, len, "COMMAND")) != NULL)
		printf(" %s",
			search_token(token, UNKNOWN_LENGTH, ":") + 1);

	printf("\n");

	if (class) {
		/* identify class ident */
		for (i = 0; pnp_tokens[i] != NULL; i++) {
			if (search_token(class, len, pnp_tokens[i]) != NULL) {
				bus->sc_class_id = i;
				goto end_detect;
			}
		}
	}
	bus->sc_class_id = PPBUS_PNP_UNKNOWN;

end_detect:
	if(str)
		free(str, M_DEVBUF);
        return;
}

/* Scan the ppbus for IEEE1284 compliant devices */
int
ppbus_scan_bus(device_t dev)
{
	struct ppbus_softc * bus = device_private(dev);
	int error;

	/* Try IEEE1284 modes, one device only (no IEEE1284.3 support) */

	error = ppbus_1284_negotiate(dev, PPBUS_NIBBLE, 0);
	if (error && bus->sc_1284_state == PPBUS_ERROR
	    && bus->sc_1284_error == PPBUS_NOT_IEEE1284)
		return (error);
	ppbus_1284_terminate(dev);

#if defined(PPBUS_VERBOSE) || defined(PPBUS_DEBUG)
	/* IEEE1284 supported, print info */
	printf("%s: IEEE1284 negotiation: modes %s",
	    device_xname(dev), "NIBBLE");

	error = ppbus_1284_negotiate(dev, PPBUS_PS2, 0);
	if (!error)
		printf("/PS2");
	ppbus_1284_terminate(dev);

	error = ppbus_1284_negotiate(dev, PPBUS_ECP, 0);
	if (!error)
		printf("/ECP");
	ppbus_1284_terminate(dev);

	error = ppbus_1284_negotiate(dev, PPBUS_ECP, PPBUS_USE_RLE);
	if (!error)
		printf("/ECP_RLE");
	ppbus_1284_terminate(dev);

	error = ppbus_1284_negotiate(dev, PPBUS_EPP, 0);
	if (!error)
		printf("/EPP");
	ppbus_1284_terminate(dev);

	printf("\n");
#endif /* PPBUS_VERBOSE || PPBUS_DEBUG */

	return 0;
}

#endif /* !DONTPROBE_1284 */

