/*	$NetBSD: ss_scanjet.c,v 1.53 2013/10/25 16:05:47 martin Exp $	*/

/*
 * Copyright (c) 1995 Kenneth Stailey.  All rights reserved.
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
 *      This product includes software developed by Kenneth Stailey.
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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * special functions for the HP ScanJet IIc and IIcx
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ss_scanjet.c,v 1.53 2013/10/25 16:05:47 martin Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/fcntl.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/bufq.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/conf.h>		/* for cdevsw */
#include <sys/scanio.h>
#include <sys/kernel.h>

#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsi_scanner.h>
#include <dev/scsipi/scsipiconf.h>
#include <dev/scsipi/scsipi_base.h>
#include <dev/scsipi/ssvar.h>

#define SCANJET_RETRIES 4

static int	scanjet_get_params(struct ss_softc *);
static int	scanjet_set_params(struct ss_softc *, struct scan_io *);
static int	scanjet_trigger_scanner(struct ss_softc *);
static int	scanjet_read(struct ss_softc *, struct buf *);

/* only used internally */
static int	scanjet_ctl_write(struct ss_softc *, char *, u_int);
static int	scanjet_ctl_read(struct ss_softc *, char *, u_int);
static int	scanjet_set_window(struct ss_softc *);
static int	scanjet_compute_sizes(struct ss_softc *);

/* structure for the special handlers */
static struct ss_special scanjet_special = {
	scanjet_set_params,
	scanjet_trigger_scanner,
	scanjet_get_params,
	NULL,			/* no special minphys */
	scanjet_read,		/* scsi 6-byte read */
	NULL,			/* no "rewind" code (yet?) */
	NULL,			/* no adf support right now */
	NULL			/* no adf support right now */
};

/* scanjet_attach: attach special functions to ss */
void
scanjet_attach(struct ss_softc *ss, struct scsipibus_attach_args *sa)
{
	int error;

	SC_DEBUG(ss->sc_periph, SCSIPI_DB1, ("scanjet_attach: start\n"));
	ss->sio.scan_scanner_type = 0;

	printf("%s: ", device_xname(ss->sc_dev));

	/* first, check the model (which determines nothing yet) */
	if (!memcmp(sa->sa_inqbuf.product, "C1750A", 6)) {
		ss->sio.scan_scanner_type = HP_SCANJET_IIC;
		printf("HP ScanJet IIc");
	} else if (!memcmp(sa->sa_inqbuf.product, "C2500A", 6)) {
		ss->sio.scan_scanner_type = HP_SCANJET_IIC;
		printf("HP ScanJet IIcx");
	} else if (!memcmp(sa->sa_inqbuf.product, "C2520A", 6)) {
		ss->sio.scan_scanner_type = HP_SCANJET_IIC;
		printf("HP ScanJet 4c");
	} else if (!memcmp(sa->sa_inqbuf.product, "C1130A", 6)) {
		ss->sio.scan_scanner_type = HP_SCANJET_IIC;
		printf("HP ScanJet 4p");
	} else if (!memcmp(sa->sa_inqbuf.product, "C5110A", 6)) {
		ss->sio.scan_scanner_type = HP_SCANJET_IIC;
		printf("HP ScanJet 5p");
	} else {
		ss->sio.scan_scanner_type = HP_SCANJET_IIC;
		printf("HP ScanJet (unknown model)");
	}

	SC_DEBUG(ss->sc_periph, SCSIPI_DB1,
	    ("scanjet_attach: scanner_type = %d\n",
	    ss->sio.scan_scanner_type));

	/* now install special handlers */
	ss->special = &scanjet_special;

	/* populate the scanio struct with legal values */
	ss->sio.scan_width		= 1200;
	ss->sio.scan_height		= 1200;
	ss->sio.scan_x_resolution	= 100;
	ss->sio.scan_y_resolution	= 100;
	ss->sio.scan_x_origin		= 0;
	ss->sio.scan_y_origin		= 0;
	ss->sio.scan_brightness		= 128;
	ss->sio.scan_contrast		= 128;
	ss->sio.scan_quality		= 100;
	ss->sio.scan_image_mode		= SIM_GRAYSCALE;

	error = scanjet_set_window(ss);
	if (error) {
		printf(" set_window failed\n");
		return;
	}
	error = scanjet_compute_sizes(ss);
	if (error) {
		printf(" compute_sizes failed\n");
		return;
	}
	printf("\n");
}

static int
scanjet_get_params(struct ss_softc *ss)
{
	return 0;
}

/*
 * check the parameters if the scanjet is capable of fulfilling it
 * but don't send the command to the scanner in case the user wants
 * to change parameters by more than one call
 */
static int
scanjet_set_params(struct ss_softc *ss, struct scan_io *sio)
{
	int error;

#if 0
	/*
	 * if the scanner is triggered, then rewind it
	 */
	if (ss->flags & SSF_TRIGGERED) {
		error = scanjet_rewind_scanner(ss);
		if (error)
			return error;
	}
#endif

	/* size constraints... */
	if (sio->scan_width == 0				 ||
	    sio->scan_x_origin + sio->scan_width > 10200 || /* 8.5" */
	    sio->scan_height == 0				 ||
	    sio->scan_y_origin + sio->scan_height > 16800)  /* 14" */
		return EINVAL;

	/* resolution (dpi)... */
	if (sio->scan_x_resolution < 100 ||
	    sio->scan_x_resolution > 400 ||
	    sio->scan_y_resolution < 100 ||
	    sio->scan_y_resolution > 400)
		return EINVAL;

	switch (sio->scan_image_mode) {
	case SIM_BINARY_MONOCHROME:
	case SIM_DITHERED_MONOCHROME:
	case SIM_GRAYSCALE:
	case SIM_COLOR:
		break;
	default:
		return EINVAL;
	}

	/* change ss_softc to the new values, but save ro-variables */
	sio->scan_scanner_type = ss->sio.scan_scanner_type;
	memcpy(&ss->sio, sio, sizeof(struct scan_io));

	error = scanjet_set_window(ss);
	if (error) {
		uprintf("%s: set_window failed\n", device_xname(ss->sc_dev));
		return error;
	}
	error = scanjet_compute_sizes(ss);
	if (error) {
		uprintf("%s: compute_sizes failed\n", device_xname(ss->sc_dev));
		return error;
	}
	return 0;
}

/*
 * trigger the scanner to start a scan operation
 * this includes sending the mode- and window-data,
 * and starting the scanner
 */
static int
scanjet_trigger_scanner(struct ss_softc *ss)
{
	char escape_codes[20];
	int error;

	error = scanjet_set_window(ss);
	if (error) {
		uprintf("%s: set_window failed\n", device_xname(ss->sc_dev));
		return error;
	}
	error = scanjet_compute_sizes(ss);
	if (error) {
		uprintf("%s: compute_sizes failed\n", device_xname(ss->sc_dev));
		return error;
	}

	/* send "trigger" operation */
	strlcpy(escape_codes, "\033*f0S", sizeof(escape_codes));
	error = scanjet_ctl_write(ss, escape_codes, strlen(escape_codes));
	if (error) {
		uprintf("%s: trigger_scanner failed\n",
		    device_xname(ss->sc_dev));
		return error;
	}
	return 0;
}

static int
scanjet_read(struct ss_softc *ss, struct buf *bp)
{
	struct scsi_rw_scanner cmd;
	struct scsipi_xfer *xs;
	struct scsipi_periph *periph = ss->sc_periph;
	int error __diagused;

	/* Fill out the scsi command */
	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = READ;

	/*
	 * Handle "fixed-block-mode" tape drives by using the
	 * block count instead of the length.
	 */
	_lto3b(bp->b_bcount, cmd.len);

	/* go ask the adapter to do all this for us */
	xs = scsipi_make_xs(periph,
	    (struct scsipi_generic *)&cmd, sizeof(cmd),
	    (u_char *)bp->b_data, bp->b_bcount,
	    SCANJET_RETRIES, 100000, bp,
	    XS_CTL_NOSLEEP | XS_CTL_ASYNC | XS_CTL_DATA_IN);
	if (xs == NULL) {
		/*
		 * out of memory. Keep this buffer in the queue, and
		 * retry later.
		 */
		callout_reset(&ss->sc_callout, hz / 2, ssrestart, periph);
		return 0;
	}
#ifdef DIAGNOSTIC
	if (bufq_get(ss->buf_queue) != bp)
		panic("ssstart(): dequeued wrong buf");
#else
	bufq_get(ss->buf_queue);
#endif
	error = scsipi_execute_xs(xs);
	/* with a scsipi_xfer preallocated, scsipi_command can't fail */
	KASSERT(error == 0);
	ss->sio.scan_window_size -= bp->b_bcount;
#if 0
	if (ss->sio.scan_window_size < 0)
		ss->sio.scan_window_size = 0;
#endif
	return 0;
}


/* Do a synchronous write.  Used to send control messages. */
static int
scanjet_ctl_write(struct ss_softc *ss, char *tbuf, u_int size)
{
	struct scsi_rw_scanner cmd;
	int flags;

	flags = 0;
	if ((ss->flags & SSF_AUTOCONF) != 0)
		flags |= XS_CTL_DISCOVERY;

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = WRITE;
	_lto3b(size, cmd.len);

	return scsipi_command(ss->sc_periph,
	    (void *)&cmd, sizeof(cmd), (void *)tbuf, size, 0, 100000, NULL,
	    flags | XS_CTL_DATA_OUT);
}


/* Do a synchronous read.  Used to read responses to control messages. */
static int
scanjet_ctl_read(struct ss_softc *ss, char *tbuf, u_int size)
{
	struct scsi_rw_scanner cmd;
	int flags;

	flags = 0;
	if ((ss->flags & SSF_AUTOCONF) != 0)
		flags |= XS_CTL_DISCOVERY;

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = READ;
	_lto3b(size, cmd.len);

	return scsipi_command(ss->sc_periph,
	    (void *)&cmd, sizeof(cmd), (void *)tbuf, size, 0, 100000, NULL,
	    flags | XS_CTL_DATA_IN);
}


#ifdef SCANJETDEBUG
static void
show_es(char *es)
{
	char *p = es;

	while (*p) {
		if (*p == '\033')
			printf("[Esc]");
		else
			printf("%c", *p);
		++p;
	}
	printf("\n");
}
#endif

/*
 * simulate SCSI_SET_WINDOW for ScanJets
 */
static int
scanjet_set_window(struct ss_softc *ss)
{
	char escape_codes[128], *p, *ep;

	p = escape_codes;
	ep = &escape_codes[128];

	p += snprintf(p, ep - p, "\033*f%ldP", ss->sio.scan_width / 4);
	p += snprintf(p, ep - p, "\033*f%ldQ", ss->sio.scan_height / 4);
	p += snprintf(p, ep - p, "\033*f%ldX", ss->sio.scan_x_origin / 4);
	p += snprintf(p, ep - p, "\033*f%ldY", ss->sio.scan_y_origin / 4);
	p += snprintf(p, ep - p, "\033*a%dR", ss->sio.scan_x_resolution);
	p += snprintf(p, ep - p, "\033*a%dS", ss->sio.scan_y_resolution);

	switch (ss->sio.scan_image_mode) {
	case SIM_BINARY_MONOCHROME:
		ss->sio.scan_bits_per_pixel = 1;
		/* use "line art" mode */
		strlcpy(p, "\033*a0T", ep - p);
		p += strlen(p);
		/* make image data be "min-is-white ala PBM */
		strlcpy(p, "\033*a0I", ep - p);
		p += strlen(p);
		break;
	case SIM_DITHERED_MONOCHROME:
		ss->sio.scan_bits_per_pixel = 1;
		/* use dithered mode */
		strlcpy(p, "\033*a3T", ep - p);
		p += strlen(p);
		/* make image data be "min-is-white ala PBM */
		strlcpy(p, "\033*a0I", ep - p);
		p += strlen(p);
		break;
	case SIM_GRAYSCALE:
		ss->sio.scan_bits_per_pixel = 8;
		/* use grayscale mode */
		strlcpy(p, "\033*a4T", ep - p);
		p += strlen(p);
		/* make image data be "min-is-black ala PGM */
		strlcpy(p, "\033*a1I", ep - p);
		p += strlen(p);
		break;
	case SIM_COLOR:
		ss->sio.scan_bits_per_pixel = 24;
		/* use RGB color mode */
		strlcpy(p, "\033*a5T", ep - p);
		p += strlen(p);
		/* make image data be "min-is-black ala PPM */
		strlcpy(p, "\033*a1I", ep - p);
		p += strlen(p);
		/* use pass-through matrix (disable NTSC) */
		strlcpy(p, "\033*u2T", ep - p);
		p += strlen(p);
		break;
	}

	p += snprintf(p, ep - p, "\033*a%dG", ss->sio.scan_bits_per_pixel);
	p += snprintf(p, ep - p, "\033*a%dL",
	    (int)(ss->sio.scan_brightness) - 128);
	p += snprintf(p, ep - p, "\033*a%dK",
	    (int)(ss->sio.scan_contrast) - 128);

	return scanjet_ctl_write(ss, escape_codes, p - escape_codes);
}

static int
scanjet_compute_sizes(struct ss_softc *ss)
{
	int error;
	static const char *wfail = "%s: interrogate write failed\n";
	static const char *rfail = "%s: interrogate read failed\n";
	static const char *dfail = "%s: bad data returned\n";
	char escape_codes[20];
	char response[20];
	char *p;

	/*
	 * Deal with the fact that the HP ScanJet IIc uses 1/300" not 1/1200"
	 * as its base unit of measurement.  PINT uses 1/1200" (yes I know
	 * ScanJet II's use decipoints as well but 1200 % 720 != 0)
	 */
	ss->sio.scan_width = (ss->sio.scan_width + 3) & 0xfffffffc;
	ss->sio.scan_height = (ss->sio.scan_height + 3) & 0xfffffffc;

	switch (ss->sio.scan_image_mode) {
	case SIM_BINARY_MONOCHROME:
	case SIM_DITHERED_MONOCHROME:
		/* bytes wide */
		strlcpy(escape_codes, "\033*s1025E", sizeof(escape_codes));
		break;
	case SIM_GRAYSCALE:
	case SIM_COLOR:
		/* pixels wide */
		strlcpy(escape_codes, "\033*s1024E", sizeof(escape_codes));
		break;
	}
	error = scanjet_ctl_write(ss, escape_codes, strlen(escape_codes));
	if (error) {
		uprintf(wfail, device_xname(ss->sc_dev));
		return error;
	}
	error = scanjet_ctl_read(ss, response, 20);
	if (error) {
		uprintf(rfail, device_xname(ss->sc_dev));
		return error;
	}
	p = strchr(response, 'd');
	if (p == 0) {
		uprintf(dfail, device_xname(ss->sc_dev));
		return EIO;
	}
	ss->sio.scan_pixels_per_line = strtoul(p + 1, NULL, 10);
	if (ss->sio.scan_image_mode < SIM_GRAYSCALE)
		ss->sio.scan_pixels_per_line *= 8;

	/* pixels high */
	strlcpy(escape_codes, "\033*s1026E", sizeof(escape_codes));
	error = scanjet_ctl_write(ss, escape_codes, strlen(escape_codes));
	if (error) {
		uprintf(wfail, device_xname(ss->sc_dev));
		return error;
	}
	error = scanjet_ctl_read(ss, response, 20);
	if (error) {
		uprintf(rfail, device_xname(ss->sc_dev));
		return error;
	}
	p = strchr(response, 'd');
	if (p == 0) {
		uprintf(dfail, device_xname(ss->sc_dev));
		return EIO;
	}
	ss->sio.scan_lines = strtoul(p + 1, NULL, 10);

	ss->sio.scan_window_size = ss->sio.scan_lines *
	    ((ss->sio.scan_pixels_per_line * ss->sio.scan_bits_per_pixel) / 8);

	return 0;
}
