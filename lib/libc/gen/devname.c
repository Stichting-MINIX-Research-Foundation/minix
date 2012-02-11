/*	$NetBSD: devname.c,v 1.21 2010/03/23 20:28:59 drochner Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Simon Burge.
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

/*-
 * Copyright (c) 1992 Keith Muller.
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Keith Muller of the University of California, San Diego.
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
static char sccsid[] = "@(#)devname.c	8.2 (Berkeley) 4/29/95";
#else
__RCSID("$NetBSD: devname.c,v 1.21 2010/03/23 20:28:59 drochner Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>

#include <db.h>
#include <fcntl.h>
#include <paths.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <err.h>

#define	DEV_SZ		317	/* show be prime for best results */
#define	VALID		1	/* entry and devname are valid */
#define	INVALID		2	/* entry valid, devname NOT valid */

typedef struct devc {
	int valid;		/* entry valid? */
	dev_t dev;		/* cached device */
	mode_t type;		/* cached file type */
	char name[NAME_MAX];	/* device name */
} DEVC;

char *
devname(dev, type)
	dev_t dev;
	mode_t type;
{
	struct {
		mode_t type;
		dev_t dev;
	} bkey;
	struct {
		mode_t type;
		int32_t dev;
	} obkey;
	static DB *db;
	static int failure;
	DBT data, key;
	DEVC *ptr, **pptr;
	static DEVC **devtb = NULL;
	static devmajor_t pts;
	static int pts_valid = 0;

	if (!db && !failure &&
	    !(db = dbopen(_PATH_DEVDB, O_RDONLY, 0, DB_HASH, NULL))) {
		warn("warning: %s", _PATH_DEVDB);
		failure = 1;
	}
	/* initialise dev cache */
	if (!failure && devtb == NULL) {
		devtb = calloc(DEV_SZ, sizeof(DEVC *));
		if (devtb == NULL)
			failure= 1;
	}
	if (failure)
		return (NULL);

	/* see if we have this dev/type cached */
	pptr = devtb + (size_t)((dev + type) % DEV_SZ);
	ptr = *pptr;

	if (ptr && ptr->valid > 0 && ptr->dev == dev && ptr->type == type) {
		if (ptr->valid == VALID)
			return (ptr->name);
		return (NULL);
	}

	if (ptr == NULL)
		*pptr = ptr = malloc(sizeof(DEVC));

	/*
	 * Keys are a mode_t followed by a dev_t.  The former is the type of
	 * the file (mode & S_IFMT), the latter is the st_rdev field.  Be
	 * sure to clear any padding that may be found in bkey.
	 */
	(void)memset(&bkey, 0, sizeof(bkey));
	bkey.dev = dev;
	bkey.type = type;
	key.data = &bkey;
	key.size = sizeof(bkey);
	if ((db->get)(db, &key, &data, 0) == 0) {
found_it:
		if (ptr == NULL)
			return (char *)data.data;
		ptr->dev = dev;
		ptr->type = type;
		strncpy(ptr->name, (char *)data.data, NAME_MAX);
		ptr->name[NAME_MAX - 1] = '\0';
		ptr->valid = VALID;
	} else {
		/* Look for a 32 bit dev_t. */
		(void)memset(&obkey, 0, sizeof(obkey));
		obkey.dev = (int32_t)(uint32_t)dev;
		obkey.type = type;
		key.data = &obkey;
		key.size = sizeof(obkey);
		if ((db->get)(db, &key, &data, 0) == 0)
			goto found_it;

		if (ptr == NULL)
			return (NULL);
		ptr->valid = INVALID;
		if (type == S_IFCHR) {
			if (!pts_valid) {
				pts = getdevmajor("pts", S_IFCHR);
				pts_valid = 1;
			}
			if (pts != NODEVMAJOR && major(dev) == pts) {
				(void)snprintf(ptr->name, sizeof(ptr->name),
				    "%s%d", _PATH_DEV_PTS +
				    sizeof(_PATH_DEV) - 1,
				    minor(dev));
				ptr->valid = VALID;
			}
		}
		ptr->dev = dev;
		ptr->type = type;
	}
	if (ptr->valid == VALID)
		return (ptr->name);
	else
		return (NULL);
}
