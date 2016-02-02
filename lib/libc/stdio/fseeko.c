/*	$NetBSD: fseeko.c,v 1.13 2014/10/19 11:17:43 justin Exp $	*/

/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
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
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: fseeko.c,v 1.13 2014/10/19 11:17:43 justin Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/types.h>
#include <sys/stat.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include "reentrant.h"
#include "local.h"

#ifdef __weak_alias
__weak_alias(fseeko, _fseeko)
#endif

#define	POS_ERR	((off_t)-1)

/*
 * Seek the given file to the given offset.
 * `Whence' must be one of the three SEEK_* macros.
 */
int
fseeko(FILE *fp, off_t offset, int whence)
{
	off_t (*seekfn)(void *, off_t, int);
	off_t target, curoff;
	size_t n;
	struct stat st;
	int havepos;

	_DIAGASSERT(fp != NULL);

	/* make sure stdio is set up */
	if (!__sdidinit)
		__sinit();

	FLOCKFILE(fp);

	/*
	 * Have to be able to seek.
	 */
	if ((seekfn = fp->_seek) == NULL) {
		errno = ESPIPE;			/* historic practice */
		FUNLOCKFILE(fp);
		return -1;
	}

	/*
	 * Change any SEEK_CUR to SEEK_SET, and check `whence' argument.
	 * After this, whence is either SEEK_SET or SEEK_END.
	 */
	switch (whence) {

	case SEEK_CUR:
		/*
		 * In order to seek relative to the current stream offset,
		 * we have to first find the current stream offset a la
		 * ftell (see ftell for details).
		 */
		(void)__sflush(fp); /* may adjust seek offset on append stream */
		if (fp->_flags & __SOFF)
			curoff = fp->_offset;
		else {
			curoff = (*seekfn)(fp->_cookie, (off_t)0, SEEK_CUR);
			if (curoff == POS_ERR) {
				FUNLOCKFILE(fp);
				return -1;
			}
		}
		if (fp->_flags & __SRD) {
			curoff -= fp->_r;
			if (HASUB(fp))
				curoff -= fp->_ur;
		} else if (fp->_flags & __SWR && fp->_p != NULL)
			curoff += fp->_p - fp->_bf._base;

		offset += curoff;
		if (offset < 0) {
			errno = EINVAL;
			FUNLOCKFILE(fp);
			return -1;
		}	
		whence = SEEK_SET;
		havepos = 1;
		break;

	case SEEK_SET:
		if (offset < 0) {
			errno = EINVAL;
			FUNLOCKFILE(fp);
			return -1;
		}
	case SEEK_END:
		curoff = 0;		/* XXX just to keep gcc quiet */
		havepos = 0;
		break;

	default:
		errno = EINVAL;
		FUNLOCKFILE(fp);
		return -1;
	}

	/*
	 * Can only optimise if:
	 *	reading (and not reading-and-writing);
	 *	not unbuffered; and
	 *	this is a `regular' Unix file (and hence seekfn==__sseek).
	 * We must check __NBF first, because it is possible to have __NBF
	 * and __SOPT both set.
	 */
	if (fp->_bf._base == NULL)
		__smakebuf(fp);
	if (fp->_flags & (__SWR | __SRW | __SNBF | __SNPT))
		goto dumb;
	if ((fp->_flags & __SOPT) == 0) {
		if (seekfn != __sseek ||
		    __sfileno(fp) == -1 || fstat(__sfileno(fp), &st) ||
		    !S_ISREG(st.st_mode)) {
			fp->_flags |= __SNPT;
			goto dumb;
		}
		fp->_blksize = st.st_blksize;
		fp->_flags |= __SOPT;
	}

	/*
	 * We are reading; we can try to optimise.
	 * Figure out where we are going and where we are now.
	 */
	if (whence == SEEK_SET)
		target = offset;
	else {
		if (fstat(__sfileno(fp), &st))
			goto dumb;
		target = st.st_size + offset;
	}

	if (!havepos) {
		if (fp->_flags & __SOFF)
			curoff = fp->_offset;
		else {
			curoff = (*seekfn)(fp->_cookie, (off_t)0, SEEK_CUR);
			if (curoff == POS_ERR)
				goto dumb;
		}
		curoff -= fp->_r;
		if (HASUB(fp))
			curoff -= fp->_ur;
	}

	/*
	 * Compute the number of bytes in the input buffer (pretending
	 * that any ungetc() input has been discarded).  Adjust current
	 * offset backwards by this count so that it represents the
	 * file offset for the first byte in the current input buffer.
	 */
	if (HASUB(fp)) {
		curoff += fp->_r;	/* kill off ungetc */
		n = fp->_up - fp->_bf._base;
		curoff -= n;
		n += fp->_ur;
	} else {
		n = fp->_p - fp->_bf._base;
		curoff -= n;
		n += fp->_r;
	}

	/*
	 * If the target offset is within the current buffer,
	 * simply adjust the pointers, clear EOF, undo ungetc(),
	 * and return.  (If the buffer was modified, we have to
	 * skip this; see fgetln.c.)
	 */
	if ((fp->_flags & __SMOD) == 0 &&
	    target >= curoff && target < curoff + (off_t)n) {
		int o = (int)(target - curoff);

		fp->_p = fp->_bf._base + o;
		_DIAGASSERT(__type_fit(int, n - o));
		fp->_r = (int)(n - o);
		if (HASUB(fp))
			FREEUB(fp);
		fp->_flags &= ~__SEOF;
		FUNLOCKFILE(fp);
		return 0;
	}

	/*
	 * The place we want to get to is not within the current buffer,
	 * but we can still be kind to the kernel copyout mechanism.
	 * By aligning the file offset to a block boundary, we can let
	 * the kernel use the VM hardware to map pages instead of
	 * copying bytes laboriously.  Using a block boundary also
	 * ensures that we only read one block, rather than two.
	 */
	curoff = target & ~(fp->_blksize - 1);
	if ((*seekfn)(fp->_cookie, curoff, SEEK_SET) == POS_ERR)
		goto dumb;
	fp->_r = 0;
 	fp->_p = fp->_bf._base;
	if (HASUB(fp))
		FREEUB(fp);
	fp->_flags &= ~__SEOF;
	n = (int)(target - curoff);
	if (n) {
		if (__srefill(fp) || (size_t)fp->_r < n)
			goto dumb;
		fp->_p += n;
		_DIAGASSERT(__type_fit(int, fp->_r - n));
		fp->_r -= (int)n;
	}
	FUNLOCKFILE(fp);
	return 0;

	/*
	 * We get here if we cannot optimise the seek ... just
	 * do it.  Allow the seek function to change fp->_bf._base.
	 */
dumb:
	if (__sflush(fp) ||
	    (*seekfn)(fp->_cookie, offset, whence) == POS_ERR) {
		FUNLOCKFILE(fp);
		return -1;
	}
	/* success: clear EOF indicator and discard ungetc() data */
	if (HASUB(fp))
		FREEUB(fp);
	fp->_p = fp->_bf._base;
	fp->_r = 0;
	/* fp->_w = 0; */	/* unnecessary (I think...) */
	fp->_flags &= ~__SEOF;
	FUNLOCKFILE(fp);
	return 0;
}
