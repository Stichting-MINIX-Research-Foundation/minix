/* $NetBSD: ppbus_device.h,v 1.6 2008/04/15 15:02:29 cegger Exp $ */

#ifndef __PPBUS_DEVICE_H
#define __PPBUS_DEVICE_H

#include <sys/device.h>

#include <dev/ppbus/ppbus_msq.h>


/* Parallel Port Bus Device context. */
struct ppbus_context {
        int valid;                      /* 1 if the struct is valid */
	int mode;                       /* XXX chipset operating mode */
	struct microseq *curpc;         /* pc in curmsq */
	struct microseq *curmsq;        /* currently executed microseqence */
};

/* Parallel Port Bus Device structure. */
struct ppbus_device_softc {
        device_t sc_dev;

	u_int16_t mode;			/* current mode of the device */
	u_int16_t capabilities;		/* ppbus capabilities */

	/* uint flags;                     flags */
	struct ppbus_context ctx;       /* context of the device */

					/* mode dependent get msq. If NULL,
				 	 * IEEE1284 code is used */
	struct ppbus_xfer
		get_xfer[PPBUS_MAX_XFER];

					/* mode dependent put msq. If NULL,
					 * IEEE1284 code is used */
	struct ppbus_xfer
		put_xfer[PPBUS_MAX_XFER];

	/* Each structure is a node in a list of child devices */
	SLIST_ENTRY(ppbus_device_softc) entries;
};

struct ppbus_attach_args {
	/* Available IEEE1284 modes */
	u_int16_t capabilities;

	/* Flags?
	u_int16_t flags;*/
};

#endif /* __PPBUS_DEVICE_H */
