/*	$NetBSD: kern_info_09.c,v 1.20 2007/12/20 23:02:44 dsl Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1991, 1993
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
 *
 *	@(#)subr_xxx.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_info_09.c,v 1.20 2007/12/20 23:02:44 dsl Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/syslog.h>
#include <sys/unistd.h>
#include <uvm/uvm_extern.h>
#include <sys/sysctl.h>

#include <sys/mount.h>
#include <sys/syscallargs.h>

/* ARGSUSED */
int
compat_09_sys_getdomainname(struct lwp *l, const struct compat_09_sys_getdomainname_args *uap, register_t *retval)
{
	/* {
		syscallarg(char *) domainname;
		syscallarg(int) len;
	} */
	int name[2];
	size_t sz;

	name[0] = CTL_KERN;
	name[1] = KERN_DOMAINNAME;
	sz = SCARG(uap,len);
	return (old_sysctl(&name[0], 2, SCARG(uap, domainname), &sz, 0, 0, l));
}


/* ARGSUSED */
int
compat_09_sys_setdomainname(struct lwp *l, const struct compat_09_sys_setdomainname_args *uap, register_t *retval)
{
	/* {
		syscallarg(char *) domainname;
		syscallarg(int) len;
	} */
	int name[2];

	name[0] = CTL_KERN;
	name[1] = KERN_DOMAINNAME;
	return (old_sysctl(&name[0], 2, 0, 0, SCARG(uap, domainname),
			   SCARG(uap, len), l));
}

struct outsname {
	char	sysname[32];
	char	nodename[32];
	char	release[32];
	char	version[32];
	char	machine[32];
};

/* ARGSUSED */
int
compat_09_sys_uname(struct lwp *l, const struct compat_09_sys_uname_args *uap, register_t *retval)
{
	/* {
		syscallarg(struct outsname *) name;
	} */
	struct outsname outsname;
	const char *cp;
	char *dp, *ep;

	strncpy(outsname.sysname, ostype, sizeof(outsname.sysname));
	strncpy(outsname.nodename, hostname, sizeof(outsname.nodename));
	strncpy(outsname.release, osrelease, sizeof(outsname.release));
	dp = outsname.version;
	ep = &outsname.version[sizeof(outsname.version) - 1];
	for (cp = version; *cp && *cp != '('; cp++)
		;
	for (cp++; *cp && *cp != ')' && dp < ep; cp++)
		*dp++ = *cp;
	for (; *cp && *cp != '#'; cp++)
		;
	for (; *cp && *cp != ':' && dp < ep; cp++)
		*dp++ = *cp;
	*dp = '\0';
	strncpy(outsname.machine, MACHINE, sizeof(outsname.machine));
	return (copyout((void *)&outsname, (void *)SCARG(uap, name),
			sizeof(struct outsname)));
}
