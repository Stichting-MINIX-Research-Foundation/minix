/*
 * Copyright (c) 2006, 2008, 2013 Reinoud Zandijk
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

#ifndef _FS_UDF_NEWFS_UDF_H_
#define _FS_UDF_NEWFS_UDF_H_

/* general settings */
#define UDF_512_TRACK	0	/* NOT recommended */
#define UDF_META_PERC  20	/* picked */

/* Identifying myself */
#define APP_VERSION_MAIN	0
#define APP_VERSION_SUB		5
#define IMPL_NAME		"*NetBSD userland UDF"


/* global variables describing disc and format requests */
extern int	 fd;			/* device: file descriptor */
extern char	*dev;			/* device: name		   */
extern struct mmc_discinfo mmc_discinfo;/* device: disc info	   */

extern char	*format_str;		/* format: string representation */
extern int	 format_flags;		/* format: attribute flags	 */
extern int	 media_accesstype;	/* derived from current mmc cap  */
extern int	 check_surface;		/* for rewritables               */

extern int	 wrtrack_skew;
extern int	 meta_perc;
extern float	 meta_fract;


/* shared structure between udf_create.c users */
struct udf_create_context context;
struct udf_disclayout     layout;

/* prototypes */
int udf_write_sector(void *sector, uint64_t location);
int udf_update_trackinfo(struct mmc_discinfo *di, struct mmc_trackinfo *ti);

/* tmp */
int writeout_write_queue(void);
int udf_surface_check(void);

#endif /* _FS_UDF_UDF_WRITE_H_ */
