/*	$NetBSD: ssvar.h,v 1.19 2012/02/28 11:41:00 mbalmer Exp $	*/

/*
 * Copyright (c) 1995 Kenneth Stailey.  All rights reserved.
 *   modified for configurable scanner support by Joachim Koenig
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
 *	This product includes software developed by Kenneth Stailey.
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
 * SCSI scanner interface description
 */

/*
 * Special handlers for impractically different scanner types.
 * Register NULL for a function if you want to try the real SCSI code
 * (with quirks table)
 */
struct ss_softc;
struct scan_io;

struct ss_special {
	int	(*set_params)(struct ss_softc *, struct scan_io *);
	int	(*trigger_scanner)(struct ss_softc *);
	int	(*get_params)(struct ss_softc *);
	/* some scanners only send line-multiples */
	void	(*minphys)(struct ss_softc *, struct buf *);
	int	(*read)(struct ss_softc *, struct buf *);
	int	(*rewind_scanner)(struct ss_softc *);
	int	(*load_adf)(struct ss_softc *);
	int	(*unload_adf)(struct ss_softc *);
};

/*
 * ss_softc has to be declared here, because the device dependent
 * modules include it
 */
struct ss_softc {
	device_t sc_dev;

	int flags;
#define SSF_TRIGGERED	0x01	/* read operation has been primed */
#define	SSF_LOADED	0x02	/* parameters loaded */
#define	SSF_AUTOCONF	0x04	/* set during auto-configuration */
	struct scsipi_periph *sc_periph; /* contains our targ, lun, etc. */
	struct scan_io sio;
	struct bufq_state *buf_queue;	/* the queue of pending IO operations */
	struct callout sc_callout;	/* to restart the buf queue */
	u_int quirks;			/* scanner is only mildly twisted */
#define SS_Q_GET_BUFFER_SIZE	0x0001	/* poll for available data in ssread()*/
/* truncate to byte boundary is assumed by default unless one of these is set */
#define SS_Q_PAD_TO_BYTE	0x0002	/* pad monochrome data to byte boundary */
#define SS_Q_PAD_TO_WORD	0x0004	/* pad monochrome data to word boundary */
#define SS_Q_THRESHOLD_FOLLOWS_BRIGHTNESS 0x0008
	struct ss_special *special;	/* special handlers for spec. devices */
};

void	ssrestart(void *);

/*
 * define the special attach routines if configured
 */
void mustek_attach(struct ss_softc *, struct scsipibus_attach_args *);
void scanjet_attach(struct ss_softc *, struct scsipibus_attach_args *);
