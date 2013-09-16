/*	$NetBSD: fdio.h,v 1.4 2008/04/28 20:24:10 martin Exp $	*/

/*-
 * Copyright (c) 1996, 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by John Kohl.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _SYS_FDIO_H_
#define _SYS_FDIO_H_

#include <sys/ioccom.h>

/* Floppy diskette definitions */

enum fdformat_result {
	FDFORMAT_SUCCESS,
	FDFORMAT_MEDIA_ERROR,		/* hardware reported a formatting
					   error */
	FDFORMAT_CONFIG_ERROR		/* something bogus in parameters */
};

#define FDFORMAT_VERSION 19961120

struct fdformat_cmd {
	u_int formatcmd_version;	/* FDFORMAT_VERSION */
	int head;		/* IN */
	int cylinder;		/* IN */
};

struct fdformat_parms {
/* list of items taken from i386 formatting glop (NEC 765);
   should be made the union of support needed for other devices. */
    u_int fdformat_version;	/* rev this when needed; write drivers to
				   allow forward compatibility, please,
				   and add elements to the end of the
				   structure */
    u_int nbps;				/* number of bytes per sector */
    u_int ncyl;				/* number of cylinders */
    u_int nspt;				/* sectors per track */
    u_int ntrk;				/* number of heads/tracks per cyl */
    u_int stepspercyl;			/* steps per cylinder */
    u_int gaplen;			/* formatting gap length */
    u_int fillbyte;			/* formatting fill byte */
    u_int xfer_rate;			/* in bits per second; driver
					   must convert */
    u_int interleave;			/* interleave factor */
};


#define FDOPT_NORETRY 0x0001	/* no retries on failure (cleared on close) */
#define FDOPT_SILENT  0x0002	/* no error messages (cleared on close) */

#define FDIOCGETOPTS  _IOR('d', 114, int) /* drive options, see previous */
#define FDIOCSETOPTS  _IOW('d', 115, int)

#define FDIOCSETFORMAT _IOW('d', 116, struct fdformat_parms) /* set format parms */
#define FDIOCGETFORMAT _IOR('d', 117, struct fdformat_parms) /* get format parms */
#define FDIOCFORMAT_TRACK _IOW('d', 118, struct fdformat_cmd) /* do it */

#endif /* _SYS_FDIO_H_ */
