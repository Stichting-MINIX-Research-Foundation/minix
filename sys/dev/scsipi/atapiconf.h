/*	$NetBSD: atapiconf.h,v 1.23 2009/10/19 18:41:16 bouyer Exp $	*/

/*
 * Copyright (c) 1996, 2001 Manuel Bouyer.  All rights reserved.
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

#ifndef _DEV_SCSIPI_ATAPICONF_H
#define _DEV_SCSIPI_ATAPICONF_H

#include <sys/device.h>
#include <dev/scsipi/scsipiconf.h>

struct atapibus_softc {
	device_t sc_dev;
	struct scsipi_channel *sc_channel;	/* our scsipi_channel */
};

extern const struct scsipi_periphsw atapi_probe_periphsw;


/*
 * We need some more data than in scsipi_adapter.
 * So define a new atapi_adapter, we'll cast channel->chan_adapter to
 * atapi_adapter when we need the extra data (only in ATAPI code)
 */
struct atapi_adapter {
	struct scsipi_adapter _generic;
	void (*atapi_probe_device)(struct atapibus_softc *, int);
};

void 	*atapi_probe_device(struct atapibus_softc *, int,
	    struct scsipi_periph *, struct scsipibus_attach_args *);
int	atapiprint(void *, const char *);
void	atapi_print_addr(struct scsipi_periph *);
int	atapi_interpret_sense(struct scsipi_xfer *);
void	atapi_scsipi_cmd(struct scsipi_xfer *);

#endif /* _DEV_SCSIPI_ATAPICONF_H */
