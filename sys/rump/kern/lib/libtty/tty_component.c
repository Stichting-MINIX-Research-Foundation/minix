/*	$NetBSD: tty_component.c,v 1.2 2015/08/20 11:59:16 christos Exp $	*/

/*
 * Copyright (c) 2010 Antti Kantee.  All Rights Reserved.
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
__KERNEL_RCSID(0, "$NetBSD: tty_component.c,v 1.2 2015/08/20 11:59:16 christos Exp $");

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/stat.h>
#include <sys/tty.h>

#include "rump_private.h"
#include "rump_vfs_private.h"

#include "ioconf.h"

RUMP_COMPONENT(RUMP_COMPONENT_KERN_VFS)
{
	extern const struct cdevsw ctty_cdevsw, ptc_cdevsw, pts_cdevsw;
	extern const struct cdevsw ptm_cdevsw;
	devmajor_t cmaj, bmaj;

	cmaj = bmaj = -1;
	FLAWLESSCALL(devsw_attach("ctty", NULL, &bmaj, &ctty_cdevsw, &cmaj));
	FLAWLESSCALL(rump_vfs_makeonedevnode(S_IFCHR, "/dev/tty", cmaj, 0));

	/* create only 10 nodes.  should be enough for everyone ... */
	cmaj = bmaj = -1;
	FLAWLESSCALL(devsw_attach("ptc", NULL, &bmaj, &ptc_cdevsw, &cmaj));
	FLAWLESSCALL(rump_vfs_makedevnodes(S_IFCHR, "/dev/ptyp", '0',
	    cmaj, 0, 9));

	cmaj = bmaj = -1;
	FLAWLESSCALL(devsw_attach("pts", NULL, &bmaj, &pts_cdevsw, &cmaj));
	FLAWLESSCALL(rump_vfs_makedevnodes(S_IFCHR, "/dev/ttyp", '0',
	    cmaj, 0, 9));

	cmaj = bmaj = -1;
	FLAWLESSCALL(devsw_attach("ptm", NULL, &bmaj, &ptm_cdevsw, &cmaj));
	FLAWLESSCALL(rump_vfs_makeonedevnode(S_IFCHR, "/dev/ptmx", cmaj, 0));
	FLAWLESSCALL(rump_vfs_makeonedevnode(S_IFCHR, "/dev/ptm", cmaj, 1));

	tty_init();
	ttyldisc_init();

	ptyattach(1);

	rump_ttycomponent = true;
}
