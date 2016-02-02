/*	$NetBSD: puffs.c,v 1.120 2015/06/17 00:15:26 christos Exp $	*/

/*
 * Copyright (c) 2005, 2006, 2007  Antti Kantee.  All Rights Reserved.
 *
 * Development of this software was supported by the
 * Google Summer of Code program and the Ulla Tuominen Foundation.
 * The Google SoC project was mentored by Bill Studenmund.
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
__RCSID("$NetBSD: puffs.c,v 1.120 2015/06/17 00:15:26 christos Exp $");
#endif /* !lint */

#include <sys/param.h>
#include <sys/mount.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <mntopts.h>
#include <paths.h>
#if !defined(__minix)
#include <pthread.h>
#endif /* !defined(__minix) */
#include <puffs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "puffs_priv.h"

/* Most file systems want this for opts, so just give it to them */
const struct mntopt puffsmopts[] = {
	MOPT_STDOPTS,
	PUFFSMOPT_STD,
	MOPT_NULL,
};

#if !defined(__minix)
pthread_mutex_t pu_lock = PTHREAD_MUTEX_INITIALIZER;
#endif /* !defined(__minix) */

#define FILLOP(lower, upper)						\
do {									\
	if (pops->puffs_node_##lower)					\
		opmask[PUFFS_VN_##upper] = 1;				\
} while (/*CONSTCOND*/0)
static void
fillvnopmask(struct puffs_ops *pops, struct puffs_kargs *pa)
{
	uint8_t *opmask = pa->pa_vnopmask;

	memset(opmask, 0, sizeof(pa->pa_vnopmask));

	FILLOP(create,   CREATE);
	FILLOP(mknod,    MKNOD);
	FILLOP(open,     OPEN);
	FILLOP(close,    CLOSE);
	FILLOP(access,   ACCESS);
	FILLOP(getattr,  GETATTR);
	FILLOP(setattr,  SETATTR);
	FILLOP(poll,     POLL);
	FILLOP(mmap,     MMAP);
	FILLOP(fsync,    FSYNC);
	FILLOP(seek,     SEEK);
	FILLOP(remove,   REMOVE);
	FILLOP(link,     LINK);
	FILLOP(rename,   RENAME);
	FILLOP(mkdir,    MKDIR);
	FILLOP(rmdir,    RMDIR);
	FILLOP(symlink,  SYMLINK);
	FILLOP(readdir,  READDIR);
	FILLOP(readlink, READLINK);
	FILLOP(reclaim,  RECLAIM);
	FILLOP(inactive, INACTIVE);
	FILLOP(print,    PRINT);
	FILLOP(read,     READ);
	FILLOP(write,    WRITE);
	FILLOP(advlock,  ADVLOCK);
	FILLOP(abortop,  ABORTOP);
	FILLOP(pathconf, PATHCONF);

	FILLOP(getextattr,  GETEXTATTR);
	FILLOP(setextattr,  SETEXTATTR);
	FILLOP(listextattr, LISTEXTATTR);
	FILLOP(deleteextattr, DELETEEXTATTR);
	FILLOP(fallocate, FALLOCATE);
	FILLOP(fdiscard, FDISCARD);
}
#undef FILLOP

/*
 * Go over all framev entries and write everything we can.  This is
 * mostly for the benefit of delivering "unmount" to the kernel.
 */
static void
finalpush(struct puffs_usermount *pu)
{
#if !defined(__minix)
	struct puffs_fctrl_io *fio;

	LIST_FOREACH(fio, &pu->pu_ios, fio_entries) {
		if (fio->stat & FIO_WRGONE)
			continue;

		puffs__framev_output(pu, fio->fctrl, fio);
	}
#endif /* !defined(__minix) */
}

/*ARGSUSED*/
void
puffs_kernerr_abort(struct puffs_usermount *pu, uint8_t type,
	int error, const char *str, puffs_cookie_t cookie)
{

#if !defined(__minix)
	warnx("abort: type %d, error %d, cookie %p (%s)",
#else
	lpuffs_debug("abort: type %d, error %d, cookie %p (%s)\n",
#endif /* !defined(__minix) */
	    type, error, cookie, str);
	abort();
}

/*ARGSUSED*/
void
puffs_kernerr_log(struct puffs_usermount *pu, uint8_t type,
	int error, const char *str, puffs_cookie_t cookie)
{

	syslog(LOG_WARNING, "kernel: type %d, error %d, cookie %p (%s)",
	    type, error, cookie, str);
}

#if !defined(__minix)
int
puffs_getselectable(struct puffs_usermount *pu)
{

	return pu->pu_fd;
}

uint64_t
puffs__nextreq(struct puffs_usermount *pu)
{
	uint64_t rv;

	PU_LOCK();
	rv = pu->pu_nextreq++ | (uint64_t)1<<63;
	PU_UNLOCK();

	return rv;
}

int
puffs_setblockingmode(struct puffs_usermount *pu, int mode)
{
	int rv, x;

	assert(puffs_getstate(pu) == PUFFS_STATE_RUNNING);

	if (mode != PUFFSDEV_BLOCK && mode != PUFFSDEV_NONBLOCK) {
		errno = EINVAL;
		return -1;
	}

	x = mode;
	rv = ioctl(pu->pu_fd, FIONBIO, &x);

	if (rv == 0) {
		if (mode == PUFFSDEV_BLOCK)
			pu->pu_state &= ~PU_ASYNCFD;
		else
			pu->pu_state |= PU_ASYNCFD;
	}

	return rv;
}
#endif /* !defined(__minix) */

int
puffs_getstate(struct puffs_usermount *pu)
{

	return pu->pu_state & PU_STATEMASK;
}

void
puffs_setstacksize(struct puffs_usermount *pu, size_t ss)
{
	long psize, minsize;
	int stackshift;
	int bonus;

	assert(puffs_getstate(pu) == PUFFS_STATE_BEFOREMOUNT);

	psize = sysconf(_SC_PAGESIZE);
	minsize = 4*psize;
	if (ss < (size_t)minsize || ss == PUFFS_STACKSIZE_MIN) {
		if (ss != PUFFS_STACKSIZE_MIN)
#if !defined(__minix)
			warnx("%s: adjusting " "stacksize to minimum %ld",
			    __func__, minsize);
#endif /* !defined(__minix) */
		ss = 4*psize;
	}
 
	stackshift = -1;
	bonus = 0;
	while (ss) {
		if (ss & 0x1)
			bonus++;
		ss >>= 1;
		stackshift++;
	}
	if (bonus > 1) {
		stackshift++;
#if !defined(__minix)
		warnx("%s: using next power of two: %d", __func__,
		    1 << stackshift);
#endif /* !defined(__minix) */
	}

	pu->pu_cc_stackshift = stackshift;
}

struct puffs_pathobj *
puffs_getrootpathobj(struct puffs_usermount *pu)
{
	struct puffs_node *pnr;

	pnr = pu->pu_pn_root;
	if (pnr == NULL) {
		errno = ENOENT;
		return NULL;
	}

	return &pnr->pn_po;
}

void
puffs_setroot(struct puffs_usermount *pu, struct puffs_node *pn)
{

	pu->pu_pn_root = pn;
}

struct puffs_node *
puffs_getroot(struct puffs_usermount *pu)
{

	return pu->pu_pn_root;
}

void
puffs_setrootinfo(struct puffs_usermount *pu, enum vtype vt,
	vsize_t vsize, dev_t rdev)
{
	struct puffs_kargs *pargs = pu->pu_kargp;

	if (puffs_getstate(pu) != PUFFS_STATE_BEFOREMOUNT) {
		warnx("%s: call has effect only before mount", __func__);
		return;
	}

	pargs->pa_root_vtype = vt;
	pargs->pa_root_vsize = vsize;
	pargs->pa_root_rdev = rdev;
}

void *
puffs_getspecific(struct puffs_usermount *pu)
{

	return pu->pu_privdata;
}

void
puffs_setspecific(struct puffs_usermount *pu, void *privdata)
{

	pu->pu_privdata = privdata;
}

void
puffs_setmntinfo(struct puffs_usermount *pu,
	const char *mntfromname, const char *puffsname)
{
	struct puffs_kargs *pargs = pu->pu_kargp;

	(void)strlcpy(pargs->pa_mntfromname, mntfromname,
	    sizeof(pargs->pa_mntfromname));
	(void)strlcpy(pargs->pa_typename, puffsname,
	    sizeof(pargs->pa_typename));
}

size_t
puffs_getmaxreqlen(struct puffs_usermount *pu)
{

	return pu->pu_maxreqlen;
}

void
puffs_setmaxreqlen(struct puffs_usermount *pu, size_t reqlen)
{

	if (puffs_getstate(pu) != PUFFS_STATE_BEFOREMOUNT)
		warnx("%s: call has effect only before mount", __func__);

	pu->pu_kargp->pa_maxmsglen = reqlen;
}

void
puffs_setfhsize(struct puffs_usermount *pu, size_t fhsize, int flags)
{

	if (puffs_getstate(pu) != PUFFS_STATE_BEFOREMOUNT)
		warnx("%s: call has effect only before mount", __func__);

	pu->pu_kargp->pa_fhsize = fhsize;
	pu->pu_kargp->pa_fhflags = flags;
}

void
puffs_setncookiehash(struct puffs_usermount *pu, int nhash)
{

	if (puffs_getstate(pu) != PUFFS_STATE_BEFOREMOUNT)
		warnx("%s: call has effect only before mount", __func__);

	pu->pu_kargp->pa_nhashbuckets = nhash;
}

void
puffs_set_pathbuild(struct puffs_usermount *pu, pu_pathbuild_fn fn)
{

	pu->pu_pathbuild = fn;
}

void
puffs_set_pathtransform(struct puffs_usermount *pu, pu_pathtransform_fn fn)
{

	pu->pu_pathtransform = fn;
}

void
puffs_set_pathcmp(struct puffs_usermount *pu, pu_pathcmp_fn fn)
{

	pu->pu_pathcmp = fn;
}

void
puffs_set_pathfree(struct puffs_usermount *pu, pu_pathfree_fn fn)
{

	pu->pu_pathfree = fn;
}

void
puffs_set_namemod(struct puffs_usermount *pu, pu_namemod_fn fn)
{

	pu->pu_namemod = fn;
}

void
puffs_set_errnotify(struct puffs_usermount *pu, pu_errnotify_fn fn)
{

	pu->pu_errnotify = fn;
}

void
puffs_set_cmap(struct puffs_usermount *pu, pu_cmap_fn fn)
{

	pu->pu_cmap = fn;
}

void
puffs_ml_setloopfn(struct puffs_usermount *pu, puffs_ml_loop_fn lfn)
{

	pu->pu_ml_lfn = lfn;
}

void
puffs_ml_settimeout(struct puffs_usermount *pu, struct timespec *ts)
{

	if (ts == NULL) {
		pu->pu_ml_timep = NULL;
	} else {
		pu->pu_ml_timeout = *ts;
		pu->pu_ml_timep = &pu->pu_ml_timeout;
	}
}

void
puffs_set_prepost(struct puffs_usermount *pu,
	pu_prepost_fn pre, pu_prepost_fn pst)
{

	pu->pu_oppre = pre;
	pu->pu_oppost = pst;
}

#if !defined(__minix)
void
puffs_setback(struct puffs_cc *pcc, int whatback)
{
	struct puffs_req *preq = puffs__framebuf_getdataptr(pcc->pcc_pb);

	assert(PUFFSOP_OPCLASS(preq->preq_opclass) == PUFFSOP_VN && (
	    preq->preq_optype == PUFFS_VN_OPEN ||
	    preq->preq_optype == PUFFS_VN_MMAP ||
	    preq->preq_optype == PUFFS_VN_REMOVE ||
	    preq->preq_optype == PUFFS_VN_RMDIR ||
	    preq->preq_optype == PUFFS_VN_INACTIVE));

	preq->preq_setbacks |= whatback & PUFFS_SETBACK_MASK;
}

int
puffs_daemon(struct puffs_usermount *pu, int nochdir, int noclose)
{
	long int n;
	int parent, value, fd;

	if (pipe(pu->pu_dpipe) == -1)
		return -1;

	switch (fork()) {
	case -1:
		return -1;
	case 0:
		parent = 0;
		break;
	default:
		parent = 1;
		break;
	}
	pu->pu_state |= PU_PUFFSDAEMON;

	if (parent) {
		close(pu->pu_dpipe[1]);
		n = read(pu->pu_dpipe[0], &value, sizeof(int));
		if (n == -1)
			err(1, "puffs_daemon");
		if (n != sizeof(value))
			errx(1, "puffs_daemon got %ld bytes", n);
		if (value) {
			errno = value;
			err(1, "puffs_daemon");
		}
		exit(0);
	} else {
		if (setsid() == -1)
			goto fail;

		if (!nochdir)
			chdir("/");

		if (!noclose) {
			fd = open(_PATH_DEVNULL, O_RDWR, 0);
			if (fd == -1)
				goto fail;
			dup2(fd, STDIN_FILENO);
			dup2(fd, STDOUT_FILENO);
			dup2(fd, STDERR_FILENO);
			if (fd > STDERR_FILENO)
				close(fd);
		}
		return 0;
	}

 fail:
	n = write(pu->pu_dpipe[1], &errno, sizeof(int));
	assert(n == 4);
	return -1;
}
#endif /* !defined(__minix) */

static void
shutdaemon(struct puffs_usermount *pu, int error)
{
#if !defined(__minix)
	ssize_t n;

	n = write(pu->pu_dpipe[1], &error, sizeof(int));
	assert(n == 4);
	close(pu->pu_dpipe[0]);
	close(pu->pu_dpipe[1]);
#endif /* !defined(__minix) */
	pu->pu_state &= ~PU_PUFFSDAEMON;
}

int
puffs_mount(struct puffs_usermount *pu, const char *dir, int mntflags,
	puffs_cookie_t cookie)
{
#if !defined(__minix)
	int rv, fd, sverrno;
	char *comfd;
#endif /* !defined(__minix) */

	pu->pu_kargp->pa_root_cookie = cookie;

#if !defined(__minix)
	/* XXXkludgehere */
	/* kauth doesn't provide this service any longer */
	if (geteuid() != 0)
		mntflags |= MNT_NOSUID | MNT_NODEV;

	/*
	 * Undocumented...  Well, documented only here.
	 *
	 * This is used for imaginative purposes.  If the env variable is
	 * set, puffs_mount() doesn't do the regular mount procedure.
	 * Rather, it crams the mount data down the comfd and sets comfd as
	 * the puffs descriptor.
	 *
	 * This shouldn't be used unless you can read my mind ( ... or write
	 * it, not to mention execute it, but that's starting to get silly).
	 */
	if ((comfd = getenv("PUFFS_COMFD")) != NULL) {
		size_t len;

		if (sscanf(comfd, "%d", &pu->pu_fd) != 1) {
			errno = EINVAL;
			rv = -1;
			goto out;
		}
		/* check that what we got at least resembles an fd */
		if (fcntl(pu->pu_fd, F_GETFL) == -1) {
			rv = -1;
			goto out;
		}

#define allwrite(buf, len)						\
do {									\
	ssize_t al_rv;							\
	al_rv = write(pu->pu_fd, buf, len);				\
	if ((size_t)al_rv != len) {					\
		if (al_rv != -1)					\
			errno = EIO;					\
		rv = -1;						\
		goto out;						\
	}								\
} while (/*CONSTCOND*/0)
		len = strlen(dir)+1;
		allwrite(&len, sizeof(len));
		allwrite(dir, len);
		len = strlen(pu->pu_kargp->pa_mntfromname)+1;
		allwrite(&len, sizeof(len));
		allwrite(pu->pu_kargp->pa_mntfromname, len);
		allwrite(&mntflags, sizeof(mntflags));
		len = sizeof(*pu->pu_kargp);
		allwrite(&len, sizeof(len));
		allwrite(pu->pu_kargp, sizeof(*pu->pu_kargp));
		allwrite(&pu->pu_flags, sizeof(pu->pu_flags));
#undef allwrite

		rv = 0;
	} else {
		char rp[MAXPATHLEN];
		size_t rplen,dirlen;

		if (realpath(dir, rp) == NULL) {
			rv = -1;
			goto out;
		}

		rplen = strlen(rp);
		dirlen = strlen(dir);
		if (strncmp(dir, rp, rplen) != 0 ||
		    strspn(dir + rplen, "/") != dirlen - rplen) {
			warnx("puffs_mount: \"%s\" is a relative path.", dir);
			warnx("puffs_mount: using \"%s\" instead.", rp);
		}

		fd = open(_PATH_PUFFS, O_RDWR);
		if (fd == -1) {
			warnx("puffs_mount: cannot open %s", _PATH_PUFFS);
			rv = -1;
			goto out;
		}
		if (fd <= 2)
			warnx("puffs_mount: device fd %d (<= 2), sure this is "
			    "what you want?", fd);

		pu->pu_kargp->pa_fd = pu->pu_fd = fd;
		if ((rv = mount(MOUNT_PUFFS, rp, mntflags,
		    pu->pu_kargp, sizeof(struct puffs_kargs))) == -1)
			goto out;
	}
#else
	/* Process the already-received mount request. */
	if (!lpuffs_pump()) {
		/* Not mounted?  This should never happen.. */
		free(pu->pu_kargp);
		pu->pu_kargp = NULL;
		errno = EINVAL;
		return -1;
	}
#endif /* !defined(__minix) */

	PU_SETSTATE(pu, PUFFS_STATE_RUNNING);

#if !defined(__minix)
 out:
	if (rv != 0)
		sverrno = errno;
	else
		sverrno = 0;
	free(pu->pu_kargp);
	pu->pu_kargp = NULL;

	if (pu->pu_state & PU_PUFFSDAEMON)
		shutdaemon(pu, sverrno);

	errno = sverrno;
	return rv;
#else
	return 0;
#endif /* !defined(__minix) */
}

struct puffs_usermount *
puffs_init(struct puffs_ops *pops, const char *mntfromname,
	const char *puffsname, void *priv, uint32_t pflags)
{
	struct puffs_usermount *pu;
	struct puffs_kargs *pargs;
	int sverrno;

	if (puffsname == PUFFS_DEFER)
		puffsname = "n/a";
	if (mntfromname == PUFFS_DEFER)
		mntfromname = "n/a";
	if (priv == PUFFS_DEFER)
		priv = NULL;

	pu = malloc(sizeof(struct puffs_usermount));
	if (pu == NULL)
		goto failfree;
	memset(pu, 0, sizeof(struct puffs_usermount));

	pargs = pu->pu_kargp = malloc(sizeof(struct puffs_kargs));
	if (pargs == NULL)
		goto failfree;
	memset(pargs, 0, sizeof(struct puffs_kargs));

	pargs->pa_vers = PUFFSVERSION;
	pargs->pa_flags = PUFFS_FLAG_KERN(pflags);
	fillvnopmask(pops, pargs);
	puffs_setmntinfo(pu, mntfromname, puffsname);

	puffs_zerostatvfs(&pargs->pa_svfsb);
	pargs->pa_root_cookie = NULL;
	pargs->pa_root_vtype = VDIR;
	pargs->pa_root_vsize = 0;
	pargs->pa_root_rdev = 0;
	pargs->pa_maxmsglen = 0;
	if (/*CONSTCOND*/ sizeof(time_t) == 4)
		pargs->pa_time32 = 1;
	else
		pargs->pa_time32 = 0;

	pu->pu_flags = pflags;
	pu->pu_ops = *pops;
	free(pops); /* XXX */

	pu->pu_privdata = priv;
	pu->pu_cc_stackshift = PUFFS_CC_STACKSHIFT_DEFAULT;
	LIST_INIT(&pu->pu_pnodelst);
	LIST_INIT(&pu->pu_ios);
	LIST_INIT(&pu->pu_ios_rmlist);
	LIST_INIT(&pu->pu_ccmagazin);
	TAILQ_INIT(&pu->pu_sched);

#if !defined(__minix)
	pu->pu_framectrl[PU_FRAMECTRL_FS].rfb = puffs__fsframe_read;
	pu->pu_framectrl[PU_FRAMECTRL_FS].wfb = puffs__fsframe_write;
	pu->pu_framectrl[PU_FRAMECTRL_FS].cmpfb = puffs__fsframe_cmp;
	pu->pu_framectrl[PU_FRAMECTRL_FS].gotfb = puffs__fsframe_gotframe;
	pu->pu_framectrl[PU_FRAMECTRL_FS].fdnotfn = puffs_framev_unmountonclose;
#endif /* !defined(__minix) */

	/* defaults for some user-settable translation functions */
	pu->pu_cmap = NULL; /* identity translation */

	pu->pu_pathbuild = puffs_stdpath_buildpath;
	pu->pu_pathfree = puffs_stdpath_freepath;
	pu->pu_pathcmp = puffs_stdpath_cmppath;
	pu->pu_pathtransform = NULL;
	pu->pu_namemod = NULL;

	pu->pu_errnotify = puffs_kernerr_log;

	PU_SETSTATE(pu, PUFFS_STATE_BEFOREMOUNT);

#if defined(__minix)
	/* Do the MINIX3-specific side of the initialization. */
	lpuffs_init(pu);
#endif /* defined(__minix) */

	return pu;

 failfree:
	/* can't unmount() from here for obvious reasons */
	sverrno = errno;
	free(pu);
	errno = sverrno;
	return NULL;
}

void
puffs_cancel(struct puffs_usermount *pu, int error)
{

	assert(puffs_getstate(pu) < PUFFS_STATE_RUNNING);
	shutdaemon(pu, error);
	free(pu);
}

/*ARGSUSED1*/
int
puffs_exit(struct puffs_usermount *pu, int unused /* strict compat */)
{
#if !defined(__minix)
	struct puffs_framebuf *pb;
	struct puffs_req *preq;
	void *winp;
	size_t winlen;
	int sverrno;

	pb = puffs_framebuf_make();
	if (pb == NULL) {
		errno = ENOMEM;
		return -1;
	}

	winlen = sizeof(struct puffs_req);
	if (puffs_framebuf_getwindow(pb, 0, &winp, &winlen) == -1) {
		sverrno = errno;
		puffs_framebuf_destroy(pb);
		errno = sverrno;
		return -1;
	}
	preq = winp;

	preq->preq_buflen = sizeof(struct puffs_req);
	preq->preq_opclass = PUFFSOP_UNMOUNT;
	preq->preq_id = puffs__nextreq(pu);

	puffs_framev_enqueue_justsend(pu, puffs_getselectable(pu), pb, 1, 0);
#else
	struct puffs_node *pn;

	lpuffs_debug("puffs_exit\n");

	while ((pn = LIST_FIRST(&pu->pu_pnodelst)) != NULL)
		puffs_pn_put(pn);

	while ((pn = LIST_FIRST(&pu->pu_pnode_removed_lst)) != NULL)
		puffs_pn_put(pn);

	puffs__cc_exit(pu);
	if (pu->pu_state & PU_HASKQ)
		close(pu->pu_kq);
	free(pu);
#endif /* !defined(__minix) */

	return 0;
}

#if !defined(__minix)
/* no sigset_t static intializer */
static int sigs[NSIG] = { 0, };
static int sigcatch = 0;

int
puffs_unmountonsignal(int sig, bool sigignore)
{

	if (sig < 0 || sig >= (int)NSIG) {
		errno = EINVAL;
		return -1;
	}
	if (sigignore)
		if (signal(sig, SIG_IGN) == SIG_ERR)
			return -1;

	if (!sigs[sig])
		sigcatch++;
	sigs[sig] = 1;

	return 0;
}
#endif /* !defined(__minix) */

/*
 * Actual mainloop.  This is called from a context which can block.
 * It is called either from puffs_mainloop (indirectly, via
 * puffs_cc_continue() or from puffs_cc_yield()).
 */
void
puffs__theloop(struct puffs_cc *pcc)
{
	struct puffs_usermount *pu = pcc->pcc_pu;
#if !defined(__minix)
	struct puffs_framectrl *pfctrl;
	struct puffs_fctrl_io *fio;
	struct kevent *curev;
	size_t nchanges;
	int ndone;
#endif /* !defined(__minix) */

#if !defined(__minix)
	while (puffs_getstate(pu) != PUFFS_STATE_UNMOUNTED) {
#else
	do {
#endif /* !defined(__minix) */

		/*
		 * Schedule existing requests.
		 */
		while ((pcc = TAILQ_FIRST(&pu->pu_sched)) != NULL) {
			TAILQ_REMOVE(&pu->pu_sched, pcc, pcc_schedent);
			puffs__goto(pcc);
		}

		if (pu->pu_ml_lfn)
			pu->pu_ml_lfn(pu);

#if !defined(__minix)
		/* XXX: can we still do these optimizations? */
#if 0
		/*
		 * Do this here, because:
		 *  a) loopfunc might generate some results
		 *  b) it's still "after" event handling (except for round 1)
		 */
		if (puffs_req_putput(ppr) == -1)
			goto out;
		puffs_req_resetput(ppr);

		/* micro optimization: skip kevent syscall if possible */
		if (pu->pu_nfds == 1 && pu->pu_ml_timep == NULL
		    && (pu->pu_state & PU_ASYNCFD) == 0) {
			pfctrl = XXX->fctrl;
			puffs_framev_input(pu, pfctrl, XXX);
			continue;
		}
#endif

		/* else: do full processing */
		/* Don't bother worrying about O(n) for now */
		LIST_FOREACH(fio, &pu->pu_ios, fio_entries) {
			if (fio->stat & FIO_WRGONE)
				continue;

			pfctrl = fio->fctrl;

			/*
			 * Try to write out everything to avoid the
			 * need for enabling EVFILT_WRITE.  The likely
			 * case is that we can fit everything into the
			 * socket buffer.
			 */
			puffs__framev_output(pu, pfctrl, fio);
		}

		/*
		 * Build list of which to enable/disable in writecheck.
		 */
		nchanges = 0;
		LIST_FOREACH(fio, &pu->pu_ios, fio_entries) {
			if (fio->stat & FIO_WRGONE)
				continue;

			/* en/disable write checks for kqueue as needed */
			assert((FIO_EN_WRITE(fio) && FIO_RM_WRITE(fio)) == 0);
			if (FIO_EN_WRITE(fio)) {
				EV_SET(&pu->pu_evs[nchanges], fio->io_fd,
				    EVFILT_WRITE, EV_ENABLE, 0, 0,
				    (uintptr_t)fio);
				fio->stat |= FIO_WR;
				nchanges++;
			}
			if (FIO_RM_WRITE(fio)) {
				EV_SET(&pu->pu_evs[nchanges], fio->io_fd,
				    EVFILT_WRITE, EV_DISABLE, 0, 0,
				    (uintptr_t)fio);
				fio->stat &= ~FIO_WR;
				nchanges++;
			}
		}

		ndone = kevent(pu->pu_kq, pu->pu_evs, nchanges,
		    pu->pu_evs, pu->pu_nevs, pu->pu_ml_timep);

		if (ndone == -1) {
			if (errno != EINTR)
				break;
			else
				continue;
		}

		/* uoptimize */
		if (ndone == 0)
			continue;

		/* iterate over the results */
		for (curev = pu->pu_evs; ndone--; curev++) {
			int what;

#if 0
			/* get & possibly dispatch events from kernel */
			if (curev->ident == puffsfd) {
				if (puffs_req_handle(pgr, ppr, 0) == -1)
					goto out;
				continue;
			}
#endif

			fio = (void *)curev->udata;
			if (__predict_true(fio))
				pfctrl = fio->fctrl;
			else
				pfctrl = NULL;
			if (curev->flags & EV_ERROR) {
				assert(curev->filter == EVFILT_WRITE);
				fio->stat &= ~FIO_WR;

				/* XXX: how to know if it's a transient error */
				puffs__framev_writeclose(pu, fio,
				    (int)curev->data);
				puffs__framev_notify(fio, PUFFS_FBIO_ERROR);
				continue;
			}

			what = 0;
			if (curev->filter == EVFILT_READ) {
				puffs__framev_input(pu, pfctrl, fio);
				what |= PUFFS_FBIO_READ;
			}

			else if (curev->filter == EVFILT_WRITE) {
				puffs__framev_output(pu, pfctrl, fio);
				what |= PUFFS_FBIO_WRITE;
			}

			else if (__predict_false(curev->filter==EVFILT_SIGNAL)){
				if ((pu->pu_state & PU_DONEXIT) == 0) {
					PU_SETSFLAG(pu, PU_DONEXIT);
					puffs_exit(pu, 0);
				}
			}
			if (what)
				puffs__framev_notify(fio, what);
		}

		/*
		 * Really free fd's now that we don't have references
		 * to them.
		 */
		while ((fio = LIST_FIRST(&pu->pu_ios_rmlist)) != NULL) {
			LIST_REMOVE(fio, fio_entries);
			free(fio);
		}
#endif /* !defined(__minix) */
	}
#if defined(__minix)
		while (lpuffs_pump());
#endif /* defined(__minix) */

	if (puffs__cc_restoremain(pu) == -1)
		warn("cannot restore main context.  impending doom");
}
int
puffs_mainloop(struct puffs_usermount *pu)
{
#if !defined(__minix)
	struct puffs_fctrl_io *fio;
#endif /* !defined(__minix) */
	struct puffs_cc *pcc;
#if !defined(__minix)
	struct kevent *curev;
	size_t nevs;
	int sverrno, i;
#else
	int sverrno;
#endif /* !defined(__minix) */

	assert(puffs_getstate(pu) >= PUFFS_STATE_RUNNING);

#if !defined(__minix)
	pu->pu_kq = kqueue();
	if (pu->pu_kq == -1)
		goto out;
#endif /* !defined(__minix) */
	pu->pu_state |= PU_HASKQ;

#if !defined(__minix)
	puffs_setblockingmode(pu, PUFFSDEV_NONBLOCK);
	if (puffs__framev_addfd_ctrl(pu, puffs_getselectable(pu),
	    PUFFS_FBIO_READ | PUFFS_FBIO_WRITE,
	    &pu->pu_framectrl[PU_FRAMECTRL_FS]) == -1)
		goto out;

	nevs = pu->pu_nevs + sigcatch;
	curev = realloc(pu->pu_evs, nevs * sizeof(struct kevent));
	if (curev == NULL)
		goto out;
	pu->pu_evs = curev;
	pu->pu_nevs = nevs;

	LIST_FOREACH(fio, &pu->pu_ios, fio_entries) {
		EV_SET(curev, fio->io_fd, EVFILT_READ, EV_ADD,
		    0, 0, (uintptr_t)fio);
		curev++;
		EV_SET(curev, fio->io_fd, EVFILT_WRITE, EV_ADD | EV_DISABLE,
		    0, 0, (uintptr_t)fio);
		curev++;
	}
	for (i = 0; i < NSIG; i++) {
		if (sigs[i]) {
			EV_SET(curev, i, EVFILT_SIGNAL, EV_ADD | EV_ENABLE,
			    0, 0, 0);
			curev++;
		}
	}
	assert(curev - pu->pu_evs == (ssize_t)pu->pu_nevs);
	if (kevent(pu->pu_kq, pu->pu_evs, pu->pu_nevs, NULL, 0, NULL) == -1)
		goto out;
#endif /* !defined(__minix) */

	pu->pu_state |= PU_INLOOP;

	/*
	 * Create alternate execution context and jump to it.  Note
	 * that we come "out" of savemain twice.  Where we come out
	 * of it depends on the architecture.  If the return address is
	 * stored on the stack, we jump out from puffs_cc_continue(),
	 * for a register return address from puffs__cc_savemain().
	 * PU_MAINRESTORE makes sure we DTRT in both cases.
	 */
	if (puffs__cc_create(pu, puffs__theloop, &pcc) == -1) {
		goto out;
	}

#if 0
	if (puffs__cc_savemain(pu) == -1) {
		goto out;
	}
#else
	/*
	 * XXX
	 * puffs__cc_savemain() uses getcontext() and then returns.
	 * the caller (this function) may overwrite the stack frame
	 * of puffs__cc_savemain(), so when we call setcontext() later and
	 * return from puffs__cc_savemain() again, the return address or
	 * saved stack pointer can be garbage.
	 * avoid this by calling getcontext() directly here.
	 */
	extern int puffs_fakecc;
	if (!puffs_fakecc) {
		PU_CLRSFLAG(pu, PU_MAINRESTORE);
		if (getcontext(&pu->pu_mainctx) == -1) {
			goto out;
		}
	}
#endif

	if ((pu->pu_state & PU_MAINRESTORE) == 0)
		puffs_cc_continue(pcc);

	finalpush(pu);
	errno = 0;

 out:
	/* store the real error for a while */
	sverrno = errno;

	errno = sverrno;
	if (errno)
		return -1;
	else
		return 0;
}
