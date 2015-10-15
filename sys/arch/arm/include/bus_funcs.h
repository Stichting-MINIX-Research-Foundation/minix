/*	$NetBSD: bus_funcs.h,v 1.6 2014/01/29 00:42:15 matt Exp $	*/

/*-
 * Copyright (c) 1996, 1997, 1998, 2001 The NetBSD Foundation, Inc.
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
 * Copyright (c) 1996 Charles M. Hannum.  All rights reserved.
 * Copyright (c) 1996 Christopher G. Demetriou.  All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _ARM_BUS_FUNCS_H_
#define _ARM_BUS_FUNCS_H_

#ifdef _KERNEL_OPT
#include "opt_cputypes.h"
#endif

/*
 * Utility macros; INTERNAL USE ONLY.
 */
#define	__bs_c(a,b)		__CONCAT(a,b)
#define	__bs_opname(op,size)	__bs_c(__bs_c(__bs_c(bs_,op),_),size)

#define	__bs_rs(sz, t, h, o)						\
	(*(t)->__bs_opname(r,sz))((t)->bs_cookie, h, o)
#define	__bs_ws(sz, t, h, o, v)						\
	(*(t)->__bs_opname(w,sz))((t)->bs_cookie, h, o, v)
#define	__bs_nonsingle(type, sz, t, h, o, a, c)				\
	(*(t)->__bs_opname(type,sz))((t)->bs_cookie, h, o, a, c)
#define	__bs_set(type, sz, t, h, o, v, c)				\
	(*(t)->__bs_opname(type,sz))((t)->bs_cookie, h, o, v, c)
#define	__bs_copy(sz, t, h1, o1, h2, o2, cnt)				\
	(*(t)->__bs_opname(c,sz))((t)->bs_cookie, h1, o1, h2, o2, cnt)

#ifdef __BUS_SPACE_HAS_STREAM_METHODS
#define	__bs_opname_s(op,size)	__bs_c(__bs_c(__bs_c(__bs_c(bs_,op),_),size),_s)
#define	__bs_rs_s(sz, t, h, o)						\
	(*(t)->__bs_opname_s(r,sz))((t)->bs_cookie, h, o)
#define	__bs_ws_s(sz, t, h, o, v)					\
	(*(t)->__bs_opname_s(w,sz))((t)->bs_cookie, h, o, v)
#define	__bs_nonsingle_s(type, sz, t, h, o, a, c)			\
	(*(t)->__bs_opname_s(type,sz))((t)->bs_cookie, h, o, a, c)
#define	__bs_set_s(type, sz, t, h, o, v, c)				\
	(*(t)->__bs_opname_s(type,sz))((t)->bs_cookie, h, o, v, c)
#define	__bs_copy_s(sz, t, h1, o1, h2, o2, cnt)				\
	(*(t)->__bs_opname_s(c,sz))((t)->bs_cookie, h1, o1, h2, o2, cnt)
#endif

/*
 * Mapping and unmapping operations.
 */
#define	bus_space_map(t, a, s, c, hp)					\
	(*(t)->bs_map)((t)->bs_cookie, (a), (s), (c), (hp))
#define	bus_space_unmap(t, h, s)					\
	(*(t)->bs_unmap)((t)->bs_cookie, (h), (s))
#define	bus_space_subregion(t, h, o, s, hp)				\
	(*(t)->bs_subregion)((t)->bs_cookie, (h), (o), (s), (hp))


/*
 * Allocation and deallocation operations.
 */
#define	bus_space_alloc(t, rs, re, s, a, b, c, ap, hp)			\
	(*(t)->bs_alloc)((t)->bs_cookie, (rs), (re), (s), (a), (b),	\
	    (c), (ap), (hp))
#define	bus_space_free(t, h, s)						\
	(*(t)->bs_free)((t)->bs_cookie, (h), (s))

/*
 * Get kernel virtual address for ranges mapped BUS_SPACE_MAP_LINEAR.
 */
#define	bus_space_vaddr(t, h)						\
	(*(t)->bs_vaddr)((t)->bs_cookie, (h))

/*
 * MMap bus space for a user application.
 */
#define bus_space_mmap(t, a, o, p, f)					\
	(*(t)->bs_mmap)((t)->bs_cookie, (a), (o), (p), (f))

/*
 * Bus barrier operations.
 */
#define	bus_space_barrier(t, h, o, l, f)				\
	(*(t)->bs_barrier)((t)->bs_cookie, (h), (o), (l), (f))

/*
 * Bus read (single) operations.
 */
#define	bus_space_read_1(t, h, o)	__bs_rs(1,(t),(h),(o))
#define	bus_space_read_2(t, h, o)	__bs_rs(2,(t),(h),(o))
#define	bus_space_read_4(t, h, o)	__bs_rs(4,(t),(h),(o))
#define	bus_space_read_8(t, h, o)	__bs_rs(8,(t),(h),(o))
#ifdef __BUS_SPACE_HAS_STREAM_METHODS
#define	bus_space_read_stream_1(t, h, o)	__bs_rs_s(1,(t),(h),(o))
#define	bus_space_read_stream_2(t, h, o)	__bs_rs_s(2,(t),(h),(o))
#define	bus_space_read_stream_4(t, h, o)	__bs_rs_s(4,(t),(h),(o))
#define	bus_space_read_stream_8(t, h, o)	__bs_rs_s(8,(t),(h),(o))
#endif


/*
 * Bus read multiple operations.
 */
#define	bus_space_read_multi_1(t, h, o, a, c)				\
	__bs_nonsingle(rm,1,(t),(h),(o),(a),(c))
#define	bus_space_read_multi_2(t, h, o, a, c)				\
	__bs_nonsingle(rm,2,(t),(h),(o),(a),(c))
#define	bus_space_read_multi_4(t, h, o, a, c)				\
	__bs_nonsingle(rm,4,(t),(h),(o),(a),(c))
#define	bus_space_read_multi_8(t, h, o, a, c)				\
	__bs_nonsingle(rm,8,(t),(h),(o),(a),(c))
#ifdef __BUS_SPACE_HAS_STREAM_METHODS
#define	bus_space_read_multi_stream_1(t, h, o, a, c)			\
	__bs_nonsingle_s(rm,1,(t),(h),(o),(a),(c))
#define	bus_space_read_multi_stream_2(t, h, o, a, c)			\
	__bs_nonsingle_s(rm,2,(t),(h),(o),(a),(c))
#define	bus_space_read_multi_stream_4(t, h, o, a, c)			\
	__bs_nonsingle_s(rm,4,(t),(h),(o),(a),(c))
#define	bus_space_read_multi_stream_8(t, h, o, a, c)			\
	__bs_nonsingle_s(rm,8,(t),(h),(o),(a),(c))
#endif


/*
 * Bus read region operations.
 */
#define	bus_space_read_region_1(t, h, o, a, c)				\
	__bs_nonsingle(rr,1,(t),(h),(o),(a),(c))
#define	bus_space_read_region_2(t, h, o, a, c)				\
	__bs_nonsingle(rr,2,(t),(h),(o),(a),(c))
#define	bus_space_read_region_4(t, h, o, a, c)				\
	__bs_nonsingle(rr,4,(t),(h),(o),(a),(c))
#define	bus_space_read_region_8(t, h, o, a, c)				\
	__bs_nonsingle(rr,8,(t),(h),(o),(a),(c))
#ifdef __BUS_SPACE_HAS_STREAM_METHODS
#define	bus_space_read_region_stream_1(t, h, o, a, c)			\
	__bs_nonsingle_s(rr,1,(t),(h),(o),(a),(c))
#define	bus_space_read_region_stream_2(t, h, o, a, c)			\
	__bs_nonsingle_s(rr,2,(t),(h),(o),(a),(c))
#define	bus_space_read_region_stream_4(t, h, o, a, c)			\
	__bs_nonsingle_s(rr,4,(t),(h),(o),(a),(c))
#define	bus_space_read_region_stream_8(t, h, o, a, c)			\
	__bs_nonsingle_s(rr,8,(t),(h),(o),(a),(c))
#endif


/*
 * Bus write (single) operations.
 */
#define	bus_space_write_1(t, h, o, v)	__bs_ws(1,(t),(h),(o),(v))
#define	bus_space_write_2(t, h, o, v)	__bs_ws(2,(t),(h),(o),(v))
#define	bus_space_write_4(t, h, o, v)	__bs_ws(4,(t),(h),(o),(v))
#define	bus_space_write_8(t, h, o, v)	__bs_ws(8,(t),(h),(o),(v))
#ifdef __BUS_SPACE_HAS_STREAM_METHODS
#define	bus_space_write_stream_1(t, h, o, v)	__bs_ws_s(1,(t),(h),(o),(v))
#define	bus_space_write_stream_2(t, h, o, v)	__bs_ws_s(2,(t),(h),(o),(v))
#define	bus_space_write_stream_4(t, h, o, v)	__bs_ws_s(4,(t),(h),(o),(v))
#define	bus_space_write_stream_8(t, h, o, v)	__bs_ws_s(8,(t),(h),(o),(v))
#endif


/*
 * Bus write multiple operations.
 */
#define	bus_space_write_multi_1(t, h, o, a, c)				\
	__bs_nonsingle(wm,1,(t),(h),(o),(a),(c))
#define	bus_space_write_multi_2(t, h, o, a, c)				\
	__bs_nonsingle(wm,2,(t),(h),(o),(a),(c))
#define	bus_space_write_multi_4(t, h, o, a, c)				\
	__bs_nonsingle(wm,4,(t),(h),(o),(a),(c))
#define	bus_space_write_multi_8(t, h, o, a, c)				\
	__bs_nonsingle(wm,8,(t),(h),(o),(a),(c))
#ifdef __BUS_SPACE_HAS_STREAM_METHODS
#define	bus_space_write_multi_stream_1(t, h, o, a, c)			\
	__bs_nonsingle_s(wm,1,(t),(h),(o),(a),(c))
#define	bus_space_write_multi_stream_2(t, h, o, a, c)			\
	__bs_nonsingle_s(wm,2,(t),(h),(o),(a),(c))
#define	bus_space_write_multi_stream_4(t, h, o, a, c)			\
	__bs_nonsingle_s(wm,4,(t),(h),(o),(a),(c))
#define	bus_space_write_multi_stream_8(t, h, o, a, c)			\
	__bs_nonsingle_s(wm,8,(t),(h),(o),(a),(c))
#endif


/*
 * Bus write region operations.
 */
#define	bus_space_write_region_1(t, h, o, a, c)				\
	__bs_nonsingle(wr,1,(t),(h),(o),(a),(c))
#define	bus_space_write_region_2(t, h, o, a, c)				\
	__bs_nonsingle(wr,2,(t),(h),(o),(a),(c))
#define	bus_space_write_region_4(t, h, o, a, c)				\
	__bs_nonsingle(wr,4,(t),(h),(o),(a),(c))
#define	bus_space_write_region_8(t, h, o, a, c)				\
	__bs_nonsingle(wr,8,(t),(h),(o),(a),(c))
#ifdef __BUS_SPACE_HAS_STREAM_METHODS
#define	bus_space_write_region_stream_1(t, h, o, a, c)			\
	__bs_nonsingle_s(wr,1,(t),(h),(o),(a),(c))
#define	bus_space_write_region_stream_2(t, h, o, a, c)			\
	__bs_nonsingle_s(wr,2,(t),(h),(o),(a),(c))
#define	bus_space_write_region_stream_4(t, h, o, a, c)			\
	__bs_nonsingle_s(wr,4,(t),(h),(o),(a),(c))
#define	bus_space_write_region_stream_8(t, h, o, a, c)			\
	__bs_nonsingle_s(wr,8,(t),(h),(o),(a),(c))
#endif


/*
 * Set multiple operations.
 */
#define	bus_space_set_multi_1(t, h, o, v, c)				\
	__bs_set(sm,1,(t),(h),(o),(v),(c))
#define	bus_space_set_multi_2(t, h, o, v, c)				\
	__bs_set(sm,2,(t),(h),(o),(v),(c))
#define	bus_space_set_multi_4(t, h, o, v, c)				\
	__bs_set(sm,4,(t),(h),(o),(v),(c))
#define	bus_space_set_multi_8(t, h, o, v, c)				\
	__bs_set(sm,8,(t),(h),(o),(v),(c))

/*
 * Set region operations.
 */
#define	bus_space_set_region_1(t, h, o, v, c)				\
	__bs_set(sr,1,(t),(h),(o),(v),(c))
#define	bus_space_set_region_2(t, h, o, v, c)				\
	__bs_set(sr,2,(t),(h),(o),(v),(c))
#define	bus_space_set_region_4(t, h, o, v, c)				\
	__bs_set(sr,4,(t),(h),(o),(v),(c))
#define	bus_space_set_region_8(t, h, o, v, c)				\
	__bs_set(sr,8,(t),(h),(o),(v),(c))

/*
 * Copy operations.
 */
#define	bus_space_copy_region_1(t, h1, o1, h2, o2, c)				\
	__bs_copy(1, t, h1, o1, h2, o2, c)
#define	bus_space_copy_region_2(t, h1, o1, h2, o2, c)				\
	__bs_copy(2, t, h1, o1, h2, o2, c)
#define	bus_space_copy_region_4(t, h1, o1, h2, o2, c)				\
	__bs_copy(4, t, h1, o1, h2, o2, c)
#define	bus_space_copy_region_8(t, h1, o1, h2, o2, c)				\
	__bs_copy(8, t, h1, o1, h2, o2, c)

/*
 * Macros to provide prototypes for all the functions used in the
 * bus_space structure
 */

#define bs_map_proto(f)							\
int	__bs_c(f,_bs_map)(void *t, bus_addr_t addr,		\
	    bus_size_t size, int cacheable, bus_space_handle_t *bshp);

#define bs_unmap_proto(f)						\
void	__bs_c(f,_bs_unmap)(void *t, bus_space_handle_t bsh,	\
	    bus_size_t size);

#define bs_subregion_proto(f)						\
int	__bs_c(f,_bs_subregion)(void *t, bus_space_handle_t bsh,	\
	    bus_size_t offset, bus_size_t size, 			\
	    bus_space_handle_t *nbshp);

#define bs_alloc_proto(f)						\
int	__bs_c(f,_bs_alloc)(void *t, bus_addr_t rstart,		\
	    bus_addr_t rend, bus_size_t size, bus_size_t align,		\
	    bus_size_t boundary, int cacheable, bus_addr_t *addrp,	\
	    bus_space_handle_t *bshp);

#define bs_free_proto(f)						\
void	__bs_c(f,_bs_free)(void *t, bus_space_handle_t bsh,	\
	    bus_size_t size);

#define bs_vaddr_proto(f)						\
void *	__bs_c(f,_bs_vaddr)(void *t, bus_space_handle_t bsh);

#define bs_mmap_proto(f)						\
paddr_t	__bs_c(f,_bs_mmap)(void *, bus_addr_t, off_t, int, int);

#define bs_barrier_proto(f)						\
void	__bs_c(f,_bs_barrier)(void *t, bus_space_handle_t bsh,	\
	    bus_size_t offset, bus_size_t len, int flags);

#define	bs_r_1_proto(f)							\
uint8_t	__bs_c(f,_bs_r_1)(void *t, bus_space_handle_t bsh,	\
		    bus_size_t offset);

#define	bs_r_2_proto(f)							\
uint16_t	__bs_c(f,_bs_r_2)(void *t, bus_space_handle_t bsh,	\
		    bus_size_t offset);					\
uint16_t	__bs_c(f,_bs_r_2_swap)(void *t, bus_space_handle_t bsh,	\
		    bus_size_t offset);

#define	bs_r_4_proto(f)							\
uint32_t	__bs_c(f,_bs_r_4)(void *t, bus_space_handle_t bsh,	\
		    bus_size_t offset);					\
uint32_t	__bs_c(f,_bs_r_4_swap)(void *t, bus_space_handle_t bsh,	\
		    bus_size_t offset);

#define	bs_r_8_proto(f)							\
uint64_t	__bs_c(f,_bs_r_8)(void *t, bus_space_handle_t bsh,	\
		    bus_size_t offset);					\
uint64_t	__bs_c(f,_bs_r_8_swap)(void *t, bus_space_handle_t bsh,	\
		    bus_size_t offset);

#define	bs_w_1_proto(f)							\
void	__bs_c(f,_bs_w_1)(void *t, bus_space_handle_t bsh,		\
	    bus_size_t offset, uint8_t value);

#define	bs_w_2_proto(f)							\
void	__bs_c(f,_bs_w_2)(void *t, bus_space_handle_t bsh,		\
	    bus_size_t offset, uint16_t value);				\
void	__bs_c(f,_bs_w_2_swap)(void *t, bus_space_handle_t bsh,		\
	    bus_size_t offset, uint16_t value);

#define	bs_w_4_proto(f)							\
void	__bs_c(f,_bs_w_4)(void *t, bus_space_handle_t bsh,		\
	    bus_size_t offset, uint32_t value);				\
void	__bs_c(f,_bs_w_4_swap)(void *t, bus_space_handle_t bsh,		\
	    bus_size_t offset, uint32_t value);

#define	bs_w_8_proto(f)							\
void	__bs_c(f,_bs_w_8)(void *t, bus_space_handle_t bsh,		\
	    bus_size_t offset, uint64_t value);				\
void	__bs_c(f,_bs_w_8_swap)(void *t, bus_space_handle_t bsh,		\
	    bus_size_t offset, uint64_t value);

#define	bs_rm_1_proto(f)						\
void	__bs_c(f,_bs_rm_1)(void *t, bus_space_handle_t bsh,		\
	    bus_size_t offset, uint8_t *addr, bus_size_t count);

#define	bs_rm_2_proto(f)						\
void	__bs_c(f,_bs_rm_2)(void *t, bus_space_handle_t bsh,		\
	    bus_size_t offset, uint16_t *addr, bus_size_t count);	\
void	__bs_c(f,_bs_rm_2_swap)(void *t, bus_space_handle_t bsh,	\
	    bus_size_t offset, uint16_t *addr, bus_size_t count);

#define	bs_rm_4_proto(f)						\
void	__bs_c(f,_bs_rm_4)(void *t, bus_space_handle_t bsh,		\
	    bus_size_t offset, uint32_t *addr, bus_size_t count);	\
void	__bs_c(f,_bs_rm_4_swap)(void *t, bus_space_handle_t bsh,	\
	    bus_size_t offset, uint32_t *addr, bus_size_t count);

#define	bs_rm_8_proto(f)						\
void	__bs_c(f,_bs_rm_8)(void *t, bus_space_handle_t bsh,		\
	    bus_size_t offset, uint64_t *addr, bus_size_t count);	\
void	__bs_c(f,_bs_rm_8_swap)(void *t, bus_space_handle_t bsh,	\
	    bus_size_t offset, uint64_t *addr, bus_size_t count);

#define	bs_wm_1_proto(f)						\
void	__bs_c(f,_bs_wm_1)(void *t, bus_space_handle_t bsh,		\
	    bus_size_t offset, const uint8_t *addr, bus_size_t count);	\

#define	bs_wm_2_proto(f)						\
void	__bs_c(f,_bs_wm_2)(void *t, bus_space_handle_t bsh,		\
	    bus_size_t offset, const uint16_t *addr, bus_size_t count);	\
void	__bs_c(f,_bs_wm_2_swap)(void *t, bus_space_handle_t bsh,	\
	    bus_size_t offset, const uint16_t *addr, bus_size_t count);

#define	bs_wm_4_proto(f)						\
void	__bs_c(f,_bs_wm_4)(void *t, bus_space_handle_t bsh,		\
	    bus_size_t offset, const uint32_t *addr, bus_size_t count);	\
void	__bs_c(f,_bs_wm_4_swap)(void *t, bus_space_handle_t bsh,	\
	    bus_size_t offset, const uint32_t *addr, bus_size_t count);

#define	bs_wm_8_proto(f)						\
void	__bs_c(f,_bs_wm_8)(void *t, bus_space_handle_t bsh,		\
	    bus_size_t offset, const uint64_t *addr, bus_size_t count);	\
void	__bs_c(f,_bs_wm_8_swap)(void *t, bus_space_handle_t bsh,	\
	    bus_size_t offset, const uint64_t *addr, bus_size_t count);

#define	bs_rr_1_proto(f)						\
void	__bs_c(f, _bs_rr_1)(void *t, bus_space_handle_t bsh,		\
	    bus_size_t offset, uint8_t *addr, bus_size_t count);

#define	bs_rr_2_proto(f)						\
void	__bs_c(f, _bs_rr_2)(void *t, bus_space_handle_t bsh,		\
	    bus_size_t offset, uint16_t *addr, bus_size_t count);	\
void	__bs_c(f, _bs_rr_2_swap)(void *t, bus_space_handle_t bsh,	\
	    bus_size_t offset, uint16_t *addr, bus_size_t count);

#define	bs_rr_4_proto(f)						\
void	__bs_c(f, _bs_rr_4)(void *t, bus_space_handle_t bsh,		\
	    bus_size_t offset, uint32_t *addr, bus_size_t count);	\
void	__bs_c(f, _bs_rr_4_swap)(void *t, bus_space_handle_t bsh,	\
	    bus_size_t offset, uint32_t *addr, bus_size_t count);

#define	bs_rr_8_proto(f)						\
void	__bs_c(f, _bs_rr_8)(void *t, bus_space_handle_t bsh,		\
	    bus_size_t offset, uint64_t *addr, bus_size_t count);	\
void	__bs_c(f, _bs_rr_8_swap)(void *t, bus_space_handle_t bsh,	\
	    bus_size_t offset, uint64_t *addr, bus_size_t count);

#define	bs_wr_1_proto(f)						\
void	__bs_c(f, _bs_wr_1)(void *t, bus_space_handle_t bsh,		\
	    bus_size_t offset, const uint8_t *addr, bus_size_t count);

#define	bs_wr_2_proto(f)						\
void	__bs_c(f, _bs_wr_2)(void *t, bus_space_handle_t bsh,		\
	    bus_size_t offset, const uint16_t *addr, bus_size_t count);	\
void	__bs_c(f, _bs_wr_2_swap)(void *t, bus_space_handle_t bsh,	\
	    bus_size_t offset, const uint16_t *addr, bus_size_t count);

#define	bs_wr_4_proto(f)						\
void	__bs_c(f, _bs_wr_4)(void *t, bus_space_handle_t bsh,		\
	    bus_size_t offset, const uint32_t *addr, bus_size_t count);	\
void	__bs_c(f, _bs_wr_4_swap)(void *t, bus_space_handle_t bsh,	\
	    bus_size_t offset, const uint32_t *addr, bus_size_t count);

#define	bs_wr_8_proto(f)						\
void	__bs_c(f, _bs_wr_8)(void *t, bus_space_handle_t bsh,		\
	    bus_size_t offset, const uint64_t *addr, bus_size_t count);	\
void	__bs_c(f, _bs_wr_8_swap)(void *t, bus_space_handle_t bsh,	\
	    bus_size_t offset, const uint64_t *addr, bus_size_t count);

#define	bs_sm_1_proto(f)						\
void	__bs_c(f,_bs_sm_1)(void *t, bus_space_handle_t bsh,		\
	    bus_size_t offset, uint8_t value, bus_size_t count);

#define	bs_sm_2_proto(f)						\
void	__bs_c(f,_bs_sm_2)(void *t, bus_space_handle_t bsh,		\
	    bus_size_t offset, uint16_t value, bus_size_t count);

#define	bs_sm_4_proto(f)						\
void	__bs_c(f,_bs_sm_4)(void *t, bus_space_handle_t bsh,		\
	    bus_size_t offset, uint32_t value, bus_size_t count);

#define	bs_sm_8_proto(f)						\
void	__bs_c(f,_bs_sm_8)(void *t, bus_space_handle_t bsh,		\
	    bus_size_t offset, uint64_t value, bus_size_t count);

#define	bs_sr_1_proto(f)						\
void	__bs_c(f,_bs_sr_1)(void *t, bus_space_handle_t bsh,		\
	    bus_size_t offset, uint8_t value, bus_size_t count);

#define	bs_sr_2_proto(f)						\
void	__bs_c(f,_bs_sr_2)(void *t, bus_space_handle_t bsh,		\
	    bus_size_t offset, uint16_t value, bus_size_t count);	\
void	__bs_c(f,_bs_sr_2_swap)(void *t, bus_space_handle_t bsh,	\
	    bus_size_t offset, uint16_t value, bus_size_t count);

#define	bs_sr_4_proto(f)						\
void	__bs_c(f,_bs_sr_4)(void *t, bus_space_handle_t bsh,		\
	    bus_size_t offset, uint32_t value, bus_size_t count);	\
void	__bs_c(f,_bs_sr_4_swap)(void *t, bus_space_handle_t bsh,	\
	    bus_size_t offset, uint32_t value, bus_size_t count);

#define	bs_sr_8_proto(f)						\
void	__bs_c(f,_bs_sr_8)(void *t, bus_space_handle_t bsh,		\
	    bus_size_t offset, uint64_t value, bus_size_t count);	\
void	__bs_c(f,_bs_sr_8_swap)(void *t, bus_space_handle_t bsh,	\
	    bus_size_t offset, uint64_t value, bus_size_t count);

#define	bs_c_1_proto(f)							\
void	__bs_c(f,_bs_c_1)(void *t, bus_space_handle_t bsh1,		\
	    bus_size_t offset1, bus_space_handle_t bsh2,		\
	    bus_size_t offset2, bus_size_t count);

#define	bs_c_2_proto(f)							\
void	__bs_c(f,_bs_c_2)(void *t, bus_space_handle_t bsh1,		\
	    bus_size_t offset1, bus_space_handle_t bsh2,		\
	    bus_size_t offset2, bus_size_t count);

#define	bs_c_4_proto(f)							\
void	__bs_c(f,_bs_c_4)(void *t, bus_space_handle_t bsh1,		\
	    bus_size_t offset1, bus_space_handle_t bsh2,		\
	    bus_size_t offset2, bus_size_t count);

#define	bs_c_8_proto(f)							\
void	__bs_c(f,_bs_c_8)(void *t, bus_space_handle_t bsh1,		\
	    bus_size_t offset1, bus_space_handle_t bsh2,		\
	    bus_size_t offset2, bus_size_t count);

#define bs_protos(f)		\
bs_map_proto(f);		\
bs_unmap_proto(f);		\
bs_subregion_proto(f);		\
bs_alloc_proto(f);		\
bs_free_proto(f);		\
bs_vaddr_proto(f);		\
bs_mmap_proto(f);		\
bs_barrier_proto(f);		\
bs_r_1_proto(f);		\
bs_r_2_proto(f);		\
bs_r_4_proto(f);		\
bs_r_8_proto(f);		\
bs_w_1_proto(f);		\
bs_w_2_proto(f);		\
bs_w_4_proto(f);		\
bs_w_8_proto(f);		\
bs_rm_1_proto(f);		\
bs_rm_2_proto(f);		\
bs_rm_4_proto(f);		\
bs_rm_8_proto(f);		\
bs_wm_1_proto(f);		\
bs_wm_2_proto(f);		\
bs_wm_4_proto(f);		\
bs_wm_8_proto(f);		\
bs_rr_1_proto(f);		\
bs_rr_2_proto(f);		\
bs_rr_4_proto(f);		\
bs_rr_8_proto(f);		\
bs_wr_1_proto(f);		\
bs_wr_2_proto(f);		\
bs_wr_4_proto(f);		\
bs_wr_8_proto(f);		\
bs_sm_1_proto(f);		\
bs_sm_2_proto(f);		\
bs_sm_4_proto(f);		\
bs_sm_8_proto(f);		\
bs_sr_1_proto(f);		\
bs_sr_2_proto(f);		\
bs_sr_4_proto(f);		\
bs_sr_8_proto(f);		\
bs_c_1_proto(f);		\
bs_c_2_proto(f);		\
bs_c_4_proto(f);		\
bs_c_8_proto(f);

/* Bus Space DMA macros */

/* Forwards needed by prototypes below. */
struct mbuf;
struct uio;

#define	bus_dmamap_create(t, s, n, m, b, f, p)			\
	(*(t)->_dmamap_create)((t), (s), (n), (m), (b), (f), (p))
#define	bus_dmamap_destroy(t, p)				\
	(*(t)->_dmamap_destroy)((t), (p))
#define	bus_dmamap_load(t, m, b, s, p, f)			\
	(*(t)->_dmamap_load)((t), (m), (b), (s), (p), (f))
#define	bus_dmamap_load_mbuf(t, m, b, f)			\
	(*(t)->_dmamap_load_mbuf)((t), (m), (b), (f))
#define	bus_dmamap_load_uio(t, m, u, f)				\
	(*(t)->_dmamap_load_uio)((t), (m), (u), (f))
#define	bus_dmamap_load_raw(t, m, sg, n, s, f)			\
	(*(t)->_dmamap_load_raw)((t), (m), (sg), (n), (s), (f))
#define	bus_dmamap_unload(t, p)					\
	(*(t)->_dmamap_unload)((t), (p))
#define	bus_dmamap_sync(t, p, o, l, ops)			\
do {									\
	if (((p)->_dm_flags & (_BUS_DMAMAP_COHERENT|_BUS_DMAMAP_IS_BOUNCING)) == _BUS_DMAMAP_COHERENT) \
		break;							\
	if (((ops) & (BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE)) != 0	\
	    && (t)->_dmamap_sync_pre != NULL)				\
		(*(t)->_dmamap_sync_pre)((t), (p), (o), (l), (ops));	\
	else if (((ops) & (BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE)) != 0 \
		 && (t)->_dmamap_sync_post != NULL)			     \
		(*(t)->_dmamap_sync_post)((t), (p), (o), (l), (ops));	     \
} while (/*CONSTCOND*/0)

#define	bus_dmamem_alloc(t, s, a, b, sg, n, r, f)		\
	(*(t)->_dmamem_alloc)((t), (s), (a), (b), (sg), (n), (r), (f))
#define	bus_dmamem_free(t, sg, n)				\
	(*(t)->_dmamem_free)((t), (sg), (n))
#define	bus_dmamem_map(t, sg, n, s, k, f)			\
	(*(t)->_dmamem_map)((t), (sg), (n), (s), (k), (f))
#define	bus_dmamem_unmap(t, k, s)				\
	(*(t)->_dmamem_unmap)((t), (k), (s))
#define	bus_dmamem_mmap(t, sg, n, o, p, f)			\
	(*(t)->_dmamem_mmap)((t), (sg), (n), (o), (p), (f))

#define	bus_dmatag_subregion(t, mna, mxa, nt, f)		\
	(*(t)->_dmatag_subregion)((t), (mna), (mxa), (nt), (f))
#define	bus_dmatag_destroy(t)					\
	(*(t)->_dmatag_destroy)(t)

#ifdef _ARM32_BUS_DMA_PRIVATE

extern paddr_t physical_start, physical_end;

int	arm32_dma_range_intersect(struct arm32_dma_range *, int,
	    paddr_t pa, psize_t size, paddr_t *pap, psize_t *sizep);

int	_bus_dmamap_create(bus_dma_tag_t, bus_size_t, int, bus_size_t,
	    bus_size_t, int, bus_dmamap_t *);
void	_bus_dmamap_destroy(bus_dma_tag_t, bus_dmamap_t);
int	_bus_dmamap_load(bus_dma_tag_t, bus_dmamap_t, void *,
	    bus_size_t, struct proc *, int);
int	_bus_dmamap_load_mbuf(bus_dma_tag_t, bus_dmamap_t,
	    struct mbuf *, int);
int	_bus_dmamap_load_uio(bus_dma_tag_t, bus_dmamap_t,
	    struct uio *, int);
int	_bus_dmamap_load_raw(bus_dma_tag_t, bus_dmamap_t,
	    bus_dma_segment_t *, int, bus_size_t, int);
void	_bus_dmamap_unload(bus_dma_tag_t, bus_dmamap_t);
void	_bus_dmamap_sync(bus_dma_tag_t, bus_dmamap_t, bus_addr_t,
	    bus_size_t, int);

#if defined(_ARM32_NEED_BUS_DMA_BOUNCE) || defined(CPU_CORTEX)
#define	_BUS_DMAMAP_SYNC_FUNCS \
	._dmamap_sync_pre = _bus_dmamap_sync,	\
	._dmamap_sync_post = _bus_dmamap_sync
#else
#define	_BUS_DMAMAP_SYNC_FUNCS \
	._dmamap_sync_pre = _bus_dmamap_sync
#endif

#define	_BUS_DMAMAP_FUNCS \
	._dmamap_create = _bus_dmamap_create,		\
	._dmamap_destroy = _bus_dmamap_destroy,		\
	._dmamap_load = _bus_dmamap_load,		\
	._dmamap_load_mbuf = _bus_dmamap_load_mbuf,	\
	._dmamap_load_raw = _bus_dmamap_load_raw,	\
	._dmamap_load_uio = _bus_dmamap_load_uio,	\
	._dmamap_unload = _bus_dmamap_unload,		\
	_BUS_DMAMAP_SYNC_FUNCS

int	_bus_dmamem_alloc(bus_dma_tag_t tag, bus_size_t size,
	    bus_size_t alignment, bus_size_t boundary,
	    bus_dma_segment_t *segs, int nsegs, int *rsegs, int flags);
void	_bus_dmamem_free(bus_dma_tag_t tag, bus_dma_segment_t *segs,
	    int nsegs);
int	_bus_dmamem_map(bus_dma_tag_t tag, bus_dma_segment_t *segs,
	    int nsegs, size_t size, void **kvap, int flags);
void	_bus_dmamem_unmap(bus_dma_tag_t tag, void *kva,
	    size_t size);
paddr_t	_bus_dmamem_mmap(bus_dma_tag_t tag, bus_dma_segment_t *segs,
	    int nsegs, off_t off, int prot, int flags);

#define	_BUS_DMAMEM_FUNCS \
	._dmamem_alloc = _bus_dmamem_alloc,	\
	._dmamem_free = _bus_dmamem_free,	\
	._dmamem_map = _bus_dmamem_map,		\
	._dmamem_unmap = _bus_dmamem_unmap,	\
	._dmamem_mmap = _bus_dmamem_mmap

int	_bus_dmamem_alloc_range(bus_dma_tag_t tag, bus_size_t size,
	    bus_size_t alignment, bus_size_t boundary,
	    bus_dma_segment_t *segs, int nsegs, int *rsegs, int flags,
	    vaddr_t low, vaddr_t high);

int	_bus_dmatag_subregion(bus_dma_tag_t, bus_addr_t, bus_addr_t,
	    bus_dma_tag_t *, int);
void 	_bus_dmatag_destroy(bus_dma_tag_t);

#define	_BUS_DMATAG_FUNCS \
	._dmatag_subregion = _bus_dmatag_subregion,	\
	._dmatag_destroy = _bus_dmatag_destroy

#endif /* _ARM32_BUS_DMA_PRIVATE */

#endif /* _ARM_BUS_FUNCS_H_ */
