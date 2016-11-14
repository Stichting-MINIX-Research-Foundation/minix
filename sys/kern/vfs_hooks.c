/*	$NetBSD: vfs_hooks.c,v 1.6 2009/03/15 17:14:40 cegger Exp $	*/

/*-
 * Copyright (c) 2005 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Julio M. Merino Vidal.
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
 * VFS hooks.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: vfs_hooks.c,v 1.6 2009/03/15 17:14:40 cegger Exp $");

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/mount.h>
#include <sys/mutex.h>

LIST_HEAD(vfs_hooks_head, vfs_hooks) vfs_hooks_head =
    LIST_HEAD_INITIALIZER(vfs_hooks_head);

kmutex_t vfs_hooks_lock;

void
vfs_hooks_init(void)
{

	mutex_init(&vfs_hooks_lock, MUTEX_DEFAULT, IPL_NONE);
}

int
vfs_hooks_attach(struct vfs_hooks *vfs_hooks)
{

	mutex_enter(&vfs_hooks_lock);
	LIST_INSERT_HEAD(&vfs_hooks_head, vfs_hooks, vfs_hooks_list);
	mutex_exit(&vfs_hooks_lock);

	return (0);
}

int
vfs_hooks_detach(struct vfs_hooks *vfs_hooks)
{
	struct vfs_hooks *hp;
	int ret = 0;

	mutex_enter(&vfs_hooks_lock);
        LIST_FOREACH(hp, &vfs_hooks_head, vfs_hooks_list) {
                if (hp == vfs_hooks) {
                        LIST_REMOVE(hp, vfs_hooks_list);
                        break;
                }
        }
        if (hp == NULL)
       		ret = ESRCH;
	mutex_exit(&vfs_hooks_lock);

	return (ret);
}

/*
 * Macro to be used in one of the vfs_hooks_* function for hooks that
 * return an error code.  Calls will stop as soon as one of the hooks
 * fails.
 */
#define VFS_HOOKS_W_ERROR(func, fargs, hook, hargs)			\
int									\
func fargs								\
{									\
	int error;							\
	struct vfs_hooks *hp;						\
 									\
	error = EJUSTRETURN;						\
 									\
	mutex_enter(&vfs_hooks_lock);					\
        LIST_FOREACH(hp, &vfs_hooks_head, vfs_hooks_list) {		\
		if (hp-> hook != NULL) {				\
			error = hp-> hook hargs;			\
			if (error != 0)					\
				break;					\
		}							\
	}								\
	mutex_exit(&vfs_hooks_lock);					\
 									\
	return error;							\
}

/*
 * Macro to be used in one of the vfs_hooks_* function for hooks that
 * do not return any error code.  All hooks will be executed
 * unconditionally.
 */
#define VFS_HOOKS_WO_ERROR(func, fargs, hook, hargs)			\
void									\
func fargs								\
{									\
	struct vfs_hooks *hp;						\
 									\
	mutex_enter(&vfs_hooks_lock);					\
        LIST_FOREACH(hp, &vfs_hooks_head, vfs_hooks_list) {		\
		if (hp-> hook != NULL)					\
			hp-> hook hargs;				\
	}								\
	mutex_exit(&vfs_hooks_lock);					\
}

/*
 * Routines to iterate over VFS hooks lists and execute them.
 */

VFS_HOOKS_WO_ERROR(vfs_hooks_unmount, (struct mount *mp), vh_unmount, (mp));
VFS_HOOKS_W_ERROR(vfs_hooks_reexport, (struct mount *mp, const char *path, void *data), vh_reexport, (mp, path, data));
