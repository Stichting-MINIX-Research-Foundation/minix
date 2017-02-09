/*	$NetBSD: p2k.c,v 1.66 2015/04/20 23:03:07 riastradh Exp $	*/

/*
 * Copyright (c) 2007, 2008, 2009  Antti Kantee.  All Rights Reserved.
 *
 * Development of this software was supported by the
 * Finnish Cultural Foundation.
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

/*
 * puffs 2k, i.e. puffs 2 kernel.  Converts the puffs protocol to
 * the kernel vfs protocol and vice versa.
 *
 * A word about reference counting: puffs in the kernel is the king of
 * reference counting.  We must maintain a vnode alive and kicking
 * until the kernel tells us to reclaim it.  Therefore we make sure
 * we never accidentally lose a vnode.  Before calling operations which
 * decrease the refcount we always bump the refcount up to compensate.
 * Come inactive, if the file system thinks that the vnode should be
 * put out of its misery, it will set the recycle flag.  We use this
 * to tell the kernel to reclaim the vnode.  Only in reclaim do we
 * really nuke the last reference.
 */

#include <sys/cdefs.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/vnode.h>
#include <sys/lock.h>
#include <sys/namei.h>
#include <sys/dirent.h>
#include <sys/hash.h>

#include <assert.h>
#include <errno.h>
#include <puffs.h>
#include <stdlib.h>
#include <stdio.h>

#include <rump/rump.h>
#include <rump/p2k.h>
#include <rump/ukfs.h>

/* NetBSD-5 compat */
#ifndef MOUNT_RUMPFS
#define MOUNT_RUMPFS    "rumpfs"
#endif

PUFFSOP_PROTOS(p2k)

LIST_HEAD(p2k_vp_hash, p2k_node);
#define NHASHBUCK (1<<16)
struct p2k_mount {
	struct vnode *p2m_rvp;
	struct puffs_usermount *p2m_pu;
	struct ukfs *p2m_ukfs;
	struct p2k_vp_hash p2m_vphash[NHASHBUCK];
	struct mount *p2m_mp;
	int p2m_nvnodes;
	int p2m_imtmpfsman;
	bool p2m_hasdebug;
};

struct p2k_node {
	struct puffs_node p2n_pn;
	struct vnode *p2n_vp;

	LIST_ENTRY(p2k_node) p2n_entries;
};

#define OPC2VP(opc) (((struct p2k_node *)opc)->p2n_vp)

static int haswizard;
static uid_t wizarduid;

static struct kauth_cred *
cred_create(const struct puffs_cred *pcr)
{
	gid_t groups[NGROUPS];
	uid_t uid;
	gid_t gid;
	short ngroups = __arraycount(groups);

	if (haswizard) {
		uid = wizarduid;
	} else {
		if (puffs_cred_getuid(pcr, &uid) == -1)
			uid = 0;
	}
	if (puffs_cred_getgid(pcr, &gid) == -1)
		gid = 0;
	puffs_cred_getgroups(pcr, groups, &ngroups);

	/* LINTED: ngroups is ok */
	return rump_pub_cred_create(uid, gid, ngroups, groups);
}

static __inline void
cred_destroy(struct kauth_cred *cred)
{

	rump_pub_cred_put(cred);
}

static struct componentname *
makecn(const struct puffs_cn *pcn)
{
	struct kauth_cred *cred;

	cred = cred_create(pcn->pcn_cred);
	/* LINTED: prehistoric types in first two args */
	return rump_pub_makecn(pcn->pcn_nameiop, pcn->pcn_flags,
	    pcn->pcn_name, pcn->pcn_namelen, cred, rump_pub_lwproc_curlwp());
}

static __inline void
freecn(struct componentname *cnp)
{

	rump_pub_freecn(cnp, RUMPCN_FREECRED);
}

static void
makelwp(struct puffs_usermount *pu)
{
	pid_t pid;
	lwpid_t lid;

	puffs_cc_getcaller(puffs_cc_getcc(pu), &pid, &lid);
	rump_pub_allbetsareoff_setid(pid, lid);
}

static volatile sig_atomic_t dodump;
static void
dumpmp(struct puffs_usermount *pu)
{
	struct statvfs svfsb;

	if (dodump && p2k_fs_statvfs(pu, &svfsb) == 0) {
		rump_pub_vfs_mount_print(svfsb.f_mntonname, dodump-1);
	}

	dodump = 0;
}

static void
sighand(int sig)
{

	if (sig == SIGINFO)
		dodump = 1;
	else if (sig == SIGUSR1)
		dodump = 2;
}

static __inline struct p2k_vp_hash *
gethash(struct p2k_mount *p2m, struct vnode *vp)
{
	uint32_t hash;

	hash = hash32_buf(&vp, sizeof(vp), HASH32_BUF_INIT);
	return &p2m->p2m_vphash[hash % NHASHBUCK];
}

/*
 * Find node based on hash of vnode pointer.  If vnode is found,
 * releases one reference to vnode based on the fact that we just
 * performed a lookup for it.
 *
 * If the optinal p2n_storage parameter is passed, it is used instead
 * of allocating more memory.  This allows for easier error recovery.
 */
static struct p2k_node *
getp2n(struct p2k_mount *p2m, struct vnode *vp, bool initial,
	struct p2k_node *p2n_storage)
{
	struct p2k_vp_hash *hl;
	struct p2k_node *p2n = NULL;

	/* p2n_storage => initial */
	assert(!p2n_storage || initial);

	hl = gethash(p2m, vp);
	if (!initial)
		LIST_FOREACH(p2n, hl, p2n_entries)
			if (p2n->p2n_vp == vp)
				break;

	hl = gethash(p2m, vp);
	if (p2n) {
		rump_pub_vp_rele(vp);
	} else {
		if (p2n_storage)
			p2n = p2n_storage;
		else
			p2n = malloc(sizeof(*p2n));
		if (!p2n) {
			rump_pub_vp_rele(vp);
			return NULL;
		}
		memset(p2n, 0, sizeof(*p2n));
		LIST_INSERT_HEAD(hl, p2n, p2n_entries);
		p2n->p2n_vp = vp;
	}
	return p2n;
}

static void
freep2n(struct p2k_node *p2n)
{

	assert(p2n->p2n_vp == NULL);
	LIST_REMOVE(p2n, p2n_entries);
	free(p2n);
}

/*ARGSUSED*/
static void
p2k_errcatcher(struct puffs_usermount *pu, uint8_t type, int error,
	const char *str, puffs_cookie_t cook)
{

	fprintf(stderr, "type %d, error %d, cookie %p (%s)\n",
	    type, error, cook, str);

	/*
	 * Trap all EINVAL responses to lookup.  It most likely means
	 * that we supplied VNON/VBAD as the type.  The real kernel
	 * doesn't panic from this either, but just handles it.
	 */
	if (type != PUFFS_VN_LOOKUP && error == EINVAL)
		abort();
}

/* just to avoid annoying loop when singlestepping */
static struct p2k_mount *
allocp2m(void)
{
	struct p2k_mount *p2m;
	int i;

	p2m = malloc(sizeof(*p2m));
	if (p2m == NULL)
		return NULL;
	memset(p2m, 0, sizeof(*p2m));

	for (i = 0; i < NHASHBUCK; i++)
		LIST_INIT(&p2m->p2m_vphash[i]);

	return p2m;
}

struct p2k_mount *
p2k_init(uint32_t puffs_flags)
{
	struct puffs_ops *pops;
	struct p2k_mount *p2m;
	char *envbuf;
	bool dodaemon;
	bool hasdebug;

	PUFFSOP_INIT(pops);

	PUFFSOP_SET(pops, p2k, fs, statvfs);
	PUFFSOP_SET(pops, p2k, fs, unmount);
	PUFFSOP_SET(pops, p2k, fs, sync);
	PUFFSOP_SET(pops, p2k, fs, fhtonode);
	PUFFSOP_SET(pops, p2k, fs, nodetofh);
	PUFFSOP_SET(pops, p2k, fs, extattrctl);

	PUFFSOP_SET(pops, p2k, node, lookup);
	PUFFSOP_SET(pops, p2k, node, create);
	PUFFSOP_SET(pops, p2k, node, mknod);
	PUFFSOP_SET(pops, p2k, node, open);
	PUFFSOP_SET(pops, p2k, node, close);
	PUFFSOP_SET(pops, p2k, node, access);
	PUFFSOP_SET(pops, p2k, node, getattr);
	PUFFSOP_SET(pops, p2k, node, setattr);
#if 0
	PUFFSOP_SET(pops, p2k, node, poll);
#endif
	PUFFSOP_SET(pops, p2k, node, mmap);
	PUFFSOP_SET(pops, p2k, node, fsync);
	PUFFSOP_SET(pops, p2k, node, seek);
	PUFFSOP_SET(pops, p2k, node, remove);
	PUFFSOP_SET(pops, p2k, node, link);
	PUFFSOP_SET(pops, p2k, node, rename);
	PUFFSOP_SET(pops, p2k, node, mkdir);
	PUFFSOP_SET(pops, p2k, node, rmdir);
	PUFFSOP_SET(pops, p2k, node, symlink);
	PUFFSOP_SET(pops, p2k, node, readdir);
	PUFFSOP_SET(pops, p2k, node, readlink);
	PUFFSOP_SET(pops, p2k, node, read);
	PUFFSOP_SET(pops, p2k, node, write);

	PUFFSOP_SET(pops, p2k, node, pathconf);

	PUFFSOP_SET(pops, p2k, node, inactive);
	PUFFSOP_SET(pops, p2k, node, reclaim);

	PUFFSOP_SET(pops, p2k, node, getextattr);
	PUFFSOP_SET(pops, p2k, node, setextattr);
	PUFFSOP_SET(pops, p2k, node, listextattr);
	PUFFSOP_SET(pops, p2k, node, deleteextattr);

	dodaemon = true;
	hasdebug = false;

	if (getenv("P2K_DEBUG") != NULL) {
		puffs_flags |= PUFFS_FLAG_OPDUMP;
		dodaemon = false;
		hasdebug = true;
	}
	if (getenv("P2K_NODETACH") != NULL) {
		dodaemon = false;
	}
	if (getenv("P2K_NOCACHE_PAGE") != NULL) {
		puffs_flags |= PUFFS_KFLAG_NOCACHE_PAGE;
	}
	if (getenv("P2K_NOCACHE_NAME") != NULL) {
		puffs_flags |= PUFFS_KFLAG_NOCACHE_NAME;
	}
	if (getenv("P2K_NOCACHE") != NULL) {
		puffs_flags |= PUFFS_KFLAG_NOCACHE;
	}
	if ((envbuf = getenv("P2K_WIZARDUID")) != NULL) {
		char *ep;

		wizarduid = strtoul(envbuf, &ep, 10);
		if (envbuf[0] == '\0' || *ep != '\0') {
			printf("P2K_WIZARDUID: invalid uid %s\n", envbuf);
		} else if (wizarduid > UID_MAX) {
			printf("P2K_WIZARDUID: uid %s out-of-range\n", envbuf);
		} else {
			haswizard = 1;
			printf("P2K WIZARD MODE: using uid %d\n", wizarduid);
		}
	}

	/*
	 * Explicitely tell that our cookies can be treated as
	 * puffs_node, since we never let libpuffs know by 
	 * calling  call puffs_pn_new()
	 */
	puffs_flags |= PUFFS_FLAG_PNCOOKIE;

	p2m = allocp2m();
	if (p2m == NULL)
		return NULL;
	p2m->p2m_pu = puffs_init(pops, PUFFS_DEFER, PUFFS_DEFER,
	    PUFFS_DEFER, puffs_flags);
	if (p2m->p2m_pu == NULL) {
		int sverrno = errno;
		free(p2m);
		errno = sverrno;
		return NULL;
	}
	p2m->p2m_hasdebug = hasdebug;

	if (dodaemon) {
		if (puffs_daemon(p2m->p2m_pu, 1, 1) == -1) {
			int sverrno = errno;
			p2k_cancel(p2m, sverrno);
			errno = sverrno;
			p2m = NULL;
		}
	}
	if (p2m)
		rump_init();

	rump_pub_lwproc_rfork(RUMP_RFCFDG);

	return p2m;
}

void
p2k_cancel(struct p2k_mount *p2m, int error)
{

	puffs_cancel(p2m->p2m_pu, error);
	free(p2m);
}

static int
setupfs(struct p2k_mount *p2m, const char *vfsname, const char *devpath,
	struct ukfs_part *part, const char *mountpath, int mntflags,
	void *arg, size_t alen)
{
	char partpath[UKFS_DEVICE_MAXPATHLEN];
	char partbuf[UKFS_DEVICE_MAXSTR];
	char typebuf[PUFFS_TYPELEN];
	struct puffs_usermount *pu = p2m->p2m_pu;
	struct p2k_node *p2n_root;
	struct ukfs *ukfs = NULL;
	extern int puffs_fakecc;
	int rv = -1, sverrno;

	strcpy(typebuf, "p2k|");
	if (strcmp(vfsname, "puffs") == 0) { /* XXX */
		struct puffs_kargs *args = arg;
		strlcat(typebuf, args->pa_typename, sizeof(typebuf));
	} else {
		strlcat(typebuf, vfsname, sizeof(typebuf));
	}

	strlcpy(partpath, devpath, sizeof(partpath));
	if (ukfs_part_tostring(part, partbuf, sizeof(partbuf))) {
		strlcat(partpath, partbuf, sizeof(partpath));
	}
	puffs_setmntinfo(pu, partpath, typebuf);

	if (ukfs_init() == -1)
		goto out;

	/* 
	 * If we're mounting rumpfs, actually do no mount and redirect
	 * requests to rump fs namespace root.  Strictly speaking, this
	 * is not correct, but I don't think anyone will notice.
	 * After all, we're mostly interested in things which reside
	 * specifically on the rootfs, namely the contents of /dev
	 */
	if (strcmp(vfsname, MOUNT_RUMPFS) == 0) {
		if ((rv = rump_pub_vfs_getmp("/", &p2m->p2m_mp)) != 0) {
			errno = rv;
			rv = -1;
			goto out;
		}
	} else {
		if (part != ukfs_part_na)
			ukfs = ukfs_mount_disk(vfsname, devpath, part,
			    mountpath, mntflags, arg, alen);
		else
			ukfs = ukfs_mount(vfsname, devpath, mountpath, mntflags,
			    arg, alen);
		if (ukfs == NULL)
			goto out;
		ukfs_setspecific(ukfs, p2m);
		p2m->p2m_ukfs = ukfs;
		p2m->p2m_mp = ukfs_getmp(ukfs);
	}
	if ((rv = rump_pub_vfs_root(p2m->p2m_mp, &p2m->p2m_rvp, 0)) != 0) {
		errno = rv;
		rv = -1;
		goto out;
	}

	p2m->p2m_pu = pu;

	/*
	 * Detect tmpfs.  XXX: this is a kludge.  See inactive().
	 *
	 * In reality we'd want "does file system use anon objects
	 * for storage?".  But since tmpfs hides the anon object from
	 * the public interface, we can't actually detect it sanely.
	 * Therefore, use this kludge.
	 */
	p2m->p2m_imtmpfsman = strcmp(vfsname, MOUNT_TMPFS) == 0;

	p2n_root = getp2n(p2m, p2m->p2m_rvp, true, NULL);
	puffs_setfhsize(pu, 0, PUFFS_FHFLAG_PASSTHROUGH);
	puffs_setstacksize(pu, PUFFS_STACKSIZE_MIN);
	puffs_fakecc = 1;
	puffs_set_prepost(pu, makelwp, NULL);

	if (p2m->p2m_hasdebug) {
		struct timespec ts;

		signal(SIGINFO, sighand);
		signal(SIGUSR1, sighand);

		ts.tv_sec = 0;
		ts.tv_nsec = 1000*1000*10; /* 10ms */
		puffs_ml_setloopfn(pu, dumpmp);
		puffs_ml_settimeout(pu, &ts);
	}
	puffs_set_errnotify(pu, p2k_errcatcher);

	puffs_setspecific(pu, p2m);
	rv = puffs_mount(pu, mountpath, mntflags, p2n_root);

 out:
	if (rv == -1) {
		sverrno = errno;
		puffs_cancel(pu, sverrno);
		if (ukfs)
			ukfs_release(p2m->p2m_ukfs, UKFS_RELFLAG_FORCE);
		free(p2m);
		errno = sverrno;
	}

	return rv;
}

int
p2k_mainloop(struct p2k_mount *p2m)
{
	int rv, sverrno;

	rv = puffs_mainloop(p2m->p2m_pu);
	sverrno = errno;
	puffs_exit(p2m->p2m_pu, 1);
	if (p2m->p2m_ukfs)
		ukfs_release(p2m->p2m_ukfs, UKFS_RELFLAG_FORCE);
	free(p2m);

	if (rv == -1)
		errno = sverrno;
	return rv;
}

int
p2k_run_fs(const char *vfsname, const char *devpath, const char *mountpath,
	int mntflags, void *arg, size_t alen, uint32_t puffs_flags)
{
	struct p2k_mount *p2m;
	int rv;

	p2m = p2k_init(puffs_flags);
	if (p2m == NULL)
		return -1;
	rv = setupfs(p2m, vfsname, devpath, ukfs_part_na, mountpath,
	    mntflags, arg, alen);
	if (rv == -1)
		return rv;
	return p2k_mainloop(p2m);
}

int
p2k_run_diskfs(const char *vfsname, const char *devpath, struct ukfs_part *part,
	const char *mountpath, int mntflags, void *arg, size_t alen,
	uint32_t puffs_flags)
{
	struct p2k_mount *p2m;
	int rv;

	p2m = p2k_init(puffs_flags);
	if (p2m == NULL)
		return -1;
	rv = setupfs(p2m, vfsname, devpath, part, mountpath, mntflags,
	    arg, alen);
	if (rv == -1)
		return rv;
	return p2k_mainloop(p2m);
}

int
p2k_setup_fs(struct p2k_mount *p2m, const char *vfsname, const char *devpath,
	const char *mountpath, int mntflags, void *arg, size_t alen)
{

	return setupfs(p2m, vfsname, devpath, ukfs_part_na, mountpath,
	    mntflags, arg, alen);
}

int
p2k_setup_diskfs(struct p2k_mount *p2m, const char *vfsname,
	const char *devpath, struct ukfs_part *part, const char *mountpath,
	int mntflags, void *arg, size_t alen)
{

	return setupfs(p2m, vfsname, devpath, part, mountpath, mntflags,
	    arg, alen);
}

int
p2k_fs_statvfs(struct puffs_usermount *pu, struct statvfs *sbp)
{
	struct p2k_mount *p2m = puffs_getspecific(pu);
	struct mount *mp = p2m->p2m_mp;

	return rump_pub_vfs_statvfs(mp, sbp);
}

/*ARGSUSED*/
int
p2k_fs_unmount(struct puffs_usermount *pu, int flags)
{
	struct p2k_mount *p2m = puffs_getspecific(pu);
	struct ukfs *fs = p2m->p2m_ukfs;
	int error = 0;

	rump_pub_vp_rele(p2m->p2m_rvp);

	if (fs) {
		if (ukfs_release(fs, 0) != 0) {
			struct statvfs svfsb;

			if (p2m->p2m_hasdebug
			    && p2k_fs_statvfs(pu, &svfsb) == 0) {
				printf("\nSOFT UNMOUNT FAILED, MP INFO DUMP\n");
				rump_pub_vfs_mount_print(svfsb.f_mntonname, 1);
			}
			ukfs_release(fs, UKFS_RELFLAG_FORCE);
			error = 0;
		}
	}
	p2m->p2m_ukfs = NULL;

	if (p2m->p2m_hasdebug) {
		printf("-- rump kernel event counters --\n");
		rump_printevcnts();
		printf("-- end of event counters --\n");
	}

	return error;
}

int
p2k_fs_sync(struct puffs_usermount *pu, int waitfor,
	const struct puffs_cred *pcr)
{
	struct p2k_mount *p2m = puffs_getspecific(pu);
	struct mount *mp = p2m->p2m_mp;
	struct kauth_cred *cred;
	int rv;

	cred = cred_create(pcr);
	rv = rump_pub_vfs_sync(mp, waitfor, cred);
	cred_destroy(cred);

	return rv;
}

/*ARGSUSED*/
int
p2k_fs_fhtonode(struct puffs_usermount *pu, void *fid, size_t fidsize,
	struct puffs_newinfo *pni)
{
	struct p2k_mount *p2m = puffs_getspecific(pu);
	struct mount *mp = p2m->p2m_mp;
	struct p2k_node *p2n;
	struct vnode *vp;
	enum rump_vtype vtype;
	voff_t vsize;
	uint64_t rdev; /* XXX: allows running this on NetBSD 5.0 */
	int rv;

	rv = rump_pub_vfs_fhtovp(mp, fid, &vp);
	if (rv)
		return rv;
	RUMP_VOP_UNLOCK(vp);

	p2n = getp2n(p2m, vp, false, NULL);
	if (p2n == NULL)
		return ENOMEM;

	puffs_newinfo_setcookie(pni, p2n);
	rump_pub_getvninfo(vp, &vtype, &vsize, (void *)&rdev);
	puffs_newinfo_setvtype(pni, (enum vtype)vtype);
	puffs_newinfo_setsize(pni, vsize);
	/* LINTED: yea, it'll lose accuracy, but that's life */
	puffs_newinfo_setrdev(pni, rdev);

	return 0;
}

/*ARGSUSED*/
int
p2k_fs_nodetofh(struct puffs_usermount *pu, puffs_cookie_t cookie, void *fid,
	size_t *fidsize)
{
	struct vnode *vp = OPC2VP(cookie);

	return rump_pub_vfs_vptofh(vp, fid, fidsize);
}

int
p2k_fs_extattrctl(struct puffs_usermount *pu, int cmd,
	puffs_cookie_t cookie, int flags,
	int attrnamespace, const char *attrname)
{
	struct p2k_mount *p2m = puffs_getspecific(pu);
	struct mount *mp = p2m->p2m_mp;
	struct vnode *vp;

	if (flags & PUFFS_EXTATTRCTL_HASNODE) {
		vp = OPC2VP(cookie);
		RUMP_VOP_LOCK(vp, LK_EXCLUSIVE | LK_RETRY);
	} else {
		vp = NULL;
	}

	/* vfsop unlocks (but doesn't release) vnode, so we're done here */
	return rump_pub_vfs_extattrctl(mp, cmd, vp, attrnamespace, attrname);
}

/*ARGSUSED*/
int
p2k_node_lookup(struct puffs_usermount *pu, puffs_cookie_t opc,
	struct puffs_newinfo *pni, const struct puffs_cn *pcn)
{
	struct p2k_mount *p2m = puffs_getspecific(pu);
	struct p2k_node *p2n_dir = opc, *p2n;
	struct componentname *cn;
	struct vnode *dvp = p2n_dir->p2n_vp, *vp;
	enum rump_vtype vtype;
	voff_t vsize;
	uint64_t rdev; /* XXX: uint64_t because of stack overwrite in compat */
	int rv;

	cn = makecn(pcn);
	RUMP_VOP_LOCK(dvp, LK_EXCLUSIVE);
	rv = RUMP_VOP_LOOKUP(dvp, &vp, cn);
	RUMP_VOP_UNLOCK(dvp);
	freecn(cn);

	if (rv) {
		if (rv == RUMP_EJUSTRETURN) {
			rv = ENOENT;
		}
		return rv;
	}

	p2n = getp2n(p2m, vp, false, NULL);
	if (p2n == NULL) {
		return ENOMEM;
	}

	puffs_newinfo_setcookie(pni, p2n);
	rump_pub_getvninfo(vp, &vtype, &vsize, (void *)&rdev);
	puffs_newinfo_setvtype(pni, (enum vtype)vtype);
	puffs_newinfo_setsize(pni, vsize);
	/* LINTED: yea, it'll lose accuracy, but that's life */
	puffs_newinfo_setrdev(pni, rdev);

	return 0;
}

#define VERS_TIMECHANGE 599000700
static int
needcompat(void)
{

	/*LINTED*/
	return __NetBSD_Version__ < VERS_TIMECHANGE
	    && rump_getversion() >= VERS_TIMECHANGE;
}

#define DOCOMPAT(va, va_compat)						\
do {									\
	if (needcompat()) {						\
		va_compat = rump_pub_vattr_init();			\
		rump_pub_vattr50_to_vattr(va, va_compat);		\
	} else {							\
		va_compat = __UNCONST(va);				\
	}								\
} while (/*CONSTCOND*/0)

#define UNDOCOMPAT(va_compat)						\
do {									\
	if (needcompat())						\
		rump_pub_vattr_free(va_compat);				\
} while (/*CONSTCOND*/0)

static int
do_makenode(struct puffs_usermount *pu, struct p2k_node *p2n_dir,
	struct puffs_newinfo *pni, const struct puffs_cn *pcn,
	const struct vattr *vap, char *link_target,
	int (*makefn)(struct vnode *, struct vnode **, struct componentname *,
		      struct vattr *),
	int (*symfn)(struct vnode *, struct vnode **, struct componentname *,
		      struct vattr *, char *))
{
	struct p2k_mount *p2m = puffs_getspecific(pu);
	struct vnode *dvp = p2n_dir->p2n_vp;
	struct p2k_node *p2n;
	struct componentname *cn;
	struct vattr *va_x;
	struct vnode *vp;
	int rv;

	p2n = malloc(sizeof(*p2n));
	if (p2n == NULL)
		return ENOMEM;
	DOCOMPAT(vap, va_x);

	cn = makecn(pcn);
	RUMP_VOP_LOCK(dvp, LK_EXCLUSIVE);
	rump_pub_vp_incref(dvp);
	if (makefn) {
		rv = makefn(dvp, &vp, cn, va_x);
	} else {
		rv = symfn(dvp, &vp, cn, va_x, link_target);
	}
	rump_pub_vp_rele(dvp);
	RUMP_VOP_UNLOCK(dvp);
	freecn(cn);

	if (rv == 0) {
		p2n = getp2n(p2m, vp, true, p2n);
		puffs_newinfo_setcookie(pni, p2n);
	} else {
		free(p2n);
	}

	UNDOCOMPAT(va_x);

	return rv;

}

/*ARGSUSED*/
int
p2k_node_create(struct puffs_usermount *pu, puffs_cookie_t opc,
	struct puffs_newinfo *pni, const struct puffs_cn *pcn,
	const struct vattr *vap)
{

	return do_makenode(pu, opc, pni, pcn, vap, NULL, RUMP_VOP_CREATE, NULL);
}

/*ARGSUSED*/
int
p2k_node_mknod(struct puffs_usermount *pu, puffs_cookie_t opc,
	struct puffs_newinfo *pni, const struct puffs_cn *pcn,
	const struct vattr *vap)
{

	return do_makenode(pu, opc, pni, pcn, vap, NULL, RUMP_VOP_MKNOD, NULL);
}

/*ARGSUSED*/
int
p2k_node_open(struct puffs_usermount *pu, puffs_cookie_t opc, int mode,
	const struct puffs_cred *pcr)
{
	struct vnode *vp = OPC2VP(opc);
	struct kauth_cred *cred;
	int rv;

	cred = cred_create(pcr);
	RUMP_VOP_LOCK(vp, LK_EXCLUSIVE);
	rv = RUMP_VOP_OPEN(vp, mode, cred);
	RUMP_VOP_UNLOCK(vp);
	cred_destroy(cred);

	return rv;
}

/*ARGSUSED*/
int
p2k_node_close(struct puffs_usermount *pu, puffs_cookie_t opc, int flags,
	const struct puffs_cred *pcr)
{
	struct vnode *vp = OPC2VP(opc);
	struct kauth_cred *cred;

	cred = cred_create(pcr);
	RUMP_VOP_LOCK(vp, LK_EXCLUSIVE);
	RUMP_VOP_CLOSE(vp, flags, cred);
	RUMP_VOP_UNLOCK(vp);
	cred_destroy(cred);

	return 0;
}

/*ARGSUSED*/
int
p2k_node_access(struct puffs_usermount *pu, puffs_cookie_t opc, int mode,
	const struct puffs_cred *pcr)
{
	struct vnode *vp = OPC2VP(opc);
	struct kauth_cred *cred;
	int rv;

	cred = cred_create(pcr);
	RUMP_VOP_LOCK(vp, LK_EXCLUSIVE);
	rv = RUMP_VOP_ACCESS(vp, mode, cred);
	RUMP_VOP_UNLOCK(vp);
	cred_destroy(cred);

	return rv;
}

/*ARGSUSED*/
int
p2k_node_getattr(struct puffs_usermount *pu, puffs_cookie_t opc,
	struct vattr *vap, const struct puffs_cred *pcr)
{
	struct vnode *vp = OPC2VP(opc);
	struct kauth_cred *cred;
	struct vattr *va_x;
	int rv;

	/* "deadfs" */
	if (!vp)
		return 0;

	if (needcompat()) {
		va_x = rump_pub_vattr_init();
	} else {
		va_x = vap;
	}

	cred = cred_create(pcr);
	RUMP_VOP_LOCK(vp, LK_EXCLUSIVE);
	rv = RUMP_VOP_GETATTR(vp, va_x, cred);
	RUMP_VOP_UNLOCK(vp);
	cred_destroy(cred);

	if (needcompat()) {
		rump_pub_vattr_to_vattr50(va_x, vap);
		rump_pub_vattr_free(va_x);
	}

	return rv;
}

/*ARGSUSED*/
int
p2k_node_setattr(struct puffs_usermount *pu, puffs_cookie_t opc,
	const struct vattr *vap, const struct puffs_cred *pcr)
{
	struct vnode *vp = OPC2VP(opc);
	struct kauth_cred *cred;
	struct vattr *va_x;
	int rv;

	/* "deadfs" */
	if (!vp)
		return 0;

	DOCOMPAT(vap, va_x);

	cred = cred_create(pcr);
	RUMP_VOP_LOCK(vp, LK_EXCLUSIVE);
	rv = RUMP_VOP_SETATTR(vp, va_x, cred);
	RUMP_VOP_UNLOCK(vp);
	cred_destroy(cred);

	UNDOCOMPAT(va_x);

	return rv;
}

/*ARGSUSED*/
int
p2k_node_fsync(struct puffs_usermount *pu, puffs_cookie_t opc,
	const struct puffs_cred *pcr, int flags, off_t offlo, off_t offhi)
{
	struct vnode *vp = OPC2VP(opc);
	struct kauth_cred *cred;
	int rv;

	/* "deadfs" */
	if (!vp)
		return 0;

	cred = cred_create(pcr);
	RUMP_VOP_LOCK(vp, LK_EXCLUSIVE);
	rv = RUMP_VOP_FSYNC(vp, cred, flags, offlo, offhi);
	RUMP_VOP_UNLOCK(vp);
	cred_destroy(cred);

	return rv;
}

/*ARGSUSED*/
int
p2k_node_mmap(struct puffs_usermount *pu, puffs_cookie_t opc, vm_prot_t flags,
	const struct puffs_cred *pcr)
{
	struct kauth_cred *cred;
	int rv;

	cred = cred_create(pcr);
	rv = RUMP_VOP_MMAP(OPC2VP(opc), flags, cred);
	cred_destroy(cred);

	return rv;
}

/*ARGSUSED*/
int
p2k_node_seek(struct puffs_usermount *pu, puffs_cookie_t opc,
	off_t oldoff, off_t newoff, const struct puffs_cred *pcr)
{
	struct vnode *vp = OPC2VP(opc);
	struct kauth_cred *cred;
	int rv;

	cred = cred_create(pcr);
	RUMP_VOP_LOCK(vp, LK_EXCLUSIVE);
	rv = RUMP_VOP_SEEK(vp, oldoff, newoff, cred);
	RUMP_VOP_UNLOCK(vp);
	cred_destroy(cred);

	return rv;
}

static int
do_nukenode(struct p2k_node *p2n_dir, struct p2k_node *p2n,
	const struct puffs_cn *pcn,
	int (*nukefn)(struct vnode *, struct vnode *, struct componentname *))
{
	struct vnode *dvp = p2n_dir->p2n_vp, *vp = p2n->p2n_vp;
	struct componentname *cn;
	int rv;

	cn = makecn(pcn);
	RUMP_VOP_LOCK(dvp, LK_EXCLUSIVE);
	rump_pub_vp_incref(dvp);
	RUMP_VOP_LOCK(vp, LK_EXCLUSIVE);
	rump_pub_vp_incref(vp);
	rv = nukefn(dvp, vp, cn);
	assert(RUMP_VOP_ISLOCKED(dvp) == 0);
	assert(RUMP_VOP_ISLOCKED(vp) == 0);
	freecn(cn);

	return rv;

}

/*ARGSUSED*/
int
p2k_node_remove(struct puffs_usermount *pu, puffs_cookie_t opc,
	puffs_cookie_t targ, const struct puffs_cn *pcn)
{

	return do_nukenode(opc, targ, pcn, RUMP_VOP_REMOVE);
}

/*ARGSUSED*/
int
p2k_node_link(struct puffs_usermount *pu, puffs_cookie_t opc,
	puffs_cookie_t targ, const struct puffs_cn *pcn)
{
	struct vnode *dvp = OPC2VP(opc);
	struct componentname *cn;
	int rv;

	cn = makecn(pcn);
	RUMP_VOP_LOCK(dvp, LK_EXCLUSIVE);
	rump_pub_vp_incref(dvp);
	rv = RUMP_VOP_LINK(dvp, OPC2VP(targ), cn);
	rump_pub_vp_rele(dvp);
	RUMP_VOP_UNLOCK(dvp);
	freecn(cn);

	return rv;
}

/*ARGSUSED*/
int
p2k_node_rename(struct puffs_usermount *pu,
	puffs_cookie_t src_dir, puffs_cookie_t src,
	const struct puffs_cn *pcn_src,
	puffs_cookie_t targ_dir, puffs_cookie_t targ,
	const struct puffs_cn *pcn_targ)
{
	struct vnode *dvp, *vp, *tdvp, *tvp = NULL;
	struct componentname *cn_src, *cn_targ;
	int rv;

	cn_src = makecn(pcn_src);
	cn_targ = makecn(pcn_targ);

	dvp = OPC2VP(src_dir);
	vp = OPC2VP(src);
	tdvp = OPC2VP(targ_dir);
	if (targ) {
		tvp = OPC2VP(targ);
	}

	rump_pub_vp_incref(dvp);
	rump_pub_vp_incref(vp);
	RUMP_VOP_LOCK(tdvp, LK_EXCLUSIVE);
	rump_pub_vp_incref(tdvp);
	if (tvp) {
		RUMP_VOP_LOCK(tvp, LK_EXCLUSIVE);
		rump_pub_vp_incref(tvp);
	}
	rv = RUMP_VOP_RENAME(dvp, vp, cn_src, tdvp, tvp, cn_targ);
	assert(RUMP_VOP_ISLOCKED(tdvp) == 0);
	if (tvp) {
		assert(RUMP_VOP_ISLOCKED(tvp) == 0);
	}
	freecn(cn_src);
	freecn(cn_targ);

	return rv;
}

/*ARGSUSED*/
int
p2k_node_mkdir(struct puffs_usermount *pu, puffs_cookie_t opc,
	struct puffs_newinfo *pni, const struct puffs_cn *pcn,
	const struct vattr *vap)
{

	return do_makenode(pu, opc, pni, pcn, vap, NULL, RUMP_VOP_MKDIR, NULL);
}

/*ARGSUSED*/
int
p2k_node_rmdir(struct puffs_usermount *pu, puffs_cookie_t opc,
	puffs_cookie_t targ, const struct puffs_cn *pcn)
{

	return do_nukenode(opc, targ, pcn, RUMP_VOP_RMDIR);
}

/*ARGSUSED*/
int
p2k_node_symlink(struct puffs_usermount *pu, puffs_cookie_t opc,
	struct puffs_newinfo *pni, const struct puffs_cn *pcn,
	const struct vattr *vap, const char *link_target)
{

	return do_makenode(pu, opc, pni, pcn, vap,
	    __UNCONST(link_target), NULL, RUMP_VOP_SYMLINK);
}

/*ARGSUSED*/
int
p2k_node_readdir(struct puffs_usermount *pu, puffs_cookie_t opc,
	struct dirent *dent, off_t *readoff, size_t *reslen,
	const struct puffs_cred *pcr, int *eofflag,
	off_t *cookies, size_t *ncookies)
{
	struct vnode *vp = OPC2VP(opc);
	struct kauth_cred *cred;
	struct uio *uio;
	off_t *vop_cookies;
	int vop_ncookies;
	int rv;

	cred = cred_create(pcr);
	uio = rump_pub_uio_setup(dent, *reslen, *readoff, RUMPUIO_READ);
	RUMP_VOP_LOCK(vp, LK_SHARED);
	if (cookies) {
		rv = RUMP_VOP_READDIR(vp, uio, cred, eofflag,
		    &vop_cookies, &vop_ncookies);
		memcpy(cookies, vop_cookies, vop_ncookies * sizeof(*cookies));
		*ncookies = vop_ncookies;
		free(vop_cookies);
	} else {
		rv = RUMP_VOP_READDIR(vp, uio, cred, eofflag, NULL, NULL);
	}
	RUMP_VOP_UNLOCK(vp);
	if (rv == 0) {
		*reslen = rump_pub_uio_getresid(uio);
		*readoff = rump_pub_uio_getoff(uio);
	}
	rump_pub_uio_free(uio);
	cred_destroy(cred);

	return rv;
}

/*ARGSUSED*/
int
p2k_node_readlink(struct puffs_usermount *pu, puffs_cookie_t opc,
	const struct puffs_cred *pcr, char *linkname, size_t *linklen)
{
	struct vnode *vp = OPC2VP(opc);
	struct kauth_cred *cred;
	struct uio *uio;
	int rv;

	cred = cred_create(pcr);
	uio = rump_pub_uio_setup(linkname, *linklen, 0, RUMPUIO_READ);
	RUMP_VOP_LOCK(vp, LK_EXCLUSIVE);
	rv = RUMP_VOP_READLINK(vp, uio, cred);
	RUMP_VOP_UNLOCK(vp);
	*linklen -= rump_pub_uio_free(uio);
	cred_destroy(cred);

	return rv;
}

/*ARGSUSED*/
int
p2k_node_read(struct puffs_usermount *pu, puffs_cookie_t opc,
	uint8_t *buf, off_t offset, size_t *resid,
	const struct puffs_cred *pcr, int ioflag)
{
	struct vnode *vp = OPC2VP(opc);
	struct kauth_cred *cred;
	struct uio *uio;
	int rv;

	cred = cred_create(pcr);
	uio = rump_pub_uio_setup(buf, *resid, offset, RUMPUIO_READ);
	RUMP_VOP_LOCK(vp, LK_SHARED);
	rv = RUMP_VOP_READ(vp, uio, ioflag, cred);
	RUMP_VOP_UNLOCK(vp);
	*resid = rump_pub_uio_free(uio);
	cred_destroy(cred);

	return rv;
}

/*ARGSUSED*/
int
p2k_node_write(struct puffs_usermount *pu, puffs_cookie_t opc,
	uint8_t *buf, off_t offset, size_t *resid,
	const struct puffs_cred *pcr, int ioflag)
{
	struct vnode *vp = OPC2VP(opc);
	struct kauth_cred *cred;
	struct uio *uio;
	int rv;

	/* "deadfs" */
	if (!vp)
		return 0;

	cred = cred_create(pcr);
	uio = rump_pub_uio_setup(buf, *resid, offset, RUMPUIO_WRITE);
	RUMP_VOP_LOCK(vp, LK_EXCLUSIVE);
	rv = RUMP_VOP_WRITE(vp, uio, ioflag, cred);
	RUMP_VOP_UNLOCK(vp);
	*resid = rump_pub_uio_free(uio);
	cred_destroy(cred);

	return rv;
}

/*ARGSUSED*/
int
p2k_node_pathconf(struct puffs_usermount *pu, puffs_cookie_t opc,
	int name, register_t *retval)
{
	struct vnode *vp = OPC2VP(opc);
	int rv;

	RUMP_VOP_LOCK(vp, LK_EXCLUSIVE);
	rv = RUMP_VOP_PATHCONF(vp, name, retval);
	RUMP_VOP_UNLOCK(vp);

	return rv;
}

/*ARGSUSED*/
int
p2k_node_getextattr(struct puffs_usermount *pu, puffs_cookie_t opc,
	int attrnamespace, const char *attrname, size_t *attrsize,
	uint8_t *attr, size_t *resid, const struct puffs_cred *pcr)
{
	struct vnode *vp = OPC2VP(opc);
	struct kauth_cred *cred;
	struct uio *uio;
	int rv;

	if (attr)
		uio = rump_pub_uio_setup(attr, *resid, 0, RUMPUIO_READ);
	else
		uio = NULL;

	cred = cred_create(pcr);
	RUMP_VOP_LOCK(vp, LK_EXCLUSIVE);
	rv = RUMP_VOP_GETEXTATTR(vp, attrnamespace, attrname, uio,
	    attrsize, cred);
	RUMP_VOP_UNLOCK(vp);
	cred_destroy(cred);

	if (uio)
		*resid = rump_pub_uio_free(uio);

	return rv;
}

/*ARGSUSED*/
int
p2k_node_setextattr(struct puffs_usermount *pu, puffs_cookie_t opc,
	int attrnamespace, const char *attrname,
	uint8_t *attr, size_t *resid, const struct puffs_cred *pcr)
{
	struct vnode *vp = OPC2VP(opc);
	struct kauth_cred *cred;
	struct uio *uio;
	int rv;

	if (attr)
		uio = rump_pub_uio_setup(attr, *resid, 0, RUMPUIO_READ);
	else
		uio = NULL;

	cred = cred_create(pcr);
	RUMP_VOP_LOCK(vp, LK_EXCLUSIVE);
	rv = RUMP_VOP_SETEXTATTR(vp, attrnamespace, attrname, uio, cred);
	RUMP_VOP_UNLOCK(vp);
	cred_destroy(cred);

	if (uio)
		*resid = rump_pub_uio_free(uio);

	return rv;
}

/*ARGSUSED*/
int
p2k_node_listextattr(struct puffs_usermount *pu, puffs_cookie_t opc,
	int attrnamespace, size_t *attrsize, uint8_t *attrs,
	size_t *resid, int flags, const struct puffs_cred *pcr)
{
	struct vnode *vp = OPC2VP(opc);
	struct kauth_cred *cred;
	struct uio *uio;
	int rv;

	if (attrs)
		uio = rump_pub_uio_setup(attrs, *resid, 0, RUMPUIO_READ);
	else
		uio = NULL;

	cred = cred_create(pcr);
	RUMP_VOP_LOCK(vp, LK_EXCLUSIVE);
	rv = RUMP_VOP_LISTEXTATTR(vp, attrnamespace, uio, attrsize,
	    flags, cred);
	RUMP_VOP_UNLOCK(vp);
	cred_destroy(cred);

	if (uio)
		*resid = rump_pub_uio_free(uio);

	return rv;
}

/*ARGSUSED*/
int
p2k_node_deleteextattr(struct puffs_usermount *pu, puffs_cookie_t opc,
	int attrnamespace, const char *attrname, const struct puffs_cred *pcr)
{
	struct vnode *vp = OPC2VP(opc);
	struct kauth_cred *cred;
	int rv;

	cred = cred_create(pcr);
	RUMP_VOP_LOCK(vp, LK_EXCLUSIVE);
	rv = RUMP_VOP_DELETEEXTATTR(vp, attrnamespace, attrname, cred);
	RUMP_VOP_UNLOCK(vp);
	cred_destroy(cred);

	return rv;
}

/* the kernel releases its last reference here */ 
int
p2k_node_inactive(struct puffs_usermount *pu, puffs_cookie_t opc)
{
	struct p2k_mount *p2m = puffs_getspecific(pu);
	struct p2k_node *p2n = opc;
	struct vnode *vp = OPC2VP(opc);
	bool recycle = false;
	int rv;

	/* deadfs */
	if (!vp)
		return 0;

	/*
	 * Flush all cached vnode pages from the rump kernel -- they
	 * are kept in puffs for all things that matter.  However,
	 * don't do this for tmpfs (vnodes are backed by an aobj), since that
	 * would cause us to clear the backing storage leaving us without
	 * a way to regain the data from "stable storage".
	 */
	if (!p2m->p2m_imtmpfsman) {
		rump_pub_vp_interlock(vp);
		RUMP_VOP_PUTPAGES(vp, 0, 0,
		    PGO_ALLPAGES|PGO_CLEANIT|PGO_FREE);
	}

	/*
	 * Ok, this is where we get nasty.  We pretend the vnode is
	 * inactive and already tell the file system that.  However,
	 * we are allowed to pretend it also grows a reference immediately
	 * after per vget(), so this does not do harm.  Cheap trick, but ...
	 *
	 * If the file system thinks the inode is done for, we release
	 * our reference and clear all knowledge of the vnode.  If,
	 * however, the inode is still active, we retain our reference
	 * until reclaim, since puffs might be flushing out some data
	 * later.
	 */
	RUMP_VOP_LOCK(vp, LK_EXCLUSIVE);
	rv = RUMP_VOP_INACTIVE(vp, &recycle);
	if (recycle) {
		puffs_setback(puffs_cc_getcc(pu), PUFFS_SETBACK_NOREF_N1);
		rump_pub_vp_rele(p2n->p2n_vp);
		p2n->p2n_vp = NULL;
	}

	return rv;
}

/*ARGSUSED*/
int
p2k_node_reclaim(struct puffs_usermount *pu, puffs_croissant_t opc)
{
	struct p2k_node *p2n = opc;

	if (p2n->p2n_vp) {
		rump_pub_vp_rele(p2n->p2n_vp);
		p2n->p2n_vp = NULL;
	}

	freep2n(p2n);
	return 0;
}
