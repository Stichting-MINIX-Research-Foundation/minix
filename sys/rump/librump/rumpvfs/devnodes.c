/*	$NetBSD: devnodes.c,v 1.11 2015/06/08 12:16:47 pooka Exp $	*/

/*
 * Copyright (c) 2009 Antti Kantee.  All Rights Reserved.
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
__KERNEL_RCSID(0, "$NetBSD: devnodes.c,v 1.11 2015/06/08 12:16:47 pooka Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/filedesc.h>
#include <sys/kmem.h>
#include <sys/lwp.h>
#include <sys/namei.h>
#include <sys/stat.h>
#include <sys/vfs_syscalls.h>

#include "rump_vfs_private.h"

/* realqvik(tm) "devfs" */
static int
makeonedevnode(dev_t devtype, const char *devname,
	devmajor_t majnum, devminor_t minnum)
{
	register_t retval;
	int error;

	error = do_sys_mknod(curlwp, devname, 0666 | devtype,
	    makedev(majnum, minnum), &retval, UIO_SYSSPACE);
	if (error == EEXIST) /* XXX: should check it's actually the same */
		error = 0;

	return error;
}

static int
makedevnodes(dev_t devtype, const char *basename, char minchar,
	devmajor_t maj, devminor_t minnum, int nnodes)
{
	int error = 0;
	char *devname, *p;
	size_t devlen;
	register_t retval;

	devlen = strlen(basename) + 1 + 1; /* +letter +0 */
	devname = kmem_zalloc(devlen, KM_SLEEP);
	strlcpy(devname, basename, devlen);
	p = devname + devlen-2;

	for (; nnodes > 0; nnodes--, minchar++, minnum++) {
		KASSERT(minchar <= 'z');
		*p = minchar;

		if ((error = do_sys_mknod(curlwp, devname, 0666 | devtype,
		    makedev(maj, minnum), &retval, UIO_SYSSPACE))) {
			if (error == EEXIST)
				error = 0;
			else
				goto out;
		}
	}

 out:
	kmem_free(devname, devlen);
	return error;
}

static int
makesymlink(const char *dst, const char *src)
{

	return do_sys_symlink(dst, src, UIO_SYSSPACE);
}

enum { NOTEXIST, SAME, DIFFERENT };
static int
doesitexist(const char *path, bool isblk, devmajor_t dmaj, devminor_t dmin)
{
	struct stat sb;
	int error;

	error = do_sys_stat(path, 0, &sb);
	/* even if not ENOENT, we might be able to create it */
	if (error)
		return NOTEXIST;

	if (major(sb.st_rdev) != dmaj || minor(sb.st_rdev) != dmin)
		return DIFFERENT;
	if (isblk && !S_ISBLK(sb.st_mode))
		return DIFFERENT;
	if (!isblk && !S_ISCHR(sb.st_mode))
		return DIFFERENT;

	return SAME;
}

static void
makeonenode(char *buf, size_t len, devmajor_t blk, devmajor_t chr,
    devminor_t dmin, const char *base, int c1, int c2)
{
	char cstr1[2] = {0,0}, cstr2[2] = {0,0};
	register_t rv;
	int error;

	if (c1 != -1) {
		cstr1[0] = '0' + c1;
		cstr1[1] = '\0';
	}

	if (c2 != -1) {
		cstr2[0] = 'a' + c2;
		cstr2[1] = '\0';

	}

	/* block device */
	snprintf(buf, len, "/dev/%s%s%s", base, cstr1, cstr2);
	if (blk != NODEVMAJOR) {
		switch (doesitexist(buf, true, blk, dmin)) {
		case DIFFERENT:
			aprint_verbose("mkdevnodes: block device %s "
			    "already exists\n", buf);
			break;
		case NOTEXIST:
			if ((error = do_sys_mknod(curlwp, buf, 0600 | S_IFBLK,
			    makedev(blk, dmin), &rv, UIO_SYSSPACE)) != 0)
				aprint_verbose("mkdevnodes: failed to "
				    "create %s: %d\n", buf, error);
			break;
		case SAME:
			/* done */
			break;
		}
		snprintf(buf, len, "/dev/r%s%s%s", base, cstr1, cstr2);
	}

	switch (doesitexist(buf, true, chr, dmin)) {
	case DIFFERENT:
		aprint_verbose("mkdevnodes: character device %s "
		    "already exists\n", buf);
		break;
	case NOTEXIST:
		if ((error = do_sys_mknod(curlwp, buf, 0600 | S_IFCHR,
		    makedev(chr, dmin), &rv, UIO_SYSSPACE)) != 0)
			aprint_verbose("mkdevnodes: failed to "
			    "create %s: %d\n", buf, error);
		break;
	case SAME:
		/* yeehaa */
		break;
	}
}

void
rump_vfs_builddevs(struct devsw_conv *dcvec, size_t dcvecsize)
{
	char *pnbuf = PNBUF_GET();
	devminor_t themin;
	struct devsw_conv *dc;
	size_t i;
	int v1, v2;

	rump_vfs_makeonedevnode = makeonedevnode;
	rump_vfs_makedevnodes = makedevnodes;
	rump_vfs_makesymlink = makesymlink;

	for (i = 0; i < dcvecsize; i++) {
		dc = &dcvec[i];

		switch (dc->d_class) {
		case DEVNODE_DONTBOTHER:
			break;
		case DEVNODE_SINGLE:
			if (dc->d_flags & DEVNODE_FLAG_ISMINOR0) {
				themin = dc->d_vectdim[0];
			} else {
				themin = 0;
			}
			makeonenode(pnbuf, MAXPATHLEN,
			    dc->d_bmajor, dc->d_cmajor, themin,
			    dc->d_name, -1, -1);
			break;
		case DEVNODE_VECTOR:
			for (v1 = 0; v1 < dc->d_vectdim[0]; v1++) {
				if (dc->d_vectdim[1] == 0) {
					makeonenode(pnbuf, MAXPATHLEN,
					    dc->d_bmajor, dc->d_cmajor,
					    v1, dc->d_name, v1, -1);
				} else {
					for (v2 = 0;
					    v2 < dc->d_vectdim[1]; v2++) {
						makeonenode(pnbuf, MAXPATHLEN,
						    dc->d_bmajor, dc->d_cmajor,
						    v1 * dc->d_vectdim[1] + v2,
						    dc->d_name, v1, v2);
					}
				}
			}

			/* add some extra sanity checks here */
			if (dc->d_flags & DEVNODE_FLAG_LINKZERO) {
				/*
				 * ok, so we cheat a bit since
				 * symlink isn't supported on rumpfs ...
				 */
				makeonenode(pnbuf, MAXPATHLEN,
				    -1, dc->d_cmajor, 0, dc->d_name, -1, -1);
				    
			}
			break;
		}
	}

	PNBUF_PUT(pnbuf);
}
