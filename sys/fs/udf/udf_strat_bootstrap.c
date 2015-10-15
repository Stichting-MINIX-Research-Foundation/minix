/* $NetBSD: udf_strat_bootstrap.c,v 1.4 2014/11/10 18:46:33 maxv Exp $ */

/*
 * Copyright (c) 2006, 2008 Reinoud Zandijk
 * All rights reserved.
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
 * 
 */

#include <sys/cdefs.h>
#ifndef lint
__KERNEL_RCSID(0, "$NetBSD: udf_strat_bootstrap.c,v 1.4 2014/11/10 18:46:33 maxv Exp $");
#endif /* not lint */


#if defined(_KERNEL_OPT)
#include "opt_compat_netbsd.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <miscfs/genfs/genfs_node.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/file.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/ioctl.h>
#include <sys/dirent.h>
#include <sys/stat.h>
#include <sys/conf.h>
#include <sys/kauth.h>
#include <sys/kthread.h>
#include <dev/clock_subr.h>

#include <fs/udf/ecma167-udf.h>
#include <fs/udf/udf_mount.h>

#include "udf.h"
#include "udf_subr.h"
#include "udf_bswap.h"


#define VTOI(vnode) ((struct udf_node *) vnode->v_data)
#define PRIV(ump) ((struct strat_private *) ump->strategy_private)

/* --------------------------------------------------------------------- */

static int
udf_create_logvol_dscr_bootstrap(struct udf_strat_args *args)
{
	panic("udf_create_logvol_dscr_bootstrap: not possible\n");
	return 0;
}


static void
udf_free_logvol_dscr_bootstrap(struct udf_strat_args *args)
{
	panic("udf_free_logvol_dscr_bootstrap: no node descriptor reading\n");
}


static int
udf_read_logvol_dscr_bootstrap(struct udf_strat_args *args)
{
	panic("udf_read_logvol_dscr_bootstrap: no node descriptor reading\n");
	return 0;
}


static int
udf_write_logvol_dscr_bootstrap(struct udf_strat_args *args)
{
	panic("udf_write_logvol_dscr_bootstrap: no writing\n");
}

/* --------------------------------------------------------------------- */

static void
udf_queuebuf_bootstrap(struct udf_strat_args *args)
{
	struct udf_mount *ump = args->ump;
	struct buf *buf = args->nestbuf;

	KASSERT(ump);
	KASSERT(buf);
	KASSERT(buf->b_iodone == nestiobuf_iodone);

	KASSERT(buf->b_flags & B_READ);
	VOP_STRATEGY(ump->devvp, buf);
}

static void
udf_discstrat_init_bootstrap(struct udf_strat_args *args)
{
	/* empty */
}


static void
udf_discstrat_finish_bootstrap(struct udf_strat_args *args)
{
	/* empty */
}

/* --------------------------------------------------------------------- */

struct udf_strategy udf_strat_bootstrap =
{
	udf_create_logvol_dscr_bootstrap,
	udf_free_logvol_dscr_bootstrap,
	udf_read_logvol_dscr_bootstrap,
	udf_write_logvol_dscr_bootstrap,
	udf_queuebuf_bootstrap,
	udf_discstrat_init_bootstrap,
	udf_discstrat_finish_bootstrap
};


