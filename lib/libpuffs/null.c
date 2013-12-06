/*	$NetBSD: null.c,v 1.33 2011/11/25 15:02:02 manu Exp $	*/

/*
 * Copyright (c) 2007  Antti Kantee.  All Rights Reserved.
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
#if !defined(lint)
__RCSID("$NetBSD: null.c,v 1.33 2011/11/25 15:02:02 manu Exp $");
#endif /* !lint */

/*
 * A "nullfs" using puffs, i.e. maps one location in the hierarchy
 * to another using standard system calls.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <puffs.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

PUFFSOP_PROTOS(puffs_null)

/*
 * set attributes to what is specified.  XXX: no rollback in case of failure
 */
static int
processvattr(const char *path, const struct vattr *va, int regular)
{
	struct timeval tv[2];

	/* XXX: -1 == PUFFS_VNOVAL, but shouldn't trust that */
	if (va->va_uid != (unsigned)-1 || va->va_gid != (unsigned)-1)
		if (lchown(path, va->va_uid, va->va_gid) == -1)
			return errno;

	if (va->va_mode != (unsigned)PUFFS_VNOVAL)
		if (lchmod(path, va->va_mode) == -1)
			return errno;

	/* sloppy */
	if (va->va_atime.tv_sec != (time_t)PUFFS_VNOVAL
	    || va->va_mtime.tv_sec != (time_t)PUFFS_VNOVAL) {
		TIMESPEC_TO_TIMEVAL(&tv[0], &va->va_atime);
		TIMESPEC_TO_TIMEVAL(&tv[1], &va->va_mtime);

		if (lutimes(path, tv) == -1)
			return errno;
	}

	if (regular && va->va_size != (u_quad_t)PUFFS_VNOVAL)
		if (truncate(path, (off_t)va->va_size) == -1)
			return errno;

	return 0;
}

/*
 * Kludge to open files which aren't writable *any longer*.  This kinda
 * works because the vfs layer does validation checks based on the file's
 * permissions to allow writable opening before opening them.  However,
 * the problem arises if we want to create a file, write to it (cache),
 * adjust permissions and then flush the file.
 */
static int
writeableopen(const char *path)
{
	struct stat sb;
	mode_t origmode;
	int sverr = 0;
	int fd;

	fd = open(path, O_WRONLY);
	if (fd == -1) {
		if (errno == EACCES) {
			if (stat(path, &sb) == -1)
				return -1;
			origmode = sb.st_mode & ALLPERMS;

			if (chmod(path, 0200) == -1)
				return -1;

			fd = open(path, O_WRONLY);
			if (fd == -1)
				sverr = errno;

			chmod(path, origmode);
			if (sverr)
				errno = sverr;
		} else
			return -1;
	}

	return fd;
}

/*ARGSUSED*/
static void *
inodecmp(struct puffs_usermount *pu, struct puffs_node *pn, void *arg)
{
	ino_t *cmpino = arg;

	if (pn->pn_va.va_fileid == *cmpino)
		return pn;
	return NULL;
}

static int
makenode(struct puffs_usermount *pu, struct puffs_newinfo *pni,
	const struct puffs_cn *pcn, const struct vattr *va, int regular)
{
	struct puffs_node *pn;
	struct stat sb;
	int rv;

	if ((rv = processvattr(PCNPATH(pcn), va, regular)) != 0)
		return rv;

	pn = puffs_pn_new(pu, NULL);
	if (!pn)
		return ENOMEM;
	puffs_setvattr(&pn->pn_va, va);

	if (lstat(PCNPATH(pcn), &sb) == -1)
		return errno;
	puffs_stat2vattr(&pn->pn_va, &sb);

	puffs_newinfo_setcookie(pni, pn);
	return 0;
}

/* This should be called first and overriden from the file system */
void
puffs_null_setops(struct puffs_ops *pops)
{

	PUFFSOP_SET(pops, puffs_null, fs, statvfs);
	PUFFSOP_SETFSNOP(pops, unmount);
	PUFFSOP_SETFSNOP(pops, sync);
	PUFFSOP_SET(pops, puffs_null, fs, fhtonode);
	PUFFSOP_SET(pops, puffs_null, fs, nodetofh);

	PUFFSOP_SET(pops, puffs_null, node, lookup);
	PUFFSOP_SET(pops, puffs_null, node, create);
	PUFFSOP_SET(pops, puffs_null, node, mknod);
	PUFFSOP_SET(pops, puffs_null, node, getattr);
	PUFFSOP_SET(pops, puffs_null, node, setattr);
	PUFFSOP_SET(pops, puffs_null, node, fsync);
	PUFFSOP_SET(pops, puffs_null, node, remove);
	PUFFSOP_SET(pops, puffs_null, node, link);
	PUFFSOP_SET(pops, puffs_null, node, rename);
	PUFFSOP_SET(pops, puffs_null, node, mkdir);
	PUFFSOP_SET(pops, puffs_null, node, rmdir);
	PUFFSOP_SET(pops, puffs_null, node, symlink);
	PUFFSOP_SET(pops, puffs_null, node, readlink);
	PUFFSOP_SET(pops, puffs_null, node, readdir);
	PUFFSOP_SET(pops, puffs_null, node, read);
	PUFFSOP_SET(pops, puffs_null, node, write);
	PUFFSOP_SET(pops, puffs_genfs, node, reclaim);
}

/*ARGSUSED*/
int
puffs_null_fs_statvfs(struct puffs_usermount *pu, struct statvfs *svfsb)
{

	if (statvfs(PNPATH(puffs_getroot(pu)), svfsb) == -1)
		return errno;

	return 0;
}

/*
 * XXX: this is the stupidest crap ever, but:
 * getfh() returns the fhandle type, when we are expected to deliver
 * the fid type.  Just adjust it a bit and stop whining.
 *
 * Yes, this really really needs fixing.  Yes, *REALLY*.
 */
#define FHANDLE_HEADERLEN 8
struct kernfid {
	unsigned short	fid_len;		/* length of data in bytes */
	unsigned short	fid_reserved;		/* compat: historic align */
	char		fid_data[0];		/* data (variable length) */
};

/*ARGSUSED*/
static void *
fhcmp(struct puffs_usermount *pu, struct puffs_node *pn, void *arg)
{
	struct kernfid *kf1, *kf2;

	if ((kf1 = pn->pn_data) == NULL)
		return NULL;
	kf2 = arg;

	if (kf1->fid_len != kf2->fid_len)
		return NULL;

	/*LINTED*/
	if (memcmp(kf1, kf2, kf1->fid_len) == 0)
		return pn;
	return NULL;
}

/*
 * This routine only supports file handles which have been issued while
 * the server was alive.  Not really stable ones, that is.
 */
/*ARGSUSED*/
int
puffs_null_fs_fhtonode(struct puffs_usermount *pu, void *fid, size_t fidsize,
	struct puffs_newinfo *pni)
{
	struct puffs_node *pn_res;

	pn_res = puffs_pn_nodewalk(pu, fhcmp, fid);
	if (pn_res == NULL)
		return ENOENT;

	puffs_newinfo_setcookie(pni, pn_res);
	puffs_newinfo_setvtype(pni, pn_res->pn_va.va_type);
	puffs_newinfo_setsize(pni, (voff_t)pn_res->pn_va.va_size);
	puffs_newinfo_setrdev(pni, pn_res->pn_va.va_rdev);
	return 0;
}

/*ARGSUSED*/
int
puffs_null_fs_nodetofh(struct puffs_usermount *pu, puffs_cookie_t opc,
	void *fid, size_t *fidsize)
{
	struct puffs_node *pn = opc;
	struct kernfid *kfid;
	void *bounce;
	int rv;

	rv = 0;
	bounce = NULL;
	if (*fidsize) {
		bounce = malloc(*fidsize + FHANDLE_HEADERLEN);
		if (!bounce)
			return ENOMEM;
		*fidsize += FHANDLE_HEADERLEN;
	}
	if (getfh(PNPATH(pn), bounce, fidsize) == -1)
		rv = errno;
	else
		memcpy(fid, (uint8_t *)bounce + FHANDLE_HEADERLEN,
		    *fidsize - FHANDLE_HEADERLEN);
	kfid = fid;
	if (rv == 0) {
		*fidsize = kfid->fid_len;
		pn->pn_data = malloc(*fidsize);
		if (pn->pn_data == NULL)
			abort(); /* lazy */
		memcpy(pn->pn_data, fid, *fidsize);
	} else {
		*fidsize -= FHANDLE_HEADERLEN;
	}
	free(bounce);

	return rv;
}

int
puffs_null_node_lookup(struct puffs_usermount *pu, puffs_cookie_t opc,
	struct puffs_newinfo *pni, const struct puffs_cn *pcn)
{
	struct puffs_node *pn = opc, *pn_res;
	struct stat sb;
	int rv;

	assert(pn->pn_va.va_type == VDIR);

	/*
	 * Note to whoever is copypasting this: you must first check
	 * if the node is there and only then do nodewalk.  Alternatively
	 * you could make sure that you don't return unlinked/rmdir'd
	 * nodes in some other fashion
	 */
	rv = lstat(PCNPATH(pcn), &sb);
	if (rv)
		return errno;

	/* XXX2: nodewalk is a bit too slow here */
	pn_res = puffs_pn_nodewalk(pu, inodecmp, &sb.st_ino);

	if (pn_res == NULL) {
		pn_res = puffs_pn_new(pu, NULL);
		if (pn_res == NULL)
			return ENOMEM;
		puffs_stat2vattr(&pn_res->pn_va, &sb);
	}

	puffs_newinfo_setcookie(pni, pn_res);
	puffs_newinfo_setvtype(pni, pn_res->pn_va.va_type);
	puffs_newinfo_setsize(pni, (voff_t)pn_res->pn_va.va_size);
	puffs_newinfo_setrdev(pni, pn_res->pn_va.va_rdev);

	return 0;
}

/*ARGSUSED*/
int
puffs_null_node_create(struct puffs_usermount *pu, puffs_cookie_t opc,
	struct puffs_newinfo *pni, const struct puffs_cn *pcn,
	const struct vattr *va)
{
	int fd, rv;

	fd = open(PCNPATH(pcn), O_RDWR | O_CREAT | O_TRUNC);
	if (fd == -1)
		return errno;
	close(fd);

	rv = makenode(pu, pni, pcn, va, 1);
	if (rv)
		unlink(PCNPATH(pcn));
	return rv;
}

/*ARGSUSED*/
int
puffs_null_node_mknod(struct puffs_usermount *pu, puffs_cookie_t opc,
	struct puffs_newinfo *pni, const struct puffs_cn *pcn,
	const struct vattr *va)
{
	mode_t mode;
	int rv;

	mode = puffs_addvtype2mode(va->va_mode, va->va_type);
	if (mknod(PCNPATH(pcn), mode, va->va_rdev) == -1)
		return errno;

	rv = makenode(pu, pni, pcn, va, 0);
	if (rv)
		unlink(PCNPATH(pcn));
	return rv;
}

/*ARGSUSED*/
int
puffs_null_node_getattr(struct puffs_usermount *pu, puffs_cookie_t opc,
	struct vattr *va, const struct puffs_cred *pcred)
{
	struct puffs_node *pn = opc;
	struct stat sb;

	if (lstat(PNPATH(pn), &sb) == -1)
		return errno;
	puffs_stat2vattr(va, &sb);

	return 0;
}

/*ARGSUSED*/
int
puffs_null_node_setattr(struct puffs_usermount *pu, puffs_cookie_t opc,
	const struct vattr *va, const struct puffs_cred *pcred)
{
	struct puffs_node *pn = opc;
	int rv;

	rv = processvattr(PNPATH(pn), va, pn->pn_va.va_type == VREG);
	if (rv)
		return rv;

	puffs_setvattr(&pn->pn_va, va);

	return 0;
}

/*ARGSUSED*/
int
puffs_null_node_fsync(struct puffs_usermount *pu, puffs_cookie_t opc,
	const struct puffs_cred *pcred, int how,
	off_t offlo, off_t offhi)
{
	struct puffs_node *pn = opc;
	int fd, rv;
	int fflags;
	struct stat sb;

	rv = 0;
	if (stat(PNPATH(pn), &sb) == -1)
		return errno;
	if (S_ISDIR(sb.st_mode)) {
		DIR *dirp;
		if ((dirp = opendir(PNPATH(pn))) == 0)
			return errno;
		fd = dirfd(dirp);
		if (fd == -1)
			return errno;

		if (fsync(fd) == -1)
			rv = errno;
	} else {
		fd = writeableopen(PNPATH(pn));
		if (fd == -1)
			return errno;

		if (how & PUFFS_FSYNC_DATAONLY)
			fflags = FDATASYNC;
		else
			fflags = FFILESYNC;
		if (how & PUFFS_FSYNC_CACHE)
			fflags |= FDISKSYNC;

		if (fsync_range(fd, fflags, offlo, offhi - offlo) == -1)
			rv = errno;
	}

	close(fd);

	return rv;
}

/*ARGSUSED*/
int
puffs_null_node_remove(struct puffs_usermount *pu, puffs_cookie_t opc,
	puffs_cookie_t targ, const struct puffs_cn *pcn)
{
	struct puffs_node *pn_targ = targ;

	if (unlink(PNPATH(pn_targ)) == -1)
		return errno;
	puffs_pn_remove(pn_targ);

	return 0;
}

/*ARGSUSED*/
int
puffs_null_node_link(struct puffs_usermount *pu, puffs_cookie_t opc,
	puffs_cookie_t targ, const struct puffs_cn *pcn)
{
	struct puffs_node *pn_targ = targ;

	if (link(PNPATH(pn_targ), PCNPATH(pcn)) == -1)
		return errno;

	return 0;
}

/*ARGSUSED*/
int
puffs_null_node_rename(struct puffs_usermount *pu, puffs_cookie_t opc,
	puffs_cookie_t src, const struct puffs_cn *pcn_src,
	puffs_cookie_t targ_dir, puffs_cookie_t targ,
	const struct puffs_cn *pcn_targ)
{
	struct puffs_node *pn_targ = targ;

	if (rename(PCNPATH(pcn_src), PCNPATH(pcn_targ)) == -1)
		return errno;

        if (pn_targ)
		puffs_pn_remove(pn_targ);

	return 0;
}

/*ARGSUSED*/
int
puffs_null_node_mkdir(struct puffs_usermount *pu, puffs_cookie_t opc,
	struct puffs_newinfo *pni, const struct puffs_cn *pcn,
	const struct vattr *va)
{
	int rv;

	if (mkdir(PCNPATH(pcn), va->va_mode) == -1)
		return errno;

	rv = makenode(pu, pni, pcn, va, 0);
	if (rv)
		rmdir(PCNPATH(pcn));
	return rv;
}

/*ARGSUSED*/
int
puffs_null_node_rmdir(struct puffs_usermount *pu, puffs_cookie_t opc,
	puffs_cookie_t targ, const struct puffs_cn *pcn)
{
	struct puffs_node *pn_targ = targ;

	if (rmdir(PNPATH(pn_targ)) == -1)
		return errno;
	puffs_pn_remove(pn_targ);

	return 0;
}

/*ARGSUSED*/
int
puffs_null_node_symlink(struct puffs_usermount *pu, puffs_cookie_t opc,
	struct puffs_newinfo *pni, const struct puffs_cn *pcn,
	const struct vattr *va, const char *linkname)
{
	int rv;

	if (symlink(linkname, PCNPATH(pcn)) == -1)
		return errno;

	rv = makenode(pu, pni, pcn, va, 0);
	if (rv)
		unlink(PCNPATH(pcn));
	return rv;
}

/*ARGSUSED*/
int
puffs_null_node_readlink(struct puffs_usermount *pu, puffs_cookie_t opc,
	const struct puffs_cred *pcred, char *linkname, size_t *linklen)
{
	struct puffs_node *pn = opc;
	ssize_t rv;

	rv = readlink(PNPATH(pn), linkname, *linklen);
	if (rv == -1)
		return errno;

	*linklen = rv;
	return 0;
}

/*ARGSUSED*/
int
puffs_null_node_readdir(struct puffs_usermount *pu, puffs_cookie_t opc,
	struct dirent *de, off_t *off, size_t *reslen,
	const struct puffs_cred *pcred, int *eofflag, off_t *cookies,
	size_t *ncookies)
{
	struct puffs_node *pn = opc;
	struct dirent entry, *result;
	DIR *dp;
	off_t i;
	int rv;

	*ncookies = 0;
	dp = opendir(PNPATH(pn));
	if (dp == NULL)
		return errno;

	rv = 0;
	i = *off;

	/*
	 * XXX: need to do trickery here, telldir/seekdir would be nice, but
	 * then we'd need to keep state, which I'm too lazy to keep
	 */
	while (i--) {
		rv = readdir_r(dp, &entry, &result);
		if (rv != 0)
			goto out;

		if (!result) {
			*eofflag = 1;
			goto out;
		}
	}

	for (;;) {
		rv = readdir_r(dp, &entry, &result);
		if (rv != 0)
			goto out;

		if (!result) {
			*eofflag = 1;
			goto out;
		}

		if (_DIRENT_SIZE(result) > *reslen)
			goto out;

		*de = *result;
		*reslen -= _DIRENT_SIZE(result);
		de = _DIRENT_NEXT(de);

		(*off)++;
		PUFFS_STORE_DCOOKIE(cookies, ncookies, *off);
	}

 out:
	closedir(dp);
	return 0;
}

/*ARGSUSED*/
int
puffs_null_node_read(struct puffs_usermount *pu, puffs_cookie_t opc,
	uint8_t *buf, off_t offset, size_t *buflen,
	const struct puffs_cred *pcred, int ioflag)
{
	struct puffs_node *pn = opc;
	ssize_t n;
	off_t off;
	int fd, rv;

	rv = 0;
	fd = open(PNPATH(pn), O_RDONLY);
	if (fd == -1)
		return errno;
	off = lseek(fd, offset, SEEK_SET);
	if (off == -1) {
		rv = errno;
		goto out;
	}

	n = read(fd, buf, *buflen);
	if (n == -1)
		rv = errno;
	else
		*buflen -= n;

 out:
	close(fd);
	return rv;
}

/*ARGSUSED*/
int
puffs_null_node_write(struct puffs_usermount *pu, puffs_cookie_t opc,
	uint8_t *buf, off_t offset, size_t *buflen,
	const struct puffs_cred *pcred, int ioflag)
{
	struct puffs_node *pn = opc;
	ssize_t n;
	off_t off;
	int fd, rv;

	rv = 0;
	fd = writeableopen(PNPATH(pn));
	if (fd == -1)
		return errno;

	off = lseek(fd, offset, SEEK_SET);
	if (off == -1) {
		rv = errno;
		goto out;
	}

	n = write(fd, buf, *buflen);
	if (n == -1)
		rv = errno;
	else
		*buflen -= n;

 out:
	close(fd);
	return rv;
}
