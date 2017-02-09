/*	$NetBSD: tap_component.c,v 1.2 2015/08/20 12:20:09 christos Exp $	*/

/*
 * Copyright (c) 2015 Wei Liu.  All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: tap_component.c,v 1.2 2015/08/20 12:20:09 christos Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/stat.h>

#include "rump_private.h"
#include "rump_net_private.h"
#include "rump_vfs_private.h"

#include "ioconf.h"

#if defined(__minix)
void tapattach(int);
#endif

CFDRIVER_DECL(tap, DV_IFNET, NULL);

RUMP_COMPONENT(RUMP_COMPONENT_NET_IF)
{
	extern const struct cdevsw tap_cdevsw;
	devmajor_t bmaj, cmaj;
	int error;

	config_cfdriver_attach(&tap_cd);
	tapattach(0);

	bmaj = cmaj = NODEVMAJOR;
	error = devsw_attach("tap", NULL, &bmaj, &tap_cdevsw, &cmaj);
	if (error != 0)
		panic("tap devsw attach failed: %d", error);

	error = rump_vfs_makeonedevnode(S_IFCHR, "/dev/tap", cmaj, 0xfffff);
	if (error != 0)
		panic("cannot create tap device node: %d", error);

	error = rump_vfs_makedevnodes(S_IFCHR, "/dev/tap", '0', cmaj, 0, 4);
	if (error != 0)
		panic("cannot create tap[0-4] device node: %d", error);
}
