/*	$NetBSD: procfs_map.c,v 1.45 2014/10/17 20:49:22 christos Exp $	*/

/*
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
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
 *	@(#)procfs_status.c	8.3 (Berkeley) 2/17/94
 *
 *	$FreeBSD: procfs_map.c,v 1.18 1998/12/04 22:54:51 archie Exp $
 */

/*
 * Copyright (c) 1993 Jan-Simon Pendry
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)procfs_status.c	8.3 (Berkeley) 2/17/94
 *
 *	$FreeBSD: procfs_map.c,v 1.18 1998/12/04 22:54:51 archie Exp $
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: procfs_map.c,v 1.45 2014/10/17 20:49:22 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/filedesc.h>
#include <miscfs/procfs/procfs.h>

#include <sys/lock.h>

#include <uvm/uvm.h>

#define BUFFERSIZE (64 * 1024)
#define MAXBUFFERSIZE (256 * 1024)

/*
 * The map entries can *almost* be read with programs like cat.  However,
 * large maps need special programs to read.  It is not easy to implement
 * a program that can sense the required size of the buffer, and then
 * subsequently do a read with the appropriate size.  This operation cannot
 * be atomic.  The best that we can do is to allow the program to do a read
 * with an arbitrarily large buffer, and return as much as we can.  We can
 * return an error code if the buffer is too small (EFBIG), then the program
 * can try a bigger buffer.
 */
int
procfs_domap(struct lwp *curl, struct proc *p, struct pfsnode *pfs,
	     struct uio *uio, int linuxmode)
{
	int error;
	struct vmspace *vm;
	struct vm_map *map;
	struct vm_map_entry *entry;
	char *buffer = NULL;
	size_t bufsize = BUFFERSIZE;
	char *path;
	struct vnode *vp;
	struct vattr va;
	dev_t dev;
	long fileid;
	size_t pos;
	int width = (int)((curl->l_proc->p_flag & PK_32) ? sizeof(int32_t) : 
	    sizeof(void *)) * 2;

	if (uio->uio_rw != UIO_READ)
		return EOPNOTSUPP;

	error = 0;

	if (linuxmode != 0)
		path = malloc(MAXPATHLEN * 4, M_TEMP, M_WAITOK);
	else
		path = NULL;

	if ((error = proc_vmspace_getref(p, &vm)) != 0)
		goto out;

	map = &vm->vm_map;
	vm_map_lock_read(map);

again:
	buffer = malloc(bufsize, M_TEMP, M_WAITOK);
	pos = 0;
	for (entry = map->header.next; entry != &map->header;
	    entry = entry->next) {

		if (UVM_ET_ISSUBMAP(entry))
			continue;

		if (linuxmode != 0) {
			*path = 0;
			dev = (dev_t)0;
			fileid = 0;
			if (UVM_ET_ISOBJ(entry) &&
			    UVM_OBJ_IS_VNODE(entry->object.uvm_obj)) {
				vp = (struct vnode *)entry->object.uvm_obj;
				vn_lock(vp, LK_SHARED | LK_RETRY);
				error = VOP_GETATTR(vp, &va, curl->l_cred);
				VOP_UNLOCK(vp);
				if (error == 0 && vp != pfs->pfs_vnode) {
					fileid = va.va_fileid;
					dev = va.va_fsid;
					error = vnode_to_path(path,
					    MAXPATHLEN * 4, vp, curl, p);
				}
			}
			pos += snprintf(buffer + pos, bufsize - pos,
			    "%.*"PRIxVADDR"-%.*"PRIxVADDR" %c%c%c%c "
			    "%.*lx %.2llx:%.2llx %-8ld %25.s %s\n",
			    width, entry->start,
			    width, entry->end,
			    (entry->protection & VM_PROT_READ) ? 'r' : '-',
			    (entry->protection & VM_PROT_WRITE) ? 'w' : '-',
			    (entry->protection & VM_PROT_EXECUTE) ? 'x' : '-',
			    (entry->etype & UVM_ET_COPYONWRITE) ? 'p' : 's',
			    width, (unsigned long)entry->offset,
			    (unsigned long long)major(dev),
			    (unsigned long long)minor(dev), fileid, "", path);
		} else {
			pos += snprintf(buffer + pos, bufsize - pos,
			    "%#"PRIxVADDR" %#"PRIxVADDR" "
			    "%c%c%c %c%c%c %s %s %d %d %d\n",
			    entry->start, entry->end,
			    (entry->protection & VM_PROT_READ) ? 'r' : '-',
			    (entry->protection & VM_PROT_WRITE) ? 'w' : '-',
			    (entry->protection & VM_PROT_EXECUTE) ? 'x' : '-',
			    (entry->max_protection & VM_PROT_READ) ? 'r' : '-',
			    (entry->max_protection & VM_PROT_WRITE) ? 'w' : '-',
			    (entry->max_protection & VM_PROT_EXECUTE) ?
				'x' : '-',
			    (entry->etype & UVM_ET_COPYONWRITE) ?
				"COW" : "NCOW",
			    (entry->etype & UVM_ET_NEEDSCOPY) ? "NC" : "NNC",
			    entry->inheritance, entry->wired_count,
			    entry->advice);
		}
		if (pos >= bufsize) {
			bufsize <<= 1;
			if (bufsize > MAXBUFFERSIZE) {
				error = ENOMEM;
				vm_map_unlock_read(map);
				uvmspace_free(vm);
				goto out;
			}
			free(buffer, M_TEMP);
			goto again;
		}
	}

	vm_map_unlock_read(map);
	uvmspace_free(vm);

	/*
	 * We support reading from an offset, because linux does.
	 * The map could have changed between the two reads, and
	 * that could result in junk, but typically it does not.
	 */
	if (uio->uio_offset < pos)
		error = uiomove(buffer + uio->uio_offset,
		    pos - uio->uio_offset, uio);
	else
		error = 0;
out:
	if (path != NULL)
		free(path, M_TEMP);
	if (buffer != NULL)
		free(buffer, M_TEMP);

	return error;
}

int
procfs_validmap(struct lwp *l, struct mount *mp)
{
	return ((l->l_flag & LW_SYSTEM) == 0);
}
