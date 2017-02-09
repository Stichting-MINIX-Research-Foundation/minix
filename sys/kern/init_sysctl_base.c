/*	$NetBSD: init_sysctl_base.c,v 1.7 2015/08/25 14:52:31 pooka Exp $ */

/*-
 * Copyright (c) 2003, 2007, 2008, 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Brown, and by Andrew Doran.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: init_sysctl_base.c,v 1.7 2015/08/25 14:52:31 pooka Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/cpu.h>
#include <sys/kernel.h>
#include <sys/disklabel.h>

static int sysctl_setlen(SYSCTLFN_PROTO);

/*
 * sets up the base nodes...
 */
void
sysctl_basenode_init(void)
{

	sysctl_createv(NULL, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "kern",
		       SYSCTL_DESCR("High kernel"),
		       NULL, 0, NULL, 0,
		       CTL_KERN, CTL_EOL);
	sysctl_createv(NULL, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "vm",
		       SYSCTL_DESCR("Virtual memory"),
		       NULL, 0, NULL, 0,
		       CTL_VM, CTL_EOL);
	sysctl_createv(NULL, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "vfs",
		       SYSCTL_DESCR("Filesystem"),
		       NULL, 0, NULL, 0,
		       CTL_VFS, CTL_EOL);
	sysctl_createv(NULL, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "net",
		       SYSCTL_DESCR("Networking"),
		       NULL, 0, NULL, 0,
		       CTL_NET, CTL_EOL);
	sysctl_createv(NULL, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "debug",
		       SYSCTL_DESCR("Debugging"),
		       NULL, 0, NULL, 0,
		       CTL_DEBUG, CTL_EOL);
	sysctl_createv(NULL, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "hw",
		       SYSCTL_DESCR("Generic CPU, I/O"),
		       NULL, 0, NULL, 0,
		       CTL_HW, CTL_EOL);
	sysctl_createv(NULL, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "machdep",
		       SYSCTL_DESCR("Machine dependent"),
		       NULL, 0, NULL, 0,
		       CTL_MACHDEP, CTL_EOL);
	/*
	 * this node is inserted so that the sysctl nodes in libc can
	 * operate.
	 */
	sysctl_createv(NULL, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "user",
		       SYSCTL_DESCR("User-level"),
		       NULL, 0, NULL, 0,
		       CTL_USER, CTL_EOL);
	sysctl_createv(NULL, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "ddb",
		       SYSCTL_DESCR("In-kernel debugger"),
		       NULL, 0, NULL, 0,
		       CTL_DDB, CTL_EOL);
	sysctl_createv(NULL, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "proc",
		       SYSCTL_DESCR("Per-process"),
		       NULL, 0, NULL, 0,
		       CTL_PROC, CTL_EOL);
	sysctl_createv(NULL, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_NODE, "vendor",
		       SYSCTL_DESCR("Vendor specific"),
		       NULL, 0, NULL, 0,
		       CTL_VENDOR, CTL_EOL);
	sysctl_createv(NULL, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "emul",
		       SYSCTL_DESCR("Emulation settings"),
		       NULL, 0, NULL, 0,
		       CTL_EMUL, CTL_EOL);
	sysctl_createv(NULL, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "security",
		       SYSCTL_DESCR("Security"),
		       NULL, 0, NULL, 0,
		       CTL_SECURITY, CTL_EOL);
}

/*
 * now add some nodes which both rump kernel and standard
 * NetBSD both need, as rump cannot use sys/kern/init_sysctl.c
 */
SYSCTL_SETUP(sysctl_kernbase_setup, "sysctl kern subtree base setup")
{

	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_STRING, "ostype",
		       SYSCTL_DESCR("Operating system type"),
		       NULL, 0, __UNCONST(&ostype), 0,
		       CTL_KERN, KERN_OSTYPE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_STRING, "osrelease",
		       SYSCTL_DESCR("Operating system release"),
		       NULL, 0, __UNCONST(&osrelease), 0,
		       CTL_KERN, KERN_OSRELEASE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_IMMEDIATE,
		       CTLTYPE_INT, "osrevision",
		       SYSCTL_DESCR("Operating system revision"),
		       NULL, __NetBSD_Version__, NULL, 0,
		       CTL_KERN, KERN_OSREV, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_STRING, "version",
		       SYSCTL_DESCR("Kernel version"),
		       NULL, 0, __UNCONST(&version), 0,
		       CTL_KERN, KERN_VERSION, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_STRING, "hostname",
		       SYSCTL_DESCR("System hostname"),
		       sysctl_setlen, 0, hostname, MAXHOSTNAMELEN,
		       CTL_KERN, KERN_HOSTNAME, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_STRING, "domainname",
		       SYSCTL_DESCR("YP domain name"),
		       sysctl_setlen, 0, domainname, MAXHOSTNAMELEN,
		       CTL_KERN, KERN_DOMAINNAME, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_IMMEDIATE,
		       CTLTYPE_INT, "rawpartition",
		       SYSCTL_DESCR("Raw partition of a disk"),
		       NULL, RAW_PART, NULL, 0,
		       CTL_KERN, KERN_RAWPARTITION, CTL_EOL);
}

SYSCTL_SETUP(sysctl_hwbase_setup, "sysctl hw subtree base setup")
{
	u_int u;
	u_quad_t q;
	const char *model = cpu_getmodel();

	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_STRING, "model",
		       SYSCTL_DESCR("Machine model"),
		       NULL, 0, __UNCONST(model), 0,
		       CTL_HW, HW_MODEL, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_STRING, "machine",
		       SYSCTL_DESCR("Machine class"),
		       NULL, 0, machine, 0,
		       CTL_HW, HW_MACHINE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_STRING, "machine_arch",
		       SYSCTL_DESCR("Machine CPU class"),
		       NULL, 0, machine_arch, 0,
		       CTL_HW, HW_MACHINE_ARCH, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_INT, "ncpu",
		       SYSCTL_DESCR("Number of CPUs configured"),
		       NULL, 0, &ncpu, 0,
		       CTL_HW, HW_NCPU, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_IMMEDIATE,
		       CTLTYPE_INT, "byteorder",
		       SYSCTL_DESCR("System byte order"),
		       NULL, BYTE_ORDER, NULL, 0,
		       CTL_HW, HW_BYTEORDER, CTL_EOL);
	u = ((u_int)physmem > (UINT_MAX / PAGE_SIZE)) ?
		UINT_MAX : physmem * PAGE_SIZE;
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_IMMEDIATE,
		       CTLTYPE_INT, "physmem",
		       SYSCTL_DESCR("Bytes of physical memory"),
		       NULL, u, NULL, 0,
		       CTL_HW, HW_PHYSMEM, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_IMMEDIATE,
		       CTLTYPE_INT, "pagesize",
		       SYSCTL_DESCR("Software page size"),
		       NULL, PAGE_SIZE, NULL, 0,
		       CTL_HW, HW_PAGESIZE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_IMMEDIATE,
		       CTLTYPE_INT, "alignbytes",
		       SYSCTL_DESCR("Alignment constraint for all possible "
				    "data types"),
		       NULL, ALIGNBYTES, NULL, 0,
		       CTL_HW, HW_ALIGNBYTES, CTL_EOL);
	q = (u_quad_t)physmem * PAGE_SIZE;
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_IMMEDIATE,
		       CTLTYPE_QUAD, "physmem64",
		       SYSCTL_DESCR("Bytes of physical memory"),
		       NULL, q, NULL, 0,
		       CTL_HW, HW_PHYSMEM64, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_INT, "ncpuonline",
		       SYSCTL_DESCR("Number of CPUs online"),
		       NULL, 0, &ncpuonline, 0,
		       CTL_HW, HW_NCPUONLINE, CTL_EOL);
}

/*
 * sysctl helper function for kern.hostname and kern.domainnname.
 * resets the relevant recorded length when the underlying name is
 * changed.
 */
static int
sysctl_setlen(SYSCTLFN_ARGS)
{
	int error;

	error = sysctl_lookup(SYSCTLFN_CALL(rnode));
	if (error || newp == NULL)
		return (error);

	switch (rnode->sysctl_num) {
	case KERN_HOSTNAME:
		hostnamelen = strlen((const char*)rnode->sysctl_data);
		break;
	case KERN_DOMAINNAME:
		domainnamelen = strlen((const char*)rnode->sysctl_data);
		break;
	}

	return (0);
}
