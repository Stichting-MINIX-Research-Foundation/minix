/*	$NetBSD: vfs_init.c,v 1.48 2015/05/06 15:57:08 hannken Exp $	*/

/*-
 * Copyright (c) 1998, 2000, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed
 * to Berkeley by John Heidemann of the UCLA Ficus project.
 *
 * Source: * @(#)i405_init.c 2.10 92/04/27 UCLA Ficus project
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
 *	@(#)vfs_init.c	8.5 (Berkeley) 5/11/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: vfs_init.c,v 1.48 2015/05/06 15:57:08 hannken Exp $");

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/time.h>
#include <sys/vnode.h>
#include <sys/stat.h>
#include <sys/namei.h>
#include <sys/ucred.h>
#include <sys/buf.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/dirhash.h>
#include <sys/sysctl.h>
#include <sys/kauth.h>

/*
 * Sigh, such primitive tools are these...
 */
#if 0
#define DODEBUG(A) A
#else
#define DODEBUG(A)
#endif

/*
 * The global list of vnode operations.
 */
extern const struct vnodeop_desc * const vfs_op_descs[];

/*
 * These vnodeopv_descs are listed here because they are not
 * associated with any particular file system, and thus cannot
 * be initialized by vfs_attach().
 */
extern const struct vnodeopv_desc dead_vnodeop_opv_desc;
extern const struct vnodeopv_desc fifo_vnodeop_opv_desc;
extern const struct vnodeopv_desc spec_vnodeop_opv_desc;

const struct vnodeopv_desc * const vfs_special_vnodeopv_descs[] = {
	&dead_vnodeop_opv_desc,
	&fifo_vnodeop_opv_desc,
	&spec_vnodeop_opv_desc,
	NULL,
};

struct vfs_list_head vfs_list =			/* vfs list */
    LIST_HEAD_INITIALIZER(vfs_list);

static kauth_listener_t mount_listener;

/*
 * This code doesn't work if the defn is **vnodop_defns with cc.
 * The problem is because of the compiler sometimes putting in an
 * extra level of indirection for arrays.  It's an interesting
 * "feature" of C.
 */
typedef int (*PFI)(void *);

/*
 * A miscellaneous routine.
 * A generic "default" routine that just returns an error.
 */
/*ARGSUSED*/
int
vn_default_error(void *v)
{

	return (EOPNOTSUPP);
}

static struct sysctllog *vfs_sysctllog;

/*
 * Top level filesystem related information gathering.
 */
static void
sysctl_vfs_setup(void)
{
	extern int vfs_magiclinks;

	sysctl_createv(&vfs_sysctllog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "generic",
		       SYSCTL_DESCR("Non-specific vfs related information"),
		       NULL, 0, NULL, 0,
		       CTL_VFS, VFS_GENERIC, CTL_EOL);
	sysctl_createv(&vfs_sysctllog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_STRING, "fstypes",
		       SYSCTL_DESCR("List of file systems present"),
		       sysctl_vfs_generic_fstypes, 0, NULL, 0,
		       CTL_VFS, VFS_GENERIC, CTL_CREATE, CTL_EOL);
	sysctl_createv(&vfs_sysctllog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "magiclinks",
		       SYSCTL_DESCR("Whether \"magic\" symlinks are expanded"),
		       NULL, 0, &vfs_magiclinks, 0,
		       CTL_VFS, VFS_GENERIC, VFS_MAGICLINKS, CTL_EOL);
}


/*
 * vfs_init.c
 *
 * Allocate and fill in operations vectors.
 *
 * An undocumented feature of this approach to defining operations is that
 * there can be multiple entries in vfs_opv_descs for the same operations
 * vector. This allows third parties to extend the set of operations
 * supported by another layer in a binary compatibile way. For example,
 * assume that NFS needed to be modified to support Ficus. NFS has an entry
 * (probably nfs_vnopdeop_decls) declaring all the operations NFS supports by
 * default. Ficus could add another entry (ficus_nfs_vnodeop_decl_entensions)
 * listing those new operations Ficus adds to NFS, all without modifying the
 * NFS code. (Of couse, the OTW NFS protocol still needs to be munged, but
 * that is a(whole)nother story.) This is a feature.
 */

/*
 * Init the vector, if it needs it.
 * Also handle backwards compatibility.
 */
static void
vfs_opv_init_explicit(const struct vnodeopv_desc *vfs_opv_desc)
{
	int (**opv_desc_vector)(void *);
	const struct vnodeopv_entry_desc *opve_descp;

	opv_desc_vector = *(vfs_opv_desc->opv_desc_vector_p);

	for (opve_descp = vfs_opv_desc->opv_desc_ops;
	     opve_descp->opve_op;
	     opve_descp++) {
		/*
		 * Sanity check:  is this operation listed
		 * in the list of operations?  We check this
		 * by seeing if its offset is zero.  Since
		 * the default routine should always be listed
		 * first, it should be the only one with a zero
		 * offset.  Any other operation with a zero
		 * offset is probably not listed in
		 * vfs_op_descs, and so is probably an error.
		 *
		 * A panic here means the layer programmer
		 * has committed the all-too common bug
		 * of adding a new operation to the layer's
		 * list of vnode operations but
		 * not adding the operation to the system-wide
		 * list of supported operations.
		 */
		if (opve_descp->opve_op->vdesc_offset == 0 &&
		    opve_descp->opve_op->vdesc_offset != VOFFSET(vop_default)) {
			printf("operation %s not listed in %s.\n",
			    opve_descp->opve_op->vdesc_name, "vfs_op_descs");
			panic ("vfs_opv_init: bad operation");
		}

		/*
		 * Fill in this entry.
		 */
		opv_desc_vector[opve_descp->opve_op->vdesc_offset] =
		    opve_descp->opve_impl;
	}
}

static void
vfs_opv_init_default(const struct vnodeopv_desc *vfs_opv_desc)
{
	int j;
	int (**opv_desc_vector)(void *);

	opv_desc_vector = *(vfs_opv_desc->opv_desc_vector_p);

	/*
	 * Force every operations vector to have a default routine.
	 */
	if (opv_desc_vector[VOFFSET(vop_default)] == NULL)
		panic("vfs_opv_init: operation vector without default routine.");

	for (j = 0; j < VNODE_OPS_COUNT; j++)
		if (opv_desc_vector[j] == NULL)
			opv_desc_vector[j] =
			    opv_desc_vector[VOFFSET(vop_default)];
}

void
vfs_opv_init(const struct vnodeopv_desc * const *vopvdpp)
{
	int (**opv_desc_vector)(void *);
	int i;

	/*
	 * Allocate the vectors.
	 */
	for (i = 0; vopvdpp[i] != NULL; i++) {
		opv_desc_vector =
		    kmem_alloc(VNODE_OPS_COUNT * sizeof(PFI), KM_SLEEP);
		memset(opv_desc_vector, 0, VNODE_OPS_COUNT * sizeof(PFI));
		*(vopvdpp[i]->opv_desc_vector_p) = opv_desc_vector;
		DODEBUG(printf("vector at %p allocated\n",
		    opv_desc_vector_p));
	}

	/*
	 * ...and fill them in.
	 */
	for (i = 0; vopvdpp[i] != NULL; i++)
		vfs_opv_init_explicit(vopvdpp[i]);

	/*
	 * Finally, go back and replace unfilled routines
	 * with their default.
	 */
	for (i = 0; vopvdpp[i] != NULL; i++)
		vfs_opv_init_default(vopvdpp[i]);
}

void
vfs_opv_free(const struct vnodeopv_desc * const *vopvdpp)
{
	int i;

	/*
	 * Free the vectors allocated in vfs_opv_init().
	 */
	for (i = 0; vopvdpp[i] != NULL; i++) {
		kmem_free(*(vopvdpp[i]->opv_desc_vector_p),
		    VNODE_OPS_COUNT * sizeof(PFI));
		*(vopvdpp[i]->opv_desc_vector_p) = NULL;
	}
}

#ifdef DEBUG
static void
vfs_op_check(void)
{
	int i;

	DODEBUG(printf("Vnode_interface_init.\n"));

	/*
	 * Check offset of each op.
	 */
	for (i = 0; vfs_op_descs[i]; i++) {
		if (vfs_op_descs[i]->vdesc_offset != i)
			panic("vfs_op_check: vfs_op_desc[] offset mismatch");
	}

	if (i != VNODE_OPS_COUNT) {
		panic("vfs_op_check: vnode ops count mismatch (%d != %d)",
			i, VNODE_OPS_COUNT);
	}

	DODEBUG(printf ("vfs_opv_numops=%d\n", VNODE_OPS_COUNT));
}
#endif /* DEBUG */

/*
 * Common routine to check if an unprivileged mount is allowed.
 *
 * We export just this part (i.e., without the access control) so that if a
 * secmodel wants to implement finer grained user mounts it can do so without
 * copying too much code. More elaborate policies (i.e., specific users allowed
 * to also create devices and/or introduce set-id binaries, or export
 * file-systems) will require a different implementation.
 *
 * This routine is intended to be called from listener context, and as such
 * does not take credentials as an argument.
 */
int
usermount_common_policy(struct mount *mp, u_long flags)
{

	/* No exporting if unprivileged. */
	if (flags & MNT_EXPORTED)
		return EPERM;

	/* Must have 'nosuid' and 'nodev'. */
	if ((flags & MNT_NODEV) == 0 || (flags & MNT_NOSUID) == 0)
		return EPERM;

	/* Retain 'noexec'. */
	if ((mp->mnt_flag & MNT_NOEXEC) && (flags & MNT_NOEXEC) == 0)
		return EPERM;

	return 0;
}

static int
mount_listener_cb(kauth_cred_t cred, kauth_action_t action, void *cookie,
    void *arg0, void *arg1, void *arg2, void *arg3)
{
	int result;
	enum kauth_system_req req;

	result = KAUTH_RESULT_DEFER;
	req = (enum kauth_system_req)arg0;

	if (action != KAUTH_SYSTEM_MOUNT)
		return result;

	if (req == KAUTH_REQ_SYSTEM_MOUNT_GET)
		result = KAUTH_RESULT_ALLOW;
	else if (req == KAUTH_REQ_SYSTEM_MOUNT_DEVICE) {
		vnode_t *devvp = arg2;
		mode_t access_mode = (mode_t)(unsigned long)arg3;
		int error;

		error = VOP_ACCESS(devvp, access_mode, cred);
		if (!error)
			result = KAUTH_RESULT_ALLOW;
	}

	return result;
}

/*
 * Initialize the vnode structures and initialize each file system type.
 */
void
vfsinit(void)
{

	/*
	 * Attach sysctl nodes
	 */
	sysctl_vfs_setup();

	/*
	 * Initialize the namei pathname buffer pool and cache.
	 */
	pnbuf_cache = pool_cache_init(MAXPATHLEN, 0, 0, 0, "pnbufpl",
	    NULL, IPL_NONE, NULL, NULL, NULL);
	KASSERT(pnbuf_cache != NULL);

	/*
	 * Initialize the vnode table
	 */
	vntblinit();

	/*
	 * Initialize the vnode name cache
	 */
	nchinit();

#ifdef DEBUG
	/*
	 * Check the list of vnode operations.
	 */
	vfs_op_check();
#endif

	/*
	 * Initialize the special vnode operations.
	 */
	vfs_opv_init(vfs_special_vnodeopv_descs);

	/*
	 * Initialise generic dirhash.
	 */
	dirhash_init();

	/*
	 * Initialise VFS hooks.
	 */
	vfs_hooks_init();

	mount_listener = kauth_listen_scope(KAUTH_SCOPE_SYSTEM,
	    mount_listener_cb, NULL);

	/*
	 * Establish each file system which was statically
	 * included in the kernel.
	 */
	module_init_class(MODULE_CLASS_VFS);
}

/*
 * Drop a reference to a file system type.
 */
void
vfs_delref(struct vfsops *vfs)
{

	mutex_enter(&vfs_list_lock);
	vfs->vfs_refcount--;
	mutex_exit(&vfs_list_lock);
}

/*
 * Establish a file system and initialize it.
 */
int
vfs_attach(struct vfsops *vfs)
{
	struct vfsops *v;
	int error = 0;

	mutex_enter(&vfs_list_lock);

	/*
	 * Make sure this file system doesn't already exist.
	 */
	LIST_FOREACH(v, &vfs_list, vfs_list) {
		if (strcmp(vfs->vfs_name, v->vfs_name) == 0) {
			error = EEXIST;
			goto out;
		}
	}

	/*
	 * Initialize the vnode operations for this file system.
	 */
	vfs_opv_init(vfs->vfs_opv_descs);

	/*
	 * Now initialize the file system itself.
	 */
	(*vfs->vfs_init)();

	/*
	 * ...and link it into the kernel's list.
	 */
	LIST_INSERT_HEAD(&vfs_list, vfs, vfs_list);

	/*
	 * Sanity: make sure the reference count is 0.
	 */
	vfs->vfs_refcount = 0;
 out:
	mutex_exit(&vfs_list_lock);
	return (error);
}

/*
 * Remove a file system from the kernel.
 */
int
vfs_detach(struct vfsops *vfs)
{
	struct vfsops *v;
	int error = 0;

	mutex_enter(&vfs_list_lock);

	/*
	 * Make sure no one is using the filesystem.
	 */
	if (vfs->vfs_refcount != 0) {
		error = EBUSY;
		goto out;
	}

	/*
	 * ...and remove it from the kernel's list.
	 */
	LIST_FOREACH(v, &vfs_list, vfs_list) {
		if (v == vfs) {
			LIST_REMOVE(v, vfs_list);
			break;
		}
	}

	if (v == NULL) {
		error = ESRCH;
		goto out;
	}

	/*
	 * Now run the file system-specific cleanups.
	 */
	(*vfs->vfs_done)();

	/*
	 * Free the vnode operations vector.
	 */
	vfs_opv_free(vfs->vfs_opv_descs);
 out:
 	mutex_exit(&vfs_list_lock);
	return (error);
}

void
vfs_reinit(void)
{
	struct vfsops *vfs;

	mutex_enter(&vfs_list_lock);
	LIST_FOREACH(vfs, &vfs_list, vfs_list) {
		if (vfs->vfs_reinit) {
			vfs->vfs_refcount++;
			mutex_exit(&vfs_list_lock);
			(*vfs->vfs_reinit)();
			mutex_enter(&vfs_list_lock);
			vfs->vfs_refcount--;
		}
	}
	mutex_exit(&vfs_list_lock);
}
