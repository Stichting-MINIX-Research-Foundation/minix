/*	$NetBSD: sysctlfs.c,v 1.17 2012/11/04 22:47:21 christos Exp $	*/

/*-
 * Copyright (c) 2006, 2007  Antti Kantee.  All Rights Reserved.
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
 * sysctlfs: mount sysctls as a file system tree.  Supports query and
 * modify of nodes in the sysctl namespace in addition to namespace
 * traversal.
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: sysctlfs.c,v 1.17 2012/11/04 22:47:21 christos Exp $");
#endif /* !lint */

#include <sys/types.h>
#include <sys/sysctl.h>

#include <stdio.h>
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <mntopts.h>
#include <paths.h>
#include <puffs.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

#ifdef RUMP_ACTION
#include <rump/rump.h>
#include <rump/rump_syscalls.h>

#define sysctl(a,b,c,d,e,f) rump_sys___sysctl(a,b,c,d,e,f)
#endif

PUFFSOP_PROTOS(sysctlfs)

struct sfsnode {
	int sysctl_flags;
	ino_t myid;
};

#define SFSPATH_DOTDOT 0
#define SFSPATH_NORMAL 1

#define N_HIERARCHY 10
typedef int SfsName[N_HIERARCHY];

struct sfsfid {
	int len;
	SfsName path;
};

static struct sfsnode rn;
static SfsName sname_root;
static struct timespec fstime;

static ino_t nextid = 3;
static mode_t fileperms;
static uid_t fileuid;
static gid_t filegid;

static int rflag;

#define ISADIR(a) ((SYSCTL_TYPE(a->sysctl_flags) == CTLTYPE_NODE))
#define SFS_MAXFILE 32768
#define SFS_NODEPERDIR 128

static int sysctlfs_domount(struct puffs_usermount *);

/*
 * build paths.  doesn't support rename (but neither does the fs)
 */
static int
sysctlfs_pathbuild(struct puffs_usermount *pu,
	const struct puffs_pathobj *parent, const struct puffs_pathobj *comp,
	size_t offset, struct puffs_pathobj *res)
{
	SfsName *sname;
	size_t clen;

	assert(parent->po_len < N_HIERARCHY); /* code uses +1 */

	sname = malloc(sizeof(SfsName));
	assert(sname != NULL);

	clen = parent->po_len;
	if (comp->po_len == SFSPATH_DOTDOT) {
		assert(clen != 0);
		clen--;
	}

	memcpy(sname, parent->po_path, clen * sizeof(int));

	res->po_path = sname;
	res->po_len = clen;

	return 0;
}

static int
sysctlfs_pathtransform(struct puffs_usermount *pu,
	const struct puffs_pathobj *p, const struct puffs_cn *pcn,
	struct puffs_pathobj *res)
{

	res->po_path = NULL;
	/*
	 * XXX: overload.  prevents us from doing rename, but the fs
	 * (and sysctl(3)) doesn't support it, so no biggie
	 */
	if (PCNISDOTDOT(pcn)) {
		res->po_len = SFSPATH_DOTDOT;
	}else {
		res->po_len = SFSPATH_NORMAL;
	}

	return 0;
}

static int
sysctlfs_pathcmp(struct puffs_usermount *pu, struct puffs_pathobj *po1,
	struct puffs_pathobj *po2, size_t clen, int checkprefix)
{

	if (memcmp(po1->po_path, po2->po_path, clen * sizeof(int)) == 0)
		return 0;
	return 1;
}

static void
sysctlfs_pathfree(struct puffs_usermount *pu, struct puffs_pathobj *po)
{

	free(po->po_path);
}

static struct puffs_node *
getnode(struct puffs_usermount *pu, struct puffs_pathobj *po, int nodetype)
{
	struct sysctlnode sn[SFS_NODEPERDIR];
	struct sysctlnode qnode;
	struct puffs_node *pn;
	struct sfsnode *sfs;
	SfsName myname, *sname;
	size_t sl, i;

	/*
	 * Check if we need to create a new in-memory node or if we
	 * already have one for this path.  Shortcut for the rootnode.
	 * Also, memcmp against zero-length would be quite true always.
	 */
	if (po->po_len == 0)
		pn = puffs_getroot(pu);
	else
		pn = puffs_pn_nodewalk(pu, puffs_path_walkcmp, po);

	if (pn == NULL) {
		/*
		 * don't know nodetype?  query...
		 *
		 * XXX1: nothing really guarantees 0 is an invalid nodetype
		 * XXX2: is there really no easier way of doing this?  we
		 *       know the whole mib path
		 */
		if (!nodetype) {
			sname = po->po_path;
			memcpy(myname, po->po_path, po->po_len * sizeof(int));

			memset(&qnode, 0, sizeof(qnode));
			qnode.sysctl_flags = SYSCTL_VERSION;
			myname[po->po_len-1] = CTL_QUERY;

			sl = sizeof(sn);
			if (sysctl(myname, po->po_len, sn, &sl,
			    &qnode, sizeof(qnode)) == -1)
				abort();
			
			for (i = 0; i < sl / sizeof(struct sysctlnode); i++) {
				 if (sn[i].sysctl_num==(*sname)[po->po_len-1]) {
					nodetype = sn[i].sysctl_flags;
					break;
				}
			}
			if (!nodetype)
				return NULL;
		}

		sfs = emalloc(sizeof(struct sfsnode));	
		sfs->sysctl_flags = nodetype;
		sfs->myid = nextid++;

		pn = puffs_pn_new(pu, sfs);
		assert(pn);
	}

	return pn;
}

int
main(int argc, char *argv[])
{
	struct puffs_usermount *pu;
	struct puffs_ops *pops;
	mntoptparse_t mp;
	int mntflags, pflags;
	int detach;
	int ch;

	setprogname(argv[0]);

	if (argc < 2)
		errx(1, "usage: %s sysctlfs [-o mntopts] mountpath",
		    getprogname());

	mntflags = pflags = 0;
	detach = 1;
	while ((ch = getopt(argc, argv, "o:rs")) != -1) {
		switch (ch) {
		case 'o':
			mp = getmntopts(optarg, puffsmopts, &mntflags, &pflags);
			if (mp == NULL)
				err(1, "getmntopts");
			freemntopts(mp);
			break;
		case 'r':
			rflag = 1;
			break;
		case 's':
			detach = 0;
			break;
		}
	}
	argv += optind;
	argc -= optind;
	pflags |= PUFFS_FLAG_BUILDPATH | PUFFS_KFLAG_NOCACHE;

	if (pflags & PUFFS_FLAG_OPDUMP)
		detach = 0;

	if (argc != 2)
		errx(1, "usage: %s [-o mntopts] mountpath", getprogname());

	PUFFSOP_INIT(pops);

	PUFFSOP_SETFSNOP(pops, unmount);
	PUFFSOP_SETFSNOP(pops, sync);
	PUFFSOP_SETFSNOP(pops, statvfs);
	PUFFSOP_SET(pops, sysctlfs, fs, nodetofh);
	PUFFSOP_SET(pops, sysctlfs, fs, fhtonode);

	PUFFSOP_SET(pops, sysctlfs, node, lookup);
	PUFFSOP_SET(pops, sysctlfs, node, getattr);
	PUFFSOP_SET(pops, sysctlfs, node, setattr);
	PUFFSOP_SET(pops, sysctlfs, node, readdir);
	PUFFSOP_SET(pops, sysctlfs, node, read);
	PUFFSOP_SET(pops, sysctlfs, node, write);
	PUFFSOP_SET(pops, puffs_genfs, node, reclaim);

	pu = puffs_init(pops, _PATH_PUFFS, "sysctlfs", NULL, pflags);
	if (pu == NULL)
		err(1, "puffs_init");

	puffs_set_pathbuild(pu, sysctlfs_pathbuild);
	puffs_set_pathtransform(pu, sysctlfs_pathtransform);
	puffs_set_pathcmp(pu, sysctlfs_pathcmp);
	puffs_set_pathfree(pu, sysctlfs_pathfree);

	puffs_setfhsize(pu, sizeof(struct sfsfid), PUFFS_FHFLAG_NFSV3);

	if (sysctlfs_domount(pu) != 0)
		errx(1, "domount");

	if (detach)
		if (puffs_daemon(pu, 1, 1) == -1)
			err(1, "puffs_daemon");

#ifdef RUMP_ACTION
	{
	extern int puffs_fakecc;
	puffs_fakecc = 1;
	rump_init();
	}
#endif

	if (puffs_mount(pu, argv[1], mntflags, puffs_getroot(pu)) == -1)
		err(1, "puffs_mount");
	if (puffs_mainloop(pu) == -1)
		err(1, "mainloop");

	return 0;
}

static int
sysctlfs_domount(struct puffs_usermount *pu)
{
	struct puffs_pathobj *po_root;
	struct puffs_node *pn_root;
	struct timeval tv_now;

	rn.myid = 2;
	rn.sysctl_flags = CTLTYPE_NODE;

	gettimeofday(&tv_now, NULL);
	TIMEVAL_TO_TIMESPEC(&tv_now, &fstime);

	pn_root = puffs_pn_new(pu, &rn);
	assert(pn_root != NULL);
	puffs_setroot(pu, pn_root);

	po_root = puffs_getrootpathobj(pu);
	po_root->po_path = &sname_root;
	po_root->po_len = 0;

	fileuid = geteuid();
	filegid = getegid();

	if (fileuid == 0)
		fileperms = 0755;
	else
		fileperms = 0555;

	return 0;
}

int
sysctlfs_fs_fhtonode(struct puffs_usermount *pu, void *fid, size_t fidsize,
	struct puffs_newinfo *pni)
{
	struct puffs_pathobj po;
	struct puffs_node *pn;
	struct sfsnode *sfs;
	struct sfsfid *sfid;

	sfid = fid;

	po.po_len = sfid->len;
	po.po_path = &sfid->path;

	pn = getnode(pu, &po, 0);
	if (pn == NULL)
		return EINVAL;
	sfs = pn->pn_data;

	puffs_newinfo_setcookie(pni, pn);
	if (ISADIR(sfs))
		puffs_newinfo_setvtype(pni, VDIR);
	else
		puffs_newinfo_setvtype(pni, VREG);

	return 0;
}

int
sysctlfs_fs_nodetofh(struct puffs_usermount *pu, void *cookie,
	void *fid, size_t *fidsize)
{
	struct puffs_node *pn = cookie;
	struct sfsfid *sfid;

	sfid = fid;
	sfid->len = PNPLEN(pn);
	memcpy(&sfid->path, PNPATH(pn), sfid->len * sizeof(int));

	return 0;
}

static void
getnodedata(struct sfsnode *sfs, struct puffs_pathobj *po,
	char *buf, size_t *bufsize)
{
	size_t sz;
	int error = 0;

	assert(!ISADIR(sfs));

	memset(buf, 0, *bufsize);
	switch (SYSCTL_TYPE(sfs->sysctl_flags)) {
	case CTLTYPE_BOOL: {
		bool b;
		sz = sizeof(bool);
		assert(sz <= *bufsize);
		if (sysctl(po->po_path, po->po_len, &b, &sz, NULL, 0) == -1) {
			error = errno;
			break;
		}
		if (rflag)
			memcpy(buf, &b, sz);
		else
			snprintf(buf, *bufsize, "%s", b ? "true" : "false");
		break;
	}
	case CTLTYPE_INT: {
		int i;
		sz = sizeof(int);
		assert(sz <= *bufsize);
		if (sysctl(po->po_path, po->po_len, &i, &sz, NULL, 0) == -1) {
			error = errno;
			break;
		}
		if (rflag)
			memcpy(buf, &i, sz);
		else
			snprintf(buf, *bufsize, "%d", i);
		break;
	}
	case CTLTYPE_QUAD: {
		quad_t q;
		sz = sizeof(q);
		assert(sz <= *bufsize);
		if (sysctl(po->po_path, po->po_len, &q, &sz, NULL, 0) == -1) {
			error = errno;
			break;
		}
		if (rflag)
			memcpy(buf, &q, sz);
		else
			snprintf(buf, *bufsize, "%" PRId64, q);
		break;
	}
	case CTLTYPE_STRUCT: {
		uint8_t snode[SFS_MAXFILE/2-1];
		unsigned i;

		sz = sizeof(snode);
		assert(sz <= *bufsize);
		if (sysctl(po->po_path, po->po_len, snode, &sz, NULL, 0) == -1){
			error = errno;
			break;
		}
		if (rflag) {
			memcpy(buf, &snode, sz);
		} else {
			for (i = 0; i < sz && 2*i < *bufsize; i++) {
				sprintf(&buf[2*i], "%02x", snode[i]);
			}
			buf[2*i] = '\0';
		}
		break;
	}
	case CTLTYPE_STRING: {
		sz = *bufsize;
		assert(sz <= *bufsize);
		if (sysctl(po->po_path, po->po_len, buf, &sz, NULL, 0) == -1) {
			error = errno;
			break;
		}
		break;
	}
	default:
		snprintf(buf, *bufsize, "invalid sysctl CTLTYPE %d",
		    SYSCTL_TYPE(sfs->sysctl_flags));
		break;
	}

	if (error) {
		*bufsize = 0;
		return;
	}

	if (rflag)
		*bufsize = sz;
	else
		*bufsize = strlen(buf);
}

static int
getlinks(struct sfsnode *sfs, struct puffs_pathobj *po)
{
	struct sysctlnode sn[SFS_NODEPERDIR];
	struct sysctlnode qnode;
	SfsName *sname;
	size_t sl;

	if (!ISADIR(sfs))
		return 1;

	memset(&qnode, 0, sizeof(qnode));
	sl = sizeof(sn);
	qnode.sysctl_flags = SYSCTL_VERSION;
	sname = po->po_path;
	(*sname)[po->po_len] = CTL_QUERY;

	if (sysctl(*sname, po->po_len + 1, sn, &sl,
	    &qnode, sizeof(qnode)) == -1)
		return 0;

	return (sl / sizeof(sn[0])) + 2;
}

static int
getsize(struct sfsnode *sfs, struct puffs_pathobj *po)
{
	char buf[SFS_MAXFILE];
	size_t sz = sizeof(buf);

	if (ISADIR(sfs))
		return getlinks(sfs, po) * 16; /* totally arbitrary */

	getnodedata(sfs, po, buf, &sz);
	if (rflag)
		return sz;
	else
		return sz + 1; /* for \n, not \0 */
}

int
sysctlfs_node_lookup(struct puffs_usermount *pu, void *opc,
	struct puffs_newinfo *pni, const struct puffs_cn *pcn)
{
	struct puffs_cn *p2cn = __UNCONST(pcn); /* XXX: fix the interface */
	struct sysctlnode sn[SFS_NODEPERDIR];
	struct sysctlnode qnode;
	struct puffs_node *pn_dir = opc;
	struct puffs_node *pn_new;
	struct sfsnode *sfs_dir = pn_dir->pn_data, *sfs_new;
	SfsName *sname = PCNPATH(pcn);
	size_t sl, i;
	int nodetype;

	assert(ISADIR(sfs_dir));

	/*
	 * If we're looking for dotdot, we already have the entire pathname
	 * in sname, courtesy of pathbuild, so we can skip this step.
	 */
	if (!PCNISDOTDOT(pcn)) {
		memset(&qnode, 0, sizeof(qnode));
		sl = SFS_NODEPERDIR * sizeof(struct sysctlnode);
		qnode.sysctl_flags = SYSCTL_VERSION;
		(*sname)[PCNPLEN(pcn)] = CTL_QUERY;

		if (sysctl(*sname, PCNPLEN(pcn) + 1, sn, &sl,
		    &qnode, sizeof(qnode)) == -1)
			return ENOENT;

		for (i = 0; i < sl / sizeof(struct sysctlnode); i++)
			if (strcmp(sn[i].sysctl_name, pcn->pcn_name) == 0)
				break;
		if (i == sl / sizeof(struct sysctlnode))
			return ENOENT;

		(*sname)[PCNPLEN(pcn)] = sn[i].sysctl_num;
		p2cn->pcn_po_full.po_len++;
		nodetype = sn[i].sysctl_flags;
	} else
		nodetype = CTLTYPE_NODE;

	pn_new = getnode(pu, &p2cn->pcn_po_full, nodetype);
	sfs_new = pn_new->pn_data;

	puffs_newinfo_setcookie(pni, pn_new);
	if (ISADIR(sfs_new))
		puffs_newinfo_setvtype(pni, VDIR);
	else
		puffs_newinfo_setvtype(pni, VREG);

	return 0;
}

int
sysctlfs_node_getattr(struct puffs_usermount *pu, void *opc, struct vattr *va,
	const struct puffs_cred *pcr)
{
	struct puffs_node *pn = opc;
	struct sfsnode *sfs = pn->pn_data;

	memset(va, 0, sizeof(struct vattr));

	if (ISADIR(sfs)) {
		va->va_type = VDIR;
		va->va_mode = 0555;
	} else {
		va->va_type = VREG;
		va->va_mode = fileperms;
	}
	va->va_uid = fileuid;
	va->va_gid = filegid;
	va->va_nlink = getlinks(sfs, &pn->pn_po);
	va->va_fileid = sfs->myid;
	va->va_size = getsize(sfs, &pn->pn_po);
	va->va_gen = 1;
	va->va_rdev = PUFFS_VNOVAL;
	va->va_blocksize = 512;
	va->va_filerev = 1;

	va->va_atime = va->va_mtime = va->va_ctime = va->va_birthtime = fstime;

	return 0;
}

int
sysctlfs_node_setattr(struct puffs_usermount *pu, void *opc,
	const struct vattr *va, const struct puffs_cred *pcr)
{

	/* dummy, but required for write */
	/* XXX: we could return EOPNOTSUPP or something */
	return 0;
}

int
sysctlfs_node_readdir(struct puffs_usermount *pu, void *opc,
	struct dirent *dent, off_t *readoff, size_t *reslen,
	const struct puffs_cred *pcr, int *eofflag,
	off_t *cookies, size_t *ncookies)
{
	struct sysctlnode sn[SFS_NODEPERDIR];
	struct sysctlnode qnode;
	struct puffs_node *pn_dir = opc;
	struct puffs_node *pn_res;
	struct puffs_pathobj po;
	struct sfsnode *sfs_dir = pn_dir->pn_data, *sfs_ent;
	SfsName *sname;
	size_t sl, i;
	enum vtype vt;
	ino_t id;

	*ncookies = 0;

 again:
	if (*readoff == DENT_DOT || *readoff == DENT_DOTDOT) {
		puffs_gendotdent(&dent, sfs_dir->myid, *readoff, reslen);
		(*readoff)++;
		PUFFS_STORE_DCOOKIE(cookies, ncookies, *readoff);
		goto again;
	}

	memset(&qnode, 0, sizeof(qnode));
	sl = SFS_NODEPERDIR * sizeof(struct sysctlnode);
	qnode.sysctl_flags = SYSCTL_VERSION;
	sname = PNPATH(pn_dir);
	(*sname)[PNPLEN(pn_dir)] = CTL_QUERY;

	if (sysctl(*sname, PNPLEN(pn_dir) + 1, sn, &sl,
	    &qnode, sizeof(qnode)) == -1)
		return ENOENT;

	po.po_path = sname;
	po.po_len = PNPLEN(pn_dir)+1;

	for (i = DENT_ADJ(*readoff); i < sl / sizeof(struct sysctlnode); i++) {
		if (SYSCTL_TYPE(sn[i].sysctl_flags) == CTLTYPE_NODE)
			vt = VDIR;
		else
			vt = VREG;

		/*
		 * check if the node exists.  if so, give it the real
		 * inode number.  otherwise just fake it.
		 */
		(*sname)[PNPLEN(pn_dir)] = sn[i].sysctl_num;
		pn_res = puffs_pn_nodewalk(pu, puffs_path_walkcmp, &po);
		if (pn_res) {
			sfs_ent = pn_res->pn_data;
			id = sfs_ent->myid;
		} else {
			id = nextid++;
		}

		if (!puffs_nextdent(&dent, sn[i].sysctl_name, id,
		    puffs_vtype2dt(vt), reslen))
			return 0;

		(*readoff)++;
		PUFFS_STORE_DCOOKIE(cookies, ncookies, *readoff);
	}

	*eofflag = 1;
	return 0;
}

int
sysctlfs_node_read(struct puffs_usermount *pu, void *opc, uint8_t *buf,
	off_t offset, size_t *resid, const struct puffs_cred *pcr,
	int ioflag)
{
	char localbuf[SFS_MAXFILE];
	struct puffs_node *pn = opc;
	struct sfsnode *sfs = pn->pn_data;
	size_t sz = sizeof(localbuf);
	int xfer;

	if (ISADIR(sfs))
		return EISDIR;

	getnodedata(sfs, &pn->pn_po, localbuf, &sz);
	if ((ssize_t)sz < offset)
		xfer = 0;
	else
		xfer = MIN(*resid, sz - offset);

	if (xfer <= 0)
		return 0;

	memcpy(buf, localbuf + offset, xfer);
	*resid -= xfer;

	if (*resid && !rflag) {
		buf[xfer] = '\n';
		(*resid)--;
	}

	return 0;
}

int
sysctlfs_node_write(struct puffs_usermount *pu, void *opc, uint8_t *buf,
	off_t offset, size_t *resid, const struct puffs_cred *cred,
	int ioflag)
{
	struct puffs_node *pn = opc;
	struct sfsnode *sfs = pn->pn_data;
	long long ll;
	int i, rv;
	bool b;

	/*
	 * I picked the wrong day to ... um, the wrong place to return errors
	 */

	/* easy to support, but just unavailable now */
	if (rflag)
		return EOPNOTSUPP;

	if (puffs_cred_isjuggernaut(cred) == 0)
		return EACCES;

	if (ISADIR(sfs))
		return EISDIR;

	if (offset != 0)
		return EINVAL;

	if (ioflag & PUFFS_IO_APPEND)
		return EINVAL;

	switch (SYSCTL_TYPE(sfs->sysctl_flags)) {
	case CTLTYPE_BOOL:
		if (strcasestr((const char *)buf, "true"))
			b = true;
		else if (strcasestr((const char *)buf, "false"))
			b = false;
		else
			return EINVAL;
		rv = sysctl(PNPATH(pn), PNPLEN(pn), NULL, NULL,
		    &b, sizeof(b));
		break;
	case CTLTYPE_INT:
		if (sscanf((const char *)buf, "%d", &i) != 1)
			return EINVAL;
		rv = sysctl(PNPATH(pn), PNPLEN(pn), NULL, NULL,
		    &i, sizeof(int));
		break;
	case CTLTYPE_QUAD:
		if (sscanf((const char *)buf, "%lld", &ll) != 1)
			return EINVAL;
		rv =  sysctl(PNPATH(pn), PNPLEN(pn), NULL, NULL,
		    &ll, sizeof(long long));
		break;
	case CTLTYPE_STRING:
		rv = sysctl(PNPATH(pn), PNPLEN(pn), NULL, NULL, buf, *resid);
		break;
	default:
		rv = EINVAL;
		break;
	}

	if (rv)
		return rv;

	*resid = 0;
	return 0;
}
