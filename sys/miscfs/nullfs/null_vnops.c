/*	$NetBSD: null_vnops.c,v 1.39 2014/02/27 16:51:38 hannken Exp $	*/

/*
 * Copyright (c) 1999 National Aeronautics & Space Administration
 * All rights reserved.
 *
 * This software was written by William Studenmund of the
 * Numerical Aerospace Simulation Facility, NASA Ames Research Center.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the National Aeronautics & Space Administration
 *    nor the names of its contributors may be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NATIONAL AERONAUTICS & SPACE ADMINISTRATION
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE ADMINISTRATION OR CONTRIB-
 * UTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * John Heidemann of the UCLA Ficus project.
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
 *	@(#)null_vnops.c	8.6 (Berkeley) 5/27/95
 *
 * Ancestors:
 *	@(#)lofs_vnops.c	1.2 (Berkeley) 6/18/92
 *      Id: lofs_vnops.c,v 1.11 1992/05/30 10:05:43 jsp Exp jsp
 *	...and...
 *	@(#)null_vnodeops.c 1.20 92/07/07 UCLA Ficus project
 */

/*
 * Null file-system.
 *
 * Implemented using layerfs, see layer_vnops.c for a description.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: null_vnops.c,v 1.39 2014/02/27 16:51:38 hannken Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/vnode.h>

#include <miscfs/genfs/genfs.h>
#include <miscfs/genfs/layer_extern.h>
#include <miscfs/nullfs/null.h>

/*
 * Global VFS data structures.
 */

int (**null_vnodeop_p)(void *);

const struct vnodeopv_entry_desc null_vnodeop_entries[] = {
	{ &vop_default_desc,	layer_bypass },

	{ &vop_lookup_desc,	layer_lookup },
	{ &vop_setattr_desc,	layer_setattr },
	{ &vop_getattr_desc,	layer_getattr },
	{ &vop_access_desc,	layer_access },
	{ &vop_fsync_desc,	layer_fsync },
	{ &vop_inactive_desc,	layer_inactive },
	{ &vop_reclaim_desc,	layer_reclaim },
	{ &vop_lock_desc,	layer_lock },
	{ &vop_print_desc,	layer_print },
	{ &vop_remove_desc,	layer_remove },
	{ &vop_rename_desc,	layer_rename },
	{ &vop_revoke_desc,	layer_revoke },
	{ &vop_rmdir_desc,	layer_rmdir },

	{ &vop_open_desc,	layer_open },	/* mount option handling */

	{ &vop_bmap_desc,	layer_bmap },
	{ &vop_getpages_desc,	layer_getpages },
	{ &vop_putpages_desc,	layer_putpages },

	{ NULL, NULL }
};

const struct vnodeopv_desc null_vnodeop_opv_desc = {
	&null_vnodeop_p, null_vnodeop_entries
};
