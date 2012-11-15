/*	$NetBSD: disklabel.c,v 1.37 2012/06/25 22:32:43 abs Exp $	*/

/*
 * Copyright (c) 1983, 1987, 1993
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

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)disklabel.c	8.2 (Berkeley) 5/3/95";
#else
__RCSID("$NetBSD: disklabel.c,v 1.37 2012/06/25 22:32:43 abs Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/param.h>
#define DKTYPENAMES
#define FSTYPENAMES
#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>

#if HAVE_NBTOOL_CONFIG_H
#include <nbinclude/sys/disklabel.h>
#include <nbinclude/disktab.h>
#else
#include <sys/disklabel.h>
#include <disktab.h>
#endif /* HAVE_NBTOOL_CONFIG_H */

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __weak_alias
__weak_alias(getdiskbyname,_getdiskbyname)
#endif

#if 0
static void	error(int);
#endif
static int	gettype(char *, const char *const *);

static const char *db_array[2] = { _PATH_DISKTAB, 0 };

int
setdisktab(const char *name)
{
	if (!name || !*name)
		return -1;

	db_array[0] = name;
	return 0;
}


struct disklabel *
getdiskbyname(const char *name)
{
	static struct	disklabel disk;
	struct	disklabel *dp = &disk;
	struct partition *pp;
	char	*buf;
	char	*cp, *cq;	/* can't be */
	char	p, max, psize[3], pbsize[3],
		pfsize[3], poffset[3], ptype[3];
	u_int32_t *dx;
	long f;

	_DIAGASSERT(name != NULL);

	if (cgetent(&buf, db_array, name) < 0)
		return NULL;

	memset(&disk, 0, sizeof(disk));
	/*
	 * typename
	 */
	cq = dp->d_typename;
	cp = buf;
	while (cq < dp->d_typename + sizeof(dp->d_typename) - 1 &&
	    (*cq = *cp) && *cq != '|' && *cq != ':')
		cq++, cp++;
	*cq = '\0';
	/*
	 * boot name (optional)  xxboot, bootxx
	 */
	cgetstr(buf, "b0", &dp->d_boot0);
	cgetstr(buf, "b1", &dp->d_boot1);

	if (cgetstr(buf, "ty", &cq) >= 0) {
		if (strcmp(cq, "removable") == 0)
			dp->d_flags |= D_REMOVABLE;
		else if (strcmp(cq, "simulated") == 0)
			dp->d_flags |= D_RAMDISK;
		free(cq);
	}
	if (cgetcap(buf, "sf", ':') != NULL)
		dp->d_flags |= D_BADSECT;

#define getnumdflt(field, dname, dflt) \
    (field) = ((cgetnum(buf, dname, &f) == -1) ? (dflt) : (u_int32_t) f)
#define	getnum(field, dname) \
	if (cgetnum(buf, dname, &f) != -1) field = (u_int32_t)f

	getnumdflt(dp->d_secsize, "se", DEV_BSIZE);
	getnum(dp->d_ntracks, "nt");
	getnum(dp->d_nsectors, "ns");
	getnum(dp->d_ncylinders, "nc");

	if (cgetstr(buf, "dt", &cq) >= 0) {
		dp->d_type = gettype(cq, dktypenames);
		free(cq);
	} else
		getnumdflt(dp->d_type, "dt", 0);
	getnumdflt(dp->d_secpercyl, "sc", dp->d_nsectors * dp->d_ntracks);
	getnumdflt(dp->d_secperunit, "su", dp->d_secpercyl * dp->d_ncylinders);
	getnumdflt(dp->d_rpm, "rm", 3600);
	getnumdflt(dp->d_interleave, "il", 1);
	getnumdflt(dp->d_trackskew, "sk", 0);
	getnumdflt(dp->d_cylskew, "cs", 0);
	getnumdflt(dp->d_headswitch, "hs", 0);
	getnumdflt(dp->d_trkseek, "ts", 0);
	getnumdflt(dp->d_bbsize, "bs", BBSIZE);
	getnumdflt(dp->d_sbsize, "sb", SBLOCKSIZE);
	strcpy(psize, "px");	/* XXX: strcpy is safe */
	strcpy(pbsize, "bx");	/* XXX: strcpy is safe */
	strcpy(pfsize, "fx");	/* XXX: strcpy is safe */
	strcpy(poffset, "ox");	/* XXX: strcpy is safe */
	strcpy(ptype, "tx");	/* XXX: strcpy is safe */
	max = 'a' - 1;
	pp = &dp->d_partitions[0];
	for (p = 'a'; p < 'a' + MAXPARTITIONS; p++, pp++) {
		long ff;

		psize[1] = pbsize[1] = pfsize[1] = poffset[1] = ptype[1] = p;
		if (cgetnum(buf, psize, &ff) == -1)
			pp->p_size = 0;
		else {
			pp->p_size = (u_int32_t)ff;
			getnum(pp->p_offset, poffset);
			getnumdflt(pp->p_fsize, pfsize, 0);
			if (pp->p_fsize) {
				long bsize;

				if (cgetnum(buf, pbsize, &bsize) == -1)
					pp->p_frag = 8;
				else
					pp->p_frag =
					    (u_int8_t)(bsize / pp->p_fsize);
			}
			getnumdflt(pp->p_fstype, ptype, 0);
			if (pp->p_fstype == 0)
				if (cgetstr(buf, ptype, &cq) >= 0) {
					pp->p_fstype = gettype(cq, fstypenames);
					free(cq);
				}
			max = p;
		}
	}
	dp->d_npartitions = max + 1 - 'a';
	strcpy(psize, "dx");	/* XXX: strcpy is safe */
	dx = dp->d_drivedata;
	for (p = '0'; p < '0' + NDDATA; p++, dx++) {
		psize[1] = p;
		getnumdflt(*dx, psize, 0);
	}
	dp->d_magic = DISKMAGIC;
	dp->d_magic2 = DISKMAGIC;
	free(buf);
	return (dp);
}

static int
gettype(char *t, const char *const *names)
{
	const char *const *nm;

	_DIAGASSERT(t != NULL);
	_DIAGASSERT(names != NULL);

	for (nm = names; *nm; nm++)
		if (strcasecmp(t, *nm) == 0)
			return (int)(nm - names);
	if (isdigit((unsigned char) *t))
		return (atoi(t));
	return (0);
}

#if 0
static void
error(err)
	int err;
{
	char *p;

	(void)write(STDERR_FILENO, "disktab: ", 9);
	(void)write(STDERR_FILENO, _PATH_DISKTAB, sizeof(_PATH_DISKTAB) - 1);
	(void)write(STDERR_FILENO, ": ", 2);
	p = strerror(err);
	(void)write(STDERR_FILENO, p, strlen(p));
	(void)write(STDERR_FILENO, "\n", 1);
}
#endif
