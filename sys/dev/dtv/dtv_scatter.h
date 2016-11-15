/* $NetBSD: dtv_scatter.h,v 1.1 2011/07/09 14:46:56 jmcneill Exp $ */

/*-
 * Copyright (c) 2011 Jared D. McNeill <jmcneill@invisible.ca>
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by Jared D. McNeill.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

#ifndef _DEV_DTV_DTV_SCATTER_H
#define _DEV_DTV_DTV_SCATTER_H

#include <sys/pool.h>

struct dtv_scatter_buf {
	pool_cache_t	sb_pool;
	size_t		sb_size;    /* size in bytes */
	size_t		sb_npages;  /* number of pages */
	uint8_t		**sb_page_ary; /* array of page pointers */
};

struct dtv_scatter_io {
	struct dtv_scatter_buf *sio_buf;
	off_t		sio_offset;
	size_t		sio_resid;
};

void	dtv_scatter_buf_init(struct dtv_scatter_buf *);
void	dtv_scatter_buf_destroy(struct dtv_scatter_buf *);
int	dtv_scatter_buf_set_size(struct dtv_scatter_buf *, size_t);
paddr_t	dtv_scatter_buf_map(struct dtv_scatter_buf *, off_t);

bool	dtv_scatter_io_init(struct dtv_scatter_buf *, off_t, size_t,
			    struct dtv_scatter_io *);
bool	dtv_scatter_io_next(struct dtv_scatter_io *, void **, size_t *);
void	dtv_scatter_io_undo(struct dtv_scatter_io *, size_t);
void	dtv_scatter_io_copyin(struct dtv_scatter_io *, const void *);
/* void	dtv_scatter_io_copyout(struct dtv_scatter_io *, void *); */
int	dtv_scatter_io_uiomove(struct dtv_scatter_io *, struct uio *);

#endif /* !_DEV_DTV_DTV_SCATTER_H */
