/*	$NetBSD: st_atapi.c,v 1.30 2015/08/24 23:13:15 pooka Exp $ */

/*
 * Copyright (c) 2001 Manuel Bouyer.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: st_atapi.c,v 1.30 2015/08/24 23:13:15 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_scsi.h"
#endif

#include <sys/param.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/bufq.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/systm.h>

#include <dev/scsipi/stvar.h>
#include <dev/scsipi/atapi_tape.h>

static int	st_atapibus_match(device_t, cfdata_t, void *);
static void	st_atapibus_attach(device_t, device_t, void *);
static int	st_atapibus_ops(struct st_softc *, int, int);
static int	st_atapibus_mode_sense(struct st_softc *, int);

CFATTACH_DECL_NEW(
	st_atapibus,
	sizeof(struct st_softc),
	st_atapibus_match,
	st_atapibus_attach,
	stdetach,
	NULL
);

static const struct scsipi_inquiry_pattern st_atapibus_patterns[] = {
	{T_SEQUENTIAL, T_REMOV,
	 "",	 "",		 ""},
};

static int
st_atapibus_match(device_t parent, cfdata_t match,  void *aux)
{
	struct scsipibus_attach_args *sa = aux;
	int priority;

	if (SCSIPI_BUSTYPE_TYPE(scsipi_periph_bustype(sa->sa_periph)) !=
	    SCSIPI_BUSTYPE_ATAPI)
		return 0;

	(void)scsipi_inqmatch(&sa->sa_inqbuf,
	    st_atapibus_patterns,
	    sizeof(st_atapibus_patterns)/sizeof(st_atapibus_patterns[0]),
	    sizeof(st_atapibus_patterns[0]), &priority);
	return priority;
}

static void
st_atapibus_attach(device_t parent, device_t self, void *aux)
{
	struct st_softc *st = device_private(self);
	struct scsipibus_attach_args *sa = aux;
	struct scsipi_periph *periph = sa->sa_periph;

	if (strcmp(sa->sa_inqbuf.vendor, "OnStream DI-30") == 0) {
		struct ast_identifypage identify;
		int error;

		error = scsipi_mode_sense(periph, SMS_DBD,
		    ATAPI_TAPE_IDENTIFY_PAGE, &identify.header,
		    sizeof(identify), XS_CTL_DISCOVERY,
		    ST_RETRIES, ST_CTL_TIME);
		if (error) {
			printf("onstream get identify: error %d\n", error);
			return;
		}
		strncpy(identify.ident, "NBSD", 4);
		error = scsipi_mode_select(periph, SMS_PF,
		    &identify.header, sizeof(identify),
		    XS_CTL_DISCOVERY, ST_RETRIES, ST_CTL_TIME);
		if (error) {
			printf("onstream set identify: error %d\n", error);
			return;
		}
	}

	st->ops = st_atapibus_ops;
	stattach(parent, self, aux);
}

static int
st_atapibus_ops(struct st_softc *st, int op, int flags)
{
	switch(op) {
	case ST_OPS_RBL:
		/* done in mode_sense */
		return 0;
	case ST_OPS_MODESENSE:
		return st_atapibus_mode_sense(st, flags);
	case ST_OPS_MODESELECT:
		return st_mode_select(st, flags);
	case ST_OPS_CMPRSS_ON:
	case ST_OPS_CMPRSS_OFF:
		return ENODEV;
	default:
		panic("st_scsibus_ops: invalid op");
		/* NOTREACHED */
	}
}

static int
st_atapibus_mode_sense(struct st_softc *st, int flags)
{
	int count, error;
	struct atapi_cappage cappage;
	struct scsipi_periph *periph = st->sc_periph;

	/* get drive capabilities, some drives needs this repeated */
	for (count = 0 ; count < 5 ; count++) {
		error = scsipi_mode_sense(periph, SMS_DBD,
		    ATAPI_TAPE_CAP_PAGE, &cappage.header, sizeof(cappage),
		    flags, ST_RETRIES, ST_CTL_TIME);
		if (error == 0) {
			st->numblks = 0; /* unused anyway */
			if (cappage.cap4 & ATAPI_TAPE_CAP_PAGE_BLK32K)
				st->media_blksize = 32768;
			else if (cappage.cap4 & ATAPI_TAPE_CAP_PAGE_BLK1K)
				st->media_blksize = 1024;
			else if (cappage.cap4 & ATAPI_TAPE_CAP_PAGE_BLK512)
				st->media_blksize = 512;
			else {
				error = ENODEV;
				continue;
			}
			st->blkmin = st->blkmax = st->media_blksize;
			st->media_density = 0;
			if (cappage.header.dev_spec & SMH_DSP_WRITE_PROT)
				st->flags |= ST_READONLY;
			else
				st->flags &= ~ST_READONLY;
			SC_DEBUG(periph, SCSIPI_DB3,
			    ("density code %d, %d-byte blocks, write-%s, ",
			    st->media_density, st->media_blksize,
			    st->flags & ST_READONLY ? "protected" : "enabled"));
			SC_DEBUG(periph, SCSIPI_DB3,
			    ("%sbuffered\n",
			    cappage.header.dev_spec & SMH_DSP_BUFF_MODE ?
			    "" : "un"));
			periph->periph_flags |= PERIPH_MEDIA_LOADED;
			return 0;
		}
	}
	return error;
}
