/*	$NetBSD: criov.c,v 1.8 2011/02/24 19:28:03 drochner Exp $ */
/*      $OpenBSD: criov.c,v 1.11 2002/06/10 19:36:43 espie Exp $	*/

/*
 * Copyright (c) 1999 Theo de Raadt
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
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
__KERNEL_RCSID(0, "$NetBSD: criov.c,v 1.8 2011/02/24 19:28:03 drochner Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>

#include <uvm/uvm_extern.h>

#include <opencrypto/cryptodev.h>
int cuio_getindx(struct uio *uio, int loc, int *off);


void
cuio_copydata(struct uio *uio, int off, int len, void *cp)
{
	struct iovec *iov = uio->uio_iov;
	int iol = uio->uio_iovcnt;
	unsigned count;

	if (off < 0)
		panic("cuio_copydata: off %d < 0", off);
	if (len < 0)
		panic("cuio_copydata: len %d < 0", len);
	while (off > 0) {
		if (iol == 0)
			panic("iov_copydata: empty in skip");
		if (off < iov->iov_len)
			break;
		off -= iov->iov_len;
		iol--;
		iov++;
	}
	while (len > 0) {
		if (iol == 0)
			panic("cuio_copydata: empty");
		count = min(iov->iov_len - off, len);
		memcpy(cp, (char *)iov->iov_base + off, count);
		len -= count;
		cp = (char *)cp + count;
		off = 0;
		iol--;
		iov++;
	}
}

void
cuio_copyback(struct uio *uio, int off, int len, void *cp)
{
	struct iovec *iov = uio->uio_iov;
	int iol = uio->uio_iovcnt;
	unsigned count;

	if (off < 0)
		panic("cuio_copyback: off %d < 0", off);
	if (len < 0)
		panic("cuio_copyback: len %d < 0", len);
	while (off > 0) {
		if (iol == 0) {
#ifdef DEBUG
			printf("cuio_copyback: empty in skip\n");
#endif
			return;
		}
		if (off < iov->iov_len)
			break;
		off -= iov->iov_len;
		iol--;
		iov++;
	}
	while (len > 0) {
		if (iol == 0) {
#ifdef DEBUG
			printf("uio_copyback: empty\n");
#endif
			return;
		}
		count = min(iov->iov_len - off, len);
		memcpy((char *)iov->iov_base + off, cp, count);
		len -= count;
		cp = (char *)cp + count;
		off = 0;
		iol--;
		iov++;
	}
}

/*
 * Return a pointer to iov/offset of location in iovec list.
 */

int
cuio_getptr(struct uio *uio, int loc, int *off)
{
	int ind, len;

	ind = 0;
	while (loc >= 0 && ind < uio->uio_iovcnt) {
		len = uio->uio_iov[ind].iov_len;
		if (len > loc) {
			*off = loc;
			return (ind);
		}
		loc -= len;
		ind++;
	}

	if (ind > 0 && loc == 0) {
		ind--;
		*off = uio->uio_iov[ind].iov_len;
		return (ind);
	}

	return (-1);
}

int
cuio_apply(struct uio *uio, int off, int len,
    int (*f)(void *, void *, unsigned int), void *fstate)
{
	int rval, ind, uiolen;
	unsigned int count;

	if (len < 0)
		panic("%s: len %d < 0", __func__, len);
	if (off < 0)
		panic("%s: off %d < 0", __func__, off);

	ind = 0;
	while (off > 0) {
		if (ind >= uio->uio_iovcnt)
			panic("cuio_apply: out of ivecs before data in uio");
		uiolen = uio->uio_iov[ind].iov_len;
		if (off < uiolen)
			break;
		off -= uiolen;
		ind++;
	}
	while (len > 0) {
		if (ind >= uio->uio_iovcnt)
			panic("cuio_apply: out of ivecs when processing uio");
		count = min(uio->uio_iov[ind].iov_len - off, len);

		rval = f(fstate,
			 ((char *)uio->uio_iov[ind].iov_base + off), count);
		if (rval)
			return (rval);

		len -= count;
		off = 0;
		ind++;
	}

	return (0);
}
