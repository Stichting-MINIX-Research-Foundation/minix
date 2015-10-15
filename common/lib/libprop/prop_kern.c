/*	$NetBSD: prop_kern.c,v 1.19 2015/05/11 16:48:34 christos Exp $	*/

/*-
 * Copyright (c) 2006, 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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

#if defined(__NetBSD__)

#include <sys/types.h>
#include <sys/ioctl.h>

#include <prop/proplib.h>

#if !defined(_KERNEL) && !defined(_STANDALONE)
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef RUMP_ACTION
#include <rump/rump_syscalls.h>
#define ioctl(a,b,c) rump_sys_ioctl(a,b,c)
#endif

static int
_prop_object_externalize_to_pref(prop_object_t obj, struct plistref *pref,
	       			 char **bufp)
{
	char *buf;

	switch (prop_object_type(obj)) {
	case PROP_TYPE_DICTIONARY:
		buf = prop_dictionary_externalize(obj);
		break;
	case PROP_TYPE_ARRAY:
		buf = prop_array_externalize(obj);
		break;
	default:
		return (ENOTSUP);
	}
	if (buf == NULL) {
		/* Assume we ran out of memory. */
		return (ENOMEM);
	}
	pref->pref_plist = buf;
	pref->pref_len = strlen(buf) + 1;

	*bufp = buf;

	return (0);
}

bool
prop_array_externalize_to_pref(prop_array_t array, struct plistref *prefp)
{
	char *buf;
	int rv;

	rv = _prop_object_externalize_to_pref(array, prefp, &buf);
	if (rv != 0)
		errno = rv;	/* pass up error value in errno */
	return (rv == 0);
}

/*
 * prop_array_externalize_to_pref --
 *	Externalize an array into a plistref for sending to the kernel.
 */
int
prop_array_send_syscall(prop_array_t array, struct plistref *prefp)
{
	if (prop_array_externalize_to_pref(array, prefp))
		return 0;
	else
		return errno;
}

bool
prop_dictionary_externalize_to_pref(prop_dictionary_t dict,
				    struct plistref *prefp)
{
	char *buf;
	int rv;

	rv = _prop_object_externalize_to_pref(dict, prefp, &buf);
	if (rv != 0)
		errno = rv;	/* pass up error value in errno */
	return (rv == 0);
}

/*
 * prop_dictionary_externalize_to_pref --
 *	Externalize an dictionary into a plistref for sending to the kernel.
 */
int
prop_dictionary_send_syscall(prop_dictionary_t dict,
			     struct plistref *prefp)
{
	if (prop_dictionary_externalize_to_pref(dict, prefp))
		return 0;
	else
		return errno;
}

static int
_prop_object_send_ioctl(prop_object_t obj, int fd, unsigned long cmd)
{
	struct plistref pref;
	char *buf;
	int error;

	error = _prop_object_externalize_to_pref(obj, &pref, &buf);
	if (error)
		return (error);

	if (ioctl(fd, cmd, &pref) == -1)
		error = errno;
	else
		error = 0;
	
	free(buf);

	return (error);
}

/*
 * prop_array_send_ioctl --
 *	Send an array to the kernel using the specified ioctl.
 */
int
prop_array_send_ioctl(prop_array_t array, int fd, unsigned long cmd)
{
	int rv;

	rv = _prop_object_send_ioctl(array, fd, cmd);
	if (rv != 0) {
		errno = rv;	/* pass up error value in errno */
		return rv;
	} else 
		return 0;
}

/*
 * prop_dictionary_send_ioctl --
 *	Send a dictionary to the kernel using the specified ioctl.
 */
int
prop_dictionary_send_ioctl(prop_dictionary_t dict, int fd, unsigned long cmd)
{
	int rv;

	rv = _prop_object_send_ioctl(dict, fd, cmd);
	if (rv != 0) {
		errno = rv;	/* pass up error value in errno */
		return rv;
	} else 
		return 0;
}

static int
_prop_object_internalize_from_pref(const struct plistref *pref,
				   prop_type_t type, prop_object_t *objp)
{
	prop_object_t obj = NULL;
	char *buf;
	int error = 0;

	if (pref->pref_len == 0) {
		/*
		 * This should never happen; we should always get the XML
		 * for an empty dictionary if it's really empty.
		 */
		error = EIO;
		goto out;
	} else {
		buf = pref->pref_plist;
		buf[pref->pref_len - 1] = '\0';	/* extra insurance */
		switch (type) {
		case PROP_TYPE_DICTIONARY:
			obj = prop_dictionary_internalize(buf);
			break;
		case PROP_TYPE_ARRAY:
			obj = prop_array_internalize(buf);
			break;
		default:
			error = ENOTSUP;
		}
		(void) munmap(buf, pref->pref_len);
		if (obj == NULL && error == 0)
			error = EIO;
	}

 out:
	if (error == 0)
		*objp = obj;
	return (error);
}

/*
 * prop_array_internalize_from_pref --
 * 	Internalize a pref into a prop_array_t object.
 */
bool
prop_array_internalize_from_pref(const struct plistref *prefp,
				 prop_array_t *arrayp)
{
	int rv;

	rv = _prop_object_internalize_from_pref(prefp, PROP_TYPE_ARRAY,
	    (prop_object_t *)arrayp);
	if (rv != 0)
		errno = rv;     /* pass up error value in errno */
	return (rv == 0);
}

/*
 * prop_array_recv_syscall --
 * 	Internalize an array received from the kernel as pref.
 */
int
prop_array_recv_syscall(const struct plistref *prefp,
			prop_array_t *arrayp)
{
	if (prop_array_internalize_from_pref(prefp, arrayp))
		return 0;
	else
		return errno;
}

/*
 * prop_dictionary_internalize_from_pref --
 * 	Internalize a pref into a prop_dictionary_t object.
 */
bool
prop_dictionary_internalize_from_pref(const struct plistref *prefp,
				      prop_dictionary_t *dictp)
{
	int rv;

	rv = _prop_object_internalize_from_pref(prefp, PROP_TYPE_DICTIONARY,
	    (prop_object_t *)dictp);
	if (rv != 0)
		errno = rv;     /* pass up error value in errno */
	return (rv == 0);
}

/*
 * prop_dictionary_recv_syscall --
 *	Internalize a dictionary received from the kernel as pref.
 */
int
prop_dictionary_recv_syscall(const struct plistref *prefp,
			     prop_dictionary_t *dictp)
{
	if (prop_dictionary_internalize_from_pref(prefp, dictp))
		return 0;
	else
		return errno;
}


/*
 * prop_array_recv_ioctl --
 *	Receive an array from the kernel using the specified ioctl.
 */
int
prop_array_recv_ioctl(int fd, unsigned long cmd, prop_array_t *arrayp)
{
	int rv;
	struct plistref pref;

	rv = ioctl(fd, cmd, &pref);
	if (rv == -1)
		return errno;

	rv = _prop_object_internalize_from_pref(&pref, PROP_TYPE_ARRAY,
			    (prop_object_t *)arrayp);
	if (rv != 0) {
		errno = rv;     /* pass up error value in errno */
		return rv;
	} else
		return 0;
}

/*
 * prop_dictionary_recv_ioctl --
 *	Receive a dictionary from the kernel using the specified ioctl.
 */
int
prop_dictionary_recv_ioctl(int fd, unsigned long cmd, prop_dictionary_t *dictp)
{
	int rv;
	struct plistref pref;

	rv = ioctl(fd, cmd, &pref);
	if (rv == -1)
		return errno;

	rv = _prop_object_internalize_from_pref(&pref, PROP_TYPE_DICTIONARY,
			    (prop_object_t *)dictp);
	if (rv != 0) {
		errno = rv;     /* pass up error value in errno */
		return rv;
	} else
		return 0;
}

/*
 * prop_dictionary_sendrecv_ioctl --
 *	Combination send/receive a dictionary to/from the kernel using
 *	the specified ioctl.
 */
int
prop_dictionary_sendrecv_ioctl(prop_dictionary_t dict, int fd,
			       unsigned long cmd, prop_dictionary_t *dictp)
{
	struct plistref pref;
	char *buf;
	int error;

	error = _prop_object_externalize_to_pref(dict, &pref, &buf);
	if (error != 0) {
		errno = error;
		return error;
	}

	if (ioctl(fd, cmd, &pref) == -1)
		error = errno;
	else
		error = 0;
	
	free(buf);

	if (error != 0)
		return error;

	error = _prop_object_internalize_from_pref(&pref, PROP_TYPE_DICTIONARY,
			    (prop_object_t *)dictp);
	if (error != 0) {
		errno = error;     /* pass up error value in errno */
		return error;
	} else
		return 0;
}
#endif /* !_KERNEL && !_STANDALONE */

#if defined(_KERNEL)
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/resource.h>
#include <sys/pool.h>

#include <uvm/uvm_extern.h>

#include "prop_object_impl.h"

/* Arbitrary limit ioctl input to 64KB */
unsigned int prop_object_copyin_limit = 65536;

/* initialize proplib for use in the kernel */
void
prop_kern_init(void)
{
	__link_set_decl(prop_linkpools, struct prop_pool_init);
	struct prop_pool_init * const *pi;

	__link_set_foreach(pi, prop_linkpools)
		pool_init((*pi)->pp, (*pi)->size, 0, 0, 0, (*pi)->wchan,
		    &pool_allocator_nointr, IPL_NONE);
}

static int
_prop_object_copyin(const struct plistref *pref, const prop_type_t type,
			  prop_object_t *objp)
{
	prop_object_t obj = NULL;
	char *buf;
	int error;

	if (pref->pref_len >= prop_object_copyin_limit)
		return EINVAL;

	/*
	 * Allocate an extra byte so we can guarantee NUL-termination.
	 *
	 * Allow malloc to fail in case pmap would be exhausted.
	 */
	buf = malloc(pref->pref_len + 1, M_TEMP, M_WAITOK | M_CANFAIL);
	if (buf == NULL)
		return (ENOMEM);
	error = copyin(pref->pref_plist, buf, pref->pref_len);
	if (error) {
		free(buf, M_TEMP);
		return (error);
	}
	buf[pref->pref_len] = '\0';

	switch (type) {
	case PROP_TYPE_ARRAY:
		obj = prop_array_internalize(buf);
		break;
	case PROP_TYPE_DICTIONARY:
		obj = prop_dictionary_internalize(buf);
		break;
	default:
		error = ENOTSUP;
	}

	free(buf, M_TEMP);
	if (obj == NULL) {
		if (error == 0)
			error = EIO;
	} else {
		*objp = obj;
	}
	return (error);
}


static int
_prop_object_copyin_ioctl(const struct plistref *pref, const prop_type_t type,
			  const u_long cmd, prop_object_t *objp)
{
	if ((cmd & IOC_IN) == 0)
		return (EFAULT);

	return _prop_object_copyin(pref, type, objp);
}

/*
 * prop_array_copyin --
 *	Copy in an array passed as a syscall arg.
 */
int
prop_array_copyin(const struct plistref *pref, prop_array_t *arrayp)
{
	return (_prop_object_copyin(pref, PROP_TYPE_ARRAY,
					  (prop_object_t *)arrayp));
}

/*
 * prop_dictionary_copyin --
 *	Copy in a dictionary passed as a syscall arg.
 */
int
prop_dictionary_copyin(const struct plistref *pref, prop_dictionary_t *dictp)
{
	return (_prop_object_copyin(pref, PROP_TYPE_DICTIONARY,
					  (prop_object_t *)dictp));
}


/*
 * prop_array_copyin_ioctl --
 *	Copy in an array send with an ioctl.
 */
int
prop_array_copyin_ioctl(const struct plistref *pref, const u_long cmd,
			prop_array_t *arrayp)
{
	return (_prop_object_copyin_ioctl(pref, PROP_TYPE_ARRAY,
					  cmd, (prop_object_t *)arrayp));
}

/*
 * prop_dictionary_copyin_ioctl --
 *	Copy in a dictionary sent with an ioctl.
 */
int
prop_dictionary_copyin_ioctl(const struct plistref *pref, const u_long cmd,
			     prop_dictionary_t *dictp)
{
	return (_prop_object_copyin_ioctl(pref, PROP_TYPE_DICTIONARY,
					  cmd, (prop_object_t *)dictp));
}

static int
_prop_object_copyout(struct plistref *pref, prop_object_t obj)
{
	struct lwp *l = curlwp;		/* XXX */
	struct proc *p = l->l_proc;
	char *buf;
	void *uaddr;
	size_t len, rlen;
	int error = 0;

	switch (prop_object_type(obj)) {
	case PROP_TYPE_ARRAY:
		buf = prop_array_externalize(obj);
		break;
	case PROP_TYPE_DICTIONARY:
		buf = prop_dictionary_externalize(obj);
		break;
	default:
		return (ENOTSUP);
	}
	if (buf == NULL)
		return (ENOMEM);

	len = strlen(buf) + 1;
	rlen = round_page(len);
	uaddr = NULL;
	error = uvm_mmap_anon(p, &uaddr, rlen);
	if (error == 0) {
		error = copyout(buf, uaddr, len);
		if (error == 0) {
			pref->pref_plist = uaddr;
			pref->pref_len   = len;
		}
	}

	free(buf, M_TEMP);

	return (error);
}

/*
 * prop_array_copyout --
 *	Copy out an array to a syscall arg.
 */
int
prop_array_copyout(struct plistref *pref, prop_array_t array)
{
	return (_prop_object_copyout(pref, array));
}

/*
 * prop_dictionary_copyout --
 *	Copy out a dictionary to a syscall arg.
 */
int
prop_dictionary_copyout(struct plistref *pref, prop_dictionary_t dict)
{
	return (_prop_object_copyout(pref, dict));
}

static int
_prop_object_copyout_ioctl(struct plistref *pref, const u_long cmd,
			   prop_object_t obj)
{
	if ((cmd & IOC_OUT) == 0)
		return (EFAULT);
	return _prop_object_copyout(pref, obj);
}


/*
 * prop_array_copyout_ioctl --
 *	Copy out an array being received with an ioctl.
 */
int
prop_array_copyout_ioctl(struct plistref *pref, const u_long cmd,
			 prop_array_t array)
{
	return (_prop_object_copyout_ioctl(pref, cmd, array));
}

/*
 * prop_dictionary_copyout_ioctl --
 *	Copy out a dictionary being received with an ioctl.
 */
int
prop_dictionary_copyout_ioctl(struct plistref *pref, const u_long cmd,
			      prop_dictionary_t dict)
{
	return (
	    _prop_object_copyout_ioctl(pref, cmd, dict));
}
#endif /* _KERNEL */

#endif /* __NetBSD__ */
