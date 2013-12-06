/*	$NetBSD: sysctl.c,v 1.32 2012/03/20 16:36:05 matt Exp $	*/

/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
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
#if 0
static char sccsid[] = "@(#)sysctl.c	8.2 (Berkeley) 1/4/94";
#else
__RCSID("$NetBSD: sysctl.c,v 1.32 2012/03/20 16:36:05 matt Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/param.h>
#define __COMPAT_SYSCTL
#include <sys/sysctl.h>

#include <assert.h>
#include <errno.h>
#include <paths.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "extern.h"

#ifdef __weak_alias
__weak_alias(sysctl,_sysctl)
#endif

/*
 * handles requests off the user subtree
 */
static int user_sysctl(const int *, u_int, void *, size_t *,
			const void *, size_t);

/*
 * copies out individual nodes taking target version into account
 */
static size_t __cvt_node_out(uint, const struct sysctlnode *, void **,
			     size_t *);

#include <stdlib.h>

#if defined(__minix)
int __sysctl(const int *name, unsigned int namelen,
	void *oldp, size_t *oldlenp,
	const void *newp, size_t newlen)
{
	return ENOENT;
}
#endif /* defined(__minix) */

int
sysctl(const int *name, unsigned int namelen,
	void *oldp, size_t *oldlenp,
	const void *newp, size_t newlen)
{
	size_t oldlen, savelen;
	int error;

	if (name[0] != CTL_USER)
		return (__sysctl(name, namelen, oldp, oldlenp,
				 newp, newlen));

	oldlen = (oldlenp == NULL) ? 0 : *oldlenp;
	savelen = oldlen;
	error = user_sysctl(name + 1, namelen - 1, oldp, &oldlen, newp, newlen);

	if (error != 0) {
		errno = error;
		return (-1);
	}

	if (oldlenp != NULL) {
		*oldlenp = oldlen;
		if (oldp != NULL && oldlen > savelen) {
			errno = ENOMEM;
			return (-1);
		}
	}

	return (0);
}

static int
user_sysctl(const int *name, unsigned int namelen,
	void *oldp, size_t *oldlenp,
	const void *newp, size_t newlen)
{
#define _INT(s, n, v, d) {					\
	.sysctl_flags = CTLFLAG_IMMEDIATE|CTLFLAG_PERMANENT|	\
			CTLTYPE_INT|SYSCTL_VERSION,		\
	sysc_init_field(_sysctl_size, sizeof(int)),		\
	.sysctl_name = (s),					\
	.sysctl_num = (n),					\
	.sysctl_un = { .scu_idata = (v), },			\
	sysc_init_field(_sysctl_desc, (d)),			\
	}

	/*
	 * the nodes under the "user" node
	 */
	static const struct sysctlnode sysctl_usermib[] = {
#if defined(lint)
		/*
		 * lint doesn't like my initializers
		 */
		0
#else /* !lint */
		{
			.sysctl_flags = SYSCTL_VERSION|CTLFLAG_PERMANENT|
				CTLTYPE_STRING,
			sysc_init_field(_sysctl_size, sizeof(_PATH_STDPATH)),
			.sysctl_name = "cs_path",
			.sysctl_num = USER_CS_PATH,
			/*
			 * XXX these nasty initializers (and the one in
			 * the _INT() macro) can go away once all ports
			 * are using gcc3, and become
			 *
			 *	.sysctl_data = _PATH_STDPATH,
			 *	.sysctl_desc = NULL,
			 */
			.sysctl_un = { .scu_data = { 
				sysc_init_field(_sud_data,
				__UNCONST(_PATH_STDPATH)),
				}, },
			sysc_init_field(_sysctl_desc,
				"A value for the PATH environment variable "
				"that finds all the standard utilities"),
		},
		_INT("bc_base_max", USER_BC_BASE_MAX, BC_BASE_MAX,
		     "The maximum ibase/obase values in the bc(1) utility"),
		_INT("bc_dim_max", USER_BC_DIM_MAX, BC_DIM_MAX,
		     "The maximum array size in the bc(1) utility"),
		_INT("bc_scale_max", USER_BC_SCALE_MAX, BC_SCALE_MAX,
		     "The maximum scale value in the bc(1) utility"),
		_INT("bc_string_max", USER_BC_STRING_MAX, BC_STRING_MAX,
		     "The maximum string length in the bc(1) utility"),
		_INT("coll_weights_max", USER_COLL_WEIGHTS_MAX,
		     COLL_WEIGHTS_MAX, "The maximum number of weights that can "
		     "be assigned to any entry of the LC_COLLATE order keyword "
		     "in the locale definition file"),
		_INT("expr_nest_max", USER_EXPR_NEST_MAX, EXPR_NEST_MAX,
		     "The maximum number of expressions that can be nested "
		     "within parenthesis by the expr(1) utility"),
		_INT("line_max", USER_LINE_MAX, LINE_MAX, "The maximum length "
		     "in bytes of a text-processing utility's input line"),
		_INT("re_dup_max", USER_RE_DUP_MAX, RE_DUP_MAX, "The maximum "
		     "number of repeated occurrences of a regular expression "
		     "permitted when using interval notation"),
		_INT("posix2_version", USER_POSIX2_VERSION, _POSIX2_VERSION,
		     "The version of POSIX 1003.2 with which the system "
		     "attempts to comply"),
#ifdef _POSIX2_C_BIND
		_INT("posix2_c_bind", USER_POSIX2_C_BIND, 1,
		     "Whether the system's C-language development facilities "
		     "support the C-Language Bindings Option"),
#else
		_INT("posix2_c_bind", USER_POSIX2_C_BIND, 0,
		     "Whether the system's C-language development facilities "
		     "support the C-Language Bindings Option"),
#endif
#ifdef POSIX2_C_DEV
		_INT("posix2_c_dev", USER_POSIX2_C_DEV, 1,
		     "Whether the system supports the C-Language Development "
		     "Utilities Option"),
#else
		_INT("posix2_c_dev", USER_POSIX2_C_DEV, 0,
		     "Whether the system supports the C-Language Development "
		     "Utilities Option"),
#endif
#ifdef POSIX2_CHAR_TERM
		_INT("posix2_char_term", USER_POSIX2_CHAR_TERM, 1,
		     "Whether the system supports at least one terminal type "
		     "capable of all operations described in POSIX 1003.2"),
#else
		_INT("posix2_char_term", USER_POSIX2_CHAR_TERM, 0,
		     "Whether the system supports at least one terminal type "
		     "capable of all operations described in POSIX 1003.2"),
#endif
#ifdef POSIX2_FORT_DEV
		_INT("posix2_fort_dev", USER_POSIX2_FORT_DEV, 1,
		     "Whether the system supports the FORTRAN Development "
		     "Utilities Option"),
#else
		_INT("posix2_fort_dev", USER_POSIX2_FORT_DEV, 0,
		     "Whether the system supports the FORTRAN Development "
		     "Utilities Option"),
#endif
#ifdef POSIX2_FORT_RUN
		_INT("posix2_fort_run", USER_POSIX2_FORT_RUN, 1,
		     "Whether the system supports the FORTRAN Runtime "
		     "Utilities Option"),
#else
		_INT("posix2_fort_run", USER_POSIX2_FORT_RUN, 0,
		     "Whether the system supports the FORTRAN Runtime "
		     "Utilities Option"),
#endif
#ifdef POSIX2_LOCALEDEF
		_INT("posix2_localedef", USER_POSIX2_LOCALEDEF, 1,
		     "Whether the system supports the creation of locales"),
#else
		_INT("posix2_localedef", USER_POSIX2_LOCALEDEF, 0,
		     "Whether the system supports the creation of locales"),
#endif
#ifdef POSIX2_SW_DEV
		_INT("posix2_sw_dev", USER_POSIX2_SW_DEV, 1,
		     "Whether the system supports the Software Development "
		     "Utilities Option"),
#else
		_INT("posix2_sw_dev", USER_POSIX2_SW_DEV, 0,
		     "Whether the system supports the Software Development "
		     "Utilities Option"),
#endif
#ifdef POSIX2_UPE
		_INT("posix2_upe", USER_POSIX2_UPE, 1,
		     "Whether the system supports the User Portability "
		     "Utilities Option"),
#else
		_INT("posix2_upe", USER_POSIX2_UPE, 0,
		     "Whether the system supports the User Portability "
		     "Utilities Option"),
#endif
		_INT("stream_max", USER_STREAM_MAX, FOPEN_MAX,
		     "The minimum maximum number of streams that a process "
		     "may have open at any one time"),
		_INT("tzname_max", USER_TZNAME_MAX, NAME_MAX,
		     "The minimum maximum number of types supported for the "
		     "name of a timezone"),
		_INT("atexit_max", USER_ATEXIT_MAX, -1,
		     "The maximum number of functions that may be registered "
		     "with atexit(3)"),
#endif /* !lint */
	};
#undef _INT

	static const int clen = sizeof(sysctl_usermib) /
		sizeof(sysctl_usermib[0]);

	const struct sysctlnode *node;
	int ni;
	size_t l, sz;

	/*
	 * none of these nodes are writable and they're all terminal (for now)
	 */
	if (namelen != 1)
		return (EINVAL);

	l = *oldlenp;
	if (name[0] == CTL_QUERY) {
		uint v;
		node = newp;
		if (node == NULL)
			return (EINVAL);
		else if (SYSCTL_VERS(node->sysctl_flags) == SYSCTL_VERS_1 &&
			 newlen == sizeof(struct sysctlnode))
			v = SYSCTL_VERS_1;
		else
			return (EINVAL);

		sz = 0;
		for (ni = 0; ni < clen; ni++)
			sz += __cvt_node_out(v, &sysctl_usermib[ni], &oldp, &l);
		*oldlenp = sz;
		return (0);
	}

	if (name[0] == CTL_DESCRIBE) {
		/*
		 * XXX make sure this is larger than the largest
		 * "user" description
		 */
		char buf[192];
		struct sysctldesc *d1 = (void *)&buf[0], *d2 = oldp;
		size_t d;

		node = newp;
		if (node != NULL &&
		    (SYSCTL_VERS(node->sysctl_flags) < SYSCTL_VERS_1 ||
		     newlen != sizeof(struct sysctlnode)))
			return (EINVAL);

		sz = 0;
		for (ni = 0; ni < clen; ni++) {
			memset(&buf[0], 0, sizeof(buf));
			if (node != NULL &&
			    node->sysctl_num != sysctl_usermib[ni].sysctl_num)
				continue;
			d1->descr_num = sysctl_usermib[ni].sysctl_num;
			d1->descr_ver = sysctl_usermib[ni].sysctl_ver;
			if (sysctl_usermib[ni].sysctl_desc == NULL)
				d1->descr_len = 1;
			else {
				size_t dlen;
				(void)strlcpy(d1->descr_str,
					sysctl_usermib[ni].sysctl_desc,
					sizeof(buf) - sizeof(*d1));
				dlen = strlen(d1->descr_str) + 1;
				_DIAGASSERT(__type_fit(uint32_t, dlen));
				d1->descr_len = (uint32_t)dlen;
			}
			d = (size_t)__sysc_desc_adv(NULL, d1->descr_len);
			if (d2 != NULL)
				memcpy(d2, d1, d);
			sz += d;
			if (node != NULL)
				break;
		}
		*oldlenp = sz;
		if (sz == 0 && node != NULL)
			return (ENOENT);
		return (0);

	}

	/*
	 * none of these nodes are writable
	 */
	if (newp != NULL || newlen != 0)
		return (EPERM);
	
	node = &sysctl_usermib[0];
	for (ni = 0; ni	< clen; ni++)
		if (name[0] == node[ni].sysctl_num)
			break;
	if (ni == clen)
		return (EOPNOTSUPP);

	node = &node[ni];
	if (node->sysctl_flags & CTLFLAG_IMMEDIATE) {
		switch (SYSCTL_TYPE(node->sysctl_flags)) {
		case CTLTYPE_INT:
			newp = &node->sysctl_idata;
			break;
		case CTLTYPE_QUAD:
			newp = &node->sysctl_qdata;
			break;
		default:
			return (EINVAL);
		}
	}
	else
		newp = node->sysctl_data;

	l = MIN(l, node->sysctl_size);
	if (oldp != NULL)
		memcpy(oldp, newp, l);
	*oldlenp = node->sysctl_size;

	return (0);
}

static size_t
__cvt_node_out(uint v, const struct sysctlnode *n, void **o, size_t *l)
{
	const void *src = n;
	size_t sz;

	switch (v) {
#if (SYSCTL_VERSION != SYSCTL_VERS_1)
#error __cvt_node_out: no support for SYSCTL_VERSION
#endif /* (SYSCTL_VERSION != SYSCTL_VERS_1) */

	case SYSCTL_VERSION:
		sz = sizeof(struct sysctlnode);
		break;

	default:
		sz = 0;
		break;
	}

	if (sz > 0 && *o != NULL && *l >= sz) {
		memcpy(*o, src, sz);
		*o = sz + (caddr_t)*o;
		*l -= sz;
	}

	return(sz);
}
