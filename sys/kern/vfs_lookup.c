/*	$NetBSD: vfs_lookup.c,v 1.203 2015/08/24 22:50:32 pooka Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *
 *	@(#)vfs_lookup.c	8.10 (Berkeley) 5/27/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: vfs_lookup.c,v 1.203 2015/08/24 22:50:32 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_magiclinks.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/syslimits.h>
#include <sys/time.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/errno.h>
#include <sys/filedesc.h>
#include <sys/hash.h>
#include <sys/proc.h>
#include <sys/syslog.h>
#include <sys/kauth.h>
#include <sys/ktrace.h>
#include <sys/dirent.h>

#ifndef MAGICLINKS
#define MAGICLINKS 0
#endif

int vfs_magiclinks = MAGICLINKS;

__CTASSERT(MAXNAMLEN == NAME_MAX);

/*
 * Substitute replacement text for 'magic' strings in symlinks.
 * Returns 0 if successful, and returns non-zero if an error
 * occurs.  (Currently, the only possible error is running out
 * of temporary pathname space.)
 *
 * Looks for "@<string>" and "@<string>/", where <string> is a
 * recognized 'magic' string.  Replaces the "@<string>" with the
 * appropriate replacement text.  (Note that in some cases the
 * replacement text may have zero length.)
 *
 * This would have been table driven, but the variance in
 * replacement strings (and replacement string lengths) made
 * that impractical.
 */
#define	VNL(x)							\
	(sizeof(x) - 1)

#define	VO	'{'
#define	VC	'}'

#define	MATCH(str)						\
	((termchar == '/' && i + VNL(str) == *len) ||		\
	 (i + VNL(str) < *len &&				\
	  cp[i + VNL(str)] == termchar)) &&			\
	!strncmp((str), &cp[i], VNL(str))

#define	SUBSTITUTE(m, s, sl)					\
	if ((newlen + (sl)) >= MAXPATHLEN)			\
		return 1;					\
	i += VNL(m);						\
	if (termchar != '/')					\
		i++;						\
	(void)memcpy(&tmp[newlen], (s), (sl));			\
	newlen += (sl);						\
	change = 1;						\
	termchar = '/';

static int
symlink_magic(struct proc *p, char *cp, size_t *len)
{
	char *tmp;
	size_t change, i, newlen, slen;
	char termchar = '/';
	char idtmp[11]; /* enough for 32 bit *unsigned* integer */


	tmp = PNBUF_GET();
	for (change = i = newlen = 0; i < *len; ) {
		if (cp[i] != '@') {
			tmp[newlen++] = cp[i++];
			continue;
		}

		i++;

		/* Check for @{var} syntax. */
		if (cp[i] == VO) {
			termchar = VC;
			i++;
		}

		/*
		 * The following checks should be ordered according
		 * to frequency of use.
		 */
		if (MATCH("machine_arch")) {
			slen = VNL(MACHINE_ARCH);
			SUBSTITUTE("machine_arch", MACHINE_ARCH, slen);
		} else if (MATCH("machine")) {
			slen = VNL(MACHINE);
			SUBSTITUTE("machine", MACHINE, slen);
		} else if (MATCH("hostname")) {
			SUBSTITUTE("hostname", hostname, hostnamelen);
		} else if (MATCH("osrelease")) {
			slen = strlen(osrelease);
			SUBSTITUTE("osrelease", osrelease, slen);
		} else if (MATCH("emul")) {
			slen = strlen(p->p_emul->e_name);
			SUBSTITUTE("emul", p->p_emul->e_name, slen);
		} else if (MATCH("kernel_ident")) {
			slen = strlen(kernel_ident);
			SUBSTITUTE("kernel_ident", kernel_ident, slen);
		} else if (MATCH("domainname")) {
			SUBSTITUTE("domainname", domainname, domainnamelen);
		} else if (MATCH("ostype")) {
			slen = strlen(ostype);
			SUBSTITUTE("ostype", ostype, slen);
		} else if (MATCH("uid")) {
			slen = snprintf(idtmp, sizeof(idtmp), "%u",
			    kauth_cred_geteuid(kauth_cred_get()));
			SUBSTITUTE("uid", idtmp, slen);
		} else if (MATCH("ruid")) {
			slen = snprintf(idtmp, sizeof(idtmp), "%u",
			    kauth_cred_getuid(kauth_cred_get()));
			SUBSTITUTE("ruid", idtmp, slen);
		} else if (MATCH("gid")) {
			slen = snprintf(idtmp, sizeof(idtmp), "%u",
			    kauth_cred_getegid(kauth_cred_get()));
			SUBSTITUTE("gid", idtmp, slen);
		} else if (MATCH("rgid")) {
			slen = snprintf(idtmp, sizeof(idtmp), "%u",
			    kauth_cred_getgid(kauth_cred_get()));
			SUBSTITUTE("rgid", idtmp, slen);
		} else {
			tmp[newlen++] = '@';
			if (termchar == VC)
				tmp[newlen++] = VO;
		}
	}

	if (change) {
		(void)memcpy(cp, tmp, newlen);
		*len = newlen;
	}
	PNBUF_PUT(tmp);

	return 0;
}

#undef VNL
#undef VO
#undef VC
#undef MATCH
#undef SUBSTITUTE

////////////////////////////////////////////////////////////

/*
 * Determine the namei hash (for the namecache) for name.
 * If *ep != NULL, hash from name to ep-1.
 * If *ep == NULL, hash from name until the first NUL or '/', and
 * return the location of this termination character in *ep.
 *
 * This function returns an equivalent hash to the MI hash32_strn().
 * The latter isn't used because in the *ep == NULL case, determining
 * the length of the string to the first NUL or `/' and then calling
 * hash32_strn() involves unnecessary double-handling of the data.
 */
uint32_t
namei_hash(const char *name, const char **ep)
{
	uint32_t	hash;

	hash = HASH32_STR_INIT;
	if (*ep != NULL) {
		for (; name < *ep; name++)
			hash = hash * 33 + *(const uint8_t *)name;
	} else {
		for (; *name != '\0' && *name != '/'; name++)
			hash = hash * 33 + *(const uint8_t *)name;
		*ep = name;
	}
	return (hash + (hash >> 5));
}

/*
 * Find the end of the first path component in NAME and return its
 * length.
 */
static size_t
namei_getcomponent(const char *name)
{
	size_t pos;

	pos = 0;
	while (name[pos] != '\0' && name[pos] != '/') {
		pos++;
	}
	return pos;
}

////////////////////////////////////////////////////////////

/*
 * Sealed abstraction for pathnames.
 *
 * System-call-layer level code that is going to call namei should
 * first create a pathbuf and adjust all the bells and whistles on it
 * as needed by context.
 */

struct pathbuf {
	char *pb_path;
	char *pb_pathcopy;
	unsigned pb_pathcopyuses;
};

static struct pathbuf *
pathbuf_create_raw(void)
{
	struct pathbuf *pb;

	pb = kmem_alloc(sizeof(*pb), KM_SLEEP);
	if (pb == NULL) {
		return NULL;
	}
	pb->pb_path = PNBUF_GET();
	if (pb->pb_path == NULL) {
		kmem_free(pb, sizeof(*pb));
		return NULL;
	}
	pb->pb_pathcopy = NULL;
	pb->pb_pathcopyuses = 0;
	return pb;
}

void
pathbuf_destroy(struct pathbuf *pb)
{
	KASSERT(pb->pb_pathcopyuses == 0);
	KASSERT(pb->pb_pathcopy == NULL);
	PNBUF_PUT(pb->pb_path);
	kmem_free(pb, sizeof(*pb));
}

struct pathbuf *
pathbuf_assimilate(char *pnbuf)
{
	struct pathbuf *pb;

	pb = kmem_alloc(sizeof(*pb), KM_SLEEP);
	if (pb == NULL) {
		return NULL;
	}
	pb->pb_path = pnbuf;
	pb->pb_pathcopy = NULL;
	pb->pb_pathcopyuses = 0;
	return pb;
}

struct pathbuf *
pathbuf_create(const char *path)
{
	struct pathbuf *pb;
	int error;

	pb = pathbuf_create_raw();
	if (pb == NULL) {
		return NULL;
	}
	error = copystr(path, pb->pb_path, PATH_MAX, NULL);
	if (error != 0) {
		KASSERT(!"kernel path too long in pathbuf_create");
		/* make sure it's null-terminated, just in case */
		pb->pb_path[PATH_MAX-1] = '\0';
	}
	return pb;
}

int
pathbuf_copyin(const char *userpath, struct pathbuf **ret)
{
	struct pathbuf *pb;
	int error;

	pb = pathbuf_create_raw();
	if (pb == NULL) {
		return ENOMEM;
	}
	error = copyinstr(userpath, pb->pb_path, PATH_MAX, NULL);
	if (error) {
		pathbuf_destroy(pb);
		return error;
	}
	*ret = pb;
	return 0;
}

/*
 * XXX should not exist:
 *   1. whether a pointer is kernel or user should be statically checkable.
 *   2. copyin should be handled by the upper part of the syscall layer,
 *      not in here.
 */
int
pathbuf_maybe_copyin(const char *path, enum uio_seg seg, struct pathbuf **ret)
{
	if (seg == UIO_USERSPACE) {
		return pathbuf_copyin(path, ret);
	} else {
		*ret = pathbuf_create(path);
		if (*ret == NULL) {
			return ENOMEM;
		}
		return 0;
	}
}

/*
 * Get a copy of the path buffer as it currently exists. If this is
 * called after namei starts the results may be arbitrary.
 */
void
pathbuf_copystring(const struct pathbuf *pb, char *buf, size_t maxlen)
{
	strlcpy(buf, pb->pb_path, maxlen);
}

/*
 * These two functions allow access to a saved copy of the original
 * path string. The first copy should be gotten before namei is
 * called. Each copy that is gotten should be put back.
 */

const char *
pathbuf_stringcopy_get(struct pathbuf *pb)
{
	if (pb->pb_pathcopyuses == 0) {
		pb->pb_pathcopy = PNBUF_GET();
		strcpy(pb->pb_pathcopy, pb->pb_path);
	}
	pb->pb_pathcopyuses++;
	return pb->pb_pathcopy;
}

void
pathbuf_stringcopy_put(struct pathbuf *pb, const char *str)
{
	KASSERT(str == pb->pb_pathcopy);
	KASSERT(pb->pb_pathcopyuses > 0);
	pb->pb_pathcopyuses--;
	if (pb->pb_pathcopyuses == 0) {
		PNBUF_PUT(pb->pb_pathcopy);
		pb->pb_pathcopy = NULL;
	}
}


////////////////////////////////////////////////////////////

/*
 * namei: convert a pathname into a pointer to a (maybe-locked) vnode,
 * and maybe also its parent directory vnode, and assorted other guff.
 * See namei(9) for the interface documentation.
 *
 *
 * The FOLLOW flag is set when symbolic links are to be followed
 * when they occur at the end of the name translation process.
 * Symbolic links are always followed for all other pathname
 * components other than the last.
 *
 * The segflg defines whether the name is to be copied from user
 * space or kernel space.
 *
 * Overall outline of namei:
 *
 *	copy in name
 *	get starting directory
 *	while (!done && !error) {
 *		call lookup to search path.
 *		if symbolic link, massage name in buffer and continue
 *	}
 */

/*
 * Search a pathname.
 * This is a very central and rather complicated routine.
 *
 * The pathname is pointed to by ni_ptr and is of length ni_pathlen.
 * The starting directory is passed in. The pathname is descended
 * until done, or a symbolic link is encountered. The variable ni_more
 * is clear if the path is completed; it is set to one if a symbolic
 * link needing interpretation is encountered.
 *
 * The flag argument is LOOKUP, CREATE, RENAME, or DELETE depending on
 * whether the name is to be looked up, created, renamed, or deleted.
 * When CREATE, RENAME, or DELETE is specified, information usable in
 * creating, renaming, or deleting a directory entry may be calculated.
 * If flag has LOCKPARENT or'ed into it, the parent directory is returned
 * locked.  Otherwise the parent directory is not returned. If the target
 * of the pathname exists and LOCKLEAF is or'ed into the flag the target
 * is returned locked, otherwise it is returned unlocked.  When creating
 * or renaming and LOCKPARENT is specified, the target may not be ".".
 * When deleting and LOCKPARENT is specified, the target may be ".".
 *
 * Overall outline of lookup:
 *
 * dirloop:
 *	identify next component of name at ndp->ni_ptr
 *	handle degenerate case where name is null string
 *	if .. and crossing mount points and on mounted filesys, find parent
 *	call VOP_LOOKUP routine for next component name
 *	    directory vnode returned in ni_dvp, locked.
 *	    component vnode returned in ni_vp (if it exists), locked.
 *	if result vnode is mounted on and crossing mount points,
 *	    find mounted on vnode
 *	if more components of name, do next level at dirloop
 *	return the answer in ni_vp, locked if LOCKLEAF set
 *	    if LOCKPARENT set, return locked parent in ni_dvp
 */


/*
 * Internal state for a namei operation.
 *
 * cnp is always equal to &ndp->ni_cnp.
 */
struct namei_state {
	struct nameidata *ndp;
	struct componentname *cnp;

	int docache;			/* == 0 do not cache last component */
	int rdonly;			/* lookup read-only flag bit */
	int slashes;

	unsigned attempt_retry:1;	/* true if error allows emul retry */
};


/*
 * Initialize the namei working state.
 */
static void
namei_init(struct namei_state *state, struct nameidata *ndp)
{

	state->ndp = ndp;
	state->cnp = &ndp->ni_cnd;

	state->docache = 0;
	state->rdonly = 0;
	state->slashes = 0;

#ifdef DIAGNOSTIC
	if (!state->cnp->cn_cred)
		panic("namei: bad cred/proc");
	if (state->cnp->cn_nameiop & (~OPMASK))
		panic("namei: nameiop contaminated with flags");
	if (state->cnp->cn_flags & OPMASK)
		panic("namei: flags contaminated with nameiops");
#endif

	/*
	 * The buffer for name translation shall be the one inside the
	 * pathbuf.
	 */
	state->ndp->ni_pnbuf = state->ndp->ni_pathbuf->pb_path;
}

/*
 * Clean up the working namei state, leaving things ready for return
 * from namei.
 */
static void
namei_cleanup(struct namei_state *state)
{
	KASSERT(state->cnp == &state->ndp->ni_cnd);

	/* nothing for now */
	(void)state;
}

//////////////////////////////

/*
 * Get the directory context.
 * Initializes the rootdir and erootdir state and returns a reference
 * to the starting dir.
 */
static struct vnode *
namei_getstartdir(struct namei_state *state)
{
	struct nameidata *ndp = state->ndp;
	struct componentname *cnp = state->cnp;
	struct cwdinfo *cwdi;		/* pointer to cwd state */
	struct lwp *self = curlwp;	/* thread doing namei() */
	struct vnode *rootdir, *erootdir, *curdir, *startdir;

	cwdi = self->l_proc->p_cwdi;
	rw_enter(&cwdi->cwdi_lock, RW_READER);

	/* root dir */
	if (cwdi->cwdi_rdir == NULL || (cnp->cn_flags & NOCHROOT)) {
		rootdir = rootvnode;
	} else {
		rootdir = cwdi->cwdi_rdir;
	}

	/* emulation root dir, if any */
	if ((cnp->cn_flags & TRYEMULROOT) == 0) {
		/* if we don't want it, don't fetch it */
		erootdir = NULL;
	} else if (cnp->cn_flags & EMULROOTSET) {
		/* explicitly set emulroot; "/../" doesn't override this */
		erootdir = ndp->ni_erootdir;
	} else if (!strncmp(ndp->ni_pnbuf, "/../", 4)) {
		/* explicit reference to real rootdir */
		erootdir = NULL;
	} else {
		/* may be null */
		erootdir = cwdi->cwdi_edir;
	}

	/* current dir */
	curdir = cwdi->cwdi_cdir;

	if (ndp->ni_pnbuf[0] != '/') {
		if (ndp->ni_atdir != NULL) {
			startdir = ndp->ni_atdir;
		} else {
			startdir = curdir;
		}
		erootdir = NULL;
	} else if (cnp->cn_flags & TRYEMULROOT && erootdir != NULL) {
		startdir = erootdir;
	} else {
		startdir = rootdir;
		erootdir = NULL;
	}

	state->ndp->ni_rootdir = rootdir;
	state->ndp->ni_erootdir = erootdir;

	/*
	 * Get a reference to the start dir so we can safely unlock cwdi.
	 *
	 * XXX: should we hold references to rootdir and erootdir while
	 * we're running? What happens if a multithreaded process chroots
	 * during namei?
	 */
	vref(startdir);

	rw_exit(&cwdi->cwdi_lock);
	return startdir;
}

/*
 * Get the directory context for the nfsd case, in parallel to
 * getstartdir. Initializes the rootdir and erootdir state and
 * returns a reference to the passed-in starting dir.
 */
static struct vnode *
namei_getstartdir_for_nfsd(struct namei_state *state)
{
	KASSERT(state->ndp->ni_atdir != NULL);

	/* always use the real root, and never set an emulation root */
	state->ndp->ni_rootdir = rootvnode;
	state->ndp->ni_erootdir = NULL;

	vref(state->ndp->ni_atdir);
	return state->ndp->ni_atdir;
}


/*
 * Ktrace the namei operation.
 */
static void
namei_ktrace(struct namei_state *state)
{
	struct nameidata *ndp = state->ndp;
	struct componentname *cnp = state->cnp;
	struct lwp *self = curlwp;	/* thread doing namei() */
	const char *emul_path;

	if (ktrpoint(KTR_NAMEI)) {
		if (ndp->ni_erootdir != NULL) {
			/*
			 * To make any sense, the trace entry need to have the
			 * text of the emulation path prepended.
			 * Usually we can get this from the current process,
			 * but when called from emul_find_interp() it is only
			 * in the exec_package - so we get it passed in ni_next
			 * (this is a hack).
			 */
			if (cnp->cn_flags & EMULROOTSET)
				emul_path = ndp->ni_next;
			else
				emul_path = self->l_proc->p_emul->e_path;
			ktrnamei2(emul_path, strlen(emul_path),
			    ndp->ni_pnbuf, ndp->ni_pathlen);
		} else
			ktrnamei(ndp->ni_pnbuf, ndp->ni_pathlen);
	}
}

/*
 * Start up namei. Find the root dir and cwd, establish the starting
 * directory for lookup, and lock it. Also calls ktrace when
 * appropriate.
 */
static int
namei_start(struct namei_state *state, int isnfsd,
	    struct vnode **startdir_ret)
{
	struct nameidata *ndp = state->ndp;
	struct vnode *startdir;

	/* length includes null terminator (was originally from copyinstr) */
	ndp->ni_pathlen = strlen(ndp->ni_pnbuf) + 1;

	/*
	 * POSIX.1 requirement: "" is not a valid file name.
	 */
	if (ndp->ni_pathlen == 1) {
		return ENOENT;
	}

	ndp->ni_loopcnt = 0;

	/* Get starting directory, set up root, and ktrace. */
	if (isnfsd) {
		startdir = namei_getstartdir_for_nfsd(state);
		/* no ktrace */
	} else {
		startdir = namei_getstartdir(state);
		namei_ktrace(state);
	}

	/* NDAT may feed us with a non directory namei_getstartdir */
	if (startdir->v_type != VDIR)
		return ENOTDIR;

	vn_lock(startdir, LK_EXCLUSIVE | LK_RETRY);

	*startdir_ret = startdir;
	return 0;
}

/*
 * Check for being at a symlink that we're going to follow.
 */
static inline int
namei_atsymlink(struct namei_state *state, struct vnode *foundobj)
{
	return (foundobj->v_type == VLNK) &&
		(state->cnp->cn_flags & (FOLLOW|REQUIREDIR));
}

/*
 * Follow a symlink.
 *
 * Updates searchdir. inhibitmagic causes magic symlinks to not be
 * interpreted; this is used by nfsd.
 *
 * Unlocks foundobj on success (ugh)
 */
static inline int
namei_follow(struct namei_state *state, int inhibitmagic,
	     struct vnode *searchdir, struct vnode *foundobj,
	     struct vnode **newsearchdir_ret)
{
	struct nameidata *ndp = state->ndp;
	struct componentname *cnp = state->cnp;

	struct lwp *self = curlwp;	/* thread doing namei() */
	struct iovec aiov;		/* uio for reading symbolic links */
	struct uio auio;
	char *cp;			/* pointer into pathname argument */
	size_t linklen;
	int error;

	KASSERT(VOP_ISLOCKED(searchdir) == LK_EXCLUSIVE);
	KASSERT(VOP_ISLOCKED(foundobj) == LK_EXCLUSIVE);
	if (ndp->ni_loopcnt++ >= MAXSYMLINKS) {
		return ELOOP;
	}
	if (foundobj->v_mount->mnt_flag & MNT_SYMPERM) {
		error = VOP_ACCESS(foundobj, VEXEC, cnp->cn_cred);
		if (error != 0)
			return error;
	}

	/* FUTURE: fix this to not use a second buffer */
	cp = PNBUF_GET();
	aiov.iov_base = cp;
	aiov.iov_len = MAXPATHLEN;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = 0;
	auio.uio_rw = UIO_READ;
	auio.uio_resid = MAXPATHLEN;
	UIO_SETUP_SYSSPACE(&auio);
	error = VOP_READLINK(foundobj, &auio, cnp->cn_cred);
	if (error) {
		PNBUF_PUT(cp);
		return error;
	}
	linklen = MAXPATHLEN - auio.uio_resid;
	if (linklen == 0) {
		PNBUF_PUT(cp);
		return ENOENT;
	}

	/*
	 * Do symlink substitution, if appropriate, and
	 * check length for potential overflow.
	 *
	 * Inhibit symlink substitution for nfsd.
	 * XXX: This is how it was before; is that a bug or a feature?
	 */
	if ((!inhibitmagic && vfs_magiclinks &&
	     symlink_magic(self->l_proc, cp, &linklen)) ||
	    (linklen + ndp->ni_pathlen >= MAXPATHLEN)) {
		PNBUF_PUT(cp);
		return ENAMETOOLONG;
	}
	if (ndp->ni_pathlen > 1) {
		/* includes a null-terminator */
		memcpy(cp + linklen, ndp->ni_next, ndp->ni_pathlen);
	} else {
		cp[linklen] = '\0';
	}
	ndp->ni_pathlen += linklen;
	memcpy(ndp->ni_pnbuf, cp, ndp->ni_pathlen);
	PNBUF_PUT(cp);

	/* we're now starting from the beginning of the buffer again */
	cnp->cn_nameptr = ndp->ni_pnbuf;

	/* must unlock this before relocking searchdir */
	VOP_UNLOCK(foundobj);

	/*
	 * Check if root directory should replace current directory.
	 */
	if (ndp->ni_pnbuf[0] == '/') {
		vput(searchdir);
		/* Keep absolute symbolic links inside emulation root */
		searchdir = ndp->ni_erootdir;
		if (searchdir == NULL ||
		    (ndp->ni_pnbuf[1] == '.' 
		     && ndp->ni_pnbuf[2] == '.'
		     && ndp->ni_pnbuf[3] == '/')) {
			ndp->ni_erootdir = NULL;
			searchdir = ndp->ni_rootdir;
		}
		vref(searchdir);
		vn_lock(searchdir, LK_EXCLUSIVE | LK_RETRY);
		while (cnp->cn_nameptr[0] == '/') {
			cnp->cn_nameptr++;
			ndp->ni_pathlen--;
		}
	}

	*newsearchdir_ret = searchdir;
	KASSERT(VOP_ISLOCKED(searchdir) == LK_EXCLUSIVE);
	return 0;
}

//////////////////////////////

/*
 * Inspect the leading path component and update the state accordingly.
 */
static int
lookup_parsepath(struct namei_state *state)
{
	const char *cp;			/* pointer into pathname argument */

	struct componentname *cnp = state->cnp;
	struct nameidata *ndp = state->ndp;

	KASSERT(cnp == &ndp->ni_cnd);

	/*
	 * Search a new directory.
	 *
	 * The last component of the filename is left accessible via
	 * cnp->cn_nameptr for callers that need the name. Callers needing
	 * the name set the SAVENAME flag. When done, they assume
	 * responsibility for freeing the pathname buffer.
	 *
	 * At this point, our only vnode state is that the search dir
	 * is held and locked.
	 */
	cnp->cn_consume = 0;
	cnp->cn_namelen = namei_getcomponent(cnp->cn_nameptr);
	cp = cnp->cn_nameptr + cnp->cn_namelen;
	if (cnp->cn_namelen > KERNEL_NAME_MAX) {
		return ENAMETOOLONG;
	}
#ifdef NAMEI_DIAGNOSTIC
	{ char c = *cp;
	*(char *)cp = '\0';
	printf("{%s}: ", cnp->cn_nameptr);
	*(char *)cp = c; }
#endif /* NAMEI_DIAGNOSTIC */
	ndp->ni_pathlen -= cnp->cn_namelen;
	ndp->ni_next = cp;
	/*
	 * If this component is followed by a slash, then move the pointer to
	 * the next component forward, and remember that this component must be
	 * a directory.
	 */
	if (*cp == '/') {
		do {
			cp++;
		} while (*cp == '/');
		state->slashes = cp - ndp->ni_next;
		ndp->ni_pathlen -= state->slashes;
		ndp->ni_next = cp;
		cnp->cn_flags |= REQUIREDIR;
	} else {
		state->slashes = 0;
		cnp->cn_flags &= ~REQUIREDIR;
	}
	/*
	 * We do special processing on the last component, whether or not it's
	 * a directory.  Cache all intervening lookups, but not the final one.
	 */
	if (*cp == '\0') {
		if (state->docache)
			cnp->cn_flags |= MAKEENTRY;
		else
			cnp->cn_flags &= ~MAKEENTRY;
		cnp->cn_flags |= ISLASTCN;
	} else {
		cnp->cn_flags |= MAKEENTRY;
		cnp->cn_flags &= ~ISLASTCN;
	}
	if (cnp->cn_namelen == 2 &&
	    cnp->cn_nameptr[1] == '.' && cnp->cn_nameptr[0] == '.')
		cnp->cn_flags |= ISDOTDOT;
	else
		cnp->cn_flags &= ~ISDOTDOT;

	return 0;
}

/*
 * Call VOP_LOOKUP for a single lookup; return a new search directory
 * (used when crossing mountpoints up or searching union mounts down) and 
 * the found object, which for create operations may be NULL on success.
 */
static int
lookup_once(struct namei_state *state,
	    struct vnode *searchdir,
	    struct vnode **newsearchdir_ret,
	    struct vnode **foundobj_ret)
{
	struct vnode *tmpvn;		/* scratch vnode */
	struct vnode *foundobj;		/* result */
	struct mount *mp;		/* mount table entry */
	struct lwp *l = curlwp;
	int error;

	struct componentname *cnp = state->cnp;
	struct nameidata *ndp = state->ndp;

	KASSERT(cnp == &ndp->ni_cnd);
	KASSERT(VOP_ISLOCKED(searchdir) == LK_EXCLUSIVE);
	*newsearchdir_ret = searchdir;

	/*
	 * Handle "..": two special cases.
	 * 1. If at root directory (e.g. after chroot)
	 *    or at absolute root directory
	 *    then ignore it so can't get out.
	 * 1a. If at the root of the emulation filesystem go to the real
	 *    root. So "/../<path>" is always absolute.
	 * 1b. If we have somehow gotten out of a jail, warn
	 *    and also ignore it so we can't get farther out.
	 * 2. If this vnode is the root of a mounted
	 *    filesystem, then replace it with the
	 *    vnode which was mounted on so we take the
	 *    .. in the other file system.
	 */
	if (cnp->cn_flags & ISDOTDOT) {
		struct proc *p = l->l_proc;

		for (;;) {
			if (searchdir == ndp->ni_rootdir ||
			    searchdir == rootvnode) {
				foundobj = searchdir;
				vref(foundobj);
				*foundobj_ret = foundobj;
				error = 0;
				goto done;
			}
			if (ndp->ni_rootdir != rootvnode) {
				int retval;

				VOP_UNLOCK(searchdir);
				retval = vn_isunder(searchdir, ndp->ni_rootdir, l);
				vn_lock(searchdir, LK_EXCLUSIVE | LK_RETRY);
				if (!retval) {
				    /* Oops! We got out of jail! */
				    log(LOG_WARNING,
					"chrooted pid %d uid %d (%s) "
					"detected outside of its chroot\n",
					p->p_pid, kauth_cred_geteuid(l->l_cred),
					p->p_comm);
				    /* Put us at the jail root. */
				    vput(searchdir);
				    searchdir = NULL;
				    foundobj = ndp->ni_rootdir;
				    vref(foundobj);
				    vref(foundobj);
				    vn_lock(foundobj, LK_EXCLUSIVE | LK_RETRY);
				    *newsearchdir_ret = foundobj;
				    *foundobj_ret = foundobj;
				    error = 0;
				    goto done;
				}
			}
			if ((searchdir->v_vflag & VV_ROOT) == 0 ||
			    (cnp->cn_flags & NOCROSSMOUNT))
				break;
			tmpvn = searchdir;
			searchdir = searchdir->v_mount->mnt_vnodecovered;
			vref(searchdir);
			vput(tmpvn);
			vn_lock(searchdir, LK_EXCLUSIVE | LK_RETRY);
			*newsearchdir_ret = searchdir;
		}
	}

	/*
	 * We now have a segment name to search for, and a directory to search.
	 * Our vnode state here is that "searchdir" is held and locked.
	 */
unionlookup:
	foundobj = NULL;
	error = VOP_LOOKUP(searchdir, &foundobj, cnp);

	if (error != 0) {
#ifdef DIAGNOSTIC
		if (foundobj != NULL)
			panic("leaf `%s' should be empty", cnp->cn_nameptr);
#endif /* DIAGNOSTIC */
#ifdef NAMEI_DIAGNOSTIC
		printf("not found\n");
#endif /* NAMEI_DIAGNOSTIC */
		if ((error == ENOENT) &&
		    (searchdir->v_vflag & VV_ROOT) &&
		    (searchdir->v_mount->mnt_flag & MNT_UNION)) {
			tmpvn = searchdir;
			searchdir = searchdir->v_mount->mnt_vnodecovered;
			vref(searchdir);
			vput(tmpvn);
			vn_lock(searchdir, LK_EXCLUSIVE | LK_RETRY);
			*newsearchdir_ret = searchdir;
			goto unionlookup;
		}

		if (error != EJUSTRETURN)
			goto done;

		/*
		 * If this was not the last component, or there were trailing
		 * slashes, and we are not going to create a directory,
		 * then the name must exist.
		 */
		if ((cnp->cn_flags & (REQUIREDIR | CREATEDIR)) == REQUIREDIR) {
			error = ENOENT;
			goto done;
		}

		/*
		 * If creating and at end of pathname, then can consider
		 * allowing file to be created.
		 */
		if (state->rdonly) {
			error = EROFS;
			goto done;
		}

		/*
		 * We return success and a NULL foundobj to indicate
		 * that the entry doesn't currently exist, leaving a
		 * pointer to the (normally, locked) directory vnode
		 * as searchdir.
		 */
		*foundobj_ret = NULL;
		error = 0;
		goto done;
	}
#ifdef NAMEI_DIAGNOSTIC
	printf("found\n");
#endif /* NAMEI_DIAGNOSTIC */

	/*
	 * Take into account any additional components consumed by the
	 * underlying filesystem.  This will include any trailing slashes after
	 * the last component consumed.
	 */
	if (cnp->cn_consume > 0) {
		ndp->ni_pathlen -= cnp->cn_consume - state->slashes;
		ndp->ni_next += cnp->cn_consume - state->slashes;
		cnp->cn_consume = 0;
		if (ndp->ni_next[0] == '\0')
			cnp->cn_flags |= ISLASTCN;
	}

	/*
	 * "searchdir" is locked and held, "foundobj" is held,
	 * they may be the same vnode.
	 */
	if (searchdir != foundobj) {
		if (cnp->cn_flags & ISDOTDOT)
			VOP_UNLOCK(searchdir);
		error = vn_lock(foundobj, LK_EXCLUSIVE);
		if (cnp->cn_flags & ISDOTDOT)
			vn_lock(searchdir, LK_EXCLUSIVE | LK_RETRY);
		if (error != 0) {
			vrele(foundobj);
			goto done;
		}
	}

	/*
	 * Check to see if the vnode has been mounted on;
	 * if so find the root of the mounted file system.
	 */
	while (foundobj->v_type == VDIR &&
	       (mp = foundobj->v_mountedhere) != NULL &&
	       (cnp->cn_flags & NOCROSSMOUNT) == 0) {
		error = vfs_busy(mp, NULL);
		if (error != 0) {
			if (searchdir != foundobj) {
				vput(foundobj);
			} else {
				vrele(foundobj);
			}
			goto done;
		}
		if (searchdir != foundobj) {
			VOP_UNLOCK(searchdir);
		}
		vput(foundobj);
		error = VFS_ROOT(mp, &foundobj);
		vfs_unbusy(mp, false, NULL);
		if (error) {
			vn_lock(searchdir, LK_EXCLUSIVE | LK_RETRY);
			goto done;
		}
		/*
		 * avoid locking vnodes from two filesystems because it's
		 * prune to deadlock.  eg. when using puffs.
		 * also, it isn't a good idea to propagate slowness of a
		 * filesystem up to the root directory.
		 * for now, only handle the common case.  (ie. foundobj is VDIR)
		 */
		if (foundobj->v_type == VDIR) {
			vrele(searchdir);
			*newsearchdir_ret = searchdir = foundobj;
			vref(searchdir);
		} else {
			VOP_UNLOCK(foundobj);
			vn_lock(searchdir, LK_EXCLUSIVE | LK_RETRY);
			vn_lock(foundobj, LK_EXCLUSIVE | LK_RETRY);
		}
	}

	*foundobj_ret = foundobj;
	error = 0;
done:
	KASSERT(VOP_ISLOCKED(*newsearchdir_ret) == LK_EXCLUSIVE);
	/*
	 * *foundobj_ret is valid only if error == 0.
	 */
	KASSERT(error != 0 || *foundobj_ret == NULL ||
	    VOP_ISLOCKED(*foundobj_ret) == LK_EXCLUSIVE);
	return error;
}

//////////////////////////////

/*
 * Do a complete path search from a single root directory.
 * (This is called up to twice if TRYEMULROOT is in effect.)
 */
static int
namei_oneroot(struct namei_state *state,
	 int neverfollow, int inhibitmagic, int isnfsd)
{
	struct nameidata *ndp = state->ndp;
	struct componentname *cnp = state->cnp;
	struct vnode *searchdir, *foundobj;
	int error;

	error = namei_start(state, isnfsd, &searchdir);
	if (error) {
		ndp->ni_dvp = NULL;
		ndp->ni_vp = NULL;
		return error;
	}
	KASSERT(searchdir->v_type == VDIR);

	/*
	 * Setup: break out flag bits into variables.
	 */
	state->docache = (cnp->cn_flags & NOCACHE) ^ NOCACHE;
	if (cnp->cn_nameiop == DELETE)
		state->docache = 0;
	state->rdonly = cnp->cn_flags & RDONLY;

	/*
	 * Keep going until we run out of path components.
	 */
	cnp->cn_nameptr = ndp->ni_pnbuf;

	/* drop leading slashes (already used them to choose startdir) */
	while (cnp->cn_nameptr[0] == '/') {
		cnp->cn_nameptr++;
		ndp->ni_pathlen--;
	}
	/* was it just "/"? */
	if (cnp->cn_nameptr[0] == '\0') {
		foundobj = searchdir;
		searchdir = NULL;
		cnp->cn_flags |= ISLASTCN;

		/* bleh */
		goto skiploop;
	}

	for (;;) {

		/*
		 * If the directory we're on is unmounted, bail out.
		 * XXX: should this also check if it's unlinked?
		 * XXX: yes it should... but how?
		 */
		if (searchdir->v_mount == NULL) {
			vput(searchdir);
			ndp->ni_dvp = NULL;
			ndp->ni_vp = NULL;
			return (ENOENT);
		}

		/*
		 * Look up the next path component.
		 * (currently, this may consume more than one)
		 */

		/* There should be no slashes here. */
		KASSERT(cnp->cn_nameptr[0] != '/');

		/* and we shouldn't have looped around if we were done */
		KASSERT(cnp->cn_nameptr[0] != '\0');

		error = lookup_parsepath(state);
		if (error) {
			vput(searchdir);
			ndp->ni_dvp = NULL;
			ndp->ni_vp = NULL;
			state->attempt_retry = 1;
			return (error);
		}

		error = lookup_once(state, searchdir, &searchdir, &foundobj);
		if (error) {
			vput(searchdir);
			ndp->ni_dvp = NULL;
			ndp->ni_vp = NULL;
			/*
			 * Note that if we're doing TRYEMULROOT we can
			 * retry with the normal root. Where this is
			 * currently set matches previous practice,
			 * but the previous practice didn't make much
			 * sense and somebody should sit down and
			 * figure out which cases should cause retry
			 * and which shouldn't. XXX.
			 */
			state->attempt_retry = 1;
			return (error);
		}

		if (foundobj == NULL) {
			/*
			 * Success with no object returned means we're
			 * creating something and it isn't already
			 * there. Break out of the main loop now so
			 * the code below doesn't have to test for
			 * foundobj == NULL.
			 */
			break;
		}

		/*
		 * Check for symbolic link. If we've reached one,
		 * follow it, unless we aren't supposed to. Back up
		 * over any slashes that we skipped, as we will need
		 * them again.
		 */
		if (namei_atsymlink(state, foundobj)) {
			ndp->ni_pathlen += state->slashes;
			ndp->ni_next -= state->slashes;
			if (neverfollow) {
				error = EINVAL;
			} else {
				/*
				 * dholland 20110410: if we're at a
				 * union mount it might make sense to
				 * use the top of the union stack here
				 * rather than the layer we found the
				 * symlink in. (FUTURE)
				 */
				error = namei_follow(state, inhibitmagic,
						     searchdir, foundobj,
						     &searchdir);
			}
			if (error) {
				KASSERT(searchdir != foundobj);
				vput(searchdir);
				vput(foundobj);
				ndp->ni_dvp = NULL;
				ndp->ni_vp = NULL;
				return error;
			}
			/* namei_follow unlocks it (ugh) so rele, not put */
			vrele(foundobj);
			foundobj = NULL;

			/*
			 * If we followed a symlink to `/' and there
			 * are no more components after the symlink,
			 * we're done with the loop and what we found
			 * is the searchdir.
			 */
			if (cnp->cn_nameptr[0] == '\0') {
				foundobj = searchdir;
				searchdir = NULL;
				cnp->cn_flags |= ISLASTCN;
				break;
			}

			continue;
		}

		/*
		 * Not a symbolic link.
		 *
		 * Check for directory, if the component was
		 * followed by a series of slashes.
		 */
		if ((foundobj->v_type != VDIR) &&
		    (cnp->cn_flags & REQUIREDIR)) {
			if (searchdir == foundobj) {
				vrele(searchdir);
			} else {
				vput(searchdir);
			}
			vput(foundobj);
			ndp->ni_dvp = NULL;
			ndp->ni_vp = NULL;
			state->attempt_retry = 1;
			return ENOTDIR;
		}

		/*
		 * Stop if we've reached the last component.
		 */
		if (cnp->cn_flags & ISLASTCN) {
			break;
		}

		/*
		 * Continue with the next component.
		 */
		cnp->cn_nameptr = ndp->ni_next;
		if (searchdir == foundobj) {
			vrele(searchdir);
		} else {
			vput(searchdir);
		}
		searchdir = foundobj;
		foundobj = NULL;
	}

 skiploop:

	if (foundobj != NULL) {
		if (foundobj == ndp->ni_erootdir) {
			/*
			 * We are about to return the emulation root.
			 * This isn't a good idea because code might
			 * repeatedly lookup ".." until the file
			 * matches that returned for "/" and loop
			 * forever.  So convert it to the real root.
			 */
			if (searchdir != NULL) {
				if (searchdir == foundobj)
					vrele(searchdir);
				else
					vput(searchdir);
				searchdir = NULL;
			}
			vput(foundobj);
			foundobj = ndp->ni_rootdir;
			vref(foundobj);
			vn_lock(foundobj, LK_EXCLUSIVE | LK_RETRY);
		}

		/*
		 * If the caller requested the parent node (i.e. it's
		 * a CREATE, DELETE, or RENAME), and we don't have one
		 * (because this is the root directory, or we crossed
		 * a mount point), then we must fail.
		 */
		if (cnp->cn_nameiop != LOOKUP &&
		    (searchdir == NULL ||
		     searchdir->v_mount != foundobj->v_mount)) {
			if (searchdir) {
				vput(searchdir);
			}
			vput(foundobj);
			foundobj = NULL;
			ndp->ni_dvp = NULL;
			ndp->ni_vp = NULL;
			state->attempt_retry = 1;

			switch (cnp->cn_nameiop) {
			    case CREATE:
				return EEXIST;
			    case DELETE:
			    case RENAME:
				return EBUSY;
			    default:
				break;
			}
			panic("Invalid nameiop\n");
		}

		/*
		 * Disallow directory write attempts on read-only lookups.
		 * Prefers EEXIST over EROFS for the CREATE case.
		 */
		if (state->rdonly &&
		    (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME)) {
			if (searchdir) {
				if (foundobj != searchdir) {
					vput(searchdir);
				} else {
					vrele(searchdir);
				}
				searchdir = NULL;
			}
			vput(foundobj);
			foundobj = NULL;
			ndp->ni_dvp = NULL;
			ndp->ni_vp = NULL;
			state->attempt_retry = 1;
			return EROFS;
		}
		if ((cnp->cn_flags & LOCKLEAF) == 0) {
			/*
			 * Note: if LOCKPARENT but not LOCKLEAF is
			 * set, and searchdir == foundobj, this code
			 * necessarily unlocks the parent as well as
			 * the leaf. That is, just because you specify
			 * LOCKPARENT doesn't mean you necessarily get
			 * a locked parent vnode. The code in
			 * vfs_syscalls.c, and possibly elsewhere,
			 * that uses this combination "knows" this, so
			 * it can't be safely changed. Feh. XXX
			 */
			VOP_UNLOCK(foundobj);
		}
	}

	/*
	 * Done.
	 */

	/*
	 * If LOCKPARENT is not set, the parent directory isn't returned.
	 */
	if ((cnp->cn_flags & LOCKPARENT) == 0 && searchdir != NULL) {
		if (searchdir == foundobj) {
			vrele(searchdir);
		} else {
			vput(searchdir);
		}
		searchdir = NULL;
	}

	ndp->ni_dvp = searchdir;
	ndp->ni_vp = foundobj;
	return 0;
}

/*
 * Do namei; wrapper layer that handles TRYEMULROOT.
 */
static int
namei_tryemulroot(struct namei_state *state,
	 int neverfollow, int inhibitmagic, int isnfsd)
{
	int error;

	struct nameidata *ndp = state->ndp;
	struct componentname *cnp = state->cnp;
	const char *savepath = NULL;

	KASSERT(cnp == &ndp->ni_cnd);

	if (cnp->cn_flags & TRYEMULROOT) {
		savepath = pathbuf_stringcopy_get(ndp->ni_pathbuf);
	}

    emul_retry:
	state->attempt_retry = 0;

	error = namei_oneroot(state, neverfollow, inhibitmagic, isnfsd);
	if (error) {
		/*
		 * Once namei has started up, the existence of ni_erootdir
		 * tells us whether we're working from an emulation root.
		 * The TRYEMULROOT flag isn't necessarily authoritative.
		 */
		if (ndp->ni_erootdir != NULL && state->attempt_retry) {
			/* Retry the whole thing using the normal root */
			cnp->cn_flags &= ~TRYEMULROOT;
			state->attempt_retry = 0;

			/* kinda gross */
			strcpy(ndp->ni_pathbuf->pb_path, savepath);
			pathbuf_stringcopy_put(ndp->ni_pathbuf, savepath);
			savepath = NULL;

			goto emul_retry;
		}
	}
	if (savepath != NULL) {
		pathbuf_stringcopy_put(ndp->ni_pathbuf, savepath);
	}
	return error;
}

/*
 * External interface.
 */
int
namei(struct nameidata *ndp)
{
	struct namei_state state;
	int error;

	namei_init(&state, ndp);
	error = namei_tryemulroot(&state,
				  0/*!neverfollow*/, 0/*!inhibitmagic*/,
				  0/*isnfsd*/);
	namei_cleanup(&state);

	if (error) {
		/* make sure no stray refs leak out */
		KASSERT(ndp->ni_dvp == NULL);
		KASSERT(ndp->ni_vp == NULL);
	}

	return error;
}

////////////////////////////////////////////////////////////

/*
 * External interface used by nfsd. This is basically different from
 * namei only in that it has the ability to pass in the "current
 * directory", and uses an extra flag "neverfollow" for which there's
 * no physical flag defined in namei.h. (There used to be a cut&paste
 * copy of about half of namei in nfsd to allow these minor
 * adjustments to exist.)
 *
 * XXX: the namei interface should be adjusted so nfsd can just use
 * ordinary namei().
 */
int
lookup_for_nfsd(struct nameidata *ndp, struct vnode *forcecwd, int neverfollow)
{
	struct namei_state state;
	int error;

	KASSERT(ndp->ni_atdir == NULL);
	ndp->ni_atdir = forcecwd;

	namei_init(&state, ndp);
	error = namei_tryemulroot(&state,
				  neverfollow, 1/*inhibitmagic*/, 1/*isnfsd*/);
	namei_cleanup(&state);

	if (error) {
		/* make sure no stray refs leak out */
		KASSERT(ndp->ni_dvp == NULL);
		KASSERT(ndp->ni_vp == NULL);
	}

	return error;
}

/*
 * A second external interface used by nfsd. This turns out to be a
 * single lookup used by the WebNFS code (ha!) to get "index.html" or
 * equivalent when asked for a directory. It should eventually evolve
 * into some kind of namei_once() call; for the time being it's kind
 * of a mess. XXX.
 *
 * dholland 20110109: I don't think it works, and I don't think it
 * worked before I started hacking and slashing either, and I doubt
 * anyone will ever notice.
 */

/*
 * Internals. This calls lookup_once() after setting up the assorted
 * pieces of state the way they ought to be.
 */
static int
do_lookup_for_nfsd_index(struct namei_state *state)
{
	int error = 0;

	struct componentname *cnp = state->cnp;
	struct nameidata *ndp = state->ndp;
	struct vnode *startdir;
	struct vnode *foundobj;
	const char *cp;			/* pointer into pathname argument */

	KASSERT(cnp == &ndp->ni_cnd);

	startdir = state->ndp->ni_atdir;

	cnp->cn_nameptr = ndp->ni_pnbuf;
	state->docache = 1;
	state->rdonly = cnp->cn_flags & RDONLY;
	ndp->ni_dvp = NULL;

	cnp->cn_consume = 0;
	cnp->cn_namelen = namei_getcomponent(cnp->cn_nameptr);
	cp = cnp->cn_nameptr + cnp->cn_namelen;
	KASSERT(cnp->cn_namelen <= KERNEL_NAME_MAX);
	ndp->ni_pathlen -= cnp->cn_namelen;
	ndp->ni_next = cp;
	state->slashes = 0;
	cnp->cn_flags &= ~REQUIREDIR;
	cnp->cn_flags |= MAKEENTRY|ISLASTCN;

	if (cnp->cn_namelen == 2 &&
	    cnp->cn_nameptr[1] == '.' && cnp->cn_nameptr[0] == '.')
		cnp->cn_flags |= ISDOTDOT;
	else
		cnp->cn_flags &= ~ISDOTDOT;

	/*
	 * Because lookup_once can change the startdir, we need our
	 * own reference to it to avoid consuming the caller's.
	 */
	vref(startdir);
	vn_lock(startdir, LK_EXCLUSIVE | LK_RETRY);
	error = lookup_once(state, startdir, &startdir, &foundobj);
	if (error == 0 && startdir == foundobj) {
		vrele(startdir);
	} else {
		vput(startdir);
	}
	if (error) {
		goto bad;
	}
	ndp->ni_vp = foundobj;

	if (foundobj == NULL) {
		return 0;
	}

	KASSERT((cnp->cn_flags & LOCKPARENT) == 0);
	if ((cnp->cn_flags & LOCKLEAF) == 0) {
		VOP_UNLOCK(foundobj);
	}
	return (0);

bad:
	ndp->ni_vp = NULL;
	return (error);
}

/*
 * External interface. The partitioning between this function and the
 * above isn't very clear - the above function exists mostly so code
 * that uses "state->" can be shuffled around without having to change
 * it to "state.".
 */
int
lookup_for_nfsd_index(struct nameidata *ndp, struct vnode *startdir)
{
	struct namei_state state;
	int error;

	KASSERT(ndp->ni_atdir == NULL);
	ndp->ni_atdir = startdir;

	/*
	 * Note: the name sent in here (is not|should not be) allowed
	 * to contain a slash.
	 */
	if (strlen(ndp->ni_pathbuf->pb_path) > KERNEL_NAME_MAX) {
		return ENAMETOOLONG;
	}
	if (strchr(ndp->ni_pathbuf->pb_path, '/')) {
		return EINVAL;
	}

	ndp->ni_pathlen = strlen(ndp->ni_pathbuf->pb_path) + 1;
	ndp->ni_pnbuf = NULL;
	ndp->ni_cnd.cn_nameptr = NULL;

	namei_init(&state, ndp);
	error = do_lookup_for_nfsd_index(&state);
	namei_cleanup(&state);

	return error;
}

////////////////////////////////////////////////////////////

/*
 * Reacquire a path name component.
 * dvp is locked on entry and exit.
 * *vpp is locked on exit unless it's NULL.
 */
int
relookup(struct vnode *dvp, struct vnode **vpp, struct componentname *cnp, int dummy)
{
	int rdonly;			/* lookup read-only flag bit */
	int error = 0;
#ifdef DEBUG
	size_t newlen;			/* DEBUG: check name len */
	const char *cp;			/* DEBUG: check name ptr */
#endif /* DEBUG */

	(void)dummy;

	/*
	 * Setup: break out flag bits into variables.
	 */
	rdonly = cnp->cn_flags & RDONLY;

	/*
	 * Search a new directory.
	 *
	 * The cn_hash value is for use by vfs_cache.
	 * The last component of the filename is left accessible via
	 * cnp->cn_nameptr for callers that need the name. Callers needing
	 * the name set the SAVENAME flag. When done, they assume
	 * responsibility for freeing the pathname buffer.
	 */
#ifdef DEBUG
#if 0
	cp = NULL;
	newhash = namei_hash(cnp->cn_nameptr, &cp);
	if ((uint32_t)newhash != (uint32_t)cnp->cn_hash)
		panic("relookup: bad hash");
#endif
	newlen = namei_getcomponent(cnp->cn_nameptr);
	if (cnp->cn_namelen != newlen)
		panic("relookup: bad len");
	cp = cnp->cn_nameptr + cnp->cn_namelen;
	while (*cp == '/')
		cp++;
	if (*cp != 0)
		panic("relookup: not last component");
#endif /* DEBUG */

	/*
	 * Check for degenerate name (e.g. / or "")
	 * which is a way of talking about a directory,
	 * e.g. like "/." or ".".
	 */
	if (cnp->cn_nameptr[0] == '\0')
		panic("relookup: null name");

	if (cnp->cn_flags & ISDOTDOT)
		panic("relookup: lookup on dot-dot");

	/*
	 * We now have a segment name to search for, and a directory to search.
	 */
	*vpp = NULL;
	error = VOP_LOOKUP(dvp, vpp, cnp);
	if ((error) != 0) {
#ifdef DIAGNOSTIC
		if (*vpp != NULL)
			panic("leaf `%s' should be empty", cnp->cn_nameptr);
#endif
		if (error != EJUSTRETURN)
			goto bad;
	}

#ifdef DIAGNOSTIC
	/*
	 * Check for symbolic link
	 */
	if (*vpp && (*vpp)->v_type == VLNK && (cnp->cn_flags & FOLLOW))
		panic("relookup: symlink found");
#endif

	/*
	 * Check for read-only lookups.
	 */
	if (rdonly && cnp->cn_nameiop != LOOKUP) {
		error = EROFS;
		if (*vpp) {
			vrele(*vpp);
		}
		goto bad;
	}
	/*
	 * Lock result.
	 */
	if (*vpp && *vpp != dvp) {
		error = vn_lock(*vpp, LK_EXCLUSIVE);
		if (error != 0) {
			vrele(*vpp);
			goto bad;
		}
	}
	return (0);

bad:
	*vpp = NULL;
	return (error);
}

/*
 * namei_simple - simple forms of namei.
 *
 * These are wrappers to allow the simple case callers of namei to be
 * left alone while everything else changes under them.
 */

/* Flags */
struct namei_simple_flags_type {
	int dummy;
};
static const struct namei_simple_flags_type ns_nn, ns_nt, ns_fn, ns_ft;
const namei_simple_flags_t NSM_NOFOLLOW_NOEMULROOT = &ns_nn;
const namei_simple_flags_t NSM_NOFOLLOW_TRYEMULROOT = &ns_nt;
const namei_simple_flags_t NSM_FOLLOW_NOEMULROOT = &ns_fn;
const namei_simple_flags_t NSM_FOLLOW_TRYEMULROOT = &ns_ft;

static
int
namei_simple_convert_flags(namei_simple_flags_t sflags)
{
	if (sflags == NSM_NOFOLLOW_NOEMULROOT)
		return NOFOLLOW | 0;
	if (sflags == NSM_NOFOLLOW_TRYEMULROOT)
		return NOFOLLOW | TRYEMULROOT;
	if (sflags == NSM_FOLLOW_NOEMULROOT)
		return FOLLOW | 0;
	if (sflags == NSM_FOLLOW_TRYEMULROOT)
		return FOLLOW | TRYEMULROOT;
	panic("namei_simple_convert_flags: bogus sflags\n");
	return 0;
}

int
namei_simple_kernel(const char *path, namei_simple_flags_t sflags,
	struct vnode **vp_ret)
{
	return nameiat_simple_kernel(NULL, path, sflags, vp_ret);
}

int
nameiat_simple_kernel(struct vnode *dvp, const char *path, 
	namei_simple_flags_t sflags, struct vnode **vp_ret)
{
	struct nameidata nd;
	struct pathbuf *pb;
	int err;

	pb = pathbuf_create(path);
	if (pb == NULL) {
		return ENOMEM;
	}

	NDINIT(&nd,
		LOOKUP,
		namei_simple_convert_flags(sflags),
		pb);

	if (dvp != NULL)
		NDAT(&nd, dvp);

	err = namei(&nd);
	if (err != 0) {
		pathbuf_destroy(pb);
		return err;
	}
	*vp_ret = nd.ni_vp;
	pathbuf_destroy(pb);
	return 0;
}

int
namei_simple_user(const char *path, namei_simple_flags_t sflags,
	struct vnode **vp_ret)
{
	return nameiat_simple_user(NULL, path, sflags, vp_ret);
}

int
nameiat_simple_user(struct vnode *dvp, const char *path,
	namei_simple_flags_t sflags, struct vnode **vp_ret)
{
	struct pathbuf *pb;
	struct nameidata nd;
	int err;

	err = pathbuf_copyin(path, &pb);
	if (err) {
		return err;
	}

	NDINIT(&nd,
		LOOKUP,
		namei_simple_convert_flags(sflags),
		pb);

	if (dvp != NULL)
		NDAT(&nd, dvp);

	err = namei(&nd);
	if (err != 0) {
		pathbuf_destroy(pb);
		return err;
	}
	*vp_ret = nd.ni_vp;
	pathbuf_destroy(pb);
	return 0;
}
