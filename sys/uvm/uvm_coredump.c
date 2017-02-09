/*	$NetBSD: uvm_coredump.c,v 1.6 2014/01/07 07:59:03 dsl Exp $	*/

/*
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
 * Copyright (c) 1991, 1993, The Regents of the University of California.
 *
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * The Mach Operating System project at Carnegie-Mellon University.
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
 *	@(#)vm_glue.c	8.6 (Berkeley) 1/5/94
 * from: Id: uvm_glue.c,v 1.1.2.8 1998/02/07 01:16:54 chs Exp
 *
 *
 * Copyright (c) 1987, 1990 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uvm_coredump.c,v 1.6 2014/01/07 07:59:03 dsl Exp $");

/*
 * uvm_coredump.c: glue functions for coredump
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>

#include <uvm/uvm.h>

/*
 * uvm_coredump_walkmap: walk a process's map for the purpose of dumping
 * a core file.
 * XXX: I'm not entirely sure the locking is this function is in anyway
 * correct.  If the process isn't actually stopped then the data passed
 * to func() is at best stale, and horrid things might happen if the
 * entry being processed is deleted (dsl).
 */

int
uvm_coredump_walkmap(struct proc *p, int (*func)(struct uvm_coredump_state *),
    void *cookie)
{
	struct uvm_coredump_state state;
	struct vmspace *vm = p->p_vmspace;
	struct vm_map *map = &vm->vm_map;
	struct vm_map_entry *entry;
	int error;

	entry = NULL;
	vm_map_lock_read(map);
	state.end = 0;
	for (;;) {
		if (entry == NULL)
			entry = map->header.next;
		else if (!uvm_map_lookup_entry(map, state.end, &entry))
			entry = entry->next;
		if (entry == &map->header)
			break;

		state.cookie = cookie;
		if (state.end > entry->start) {
			state.start = state.end;
		} else {
			state.start = entry->start;
		}
		state.realend = entry->end;
		state.end = entry->end;
		state.prot = entry->protection;
		state.flags = 0;

		/*
		 * Dump the region unless one of the following is true:
		 *
		 * (1) the region has neither object nor amap behind it
		 *     (ie. it has never been accessed).
		 *
		 * (2) the region has no amap and is read-only
		 *     (eg. an executable text section).
		 *
		 * (3) the region's object is a device.
		 *
		 * (4) the region is unreadable by the process.
		 */

		KASSERT(!UVM_ET_ISSUBMAP(entry));
		KASSERT(state.start < VM_MAXUSER_ADDRESS);
		KASSERT(state.end <= VM_MAXUSER_ADDRESS);
		if (entry->object.uvm_obj == NULL &&
		    entry->aref.ar_amap == NULL) {
			state.realend = state.start;
		} else if ((entry->protection & VM_PROT_WRITE) == 0 &&
		    entry->aref.ar_amap == NULL) {
			state.realend = state.start;
		} else if (entry->object.uvm_obj != NULL &&
		    UVM_OBJ_IS_DEVICE(entry->object.uvm_obj)) {
			state.realend = state.start;
		} else if ((entry->protection & VM_PROT_READ) == 0) {
			state.realend = state.start;
		} else {
			if (state.start >= (vaddr_t)vm->vm_maxsaddr)
				state.flags |= UVM_COREDUMP_STACK;

			/*
			 * If this an anonymous entry, only dump instantiated
			 * pages.
			 */
			if (entry->object.uvm_obj == NULL) {
				vaddr_t end;

				amap_lock(entry->aref.ar_amap);
				for (end = state.start;
				     end < state.end; end += PAGE_SIZE) {
					struct vm_anon *anon;
					anon = amap_lookup(&entry->aref,
					    end - entry->start);
					/*
					 * If we have already encountered an
					 * uninstantiated page, stop at the
					 * first instantied page.
					 */
					if (anon != NULL &&
					    state.realend != state.end) {
						state.end = end;
						break;
					}

					/*
					 * If this page is the first
					 * uninstantiated page, mark this as
					 * the real ending point.  Continue to
					 * counting uninstantiated pages.
					 */
					if (anon == NULL &&
					    state.realend == state.end) {
						state.realend = end;
					}
				}
				amap_unlock(entry->aref.ar_amap);
			}
		}

		vm_map_unlock_read(map);
		error = (*func)(&state);
		if (error)
			return (error);
		vm_map_lock_read(map);
	}
	vm_map_unlock_read(map);

	return (0);
}

static int
count_segs(struct uvm_coredump_state *s)
{
    (*(int *)s->cookie)++;

    return 0;
}

int
uvm_coredump_count_segs(struct proc *p)
{
	int count = 0;

	uvm_coredump_walkmap(p, count_segs, &count);
	return count;
}
