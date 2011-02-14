#	$NetBSD: Makefile.inc,v 1.2 2010/02/12 22:34:38 dyoung Exp $
#	from: @(#)Makefile.inc	8.6 (Berkeley) 5/4/95

# gen sources
.PATH:	${.CURDIR}/atomic

MAN+=	atomic_add.3 atomic_and.3 atomic_cas.3 atomic_dec.3 \
	atomic_inc.3 atomic_ops.3 atomic_or.3 atomic_swap.3 \
	membar_ops.3

MLINKS+=atomic_add.3 atomic_add_32.3 \
	atomic_add.3 atomic_add_int.3 \
	atomic_add.3 atomic_add_long.3 \
	atomic_add.3 atomic_add_ptr.3 \
	atomic_add.3 atomic_add_64.3 \
	atomic_add.3 atomic_add_32_nv.3 \
	atomic_add.3 atomic_add_int_nv.3 \
	atomic_add.3 atomic_add_long_nv.3 \
	atomic_add.3 atomic_add_ptr_nv.3 \
	atomic_add.3 atomic_add_64_nv.3 \
	atomic_and.3 atomic_and_32.3 \
	atomic_and.3 atomic_and_uint.3 \
	atomic_and.3 atomic_and_ulong.3 \
	atomic_and.3 atomic_and_64.3 \
	atomic_and.3 atomic_and_32_nv.3 \
	atomic_and.3 atomic_and_uint_nv.3 \
	atomic_and.3 atomic_and_ulong_nv.3 \
	atomic_and.3 atomic_and_64_nv.3 \
	atomic_cas.3 atomic_cas_32.3 \
	atomic_cas.3 atomic_cas_uint.3 \
	atomic_cas.3 atomic_cas_ulong.3 \
	atomic_cas.3 atomic_cas_ptr.3 \
	atomic_cas.3 atomic_cas_64.3 \
	atomic_cas.3 atomic_cas_32_ni.3 \
	atomic_cas.3 atomic_cas_uint_ni.3 \
	atomic_cas.3 atomic_cas_ulong_ni.3 \
	atomic_cas.3 atomic_cas_ptr_ni.3 \
	atomic_cas.3 atomic_cas_64_ni.3 \
	atomic_dec.3 atomic_dec_32.3 \
	atomic_dec.3 atomic_dec_uint.3 \
	atomic_dec.3 atomic_dec_ulong.3 \
	atomic_dec.3 atomic_dec_ptr.3 \
	atomic_dec.3 atomic_dec_64.3 \
	atomic_dec.3 atomic_dec_32_nv.3 \
	atomic_dec.3 atomic_dec_uint_nv.3 \
	atomic_dec.3 atomic_dec_ulong_nv.3 \
	atomic_dec.3 atomic_dec_ptr_nv.3 \
	atomic_dec.3 atomic_dec_64_nv.3 \
	atomic_inc.3 atomic_inc_32.3 \
	atomic_inc.3 atomic_inc_uint.3 \
	atomic_inc.3 atomic_inc_ulong.3 \
	atomic_inc.3 atomic_inc_ptr.3 \
	atomic_inc.3 atomic_inc_64.3 \
	atomic_inc.3 atomic_inc_32_nv.3 \
	atomic_inc.3 atomic_inc_uint_nv.3 \
	atomic_inc.3 atomic_inc_ulong_nv.3 \
	atomic_inc.3 atomic_inc_ptr_nv.3 \
	atomic_inc.3 atomic_inc_64_nv.3 \
	atomic_or.3 atomic_or_32.3 \
	atomic_or.3 atomic_or_uint.3 \
	atomic_or.3 atomic_or_ulong.3 \
	atomic_or.3 atomic_or_64.3 \
	atomic_or.3 atomic_or_32_nv.3 \
	atomic_or.3 atomic_or_uint_nv.3 \
	atomic_or.3 atomic_or_ulong_nv.3 \
	atomic_or.3 atomic_or_64_nv.3 \
	atomic_swap.3 atomic_swap_32.3 \
	atomic_swap.3 atomic_swap_uint.3 \
	atomic_swap.3 atomic_swap_ulong.3 \
	atomic_swap.3 atomic_swap_ptr.3 \
	atomic_swap.3 atomic_swap_64.3 \
	membar_ops.3 membar_enter.3 \
	membar_ops.3 membar_exit.3 \
	membar_ops.3 membar_producer.3 \
	membar_ops.3 membar_consumer.3 \
	membar_ops.3 membar_sync.3
