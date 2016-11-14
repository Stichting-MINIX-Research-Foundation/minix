/*	$NetBSD: sysv_shm.c,v 1.128 2015/05/13 01:16:15 pgoyette Exp $	*/

/*-
 * Copyright (c) 1999, 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center, and by Mindaugas Rasiukevicius.
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
 * Copyright (c) 1994 Adam Glass and Charles M. Hannum.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Adam Glass and Charles M.
 *	Hannum.
 * 4. The names of the authors may not be used to endorse or promote products
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
__KERNEL_RCSID(0, "$NetBSD: sysv_shm.c,v 1.128 2015/05/13 01:16:15 pgoyette Exp $");

#ifdef _KERNEL_OPT
#include "opt_sysv.h"
#endif

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/shm.h>
#include <sys/mutex.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/mount.h>		/* XXX for <sys/syscallargs.h> */
#include <sys/syscallargs.h>
#include <sys/queue.h>
#include <sys/kauth.h>

#include <uvm/uvm_extern.h>
#include <uvm/uvm_object.h>

struct shmmap_entry {
	SLIST_ENTRY(shmmap_entry) next;
	vaddr_t va;
	int shmid;
};

int			shm_nused		__cacheline_aligned;
struct shmid_ds *	shmsegs			__read_mostly;

static kmutex_t		shm_lock		__cacheline_aligned;
static kcondvar_t *	shm_cv			__cacheline_aligned;
static int		shm_last_free		__cacheline_aligned;
static size_t		shm_committed		__cacheline_aligned;
static int		shm_use_phys		__read_mostly;

static kcondvar_t	shm_realloc_cv;
static bool		shm_realloc_state;
static u_int		shm_realloc_disable;

struct shmmap_state {
	unsigned int nitems;
	unsigned int nrefs;
	SLIST_HEAD(, shmmap_entry) entries;
};

extern int kern_has_sysvshm;

#ifdef SHMDEBUG
#define SHMPRINTF(a) printf a
#else
#define SHMPRINTF(a)
#endif

static int shmrealloc(int);

/*
 * Find the shared memory segment by the identifier.
 *  => must be called with shm_lock held;
 */
static struct shmid_ds *
shm_find_segment_by_shmid(int shmid)
{
	int segnum;
	struct shmid_ds *shmseg;

	KASSERT(mutex_owned(&shm_lock));

	segnum = IPCID_TO_IX(shmid);
	if (segnum < 0 || segnum >= shminfo.shmmni)
		return NULL;
	shmseg = &shmsegs[segnum];
	if ((shmseg->shm_perm.mode & SHMSEG_ALLOCATED) == 0)
		return NULL;
	if ((shmseg->shm_perm.mode &
	    (SHMSEG_REMOVED|SHMSEG_RMLINGER)) == SHMSEG_REMOVED)
		return NULL;
	if (shmseg->shm_perm._seq != IPCID_TO_SEQ(shmid))
		return NULL;

	return shmseg;
}

/*
 * Free memory segment.
 *  => must be called with shm_lock held;
 */
static void
shm_free_segment(int segnum)
{
	struct shmid_ds *shmseg;
	size_t size;
	bool wanted;

	KASSERT(mutex_owned(&shm_lock));

	shmseg = &shmsegs[segnum];
	SHMPRINTF(("shm freeing key 0x%lx seq 0x%x\n",
	    shmseg->shm_perm._key, shmseg->shm_perm._seq));

	size = (shmseg->shm_segsz + PGOFSET) & ~PGOFSET;
	wanted = (shmseg->shm_perm.mode & SHMSEG_WANTED);

	shmseg->_shm_internal = NULL;
	shm_committed -= btoc(size);
	shm_nused--;
	shmseg->shm_perm.mode = SHMSEG_FREE;
	shm_last_free = segnum;
	if (wanted == true)
		cv_broadcast(&shm_cv[segnum]);
}

/*
 * Delete entry from the shm map.
 *  => must be called with shm_lock held;
 */
static struct uvm_object *
shm_delete_mapping(struct shmmap_state *shmmap_s,
    struct shmmap_entry *shmmap_se)
{
	struct uvm_object *uobj = NULL;
	struct shmid_ds *shmseg;
	int segnum;

	KASSERT(mutex_owned(&shm_lock));

	segnum = IPCID_TO_IX(shmmap_se->shmid);
	shmseg = &shmsegs[segnum];
	SLIST_REMOVE(&shmmap_s->entries, shmmap_se, shmmap_entry, next);
	shmmap_s->nitems--;
	shmseg->shm_dtime = time_second;
	if ((--shmseg->shm_nattch <= 0) &&
	    (shmseg->shm_perm.mode & SHMSEG_REMOVED)) {
		uobj = shmseg->_shm_internal;
		shm_free_segment(segnum);
	}

	return uobj;
}

/*
 * Get a non-shared shm map for that vmspace.  Note, that memory
 * allocation might be performed with lock held.
 */
static struct shmmap_state *
shmmap_getprivate(struct proc *p)
{
	struct shmmap_state *oshmmap_s, *shmmap_s;
	struct shmmap_entry *oshmmap_se, *shmmap_se;

	KASSERT(mutex_owned(&shm_lock));

	/* 1. A shm map with refcnt = 1, used by ourselves, thus return */
	oshmmap_s = (struct shmmap_state *)p->p_vmspace->vm_shm;
	if (oshmmap_s && oshmmap_s->nrefs == 1)
		return oshmmap_s;

	/* 2. No shm map preset - create a fresh one */
	shmmap_s = kmem_zalloc(sizeof(struct shmmap_state), KM_SLEEP);
	shmmap_s->nrefs = 1;
	SLIST_INIT(&shmmap_s->entries);
	p->p_vmspace->vm_shm = (void *)shmmap_s;

	if (oshmmap_s == NULL)
		return shmmap_s;

	SHMPRINTF(("shmmap_getprivate: vm %p split (%d entries), was used by %d\n",
	    p->p_vmspace, oshmmap_s->nitems, oshmmap_s->nrefs));

	/* 3. A shared shm map, copy to a fresh one and adjust refcounts */
	SLIST_FOREACH(oshmmap_se, &oshmmap_s->entries, next) {
		shmmap_se = kmem_alloc(sizeof(struct shmmap_entry), KM_SLEEP);
		shmmap_se->va = oshmmap_se->va;
		shmmap_se->shmid = oshmmap_se->shmid;
		SLIST_INSERT_HEAD(&shmmap_s->entries, shmmap_se, next);
	}
	shmmap_s->nitems = oshmmap_s->nitems;
	oshmmap_s->nrefs--;

	return shmmap_s;
}

/*
 * Lock/unlock the memory.
 *  => must be called with shm_lock held;
 *  => called from one place, thus, inline;
 */
static inline int
shm_memlock(struct lwp *l, struct shmid_ds *shmseg, int shmid, int cmd)
{
	struct proc *p = l->l_proc;
	struct shmmap_entry *shmmap_se;
	struct shmmap_state *shmmap_s;
	size_t size;
	int error;

	KASSERT(mutex_owned(&shm_lock));
	shmmap_s = shmmap_getprivate(p);

	/* Find our shared memory address by shmid */
	SLIST_FOREACH(shmmap_se, &shmmap_s->entries, next) {
		if (shmmap_se->shmid != shmid)
			continue;

		size = (shmseg->shm_segsz + PGOFSET) & ~PGOFSET;

		if (cmd == SHM_LOCK &&
		    (shmseg->shm_perm.mode & SHMSEG_WIRED) == 0) {
			/* Wire the object and map, then tag it */
			error = uvm_obj_wirepages(shmseg->_shm_internal,
			    0, size, NULL);
			if (error)
				return EIO;
			error = uvm_map_pageable(&p->p_vmspace->vm_map,
			    shmmap_se->va, shmmap_se->va + size, false, 0);
			if (error) {
				uvm_obj_unwirepages(shmseg->_shm_internal,
				    0, size);
				if (error == EFAULT)
					error = ENOMEM;
				return error;
			}
			shmseg->shm_perm.mode |= SHMSEG_WIRED;

		} else if (cmd == SHM_UNLOCK &&
		    (shmseg->shm_perm.mode & SHMSEG_WIRED) != 0) {
			/* Unwire the object and map, then untag it */
			uvm_obj_unwirepages(shmseg->_shm_internal, 0, size);
			error = uvm_map_pageable(&p->p_vmspace->vm_map,
			    shmmap_se->va, shmmap_se->va + size, true, 0);
			if (error)
				return EIO;
			shmseg->shm_perm.mode &= ~SHMSEG_WIRED;
		}
	}

	return 0;
}

/*
 * Unmap shared memory.
 */
int
sys_shmdt(struct lwp *l, const struct sys_shmdt_args *uap, register_t *retval)
{
	/* {
		syscallarg(const void *) shmaddr;
	} */
	struct proc *p = l->l_proc;
	struct shmmap_state *shmmap_s1, *shmmap_s;
	struct shmmap_entry *shmmap_se;
	struct uvm_object *uobj;
	struct shmid_ds *shmseg;
	size_t size;

	mutex_enter(&shm_lock);
	/* In case of reallocation, we will wait for completion */
	while (__predict_false(shm_realloc_state))
		cv_wait(&shm_realloc_cv, &shm_lock);

	shmmap_s1 = (struct shmmap_state *)p->p_vmspace->vm_shm;
	if (shmmap_s1 == NULL) {
		mutex_exit(&shm_lock);
		return EINVAL;
	}

	/* Find the map entry */
	SLIST_FOREACH(shmmap_se, &shmmap_s1->entries, next)
		if (shmmap_se->va == (vaddr_t)SCARG(uap, shmaddr))
			break;
	if (shmmap_se == NULL) {
		mutex_exit(&shm_lock);
		return EINVAL;
	}

	shmmap_s = shmmap_getprivate(p);
	if (shmmap_s != shmmap_s1) {
		/* Map has been copied, lookup entry in new map */
		SLIST_FOREACH(shmmap_se, &shmmap_s->entries, next)
			if (shmmap_se->va == (vaddr_t)SCARG(uap, shmaddr))
				break;
		if (shmmap_se == NULL) {
			mutex_exit(&shm_lock);
			return EINVAL;
		}
	}

	SHMPRINTF(("shmdt: vm %p: remove %d @%lx\n",
	    p->p_vmspace, shmmap_se->shmid, shmmap_se->va));

	/* Delete the entry from shm map */
	uobj = shm_delete_mapping(shmmap_s, shmmap_se);
	shmseg = &shmsegs[IPCID_TO_IX(shmmap_se->shmid)];
	size = (shmseg->shm_segsz + PGOFSET) & ~PGOFSET;
	mutex_exit(&shm_lock);

	uvm_deallocate(&p->p_vmspace->vm_map, shmmap_se->va, size);
	if (uobj != NULL) {
		uao_detach(uobj);
	}
	kmem_free(shmmap_se, sizeof(struct shmmap_entry));

	return 0;
}

/*
 * Map shared memory.
 */
int
sys_shmat(struct lwp *l, const struct sys_shmat_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) shmid;
		syscallarg(const void *) shmaddr;
		syscallarg(int) shmflg;
	} */
	int error, flags = 0;
	struct proc *p = l->l_proc;
	kauth_cred_t cred = l->l_cred;
	struct shmid_ds *shmseg;
	struct shmmap_state *shmmap_s;
	struct shmmap_entry *shmmap_se;
	struct uvm_object *uobj;
	struct vmspace *vm;
	vaddr_t attach_va;
	vm_prot_t prot;
	vsize_t size;

	/* Allocate a new map entry and set it */
	shmmap_se = kmem_alloc(sizeof(struct shmmap_entry), KM_SLEEP);
	shmmap_se->shmid = SCARG(uap, shmid);

	mutex_enter(&shm_lock);
	/* In case of reallocation, we will wait for completion */
	while (__predict_false(shm_realloc_state))
		cv_wait(&shm_realloc_cv, &shm_lock);

	shmseg = shm_find_segment_by_shmid(SCARG(uap, shmid));
	if (shmseg == NULL) {
		error = EINVAL;
		goto err;
	}
	error = ipcperm(cred, &shmseg->shm_perm,
	    (SCARG(uap, shmflg) & SHM_RDONLY) ? IPC_R : IPC_R|IPC_W);
	if (error)
		goto err;

	vm = p->p_vmspace;
	shmmap_s = (struct shmmap_state *)vm->vm_shm;
	if (shmmap_s && shmmap_s->nitems >= shminfo.shmseg) {
		error = EMFILE;
		goto err;
	}

	size = (shmseg->shm_segsz + PGOFSET) & ~PGOFSET;
	prot = VM_PROT_READ;
	if ((SCARG(uap, shmflg) & SHM_RDONLY) == 0)
		prot |= VM_PROT_WRITE;
	if (SCARG(uap, shmaddr)) {
		flags |= UVM_FLAG_FIXED;
		if (SCARG(uap, shmflg) & SHM_RND)
			attach_va =
			    (vaddr_t)SCARG(uap, shmaddr) & ~(SHMLBA-1);
		else if (((vaddr_t)SCARG(uap, shmaddr) & (SHMLBA-1)) == 0)
			attach_va = (vaddr_t)SCARG(uap, shmaddr);
		else {
			error = EINVAL;
			goto err;
		}
	} else {
		/* This is just a hint to uvm_map() about where to put it. */
		attach_va = p->p_emul->e_vm_default_addr(p,
		    (vaddr_t)vm->vm_daddr, size);
	}

	/*
	 * Create a map entry, add it to the list and increase the counters.
	 * The lock will be dropped before the mapping, disable reallocation.
	 */
	shmmap_s = shmmap_getprivate(p);
	SLIST_INSERT_HEAD(&shmmap_s->entries, shmmap_se, next);
	shmmap_s->nitems++;
	shmseg->shm_lpid = p->p_pid;
	shmseg->shm_nattch++;
	shm_realloc_disable++;
	mutex_exit(&shm_lock);

	/*
	 * Add a reference to the memory object, map it to the
	 * address space, and lock the memory, if needed.
	 */
	uobj = shmseg->_shm_internal;
	uao_reference(uobj);
	error = uvm_map(&vm->vm_map, &attach_va, size, uobj, 0, 0,
	    UVM_MAPFLAG(prot, prot, UVM_INH_SHARE, UVM_ADV_RANDOM, flags));
	if (error)
		goto err_detach;
	if (shm_use_phys || (shmseg->shm_perm.mode & SHMSEG_WIRED)) {
		error = uvm_map_pageable(&vm->vm_map, attach_va,
		    attach_va + size, false, 0);
		if (error) {
			if (error == EFAULT)
				error = ENOMEM;
			uvm_deallocate(&vm->vm_map, attach_va, size);
			goto err_detach;
		}
	}

	/* Set the new address, and update the time */
	mutex_enter(&shm_lock);
	shmmap_se->va = attach_va;
	shmseg->shm_atime = time_second;
	shm_realloc_disable--;
	retval[0] = attach_va;
	SHMPRINTF(("shmat: vm %p: add %d @%lx\n",
	    p->p_vmspace, shmmap_se->shmid, attach_va));
err:
	cv_broadcast(&shm_realloc_cv);
	mutex_exit(&shm_lock);
	if (error && shmmap_se) {
		kmem_free(shmmap_se, sizeof(struct shmmap_entry));
	}
	return error;

err_detach:
	uao_detach(uobj);
	mutex_enter(&shm_lock);
	uobj = shm_delete_mapping(shmmap_s, shmmap_se);
	shm_realloc_disable--;
	cv_broadcast(&shm_realloc_cv);
	mutex_exit(&shm_lock);
	if (uobj != NULL) {
		uao_detach(uobj);
	}
	kmem_free(shmmap_se, sizeof(struct shmmap_entry));
	return error;
}

/*
 * Shared memory control operations.
 */
int
sys___shmctl50(struct lwp *l, const struct sys___shmctl50_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(int) shmid;
		syscallarg(int) cmd;
		syscallarg(struct shmid_ds *) buf;
	} */
	struct shmid_ds shmbuf;
	int cmd, error;

	cmd = SCARG(uap, cmd);
	if (cmd == IPC_SET) {
		error = copyin(SCARG(uap, buf), &shmbuf, sizeof(shmbuf));
		if (error)
			return error;
	}

	error = shmctl1(l, SCARG(uap, shmid), cmd,
	    (cmd == IPC_SET || cmd == IPC_STAT) ? &shmbuf : NULL);

	if (error == 0 && cmd == IPC_STAT)
		error = copyout(&shmbuf, SCARG(uap, buf), sizeof(shmbuf));

	return error;
}

int
shmctl1(struct lwp *l, int shmid, int cmd, struct shmid_ds *shmbuf)
{
	struct uvm_object *uobj = NULL;
	kauth_cred_t cred = l->l_cred;
	struct shmid_ds *shmseg;
	int error = 0;

	mutex_enter(&shm_lock);
	/* In case of reallocation, we will wait for completion */
	while (__predict_false(shm_realloc_state))
		cv_wait(&shm_realloc_cv, &shm_lock);

	shmseg = shm_find_segment_by_shmid(shmid);
	if (shmseg == NULL) {
		mutex_exit(&shm_lock);
		return EINVAL;
	}

	switch (cmd) {
	case IPC_STAT:
		if ((error = ipcperm(cred, &shmseg->shm_perm, IPC_R)) != 0)
			break;
		memcpy(shmbuf, shmseg, sizeof(struct shmid_ds));
		break;
	case IPC_SET:
		if ((error = ipcperm(cred, &shmseg->shm_perm, IPC_M)) != 0)
			break;
		shmseg->shm_perm.uid = shmbuf->shm_perm.uid;
		shmseg->shm_perm.gid = shmbuf->shm_perm.gid;
		shmseg->shm_perm.mode =
		    (shmseg->shm_perm.mode & ~ACCESSPERMS) |
		    (shmbuf->shm_perm.mode & ACCESSPERMS);
		shmseg->shm_ctime = time_second;
		break;
	case IPC_RMID:
		if ((error = ipcperm(cred, &shmseg->shm_perm, IPC_M)) != 0)
			break;
		shmseg->shm_perm._key = IPC_PRIVATE;
		shmseg->shm_perm.mode |= SHMSEG_REMOVED;
		if (shmseg->shm_nattch <= 0) {
			uobj = shmseg->_shm_internal;
			shm_free_segment(IPCID_TO_IX(shmid));
		}
		break;
	case SHM_LOCK:
	case SHM_UNLOCK:
		if ((error = kauth_authorize_system(cred,
		    KAUTH_SYSTEM_SYSVIPC,
		    (cmd == SHM_LOCK) ? KAUTH_REQ_SYSTEM_SYSVIPC_SHM_LOCK :
		    KAUTH_REQ_SYSTEM_SYSVIPC_SHM_UNLOCK, NULL, NULL, NULL)) != 0)
			break;
		error = shm_memlock(l, shmseg, shmid, cmd);
		break;
	default:
		error = EINVAL;
	}

	mutex_exit(&shm_lock);
	if (uobj != NULL)
		uao_detach(uobj);
	return error;
}

/*
 * Try to take an already existing segment.
 *  => must be called with shm_lock held;
 *  => called from one place, thus, inline;
 */
static inline int
shmget_existing(struct lwp *l, const struct sys_shmget_args *uap, int mode,
    register_t *retval)
{
	struct shmid_ds *shmseg;
	kauth_cred_t cred = l->l_cred;
	int segnum, error;
again:
	KASSERT(mutex_owned(&shm_lock));

	/* Find segment by key */
	for (segnum = 0; segnum < shminfo.shmmni; segnum++)
		if ((shmsegs[segnum].shm_perm.mode & SHMSEG_ALLOCATED) &&
		    shmsegs[segnum].shm_perm._key == SCARG(uap, key))
			break;
	if (segnum == shminfo.shmmni) {
		/* Not found */
		return -1;
	}

	shmseg = &shmsegs[segnum];
	if (shmseg->shm_perm.mode & SHMSEG_REMOVED) {
		/*
		 * This segment is in the process of being allocated.  Wait
		 * until it's done, and look the key up again (in case the
		 * allocation failed or it was freed).
		 */
		shmseg->shm_perm.mode |= SHMSEG_WANTED;
		error = cv_wait_sig(&shm_cv[segnum], &shm_lock);
		if (error)
			return error;
		goto again;
	}

	/*
	 * First check the flags, to generate a useful error when a
	 * segment already exists.
	 */
	if ((SCARG(uap, shmflg) & (IPC_CREAT | IPC_EXCL)) ==
	    (IPC_CREAT | IPC_EXCL))
		return EEXIST;

	/* Check the permission and segment size. */
	error = ipcperm(cred, &shmseg->shm_perm, mode);
	if (error)
		return error;
	if (SCARG(uap, size) && SCARG(uap, size) > shmseg->shm_segsz)
		return EINVAL;

	*retval = IXSEQ_TO_IPCID(segnum, shmseg->shm_perm);
	return 0;
}

int
sys_shmget(struct lwp *l, const struct sys_shmget_args *uap, register_t *retval)
{
	/* {
		syscallarg(key_t) key;
		syscallarg(size_t) size;
		syscallarg(int) shmflg;
	} */
	struct shmid_ds *shmseg;
	kauth_cred_t cred = l->l_cred;
	key_t key = SCARG(uap, key);
	size_t size;
	int error, mode, segnum;
	bool lockmem;

	mode = SCARG(uap, shmflg) & ACCESSPERMS;
	if (SCARG(uap, shmflg) & _SHM_RMLINGER)
		mode |= SHMSEG_RMLINGER;

	SHMPRINTF(("shmget: key 0x%lx size 0x%zx shmflg 0x%x mode 0x%x\n",
	    SCARG(uap, key), SCARG(uap, size), SCARG(uap, shmflg), mode));

	mutex_enter(&shm_lock);
	/* In case of reallocation, we will wait for completion */
	while (__predict_false(shm_realloc_state))
		cv_wait(&shm_realloc_cv, &shm_lock);

	if (key != IPC_PRIVATE) {
		error = shmget_existing(l, uap, mode, retval);
		if (error != -1) {
			mutex_exit(&shm_lock);
			return error;
		}
		if ((SCARG(uap, shmflg) & IPC_CREAT) == 0) {
			mutex_exit(&shm_lock);
			return ENOENT;
		}
	}
	error = 0;

	/*
	 * Check the for the limits.
	 */
	size = SCARG(uap, size);
	if (size < shminfo.shmmin || size > shminfo.shmmax) {
		mutex_exit(&shm_lock);
		return EINVAL;
	}
	if (shm_nused >= shminfo.shmmni) {
		mutex_exit(&shm_lock);
		return ENOSPC;
	}
	size = (size + PGOFSET) & ~PGOFSET;
	if (shm_committed + btoc(size) > shminfo.shmall) {
		mutex_exit(&shm_lock);
		return ENOMEM;
	}

	/* Find the first available segment */
	if (shm_last_free < 0) {
		for (segnum = 0; segnum < shminfo.shmmni; segnum++)
			if (shmsegs[segnum].shm_perm.mode & SHMSEG_FREE)
				break;
		KASSERT(segnum < shminfo.shmmni);
	} else {
		segnum = shm_last_free;
		shm_last_free = -1;
	}

	/*
	 * Initialize the segment.
	 * We will drop the lock while allocating the memory, thus mark the
	 * segment present, but removed, that no other thread could take it.
	 * Also, disable reallocation, while lock is dropped.
	 */
	shmseg = &shmsegs[segnum];
	shmseg->shm_perm.mode = SHMSEG_ALLOCATED | SHMSEG_REMOVED;
	shm_committed += btoc(size);
	shm_nused++;
	lockmem = shm_use_phys;
	shm_realloc_disable++;
	mutex_exit(&shm_lock);

	/* Allocate the memory object and lock it if needed */
	shmseg->_shm_internal = uao_create(size, 0);
	if (lockmem) {
		/* Wire the pages and tag it */
		error = uvm_obj_wirepages(shmseg->_shm_internal, 0, size, NULL);
		if (error) {
			uao_detach(shmseg->_shm_internal);
			mutex_enter(&shm_lock);
			shm_free_segment(segnum);
			shm_realloc_disable--;
			mutex_exit(&shm_lock);
			return error;
		}
	}

	/*
	 * Please note, while segment is marked, there are no need to hold the
	 * lock, while setting it (except shm_perm.mode).
	 */
	shmseg->shm_perm._key = SCARG(uap, key);
	shmseg->shm_perm._seq = (shmseg->shm_perm._seq + 1) & 0x7fff;
	*retval = IXSEQ_TO_IPCID(segnum, shmseg->shm_perm);

	shmseg->shm_perm.cuid = shmseg->shm_perm.uid = kauth_cred_geteuid(cred);
	shmseg->shm_perm.cgid = shmseg->shm_perm.gid = kauth_cred_getegid(cred);
	shmseg->shm_segsz = SCARG(uap, size);
	shmseg->shm_cpid = l->l_proc->p_pid;
	shmseg->shm_lpid = shmseg->shm_nattch = 0;
	shmseg->shm_atime = shmseg->shm_dtime = 0;
	shmseg->shm_ctime = time_second;

	/*
	 * Segment is initialized.
	 * Enter the lock, mark as allocated, and notify waiters (if any).
	 * Also, unmark the state of reallocation.
	 */
	mutex_enter(&shm_lock);
	shmseg->shm_perm.mode = (shmseg->shm_perm.mode & SHMSEG_WANTED) |
	    (mode & (ACCESSPERMS | SHMSEG_RMLINGER)) |
	    SHMSEG_ALLOCATED | (lockmem ? SHMSEG_WIRED : 0);
	if (shmseg->shm_perm.mode & SHMSEG_WANTED) {
		shmseg->shm_perm.mode &= ~SHMSEG_WANTED;
		cv_broadcast(&shm_cv[segnum]);
	}
	shm_realloc_disable--;
	cv_broadcast(&shm_realloc_cv);
	mutex_exit(&shm_lock);

	return error;
}

void
shmfork(struct vmspace *vm1, struct vmspace *vm2)
{
	struct shmmap_state *shmmap_s;
	struct shmmap_entry *shmmap_se;

	SHMPRINTF(("shmfork %p->%p\n", vm1, vm2));
	mutex_enter(&shm_lock);
	vm2->vm_shm = vm1->vm_shm;
	if (vm1->vm_shm) {
		shmmap_s = (struct shmmap_state *)vm1->vm_shm;
		SLIST_FOREACH(shmmap_se, &shmmap_s->entries, next)
			shmsegs[IPCID_TO_IX(shmmap_se->shmid)].shm_nattch++;
		shmmap_s->nrefs++;
	}
	mutex_exit(&shm_lock);
}

void
shmexit(struct vmspace *vm)
{
	struct shmmap_state *shmmap_s;
	struct shmmap_entry *shmmap_se;

	mutex_enter(&shm_lock);
	shmmap_s = (struct shmmap_state *)vm->vm_shm;
	if (shmmap_s == NULL) {
		mutex_exit(&shm_lock);
		return;
	}
	vm->vm_shm = NULL;

	if (--shmmap_s->nrefs > 0) {
		SHMPRINTF(("shmexit: vm %p drop ref (%d entries), refs = %d\n",
		    vm, shmmap_s->nitems, shmmap_s->nrefs));
		SLIST_FOREACH(shmmap_se, &shmmap_s->entries, next) {
			shmsegs[IPCID_TO_IX(shmmap_se->shmid)].shm_nattch--;
		}
		mutex_exit(&shm_lock);
		return;
	}

	SHMPRINTF(("shmexit: vm %p cleanup (%d entries)\n", vm, shmmap_s->nitems));
	if (shmmap_s->nitems == 0) {
		mutex_exit(&shm_lock);
		kmem_free(shmmap_s, sizeof(struct shmmap_state));
		return;
	}

	/*
	 * Delete the entry from shm map.
	 */
	for (;;) {
		struct shmid_ds *shmseg;
		struct uvm_object *uobj;
		size_t sz;

		shmmap_se = SLIST_FIRST(&shmmap_s->entries);
		KASSERT(shmmap_se != NULL);

		shmseg = &shmsegs[IPCID_TO_IX(shmmap_se->shmid)];
		sz = (shmseg->shm_segsz + PGOFSET) & ~PGOFSET;
		/* shm_delete_mapping() removes from the list. */
		uobj = shm_delete_mapping(shmmap_s, shmmap_se);
		mutex_exit(&shm_lock);

		uvm_deallocate(&vm->vm_map, shmmap_se->va, sz);
		if (uobj != NULL) {
			uao_detach(uobj);
		}
		kmem_free(shmmap_se, sizeof(struct shmmap_entry));

		if (SLIST_EMPTY(&shmmap_s->entries)) {
			break;
		}
		mutex_enter(&shm_lock);
		KASSERT(!SLIST_EMPTY(&shmmap_s->entries));
	}
	kmem_free(shmmap_s, sizeof(struct shmmap_state));
}

static int
shmrealloc(int newshmni)
{
	vaddr_t v;
	struct shmid_ds *oldshmsegs, *newshmsegs;
	kcondvar_t *newshm_cv, *oldshm_cv;
	size_t sz;
	int i, lsegid, oldshmni;

	if (newshmni < 1)
		return EINVAL;

	/* Allocate new memory area */
	sz = ALIGN(newshmni * sizeof(struct shmid_ds)) +
	    ALIGN(newshmni * sizeof(kcondvar_t));
	sz = round_page(sz);
	v = uvm_km_alloc(kernel_map, sz, 0, UVM_KMF_WIRED|UVM_KMF_ZERO);
	if (v == 0)
		return ENOMEM;

	mutex_enter(&shm_lock);
	while (shm_realloc_state || shm_realloc_disable)
		cv_wait(&shm_realloc_cv, &shm_lock);

	/*
	 * Get the number of last segment.  Fail we are trying to
	 * reallocate less memory than we use.
	 */
	lsegid = 0;
	for (i = 0; i < shminfo.shmmni; i++)
		if ((shmsegs[i].shm_perm.mode & SHMSEG_FREE) == 0)
			lsegid = i;
	if (lsegid >= newshmni) {
		mutex_exit(&shm_lock);
		uvm_km_free(kernel_map, v, sz, UVM_KMF_WIRED);
		return EBUSY;
	}
	shm_realloc_state = true;

	newshmsegs = (void *)v;
	newshm_cv = (void *)((uintptr_t)newshmsegs +
	    ALIGN(newshmni * sizeof(struct shmid_ds)));

	/* Copy all memory to the new area */
	for (i = 0; i < shm_nused; i++) {
		cv_init(&newshm_cv[i], "shmwait");
		(void)memcpy(&newshmsegs[i], &shmsegs[i],
		    sizeof(newshmsegs[0]));
	}

	/* Mark as free all new segments, if there is any */
	for (; i < newshmni; i++) {
		cv_init(&newshm_cv[i], "shmwait");
		newshmsegs[i].shm_perm.mode = SHMSEG_FREE;
		newshmsegs[i].shm_perm._seq = 0;
	}

	oldshmsegs = shmsegs;
	oldshmni = shminfo.shmmni;
	shminfo.shmmni = newshmni;
	shmsegs = newshmsegs;
	shm_cv = newshm_cv;

	/* Reallocation completed - notify all waiters, if any */
	shm_realloc_state = false;
	cv_broadcast(&shm_realloc_cv);
	mutex_exit(&shm_lock);

	/* Release now unused resources. */
	oldshm_cv = (void *)((uintptr_t)oldshmsegs +
	    ALIGN(oldshmni * sizeof(struct shmid_ds)));
	for (i = 0; i < oldshmni; i++)
		cv_destroy(&oldshm_cv[i]);

	sz = ALIGN(oldshmni * sizeof(struct shmid_ds)) +
	    ALIGN(oldshmni * sizeof(kcondvar_t));
	sz = round_page(sz);
	uvm_km_free(kernel_map, (vaddr_t)oldshmsegs, sz, UVM_KMF_WIRED);

	return 0;
}

void
shminit(void)
{
	vaddr_t v;
	size_t sz;
	int i;

	mutex_init(&shm_lock, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&shm_realloc_cv, "shmrealc");

	/* Allocate the wired memory for our structures */
	sz = ALIGN(shminfo.shmmni * sizeof(struct shmid_ds)) +
	    ALIGN(shminfo.shmmni * sizeof(kcondvar_t));
	sz = round_page(sz);
	v = uvm_km_alloc(kernel_map, sz, 0, UVM_KMF_WIRED|UVM_KMF_ZERO);
	if (v == 0)
		panic("sysv_shm: cannot allocate memory");
	shmsegs = (void *)v;
	shm_cv = (void *)((uintptr_t)shmsegs +
	    ALIGN(shminfo.shmmni * sizeof(struct shmid_ds)));

	if (shminfo.shmmax == 0)
		shminfo.shmmax = max(physmem / 4, 1024) * PAGE_SIZE;
	else
		shminfo.shmmax *= PAGE_SIZE;
	shminfo.shmall = shminfo.shmmax / PAGE_SIZE;

	for (i = 0; i < shminfo.shmmni; i++) {
		cv_init(&shm_cv[i], "shmwait");
		shmsegs[i].shm_perm.mode = SHMSEG_FREE;
		shmsegs[i].shm_perm._seq = 0;
	}
	shm_last_free = 0;
	shm_nused = 0;
	shm_committed = 0;
	shm_realloc_disable = 0;
	shm_realloc_state = false;

	kern_has_sysvshm = 1;

	sysvipcinit();
}

int
shmfini(void)
{
	size_t sz;
	int i;
	vaddr_t v = (vaddr_t)shmsegs;

	mutex_enter(&shm_lock);
	if (shm_nused) {
		mutex_exit(&shm_lock);
		return 1;
	}

	/* Destroy all condvars */
	for (i = 0; i < shminfo.shmmni; i++)
		cv_destroy(&shm_cv[i]);
	cv_destroy(&shm_realloc_cv);

	/* Free the allocated/wired memory */
	sz = ALIGN(shminfo.shmmni * sizeof(struct shmid_ds)) +
	    ALIGN(shminfo.shmmni * sizeof(kcondvar_t));
	sz = round_page(sz);
	uvm_km_free(kernel_map, v, sz, UVM_KMF_WIRED);

	/* Release and destroy our mutex */
	mutex_exit(&shm_lock);
	mutex_destroy(&shm_lock);

	kern_has_sysvshm = 0;

	return 0;
}

static int
sysctl_ipc_shmmni(SYSCTLFN_ARGS)
{
	int newsize, error;
	struct sysctlnode node;
	node = *rnode;
	node.sysctl_data = &newsize;

	newsize = shminfo.shmmni;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

	sysctl_unlock();
	error = shmrealloc(newsize);
	sysctl_relock();
	return error;
}

static int
sysctl_ipc_shmmaxpgs(SYSCTLFN_ARGS)
{
	uint32_t newsize;
	int error;
	struct sysctlnode node;
	node = *rnode;
	node.sysctl_data = &newsize;

	newsize = shminfo.shmall;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

	if (newsize < 1)
		return EINVAL;

	shminfo.shmall = newsize;
	shminfo.shmmax = (uint64_t)shminfo.shmall * PAGE_SIZE;

	return 0;
}

static int
sysctl_ipc_shmmax(SYSCTLFN_ARGS)
{
	uint64_t newsize;
	int error;
	struct sysctlnode node;
	node = *rnode;
	node.sysctl_data = &newsize;

	newsize = shminfo.shmmax;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

	if (newsize < PAGE_SIZE)
		return EINVAL;

	shminfo.shmmax = round_page(newsize);
	shminfo.shmall = shminfo.shmmax >> PAGE_SHIFT;

	return 0;
}

SYSCTL_SETUP(sysctl_ipc_shm_setup, "sysctl kern.ipc subtree setup")
{

	sysctl_createv(clog, 0, NULL, NULL,
		CTLFLAG_PERMANENT,
		CTLTYPE_NODE, "ipc",
		SYSCTL_DESCR("SysV IPC options"),
		NULL, 0, NULL, 0,
		CTL_KERN, KERN_SYSVIPC, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_QUAD, "shmmax",
		SYSCTL_DESCR("Max shared memory segment size in bytes"),
		sysctl_ipc_shmmax, 0, &shminfo.shmmax, 0,
		CTL_KERN, KERN_SYSVIPC, KERN_SYSVIPC_SHMMAX, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "shmmni",
		SYSCTL_DESCR("Max number of shared memory identifiers"),
		sysctl_ipc_shmmni, 0, &shminfo.shmmni, 0,
		CTL_KERN, KERN_SYSVIPC, KERN_SYSVIPC_SHMMNI, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "shmseg",
		SYSCTL_DESCR("Max shared memory segments per process"),
		NULL, 0, &shminfo.shmseg, 0,
		CTL_KERN, KERN_SYSVIPC, KERN_SYSVIPC_SHMSEG, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "shmmaxpgs",
		SYSCTL_DESCR("Max amount of shared memory in pages"),
		sysctl_ipc_shmmaxpgs, 0, &shminfo.shmall, 0,
		CTL_KERN, KERN_SYSVIPC, KERN_SYSVIPC_SHMMAXPGS, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "shm_use_phys",
		SYSCTL_DESCR("Enable/disable locking of shared memory in "
		    "physical memory"), NULL, 0, &shm_use_phys, 0,
		CTL_KERN, KERN_SYSVIPC, KERN_SYSVIPC_SHMUSEPHYS, CTL_EOL);
}
