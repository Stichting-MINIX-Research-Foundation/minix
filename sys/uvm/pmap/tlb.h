/*	$NetBSD: tlb.h,v 1.2 2015/09/21 15:50:19 matt Exp $	*/
/*-
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas of 3am Software Foundry.
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
#ifndef _UVM_PMAP_PMAP_TLB_TLB_H_
#define	_UVM_PMAP_PMAP_TLB_TLB_H_

#if defined(_KERNEL) || defined(_KMEMUSER)

struct tlbmask;

struct tlb_md_ops {
	void	(*md_tlb_set_asid)(tlb_asid_t);
	tlb_asid_t
		(*md_tlb_get_asid)(void);
	void	(*md_tlb_invalidate_all)(void);
	void	(*md_tlb_invalidate_globals)(void);
	void	(*md_tlb_invalidate_asids)(tlb_asid_t, tlb_asid_t);
	void	(*md_tlb_invalidate_addr)(vaddr_t, tlb_asid_t);
	bool	(*md_tlb_update_addr)(vaddr_t, tlb_asid_t, pt_entry_t, bool);
	void	(*md_tlb_read_entry)(size_t, struct tlbmask *);
	void	(*md_tlb_write_entry)(size_t, const struct tlbmask *);
	u_int	(*md_tlb_record_asids)(u_long *);
	void	(*md_tlb_dump)(void (*)(const char *, ...));
	void	(*md_tlb_walk)(void *, bool (*)(void *, vaddr_t, tlb_asid_t,
		    pt_entry_t));
};

tlb_asid_t
	tlb_get_asid(void);
void	tlb_set_asid(tlb_asid_t);
void	tlb_invalidate_all(void);
void	tlb_invalidate_globals(void);
void	tlb_invalidate_asids(tlb_asid_t, tlb_asid_t);
void	tlb_invalidate_addr(vaddr_t, tlb_asid_t);
bool	tlb_update_addr(vaddr_t, tlb_asid_t, pt_entry_t, bool);
u_int	tlb_record_asids(u_long *);
void	tlb_enter_addr(size_t, const struct tlbmask *);
void	tlb_read_entry(size_t, struct tlbmask *);
void	tlb_write_entry(size_t, const struct tlbmask *);
void	tlb_walk(void *, bool (*)(void *, vaddr_t, tlb_asid_t, pt_entry_t));
void	tlb_dump(void (*)(const char *, ...));

#endif /* _KERNEL || _KMEMUSER */

#endif /* !_UVM_PMAP_PMAP_TLB_TLB_H_ */
