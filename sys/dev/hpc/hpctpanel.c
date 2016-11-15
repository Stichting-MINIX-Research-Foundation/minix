/*	$NetBSD: hpctpanel.c,v 1.5 2007/03/04 06:01:47 christos Exp $	*/

/*
 * Copyright (c) 1999-2003 TAKEMURA Shin All rights reserved.
 * Copyright (c) 1999 PocketBSD Project. All rights reserved.
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
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: hpctpanel.c,v 1.5 2007/03/04 06:01:47 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <dev/wscons/wsconsio.h>
#include <dev/hpc/hpctpanelvar.h>

#include <machine/platid.h>
#include <machine/platid_mask.h>

int
hpc_tpanel_ioctl(struct tpcalib_softc *sc, u_long cmd, void *data, int flag,
    struct lwp *l)
{
	struct wsmouse_id *id;
	const char *idstr;
	int s;

	switch (cmd) {
        case WSMOUSEIO_GTYPE:
		*(u_int *)data = WSMOUSE_TYPE_TPANEL;
		return (0);

	case WSMOUSEIO_GETID:
		/*
		 * return unique ID string,
		 * "<vendor> <model> <serial number>"
		 */
		id = (struct wsmouse_id *)data;
		if (id->type != WSMOUSE_ID_TYPE_UIDSTR)
			return (EINVAL);
		idstr = platid_name(&platid);
		s = strlen(idstr);
		if (WSMOUSE_ID_MAXLEN - 10 < s)
			s = WSMOUSE_ID_MAXLEN - 10;
		memcpy(id->data, idstr, s);
		strcpy(&id->data[s], " SN000000");
		id->length = s + 9;
		break;

        case WSMOUSEIO_SCALIBCOORDS:
        case WSMOUSEIO_GCALIBCOORDS:
                return tpcalib_ioctl(sc, cmd, data, flag, l);

	default:
		return EPASSTHROUGH;
	}

	return 0;
}
