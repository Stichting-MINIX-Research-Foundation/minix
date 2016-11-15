/*	$NetBSD: openfirmio.c,v 1.13 2014/07/25 08:10:37 dholland Exp $ */

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *
 *	@(#)openfirm.c	8.1 (Berkeley) 6/11/93
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: openfirmio.c,v 1.13 2014/07/25 08:10:37 dholland Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/event.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/openfirmio.h>

static	int lastnode;			/* speed hack */

static int openfirmcheckid (int, int);
static int openfirmgetstr (int, char *, char **);

void openfirmattach (int);

dev_type_ioctl(openfirmioctl);

const struct cdevsw openfirm_cdevsw = {
	.d_open = nullopen,
	.d_close = nullclose,
	.d_read = noread,
	.d_write = nowrite,
	.d_ioctl = openfirmioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = 0
};

void
openfirmattach(int num)
{
	/* nothing */
}

/*
 * Verify target ID is valid (exists in the OPENPROM tree), as
 * listed from node ID sid forward.
 */
static int
openfirmcheckid(int sid, int tid)
{

	for (; sid != 0; sid = OF_peer(sid))
		if (sid == tid || openfirmcheckid(OF_child(sid), tid))
			return (1);

	return (0);
}

static int
openfirmgetstr(int len, char *user, char **cpp)
{
	int error;
	char *cp;

	/* Reject obvious bogus requests */
	if ((u_int)len > (8 * 1024) - 1)
		return (ENAMETOOLONG);

	*cpp = cp = malloc(len + 1, M_TEMP, M_WAITOK);
	error = copyin(user, cp, len);
	cp[len] = '\0';
	return (error);
}

int
openfirmioctl(dev_t dev, u_long cmd, void *data, int flags, struct lwp *l)
{
	struct ofiocdesc *of;
	int node, len, ok, error, s;
	char *name, *value;

	if (cmd == OFIOCGETOPTNODE) {
		s = splhigh();
		*(int *) data = OF_finddevice("/options");
		splx(s);
		return (0);
	}

	/* Verify node id */
	of = (struct ofiocdesc *)data;
	node = of->of_nodeid;
	if (node != 0 && node != lastnode) {
		/* Not an easy one, must search for it */
		s = splhigh();
		ok = openfirmcheckid(OF_peer(0), node);
		splx(s);
		if (!ok)
			return (EINVAL);
		lastnode = node;
	}

	name = value = NULL;
	error = 0;
	switch (cmd) {

	case OFIOCGET:
		if ((flags & FREAD) == 0)
			return (EBADF);
		if (node == 0)
			return (EINVAL);
		error = openfirmgetstr(of->of_namelen, of->of_name, &name);
		if (error)
			break;
		s = splhigh();
		len = OF_getproplen(node, name);
		splx(s);
		if (len > of->of_buflen) {
			error = ENOMEM;
			break;
		}
		of->of_buflen = len;
		/* -1 means no entry; 0 means no value */
		if (len <= 0)
			break;
		value = malloc(len, M_TEMP, M_WAITOK);
		if (value == NULL) {
			error = ENOMEM;
			break;
		}
		s = splhigh();
		len = OF_getprop(node, name, (void *)value, len);
		splx(s);
		error = copyout(value, of->of_buf, len);
		break;


	case OFIOCSET:
		if ((flags & FWRITE) == 0)
			return (EBADF);
		if (node == 0)
			return (EINVAL);
		error = openfirmgetstr(of->of_namelen, of->of_name, &name);
		if (error)
			break;
		error = openfirmgetstr(of->of_buflen, of->of_buf, &value);
		if (error)
			break;
		s = splhigh();
		len = OF_setprop(node, name, value, of->of_buflen + 1);
		splx(s);

		/* 
		 * XXX
		 * some OF implementations return the buffer length including 
		 * the trailing zero ( like macppc ) and some without ( like
		 * FirmWorks OF used in Shark )
		 */
		if ((len != (of->of_buflen + 1)) && (len != of->of_buflen))
			error = EINVAL;
		break;

	case OFIOCNEXTPROP: {
		char newname[32];
		if ((flags & FREAD) == 0)
			return (EBADF);
		if (node == 0)
			return (EINVAL);
		if (of->of_namelen != 0) {
			error = openfirmgetstr(of->of_namelen, of->of_name,
			    &name);
			if (error)
				break;
		}
		s = splhigh();
		ok = OF_nextprop(node, name, newname);
		splx(s);
		if (ok == 0) {
			error = ENOENT;
			break;
		}
		if (ok == -1) {
			error = EINVAL;
			break;
		}
		len = strlen(newname);
		if (len > of->of_buflen)
			len = of->of_buflen;
		else
			of->of_buflen = len;
		error = copyout(newname, of->of_buf, len);
		break;
	}

	case OFIOCGETNEXT:
		if ((flags & FREAD) == 0)
			return (EBADF);
		s = splhigh();
		node = OF_peer(node);
		splx(s);
		*(int *)data = lastnode = node;
		break;

	case OFIOCGETCHILD:
		if ((flags & FREAD) == 0)
			return (EBADF);
		if (node == 0)
			return (EINVAL);
		s = splhigh();
		node = OF_child(node);
		splx(s);
		*(int *)data = lastnode = node;
		break;

	case OFIOCFINDDEVICE:
		if ((flags & FREAD) == 0)
			return (EBADF);
		error = openfirmgetstr(of->of_namelen, of->of_name, &name);
		if (error)
			break;
		node = OF_finddevice(name);
		if (node == 0 || node == -1) {
			error = ENOENT;
			break;
		}
		of->of_nodeid = lastnode = node;
		break;

	default:
		return (ENOTTY);
	}

	if (name)
		free(name, M_TEMP);
	if (value)
		free(value, M_TEMP);

	return (error);
}
