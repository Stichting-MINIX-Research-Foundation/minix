/*	$NetBSD: if_agrmonitor.c,v 1.4 2008/03/24 09:14:52 yamt Exp $	*/

/*-
 * Copyright (c)2005 YAMAMOTO Takashi,
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
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_agrmonitor.c,v 1.4 2008/03/24 09:14:52 yamt Exp $");

#include <sys/param.h>
#include <sys/callout.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/if_media.h>

#include <net/agr/if_agrvar_impl.h>
#include <net/agr/if_agrsubr.h>

void
agrport_monitor(struct agr_port *port)
{
	struct agr_softc *sc = AGR_SC_FROM_PORT(port);
	u_int media;
	u_int status;

	/*
	 * XXX XXX
	 * assuming that it's safe to use SIOCGIFMEDIA from callout handler.
	 * maybe it's better to have a worker thread.
	 */

	media = IFM_ETHER | IFM_NONE;
	status = IFM_AVALID;
	if ((~port->port_ifp->if_flags & (IFF_RUNNING | IFF_UP))
	    == 0) {
		int error;

		error = agr_port_getmedia(port, &media, &status);
		if (error) {
#if defined(DEBUG)
			printf("getmedia: error %d\n", error);
#endif /* defined(DEBUG) */
		}
	}

	if ((status & (IFM_AVALID | IFM_ACTIVE)) == IFM_AVALID) {
		media = IFM_ETHER | IFM_NONE; /* XXX ether */
	}

	if (media == IFM_NONE) {
		/*
		 * possible eg. when the phy is not configured.
		 */
#if defined(DEBUG)
		printf("%s: IFM_NONE\n", __func__);
#endif /* defined(DEBUG) */
		media = IFM_ETHER | IFM_NONE; /* XXX ether */
	}

	if (port->port_media == media) {
		return;
	}

	port->port_media = media;
	if (sc->sc_iftop->iftop_portstate) {
		(*sc->sc_iftop->iftop_portstate)(port);
	}
}
