/*	$NetBSD: kern_veriexec.c,v 1.11 2015/08/04 12:44:04 maxv Exp $	*/

/*-
 * Copyright (c) 2005, 2006 Elad Efrat <elad@NetBSD.org>
 * Copyright (c) 2005, 2006 Brett Lymn <blymn@NetBSD.org>
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
 * 3. The name of the authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_veriexec.c,v 1.11 2015/08/04 12:44:04 maxv Exp $");

#include "opt_veriexec.h"

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/kmem.h>
#include <sys/vnode.h>
#include <sys/namei.h>
#include <sys/once.h>
#include <sys/proc.h>
#include <sys/rwlock.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>
#include <sys/inttypes.h>
#include <sys/verified_exec.h>
#include <sys/sha1.h>
#include <sys/sha2.h>
#include <sys/rmd160.h>
#include <sys/md5.h>
#include <sys/fileassoc.h>
#include <sys/kauth.h>
#include <sys/conf.h>
#include <miscfs/specfs/specdev.h>
#include <prop/proplib.h>
#include <sys/fcntl.h>

/* Readable values for veriexec_file_report(). */
#define	REPORT_ALWAYS		0x01	/* Always print */
#define	REPORT_VERBOSE		0x02	/* Print when verbose >= 1 */
#define	REPORT_DEBUG		0x04	/* Print when verbose >= 2 (debug) */
#define	REPORT_PANIC		0x08	/* Call panic() */
#define	REPORT_ALARM		0x10	/* Alarm - also print pid/uid/.. */
#define	REPORT_LOGMASK		(REPORT_ALWAYS|REPORT_VERBOSE|REPORT_DEBUG)

/* state of locking for veriexec_file_verify */
#define VERIEXEC_UNLOCKED	0x00	/* Nothing locked, callee does it */
#define VERIEXEC_LOCKED		0x01	/* Global op lock held */

/* state of file locking for veriexec_file_verify */
#define VERIEXEC_FILE_UNLOCKED	0x02	/* Nothing locked, callee does it */
#define VERIEXEC_FILE_LOCKED	0x04	/* File locked */

#define VERIEXEC_RW_UPGRADE(lock)	while((rw_tryupgrade(lock)) == 0){};

struct veriexec_fpops {
	const char *type;
	size_t hash_len;
	size_t context_size;
	veriexec_fpop_init_t init;
	veriexec_fpop_update_t update;
	veriexec_fpop_final_t final;
	LIST_ENTRY(veriexec_fpops) entries;
};

/* Veriexec per-file entry data. */
struct veriexec_file_entry {
	krwlock_t lock;				/* r/w lock */
	u_char *filename;			/* File name. */
	u_char type;				/* Entry type. */
	u_char status;				/* Evaluation status. */
	u_char *fp;				/* Fingerprint. */
	struct veriexec_fpops *ops;		/* Fingerprint ops vector*/
	size_t filename_len;			/* Length of filename. */
};

/* Veriexec per-table data. */
struct veriexec_table_entry {
	uint64_t vte_count;			/* Number of Veriexec entries. */
	const struct sysctlnode *vte_node;
};

static int veriexec_verbose;
static int veriexec_strict;
static int veriexec_bypass = 1;

static char *veriexec_fp_names = NULL;
static size_t veriexec_name_max = 0;

static const struct sysctlnode *veriexec_count_node;

static fileassoc_t veriexec_hook;
static specificdata_key_t veriexec_mountspecific_key;

static LIST_HEAD(, veriexec_fpops) veriexec_fpops_list =
	LIST_HEAD_INITIALIZER(veriexec_fpops_list);

static int veriexec_raw_cb(kauth_cred_t, kauth_action_t, void *,
    void *, void *, void *, void *);
static struct veriexec_fpops *veriexec_fpops_lookup(const char *);
static void veriexec_file_free(struct veriexec_file_entry *);

static unsigned int veriexec_tablecount = 0;

/*
 * Veriexec operations global lock - most ops hold this as a read
 * lock, it is upgraded to a write lock when destroying veriexec file
 * table entries.
 */
static krwlock_t veriexec_op_lock;

/*
 * Sysctl helper routine for Veriexec.
 */
static int
sysctl_kern_veriexec_algorithms(SYSCTLFN_ARGS)
{
	size_t len;
	int error;
	const char *p;

	if (newp != NULL)
		return EPERM;

	if (namelen != 0)
		return EINVAL;

	p = veriexec_fp_names == NULL ? "" : veriexec_fp_names;

	len = strlen(p) + 1;

	if (*oldlenp < len && oldp)
		return ENOMEM;

	if (oldp && (error = copyout(p, oldp, len)) != 0)
		return error;

	*oldlenp = len;
	return 0;
}

static int
sysctl_kern_veriexec_strict(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	int error, newval;

	node = *rnode;
	node.sysctl_data = &newval;

	newval = veriexec_strict;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

	if (newval < veriexec_strict)
		return EPERM;

	veriexec_strict = newval;

	return 0;
}

SYSCTL_SETUP(sysctl_kern_veriexec_setup, "sysctl kern.veriexec setup")
{
	const struct sysctlnode *rnode = NULL;

	sysctl_createv(clog, 0, NULL, &rnode,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "veriexec",
		       SYSCTL_DESCR("Veriexec"),
		       NULL, 0, NULL, 0,
		       CTL_KERN, CTL_CREATE, CTL_EOL);

	sysctl_createv(clog, 0, &rnode, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "verbose",
		       SYSCTL_DESCR("Veriexec verbose level"),
		       NULL, 0, &veriexec_verbose, 0,
		       CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, &rnode, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "strict",
		       SYSCTL_DESCR("Veriexec strict level"),
		       sysctl_kern_veriexec_strict, 0, NULL, 0,
		       CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, &rnode, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_STRING, "algorithms",
		       SYSCTL_DESCR("Veriexec supported hashing "
				    "algorithms"),
		       sysctl_kern_veriexec_algorithms, 0, NULL, 0,
		       CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, &rnode, &veriexec_count_node,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "count",
		       SYSCTL_DESCR("Number of fingerprints on mount(s)"),
		       NULL, 0, NULL, 0,
		       CTL_CREATE, CTL_EOL);
}

/*
 * Add ops to the fingerprint ops vector list.
 */
int
veriexec_fpops_add(const char *fp_type, size_t hash_len, size_t ctx_size,
    veriexec_fpop_init_t init, veriexec_fpop_update_t update,
    veriexec_fpop_final_t final)
{
	struct veriexec_fpops *ops;

	KASSERT((init != NULL) && (update != NULL) && (final != NULL));
	KASSERT((hash_len != 0) && (ctx_size != 0));
	KASSERT(fp_type != NULL);

	if (veriexec_fpops_lookup(fp_type) != NULL)
		return (EEXIST);

	ops = kmem_alloc(sizeof(*ops), KM_SLEEP);
	ops->type = fp_type;
	ops->hash_len = hash_len;
	ops->context_size = ctx_size;
	ops->init = init;
	ops->update = update;
	ops->final = final;

	LIST_INSERT_HEAD(&veriexec_fpops_list, ops, entries);

	/*
	 * If we don't have space for any names, allocate enough for six
	 * which should be sufficient. (it's also enough for all algorithms
	 * we can support at the moment)
	 */
	if (veriexec_fp_names == NULL) {
		veriexec_name_max = 64;
		veriexec_fp_names = kmem_zalloc(veriexec_name_max, KM_SLEEP);
	}

	/*
	 * If we're running out of space for storing supported algorithms,
	 * extend the buffer with space for four names.
	 */
	while (veriexec_name_max - (strlen(veriexec_fp_names) + 1) <
	    strlen(fp_type)) {
		char *newp;
		unsigned int new_max;

		/* Add space for four algorithm names. */
		new_max = veriexec_name_max + 64;
		newp = kmem_zalloc(new_max, KM_SLEEP);
		strlcpy(newp, veriexec_fp_names, new_max);
		kmem_free(veriexec_fp_names, veriexec_name_max);
		veriexec_fp_names = newp;
		veriexec_name_max = new_max;
	}

	if (*veriexec_fp_names != '\0')
		strlcat(veriexec_fp_names, " ", veriexec_name_max);

	strlcat(veriexec_fp_names, fp_type, veriexec_name_max);

	return (0);
}

static void
veriexec_mountspecific_dtor(void *v)
{
	struct veriexec_table_entry *vte = v;

	if (vte == NULL) {
		return;
	}
	sysctl_free(__UNCONST(vte->vte_node));
	veriexec_tablecount--;
	kmem_free(vte, sizeof(*vte));
}

static int
veriexec_listener_cb(kauth_cred_t cred, kauth_action_t action, void *cookie,
    void *arg0, void *arg1, void *arg2, void *arg3)
{
	int result;
	enum kauth_system_req req;

	if (action != KAUTH_SYSTEM_VERIEXEC)
		return KAUTH_RESULT_DEFER;

	result = KAUTH_RESULT_DEFER;
	req = (enum kauth_system_req)arg0;

	if (req == KAUTH_REQ_SYSTEM_VERIEXEC_MODIFY &&
	    veriexec_strict > VERIEXEC_LEARNING) {
		log(LOG_WARNING, "Veriexec: Strict mode, modifying "
		    "tables not permitted.\n");

		result = KAUTH_RESULT_DENY;
	}

	return result;
}

/*
 * Initialise Veriexec.
 */
void
veriexec_init(void)
{
	int error;

	/* Register a fileassoc for Veriexec. */
	error = fileassoc_register("veriexec",
	    (fileassoc_cleanup_cb_t)veriexec_file_free, &veriexec_hook);
	if (error)
		panic("Veriexec: Can't register fileassoc: error=%d", error);

	/* Register listener to handle raw disk access. */
	if (kauth_listen_scope(KAUTH_SCOPE_DEVICE, veriexec_raw_cb, NULL) ==
	    NULL)
		panic("Veriexec: Can't listen on device scope");

	error = mount_specific_key_create(&veriexec_mountspecific_key,
	    veriexec_mountspecific_dtor);
	if (error)
		panic("Veriexec: Can't create mountspecific key");

	if (kauth_listen_scope(KAUTH_SCOPE_SYSTEM, veriexec_listener_cb,
	    NULL) == NULL)
		panic("Veriexec: Can't listen on system scope");

	rw_init(&veriexec_op_lock);

#define	FPOPS_ADD(a, b, c, d, e, f)	\
	veriexec_fpops_add(a, b, c, (veriexec_fpop_init_t)d, \
	 (veriexec_fpop_update_t)e, (veriexec_fpop_final_t)f)

#ifdef VERIFIED_EXEC_FP_RMD160
	FPOPS_ADD("RMD160", RMD160_DIGEST_LENGTH, sizeof(RMD160_CTX),
	    RMD160Init, RMD160Update, RMD160Final);
#endif /* VERIFIED_EXEC_FP_RMD160 */

#ifdef VERIFIED_EXEC_FP_SHA256
	FPOPS_ADD("SHA256", SHA256_DIGEST_LENGTH, sizeof(SHA256_CTX),
	    SHA256_Init, SHA256_Update, SHA256_Final);
#endif /* VERIFIED_EXEC_FP_SHA256 */

#ifdef VERIFIED_EXEC_FP_SHA384
	FPOPS_ADD("SHA384", SHA384_DIGEST_LENGTH, sizeof(SHA384_CTX),
	    SHA384_Init, SHA384_Update, SHA384_Final);
#endif /* VERIFIED_EXEC_FP_SHA384 */

#ifdef VERIFIED_EXEC_FP_SHA512
	FPOPS_ADD("SHA512", SHA512_DIGEST_LENGTH, sizeof(SHA512_CTX),
	    SHA512_Init, SHA512_Update, SHA512_Final);
#endif /* VERIFIED_EXEC_FP_SHA512 */

#ifdef VERIFIED_EXEC_FP_SHA1
	FPOPS_ADD("SHA1", SHA1_DIGEST_LENGTH, sizeof(SHA1_CTX),
	    SHA1Init, SHA1Update, SHA1Final);
#endif /* VERIFIED_EXEC_FP_SHA1 */

#ifdef VERIFIED_EXEC_FP_MD5
	FPOPS_ADD("MD5", MD5_DIGEST_LENGTH, sizeof(MD5_CTX),
	    MD5Init, MD5Update, MD5Final);
#endif /* VERIFIED_EXEC_FP_MD5 */

#undef FPOPS_ADD
}

static struct veriexec_fpops *
veriexec_fpops_lookup(const char *name)
{
	struct veriexec_fpops *ops;

	if (name == NULL)
		return (NULL);

	LIST_FOREACH(ops, &veriexec_fpops_list, entries) {
		if (strcasecmp(name, ops->type) == 0)
			return (ops);
	}

	return (NULL);
}

/*
 * Calculate fingerprint. Information on hash length and routines used is
 * extracted from veriexec_hash_list according to the hash type.
 *
 * NOTE: vfe is assumed to be locked for writing on entry.
 */
static int
veriexec_fp_calc(struct lwp *l, struct vnode *vp, int file_lock_state,
    struct veriexec_file_entry *vfe, u_char *fp)
{
	struct vattr va;
	void *ctx;
	u_char *buf;
	off_t offset, len;
	size_t resid;
	int error;

	KASSERT(file_lock_state != VERIEXEC_LOCKED);
	KASSERT(file_lock_state != VERIEXEC_UNLOCKED);

	if (file_lock_state == VERIEXEC_FILE_UNLOCKED)
		vn_lock(vp, LK_SHARED | LK_RETRY);
	error = VOP_GETATTR(vp, &va, l->l_cred);
	if (file_lock_state == VERIEXEC_FILE_UNLOCKED)
		VOP_UNLOCK(vp);
	if (error)
		return (error);

	ctx = kmem_alloc(vfe->ops->context_size, KM_SLEEP);
	buf = kmem_alloc(PAGE_SIZE, KM_SLEEP);

	(vfe->ops->init)(ctx);

	len = 0;
	error = 0;
	for (offset = 0; offset < va.va_size; offset += PAGE_SIZE) {
		len = ((va.va_size - offset) < PAGE_SIZE) ?
		    (va.va_size - offset) : PAGE_SIZE;

		error = vn_rdwr(UIO_READ, vp, buf, len, offset,
				UIO_SYSSPACE,
				((file_lock_state == VERIEXEC_FILE_LOCKED)?
				 IO_NODELOCKED : 0),
				l->l_cred, &resid, NULL);

		if (error) {
			goto bad;
		}

		(vfe->ops->update)(ctx, buf, (unsigned int) len);

		if (len != PAGE_SIZE)
			break;
	}

	(vfe->ops->final)(fp, ctx);

bad:
	kmem_free(ctx, vfe->ops->context_size);
	kmem_free(buf, PAGE_SIZE);

	return (error);
}

/* Compare two fingerprints of the same type. */
static int
veriexec_fp_cmp(struct veriexec_fpops *ops, u_char *fp1, u_char *fp2)
{
	if (veriexec_verbose >= 2) {
		int i;

		printf("comparing hashes...\n");
		printf("fp1: ");
		for (i = 0; i < ops->hash_len; i++) {
			printf("%02x", fp1[i]);
		}
		printf("\nfp2: ");
		for (i = 0; i < ops->hash_len; i++) {
			printf("%02x", fp2[i]);
		}
		printf("\n");
	}

	return (memcmp(fp1, fp2, ops->hash_len));
}

static int
veriexec_fp_status(struct lwp *l, struct vnode *vp, int file_lock_state,
    struct veriexec_file_entry *vfe, u_char *status)
{
	size_t hash_len = vfe->ops->hash_len;
	u_char *digest;
	int error;

	digest = kmem_zalloc(hash_len, KM_SLEEP);

	error = veriexec_fp_calc(l, vp, file_lock_state, vfe, digest);
	if (error)
		goto out;

	/* Compare fingerprint with loaded data. */
	if (veriexec_fp_cmp(vfe->ops, vfe->fp, digest) == 0)
		*status = FINGERPRINT_VALID;
	else
		*status = FINGERPRINT_NOMATCH;

out:
	kmem_free(digest, hash_len);
	return error;
}


static struct veriexec_table_entry *
veriexec_table_lookup(struct mount *mp)
{
	/* XXX: From raidframe init */
	if (mp == NULL)
		return NULL;

	return mount_getspecific(mp, veriexec_mountspecific_key);
}

static struct veriexec_file_entry *
veriexec_get(struct vnode *vp)
{
	return (fileassoc_lookup(vp, veriexec_hook));
}

bool
veriexec_lookup(struct vnode *vp)
{
	return (veriexec_get(vp) == NULL ? false : true);
}

/*
 * Routine for maintaining mostly consistent message formats in Veriexec.
 */
static void
veriexec_file_report(struct veriexec_file_entry *vfe, const u_char *msg,
    const u_char *filename, struct lwp *l, int f)
{
	if (vfe != NULL && vfe->filename != NULL)
		filename = vfe->filename;
	if (filename == NULL)
		return;

	if (((f & REPORT_LOGMASK) >> 1) <= veriexec_verbose) {
		if (!(f & REPORT_ALARM) || (l == NULL))
			log(LOG_NOTICE, "Veriexec: %s [%s]\n", msg,
			    filename);
		else
			log(LOG_ALERT, "Veriexec: %s [%s, prog=%s pid=%u, "
			    "uid=%u, gid=%u]\n", msg, filename,
			    l->l_proc->p_comm, l->l_proc->p_pid,
			    kauth_cred_getuid(l->l_cred),
			    kauth_cred_getgid(l->l_cred));
	}

	if (f & REPORT_PANIC)
		panic("Veriexec: Unrecoverable error.");
}

/*
 * Verify the fingerprint of the given file. If we're called directly from
 * sys_execve(), 'flag' will be VERIEXEC_DIRECT. If we're called from
 * exec_script(), 'flag' will be VERIEXEC_INDIRECT.  If we are called from
 * vn_open(), 'flag' will be VERIEXEC_FILE.
 *
 * 'veriexec_op_lock' must be locked (and remains locked).
 *
 * NOTE: The veriexec file entry pointer (vfep) will be returned LOCKED
 *       on no error.
 */
static int
veriexec_file_verify(struct lwp *l, struct vnode *vp, const u_char *name,
    int flag, int file_lock_state, struct veriexec_file_entry **vfep)
{
	struct veriexec_file_entry *vfe;
	int error = 0;

	KASSERT(rw_lock_held(&veriexec_op_lock));
	KASSERT(file_lock_state != VERIEXEC_LOCKED);
	KASSERT(file_lock_state != VERIEXEC_UNLOCKED);

#define VFE_NEEDS_EVAL(vfe) ((vfe->status == FINGERPRINT_NOTEVAL) || \
			     (vfe->type & VERIEXEC_UNTRUSTED))

	if (vfep != NULL)
		*vfep = NULL;

	if (vp->v_type != VREG)
		return (0);

	/* Lookup veriexec table entry, save pointer if requested. */
	vfe = veriexec_get(vp);
	if (vfep != NULL)
		*vfep = vfe;

	/* No entry in the veriexec tables. */
	if (vfe == NULL) {
		veriexec_file_report(NULL, "No entry.", name,
		    l, REPORT_VERBOSE);

		/*
		 * Lockdown mode: Deny access to non-monitored files.
		 * IPS mode: Deny execution of non-monitored files.
		 */
		if ((veriexec_strict >= VERIEXEC_LOCKDOWN) ||
		    ((veriexec_strict >= VERIEXEC_IPS) &&
		     (flag != VERIEXEC_FILE)))
			return (EPERM);

		return (0);
	}

	/*
	 * Grab the lock for the entry, if we need to do an evaluation
	 * then the lock is a write lock, after we have the write
	 * lock, check if we really need it - some other thread may
	 * have already done the work for us.
	 */
	if (VFE_NEEDS_EVAL(vfe)) {
		rw_enter(&vfe->lock, RW_WRITER);
		if (!VFE_NEEDS_EVAL(vfe))
			rw_downgrade(&vfe->lock);
	} else
		rw_enter(&vfe->lock, RW_READER);

	/* Evaluate fingerprint if needed. */
	if (VFE_NEEDS_EVAL(vfe)) {
		u_char status;

		error = veriexec_fp_status(l, vp, file_lock_state, vfe, &status);
		if (error) {
			veriexec_file_report(vfe, "Fingerprint calculation error.",
			    name, NULL, REPORT_ALWAYS);
			rw_exit(&vfe->lock);
			return (error);
		}
		vfe->status = status;
		rw_downgrade(&vfe->lock);
	}

	if (!(vfe->type & flag)) {
		veriexec_file_report(vfe, "Incorrect access type.", name, l,
		    REPORT_ALWAYS|REPORT_ALARM);

		/* IPS mode: Enforce access type. */
		if (veriexec_strict >= VERIEXEC_IPS) {
			rw_exit(&vfe->lock);
			return (EPERM);
		}
	}

	switch (vfe->status) {
	case FINGERPRINT_NOTEVAL:
		/* Should not happen. */
		rw_exit(&vfe->lock);
		veriexec_file_report(vfe, "Not-evaluated status "
		    "post evaluation; inconsistency detected.", name,
		    NULL, REPORT_ALWAYS|REPORT_PANIC);
		/* NOTREACHED */

	case FINGERPRINT_VALID:
		/* Valid fingerprint. */
		veriexec_file_report(vfe, "Match.", name, NULL,
		    REPORT_VERBOSE);

		break;

	case FINGERPRINT_NOMATCH:
		/* Fingerprint mismatch. */
		veriexec_file_report(vfe, "Mismatch.", name,
		    NULL, REPORT_ALWAYS|REPORT_ALARM);

		/* IDS mode: Deny access on fingerprint mismatch. */
		if (veriexec_strict >= VERIEXEC_IDS) {
			rw_exit(&vfe->lock);
			error = EPERM;
		}

		break;

	default:
		/* Should never happen. */
		rw_exit(&vfe->lock);
		veriexec_file_report(vfe, "Invalid status "
		    "post evaluation.", name, NULL, REPORT_ALWAYS|REPORT_PANIC);
		/* NOTREACHED */
	}

	return (error);
}

int
veriexec_verify(struct lwp *l, struct vnode *vp, const u_char *name, int flag,
    bool *found)
{
	struct veriexec_file_entry *vfe;
	int r;

	if (veriexec_bypass && (veriexec_strict == VERIEXEC_LEARNING))
		return 0;

	rw_enter(&veriexec_op_lock, RW_READER);
	r = veriexec_file_verify(l, vp, name, flag, VERIEXEC_FILE_UNLOCKED,
	    &vfe);
	rw_exit(&veriexec_op_lock);

	if ((r  == 0) && (vfe != NULL))
		rw_exit(&vfe->lock);

	if (found != NULL)
		*found = (vfe != NULL) ? true : false;

	return (r);
}

/*
 * Veriexec remove policy code.
 */
int
veriexec_removechk(struct lwp *l, struct vnode *vp, const char *pathbuf)
{
	struct veriexec_file_entry *vfe;
	int error;

	if (veriexec_bypass && (veriexec_strict == VERIEXEC_LEARNING))
		return 0;

	rw_enter(&veriexec_op_lock, RW_READER);
	vfe = veriexec_get(vp);
	rw_exit(&veriexec_op_lock);

	if (vfe == NULL) {
		/* Lockdown mode: Deny access to non-monitored files. */
		if (veriexec_strict >= VERIEXEC_LOCKDOWN)
			return (EPERM);

		return (0);
	}

	veriexec_file_report(vfe, "Remove request.", pathbuf, l,
	    REPORT_ALWAYS|REPORT_ALARM);

	/* IDS mode: Deny removal of monitored files. */
	if (veriexec_strict >= VERIEXEC_IDS)
		error = EPERM;
	else
		error = veriexec_file_delete(l, vp);

	return error;
}

/*
 * Veriexec rename policy.
 *
 * XXX: Once there's a way to hook after a successful rename, it would be
 * XXX: nice to update vfe->filename to the new name if it's not NULL and
 * XXX: the new name is absolute (ie., starts with a slash).
 */
int
veriexec_renamechk(struct lwp *l, struct vnode *fromvp, const char *fromname,
    struct vnode *tovp, const char *toname)
{
	struct veriexec_file_entry *fvfe = NULL, *tvfe = NULL;

	if (veriexec_bypass && (veriexec_strict == VERIEXEC_LEARNING))
		return 0;

	rw_enter(&veriexec_op_lock, RW_READER);

	if (veriexec_strict >= VERIEXEC_LOCKDOWN) {
		log(LOG_ALERT, "Veriexec: Preventing rename of `%s' to "
		    "`%s', uid=%u, pid=%u: Lockdown mode.\n", fromname, toname,
		    kauth_cred_geteuid(l->l_cred), l->l_proc->p_pid);
		rw_exit(&veriexec_op_lock);
		return (EPERM);
	}

	fvfe = veriexec_get(fromvp);
	if (tovp != NULL)
		tvfe = veriexec_get(tovp);

	if ((fvfe == NULL) && (tvfe == NULL)) {
		/* None of them is monitored */
		rw_exit(&veriexec_op_lock);
		return 0;
	}

	if (veriexec_strict >= VERIEXEC_IPS) {
		log(LOG_ALERT, "Veriexec: Preventing rename of `%s' "
		    "to `%s', uid=%u, pid=%u: IPS mode, %s "
		    "monitored.\n", fromname, toname,
		    kauth_cred_geteuid(l->l_cred),
		    l->l_proc->p_pid, (fvfe != NULL && tvfe != NULL) ?
		    "files" : "file");
		rw_exit(&veriexec_op_lock);
		return (EPERM);
	}

	if (fvfe != NULL) {
		/*
		 * Monitored file is renamed; filename no longer relevant.
		 */

		/*
		 * XXX: We could keep the buffer, and when (and if) updating the
		 * XXX: filename post-rename, re-allocate it only if it's not
		 * XXX: big enough for the new filename.
		 */

		/* XXX: Get write lock on fvfe here? */

		VERIEXEC_RW_UPGRADE(&veriexec_op_lock);
		/* once we have the op lock in write mode
		 * there should be no locks on any file
		 * entries so we can destroy the object.
		 */

		if (fvfe->filename_len > 0)
			kmem_free(fvfe->filename, fvfe->filename_len);

		fvfe->filename = NULL;
		fvfe->filename_len = 0;

		rw_downgrade(&veriexec_op_lock);
	}

	log(LOG_NOTICE, "Veriexec: %s file `%s' renamed to "
	    "%s file `%s', uid=%u, pid=%u.\n", (fvfe != NULL) ?
	    "Monitored" : "Non-monitored", fromname, (tvfe != NULL) ?
	    "monitored" : "non-monitored", toname,
	    kauth_cred_geteuid(l->l_cred), l->l_proc->p_pid);

	rw_exit(&veriexec_op_lock);

	if (tvfe != NULL) {
		/*
		 * Monitored file is overwritten. Remove the entry.
		 */
		(void)veriexec_file_delete(l, tovp);
	}

	return (0);
}

static void
veriexec_file_free(struct veriexec_file_entry *vfe)
{
	if (vfe != NULL) {
		if (vfe->fp != NULL)
			kmem_free(vfe->fp, vfe->ops->hash_len);
		if (vfe->filename != NULL)
			kmem_free(vfe->filename, vfe->filename_len);
		rw_destroy(&vfe->lock);
		kmem_free(vfe, sizeof(*vfe));
	}
}

static void
veriexec_file_purge(struct veriexec_file_entry *vfe, int have_lock)
{
	if (vfe == NULL)
		return;

	if (have_lock == VERIEXEC_UNLOCKED)
		rw_enter(&vfe->lock, RW_WRITER);
	else
		VERIEXEC_RW_UPGRADE(&vfe->lock);

	vfe->status = FINGERPRINT_NOTEVAL;
	if (have_lock == VERIEXEC_UNLOCKED)
		rw_exit(&vfe->lock);
	else
		rw_downgrade(&vfe->lock);
}

static void
veriexec_file_purge_cb(struct veriexec_file_entry *vfe, void *cookie)
{
	veriexec_file_purge(vfe, VERIEXEC_UNLOCKED);
}

/*
 * Invalidate a Veriexec file entry.
 * XXX: This should be updated when per-page fingerprints are added.
 */
void
veriexec_purge(struct vnode *vp)
{
	rw_enter(&veriexec_op_lock, RW_READER);
	veriexec_file_purge(veriexec_get(vp), VERIEXEC_UNLOCKED);
	rw_exit(&veriexec_op_lock);
}

/*
 * Enforce raw disk access policy.
 *
 * IDS mode: Invalidate fingerprints on a mount if it's opened for writing.
 * IPS mode: Don't allow raw writing to disks we monitor.
 * Lockdown mode: Don't allow raw writing to all disks.
 *
 * XXX: This is bogus. There's an obvious race condition between the time
 * XXX: the disk is open for writing, in which an attacker can access a
 * XXX: monitored file to get its signature cached again, and when the raw
 * XXX: file is overwritten on disk.
 * XXX:
 * XXX: To solve this, we need something like the following:
 * XXX:		open raw disk:
 * XXX:		  - raise refcount,
 * XXX:		  - invalidate fingerprints,
 * XXX:		  - mark all entries for that disk with "no cache" flag
 * XXX:
 * XXX:		veriexec_verify:
 * XXX:		  - if "no cache", don't cache evaluation result
 * XXX:
 * XXX:		close raw disk:
 * XXX:		  - lower refcount,
 * XXX:		  - if refcount == 0, remove "no cache" flag from all entries
 */
static int
veriexec_raw_cb(kauth_cred_t cred, kauth_action_t action, void *cookie,
    void *arg0, void *arg1, void *arg2, void *arg3)
{
	int result;
	enum kauth_device_req req;
	struct veriexec_table_entry *vte;

	result = KAUTH_RESULT_DENY;
	req = (enum kauth_device_req)arg0;

	switch (action) {
	case KAUTH_DEVICE_RAWIO_SPEC: {
		struct vnode *vp, *bvp;
		int error;

		if (req == KAUTH_REQ_DEVICE_RAWIO_SPEC_READ) {
			result = KAUTH_RESULT_DEFER;
			break;
		}

		vp = arg1;
		KASSERT(vp != NULL);

		/* Handle /dev/mem and /dev/kmem. */
		if (iskmemvp(vp)) {
			if (veriexec_strict < VERIEXEC_IPS)
				result = KAUTH_RESULT_DEFER;

			break;
		}

		error = rawdev_mounted(vp, &bvp);
		if (error == EINVAL) {
			result = KAUTH_RESULT_DEFER;
			break;
		}

		/*
		 * XXX: See vfs_mountedon() comment in rawdev_mounted().
		 */
		vte = veriexec_table_lookup(bvp->v_mount);
		if (vte == NULL) {
			result = KAUTH_RESULT_DEFER;
			break;
		}

		switch (veriexec_strict) {
		case VERIEXEC_LEARNING:
		case VERIEXEC_IDS:
			result = KAUTH_RESULT_DEFER;

			rw_enter(&veriexec_op_lock, RW_WRITER);
			fileassoc_table_run(bvp->v_mount, veriexec_hook,
			    (fileassoc_cb_t)veriexec_file_purge_cb, NULL);
			rw_exit(&veriexec_op_lock);

			break;
		case VERIEXEC_IPS:
			result = KAUTH_RESULT_DENY;
			break;
		case VERIEXEC_LOCKDOWN:
			result = KAUTH_RESULT_DENY;
			break;
		}

		break;
		}

	case KAUTH_DEVICE_RAWIO_PASSTHRU:
		/* XXX What can we do here? */
		if (veriexec_strict < VERIEXEC_IPS)
			result = KAUTH_RESULT_DEFER;

		break;

	default:
		result = KAUTH_RESULT_DEFER;
		break;
	}

	return (result);
}

/*
 * Create a new Veriexec table.
 */
static struct veriexec_table_entry *
veriexec_table_add(struct lwp *l, struct mount *mp)
{
	struct veriexec_table_entry *vte;
	u_char buf[16];

	vte = kmem_zalloc(sizeof(*vte), KM_SLEEP);
	mount_setspecific(mp, veriexec_mountspecific_key, vte);

	snprintf(buf, sizeof(buf), "table%u", veriexec_tablecount++);
	sysctl_createv(NULL, 0, &veriexec_count_node, &vte->vte_node,
		       0, CTLTYPE_NODE, buf, NULL, NULL, 0, NULL,
		       0, CTL_CREATE, CTL_EOL);

	sysctl_createv(NULL, 0, &vte->vte_node, NULL,
		       CTLFLAG_READONLY, CTLTYPE_STRING, "mntpt",
		       NULL, NULL, 0, mp->mnt_stat.f_mntonname,
		       0, CTL_CREATE, CTL_EOL);
	sysctl_createv(NULL, 0, &vte->vte_node, NULL,
		       CTLFLAG_READONLY, CTLTYPE_STRING, "fstype",
		       NULL, NULL, 0, mp->mnt_stat.f_fstypename,
		       0, CTL_CREATE, CTL_EOL);
	sysctl_createv(NULL, 0, &vte->vte_node, NULL,
		       CTLFLAG_READONLY, CTLTYPE_QUAD, "nentries",
		       NULL, NULL, 0, &vte->vte_count, 0, CTL_CREATE, CTL_EOL);

	return (vte);
}

/*
 * Add a file to be monitored by Veriexec.
 *
 * Expected elements in dict: file, fp, fp-type, entry-type.
 */
int
veriexec_file_add(struct lwp *l, prop_dictionary_t dict)
{
	struct veriexec_table_entry *vte;
	struct veriexec_file_entry *vfe = NULL;
	struct vnode *vp;
	const char *file, *fp_type;
	int error;

	if (!prop_dictionary_get_cstring_nocopy(dict, "file", &file))
		return (EINVAL);

	error = namei_simple_kernel(file, NSM_FOLLOW_NOEMULROOT, &vp);
	if (error)
		return (error);

	/* Add only regular files. */
	if (vp->v_type != VREG) {
		log(LOG_ERR, "Veriexec: Not adding `%s': Not a regular file.\n",
		    file);
		error = EBADF;
		goto out;
	}

	vfe = kmem_zalloc(sizeof(*vfe), KM_SLEEP);
	rw_init(&vfe->lock);

	/* Lookup fingerprint hashing algorithm. */
	fp_type = prop_string_cstring_nocopy(prop_dictionary_get(dict,
	    "fp-type"));
	if ((vfe->ops = veriexec_fpops_lookup(fp_type)) == NULL) {
		log(LOG_ERR, "Veriexec: Invalid or unknown fingerprint type "
		    "`%s' for file `%s'.\n", fp_type, file);
		error = EOPNOTSUPP;
		goto out;
	}

	if (prop_data_size(prop_dictionary_get(dict, "fp")) !=
	    vfe->ops->hash_len) {
		log(LOG_ERR, "Veriexec: Bad fingerprint length for `%s'.\n",
		    file);
		error = EINVAL;
		goto out;
	}

	vfe->fp = kmem_alloc(vfe->ops->hash_len, KM_SLEEP);
	memcpy(vfe->fp, prop_data_data_nocopy(prop_dictionary_get(dict, "fp")),
	    vfe->ops->hash_len);

	rw_enter(&veriexec_op_lock, RW_WRITER);

	if (veriexec_get(vp)) {
		/* We already have an entry for this file. */
		error = EEXIST;
		goto unlock_out;
	}

	/* Continue entry initialization. */
	if (prop_dictionary_get_uint8(dict, "entry-type", &vfe->type) == FALSE)
		vfe->type = 0;
	else {
		uint8_t extra_flags;

		extra_flags = vfe->type & ~(VERIEXEC_DIRECT |
		    VERIEXEC_INDIRECT | VERIEXEC_FILE | VERIEXEC_UNTRUSTED);
		if (extra_flags) {
			log(LOG_NOTICE, "Veriexec: Contaminated flags `0x%x' "
			    "for `%s', skipping.\n", extra_flags, file);
			error = EINVAL;
			goto unlock_out;
		}
	}
	if (!(vfe->type & (VERIEXEC_DIRECT | VERIEXEC_INDIRECT |
	    VERIEXEC_FILE)))
		vfe->type |= VERIEXEC_DIRECT;

	vfe->status = FINGERPRINT_NOTEVAL;
	if (prop_bool_true(prop_dictionary_get(dict, "keep-filename"))) {
		vfe->filename_len = strlen(file) + 1;
		vfe->filename = kmem_alloc(vfe->filename_len, KM_SLEEP);
		strlcpy(vfe->filename, file, vfe->filename_len);
	} else
		vfe->filename = NULL;

	if (prop_bool_true(prop_dictionary_get(dict, "eval-on-load")) ||
	    (vfe->type & VERIEXEC_UNTRUSTED)) {
		u_char status;

		error = veriexec_fp_status(l, vp, VERIEXEC_FILE_UNLOCKED,
		    vfe, &status);
		if (error)
			goto unlock_out;
		vfe->status = status;
	}

	vte = veriexec_table_lookup(vp->v_mount);
	if (vte == NULL)
		vte = veriexec_table_add(l, vp->v_mount);

	/* XXX if we bail below this, we might want to gc newly created vtes. */

	error = fileassoc_add(vp, veriexec_hook, vfe);
	if (error)
		goto unlock_out;

	vte->vte_count++;

	veriexec_file_report(NULL, "New entry.", file, NULL, REPORT_DEBUG);
	veriexec_bypass = 0;

  unlock_out:
	rw_exit(&veriexec_op_lock);

  out:
	vrele(vp);
	if (error)
		veriexec_file_free(vfe);

	return (error);
}

int
veriexec_table_delete(struct lwp *l, struct mount *mp)
{
	struct veriexec_table_entry *vte;

	vte = veriexec_table_lookup(mp);
	if (vte == NULL)
		return (ENOENT);

	veriexec_mountspecific_dtor(vte);
	mount_setspecific(mp, veriexec_mountspecific_key, NULL);

	return (fileassoc_table_clear(mp, veriexec_hook));
}

int
veriexec_file_delete(struct lwp *l, struct vnode *vp)
{
	struct veriexec_table_entry *vte;
	int error;

	vte = veriexec_table_lookup(vp->v_mount);
	if (vte == NULL)
		return (ENOENT);

	rw_enter(&veriexec_op_lock, RW_WRITER);
	error = fileassoc_clear(vp, veriexec_hook);
	rw_exit(&veriexec_op_lock);
	if (!error) {
		KASSERT(vte->vte_count > 0);
		vte->vte_count--;
	}

	return (error);
}

/*
 * Convert Veriexec entry data to a dictionary readable by userland tools.
 */
static void
veriexec_file_convert(struct veriexec_file_entry *vfe, prop_dictionary_t rdict)
{
	if (vfe->filename)
		prop_dictionary_set(rdict, "file",
		    prop_string_create_cstring(vfe->filename));
	prop_dictionary_set_uint8(rdict, "entry-type", vfe->type);
	prop_dictionary_set_uint8(rdict, "status", vfe->status);
	prop_dictionary_set(rdict, "fp-type",
	    prop_string_create_cstring(vfe->ops->type));
	prop_dictionary_set(rdict, "fp",
	    prop_data_create_data(vfe->fp, vfe->ops->hash_len));
}

int
veriexec_convert(struct vnode *vp, prop_dictionary_t rdict)
{
	struct veriexec_file_entry *vfe;

	rw_enter(&veriexec_op_lock, RW_READER);

	vfe = veriexec_get(vp);
	if (vfe == NULL) {
		rw_exit(&veriexec_op_lock);
		return (ENOENT);
	}

	rw_enter(&vfe->lock, RW_READER);
	veriexec_file_convert(vfe, rdict);
	rw_exit(&vfe->lock);

	rw_exit(&veriexec_op_lock);
	return (0);
}

int
veriexec_unmountchk(struct mount *mp)
{
	int error;

	if ((veriexec_bypass && (veriexec_strict == VERIEXEC_LEARNING))
	    || doing_shutdown)
		return (0);

	rw_enter(&veriexec_op_lock, RW_READER);

	switch (veriexec_strict) {
	case VERIEXEC_LEARNING:
		error = 0;
		break;

	case VERIEXEC_IDS:
		if (veriexec_table_lookup(mp) != NULL) {
			log(LOG_INFO, "Veriexec: IDS mode, allowing unmount "
			    "of \"%s\".\n", mp->mnt_stat.f_mntonname);
		}

		error = 0;
		break;

	case VERIEXEC_IPS: {
		struct veriexec_table_entry *vte;

		vte = veriexec_table_lookup(mp);
		if ((vte != NULL) && (vte->vte_count > 0)) {
			log(LOG_ALERT, "Veriexec: IPS mode, preventing"
			    " unmount of \"%s\" with monitored files.\n",
			    mp->mnt_stat.f_mntonname);

			error = EPERM;
		} else
			error = 0;
		break;
		}

	case VERIEXEC_LOCKDOWN:
	default:
		log(LOG_ALERT, "Veriexec: Lockdown mode, preventing unmount "
		    "of \"%s\".\n", mp->mnt_stat.f_mntonname);
		error = EPERM;
		break;
	}

	rw_exit(&veriexec_op_lock);
	return (error);
}

int
veriexec_openchk(struct lwp *l, struct vnode *vp, const char *path, int fmode)
{
	struct veriexec_file_entry *vfe = NULL;
	int error = 0;

	if (veriexec_bypass && (veriexec_strict == VERIEXEC_LEARNING))
		return 0;

	if (vp == NULL) {
		/* If no creation requested, let this fail normally. */
		if (!(fmode & O_CREAT))
			goto out;

		/* Lockdown mode: Prevent creation of new files. */
		if (veriexec_strict >= VERIEXEC_LOCKDOWN) {
			log(LOG_ALERT, "Veriexec: Preventing new file "
			    "creation in `%s'.\n", path);
			error = EPERM;
		}

		goto out;
	}

	rw_enter(&veriexec_op_lock, RW_READER);
	error = veriexec_file_verify(l, vp, path, VERIEXEC_FILE,
				     VERIEXEC_FILE_LOCKED, &vfe);

	if (error) {
		rw_exit(&veriexec_op_lock);
		goto out;
	}

	if ((vfe != NULL) && ((fmode & FWRITE) || (fmode & O_TRUNC))) {
		veriexec_file_report(vfe, "Write access request.", path, l,
		    REPORT_ALWAYS | REPORT_ALARM);

		/* IPS mode: Deny write access to monitored files. */
		if (veriexec_strict >= VERIEXEC_IPS)
			error = EPERM;
		else
			veriexec_file_purge(vfe, VERIEXEC_LOCKED);
	}

	if (vfe != NULL)
		rw_exit(&vfe->lock);

	rw_exit(&veriexec_op_lock);
 out:
	return (error);
}

static void
veriexec_file_dump(struct veriexec_file_entry *vfe, prop_array_t entries)
{
	prop_dictionary_t entry;

	/* If we don't have a filename, this is meaningless. */
	if (vfe->filename == NULL)
		return;

	entry = prop_dictionary_create();

	veriexec_file_convert(vfe, entry);

	prop_array_add(entries, entry);
}

int
veriexec_dump(struct lwp *l, prop_array_t rarray)
{
	struct mount *mp, *nmp;

	mutex_enter(&mountlist_lock);
	for (mp = TAILQ_FIRST(&mountlist); mp != NULL; mp = nmp) {
		/* If it fails, the file-system is [being] unmounted. */
		if (vfs_busy(mp, &nmp) != 0)
			continue;

		fileassoc_table_run(mp, veriexec_hook,
		    (fileassoc_cb_t)veriexec_file_dump, rarray);

		vfs_unbusy(mp, false, &nmp);
	}
	mutex_exit(&mountlist_lock);

	return (0);
}

int
veriexec_flush(struct lwp *l)
{
	struct mount *mp, *nmp;
	int error = 0;

	mutex_enter(&mountlist_lock);
	for (mp = TAILQ_FIRST(&mountlist); mp != NULL; mp = nmp) {
		int lerror;

		/* If it fails, the file-system is [being] unmounted. */
		if (vfs_busy(mp, &nmp) != 0)
			continue;

		lerror = veriexec_table_delete(l, mp);
		if (lerror && lerror != ENOENT)
			error = lerror;

		vfs_unbusy(mp, false, &nmp);
	}
	mutex_exit(&mountlist_lock);

	return (error);
}
