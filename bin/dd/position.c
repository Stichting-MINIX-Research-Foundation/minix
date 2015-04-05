/*	$NetBSD: position.c,v 1.18 2010/11/22 21:04:28 pooka Exp $	*/

/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Keith Muller of the University of California, San Diego and Lance
 * Visser of Convex Computer Corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 */

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)position.c	8.3 (Berkeley) 4/2/94";
#else
__RCSID("$NetBSD: position.c,v 1.18 2010/11/22 21:04:28 pooka Exp $");
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mtio.h>
#include <sys/time.h>

#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dd.h"
#include "extern.h"

/*
 * Position input/output data streams before starting the copy.  Device type
 * dependent.  Seekable devices use lseek, and the rest position by reading.
 * Seeking past the end of file can cause null blocks to be written to the
 * output.
 */
void
pos_in(void)
{
	int bcnt, cnt, nr, warned;

	/* If not a pipe or tape device, try to seek on it. */
	if (!(in.flags & (ISPIPE|ISTAPE))) {
		if (ddop_lseek(in, in.fd,
		    (off_t)in.offset * (off_t)in.dbsz, SEEK_CUR) == -1) {
			err(EXIT_FAILURE, "%s", in.name);
			/* NOTREACHED */
		}
		return;
		/* NOTREACHED */
	}

	/*
	 * Read the data.  If a pipe, read until satisfy the number of bytes
	 * being skipped.  No differentiation for reading complete and partial
	 * blocks for other devices.
	 */
	for (bcnt = in.dbsz, cnt = in.offset, warned = 0; cnt;) {
		if ((nr = ddop_read(in, in.fd, in.db, bcnt)) > 0) {
			if (in.flags & ISPIPE) {
				if (!(bcnt -= nr)) {
					bcnt = in.dbsz;
					--cnt;
				}
			} else
				--cnt;
			continue;
		}

		if (nr == 0) {
			if (files_cnt > 1) {
				--files_cnt;
				continue;
			}
			errx(EXIT_FAILURE, "skip reached end of input");
			/* NOTREACHED */
		}

		/*
		 * Input error -- either EOF with no more files, or I/O error.
		 * If noerror not set die.  POSIX requires that the warning
		 * message be followed by an I/O display.
		 */
		if (ddflags & C_NOERROR) {
			if (!warned) {

				warn("%s", in.name);
				warned = 1;
				summary();
			}
			continue;
		}
		err(EXIT_FAILURE, "%s", in.name);
		/* NOTREACHED */
	}
}

void
pos_out(void)
{
	struct mtop t_op;
	int n;
	uint64_t cnt;

	/*
	 * If not a tape, try seeking on the file.  Seeking on a pipe is
	 * going to fail, but don't protect the user -- they shouldn't
	 * have specified the seek operand.
	 */
	if (!(out.flags & ISTAPE)) {
		if (ddop_lseek(out, out.fd,
		    (off_t)out.offset * (off_t)out.dbsz, SEEK_SET) == -1)
			err(EXIT_FAILURE, "%s", out.name);
			/* NOTREACHED */
		return;
	}

	/* If no read access, try using mtio. */
	if (out.flags & NOREAD) {
		t_op.mt_op = MTFSR;
		t_op.mt_count = out.offset;

		if (ddop_ioctl(out, out.fd, MTIOCTOP, &t_op) < 0)
			err(EXIT_FAILURE, "%s", out.name);
			/* NOTREACHED */
		return;
	}

	/* Read it. */
	for (cnt = 0; cnt < out.offset; ++cnt) {
		if ((n = ddop_read(out, out.fd, out.db, out.dbsz)) > 0)
			continue;

		if (n < 0)
			err(EXIT_FAILURE, "%s", out.name);
			/* NOTREACHED */

		/*
		 * If reach EOF, fill with NUL characters; first, back up over
		 * the EOF mark.  Note, cnt has not yet been incremented, so
		 * the EOF read does not count as a seek'd block.
		 */
		t_op.mt_op = MTBSR;
		t_op.mt_count = 1;
		if (ddop_ioctl(out, out.fd, MTIOCTOP, &t_op) == -1)
			err(EXIT_FAILURE, "%s", out.name);
			/* NOTREACHED */

		while (cnt++ < out.offset)
			if ((uint64_t)(n = bwrite(&out,
			    out.db, out.dbsz)) != out.dbsz)
				err(EXIT_FAILURE, "%s", out.name);
				/* NOTREACHED */
		break;
	}
}
