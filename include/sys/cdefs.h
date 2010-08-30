/*	$NetBSD: cdefs.h,v 1.78 2009/10/02 21:05:28 christos Exp $	*/

/*
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Berkeley Software Design, Inc.
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
 *	@(#)cdefs.h	8.8 (Berkeley) 1/9/95
 */

#ifndef	_SYS_CDEFS_H_
#define	_SYS_CDEFS_H_

/*
 * Macro to test if we're using a GNU C compiler of a specific vintage
 * or later, for e.g. features that appeared in a particular version
 * of GNU C.  Usage:
 *
 *	#if __GNUC_PREREQ__(major, minor)
 *	...cool feature...
 *	#else
 *	...delete feature...
 *	#endif
 */
#ifdef __GNUC__
#define	__GNUC_PREREQ__(x, y)						\
	((__GNUC__ == (x) && __GNUC_MINOR__ >= (y)) ||			\
	 (__GNUC__ > (x)))
#else
#define	__GNUC_PREREQ__(x, y)	0
#endif

#if 0
#include <machine/cdefs.h>
#ifdef __ELF__
#include <sys/cdefs_elf.h>
#else
#include <sys/cdefs_aout.h>
#endif
#endif

#if defined(__cplusplus)
#define	__BEGIN_DECLS		extern "C" {
#define	__END_DECLS		}
#define	__static_cast(x,y)	static_cast<x>(y)
#else
#define	__BEGIN_DECLS
#define	__END_DECLS
#define	__static_cast(x,y)	(x)y
#endif

/*
 * The __CONCAT macro is used to concatenate parts of symbol names, e.g.
 * with "#define OLD(foo) __CONCAT(old,foo)", OLD(foo) produces oldfoo.
 * The __CONCAT macro is a bit tricky to use if it must work in non-ANSI
 * mode -- there must be no spaces between its arguments, and for nested
 * __CONCAT's, all the __CONCAT's must be at the left.  __CONCAT can also
 * concatenate double-quoted strings produced by the __STRING macro, but
 * this only works with ANSI C.
 */

#define	___STRING(x)	__STRING(x)
#define	___CONCAT(x,y)	__CONCAT(x,y)

#if __STDC__ || defined(__cplusplus)
#define	__P(protos)	protos		/* full-blown ANSI C */
#define	__CONCAT1(x,y)	x ## y
#define	__CONCAT(x,y)	__CONCAT1(x,y)
#define	__STRING(x)	#x

#define	__const		const		/* define reserved names to standard */
#define	__signed	signed
#define	__volatile	volatile
#if defined(__cplusplus) || defined(__PCC__)
#define	__inline	inline		/* convert to C++/C99 keyword */
#else
#if !defined(__GNUC__) && !defined(__lint__)
#define	__inline			/* delete GCC keyword */
#endif /* !__GNUC__  && !__lint__ */
#endif /* !__cplusplus */

#else	/* !(__STDC__ || __cplusplus) */
#define	__P(protos)	()		/* traditional C preprocessor */
#define	__CONCAT(x,y)	x/**/y
#define	__STRING(x)	"x"

#ifndef __GNUC__
#define	__const				/* delete pseudo-ANSI C keywords */
#define	__inline
#define	__signed
#define	__volatile
#endif	/* !__GNUC__ */

/*
 * In non-ANSI C environments, new programs will want ANSI-only C keywords
 * deleted from the program and old programs will want them left alone.
 * Programs using the ANSI C keywords const, inline etc. as normal
 * identifiers should define -DNO_ANSI_KEYWORDS.
 */
#ifndef	NO_ANSI_KEYWORDS
#define	const		__const		/* convert ANSI C keywords */
#define	inline		__inline
#define	signed		__signed
#define	volatile	__volatile
#endif /* !NO_ANSI_KEYWORDS */
#endif	/* !(__STDC__ || __cplusplus) */

/*
 * Used for internal auditing of the NetBSD source tree.
 */
#ifdef __AUDIT__
#define	__aconst	__const
#else
#define	__aconst
#endif

/*
 * The following macro is used to remove const cast-away warnings
 * from gcc -Wcast-qual; it should be used with caution because it
 * can hide valid errors; in particular most valid uses are in
 * situations where the API requires it, not to cast away string
 * constants. We don't use *intptr_t on purpose here and we are
 * explicit about unsigned long so that we don't have additional
 * dependencies.
 */
#define __UNCONST(a)	((void *)(unsigned long)(const void *)(a))

/*
 * The following macro is used to remove the volatile cast-away warnings
 * from gcc -Wcast-qual; as above it should be used with caution
 * because it can hide valid errors or warnings.  Valid uses include
 * making it possible to pass a volatile pointer to memset().
 * For the same reasons as above, we use unsigned long and not intptr_t.
 */
#define __UNVOLATILE(a)	((void *)(unsigned long)(volatile void *)(a))

/*
 * GCC2 provides __extension__ to suppress warnings for various GNU C
 * language extensions under "-ansi -pedantic".
 */
#if !__GNUC_PREREQ__(2, 0)
#define	__extension__		/* delete __extension__ if non-gcc or gcc1 */
#endif

/*
 * GCC1 and some versions of GCC2 declare dead (non-returning) and
 * pure (no side effects) functions using "volatile" and "const";
 * unfortunately, these then cause warnings under "-ansi -pedantic".
 * GCC2 uses a new, peculiar __attribute__((attrs)) style.  All of
 * these work for GNU C++ (modulo a slight glitch in the C++ grammar
 * in the distribution version of 2.5.5).
 *
 * GCC defines a pure function as depending only on its arguments and
 * global variables.  Typical examples are strlen and sqrt.
 *
 * GCC defines a const function as depending only on its arguments.
 * Therefore calling a const function again with identical arguments
 * will always produce the same result.
 *
 * Rounding modes for floating point operations are considered global
 * variables and prevent sqrt from being a const function.
 *
 * Calls to const functions can be optimised away and moved around
 * without limitations.
 */
#if !__GNUC_PREREQ__(2, 0)
#define __attribute__(x)
#endif

#if __GNUC_PREREQ__(2, 5)
#define	__dead		__attribute__((__noreturn__))
#elif defined(__GNUC__)
#define	__dead		__volatile
#else
#define	__dead
#endif

#if __GNUC_PREREQ__(2, 96)
#define	__pure		__attribute__((__pure__))
#elif defined(__GNUC__)
#define	__pure		__const
#else
#define	__pure
#endif

#if __GNUC_PREREQ__(2, 5)
#define	__constfunc	__attribute__((__const__))
#else
#define	__constfunc
#endif

#if __GNUC_PREREQ__(3, 0)
#define	__noinline	__attribute__((__noinline__))
#else
#define	__noinline	/* nothing */
#endif

#if __GNUC_PREREQ__(2, 7)
#define	__unused	__attribute__((__unused__))
#else
#define	__unused	/* delete */
#endif

#if __GNUC_PREREQ__(3, 1)
#define	__used		__attribute__((__used__))
#else
#define	__used		__unused
#endif

#if defined(__lint__)
#define	__packed	__packed
#define	__aligned(x)	/* delete */
#define	__section(x)	/* delete */
#elif __GNUC_PREREQ__(2, 7)
#define	__packed	__attribute__((__packed__))
#define	__aligned(x)	__attribute__((__aligned__(x)))
#define	__section(x)	__attribute__((__section__(x)))
#elif defined(__PCC__)
#define	__packed	_Pragma("packed 1")
#define	__aligned(x)   	_Pragma("aligned " __STRING(x))
#define	__section(x)   	_Pragma("section " ## x)
#else
#define	__packed	error: no __packed for this compiler
#define	__aligned(x)	error: no __aligned for this compiler
#define	__section(x)	error: no __section for this compiler
#endif

/*
 * C99 defines the restrict type qualifier keyword, which was made available
 * in GCC 2.92.
 */
#if defined(__lint__)
#define	__restrict	/* delete __restrict when not supported */
#elif __STDC_VERSION__ >= 199901L
#define	__restrict	restrict
#elif !__GNUC_PREREQ__(2, 92)
#define	__restrict	/* delete __restrict when not supported */
#endif

/*
 * C99 defines __func__ predefined identifier, which was made available
 * in GCC 2.95.
 */
#if !(__STDC_VERSION__ >= 199901L)
#if __GNUC_PREREQ__(2, 6)
#define	__func__	__PRETTY_FUNCTION__
#elif __GNUC_PREREQ__(2, 4)
#define	__func__	__FUNCTION__
#else
#define	__func__	""
#endif
#endif /* !(__STDC_VERSION__ >= 199901L) */

#if defined(_KERNEL)
#if defined(NO_KERNEL_RCSIDS)
#undef __KERNEL_RCSID
#define	__KERNEL_RCSID(_n, _s)		/* nothing */
#endif /* NO_KERNEL_RCSIDS */
#endif /* _KERNEL */

#if 0
#if !defined(_STANDALONE) && !defined(_KERNEL)
#if defined(__GNUC__) || defined(__PCC__)
#define	__RENAME(x)	___RENAME(x)
#else
#ifdef __lint__
#define	__RENAME(x)	__symbolrename(x)
#else
#error "No function renaming possible"
#endif /* __lint__ */
#endif /* __GNUC__ */
#else /* _STANDALONE || _KERNEL */
#define	__RENAME(x)	no renaming in kernel or standalone environment
#endif
#endif
/*
 * A barrier to stop the optimizer from moving code or assume live
 * register values. This is gcc specific, the version is more or less
 * arbitrary, might work with older compilers.
 */
#if __GNUC_PREREQ__(2, 95)
#define	__insn_barrier()	__asm __volatile("":::"memory")
#else
#define	__insn_barrier()	/* */
#endif

/*
 * GNU C version 2.96 adds explicit branch prediction so that
 * the CPU back-end can hint the processor and also so that
 * code blocks can be reordered such that the predicted path
 * sees a more linear flow, thus improving cache behavior, etc.
 *
 * The following two macros provide us with a way to use this
 * compiler feature.  Use __predict_true() if you expect the expression
 * to evaluate to true, and __predict_false() if you expect the
 * expression to evaluate to false.
 *
 * A few notes about usage:
 *
 *	* Generally, __predict_false() error condition checks (unless
 *	  you have some _strong_ reason to do otherwise, in which case
 *	  document it), and/or __predict_true() `no-error' condition
 *	  checks, assuming you want to optimize for the no-error case.
 *
 *	* Other than that, if you don't know the likelihood of a test
 *	  succeeding from empirical or other `hard' evidence, don't
 *	  make predictions.
 *
 *	* These are meant to be used in places that are run `a lot'.
 *	  It is wasteful to make predictions in code that is run
 *	  seldomly (e.g. at subsystem initialization time) as the
 *	  basic block reordering that this affects can often generate
 *	  larger code.
 */
#if __GNUC_PREREQ__(2, 96)
#define	__predict_true(exp)	__builtin_expect((exp) != 0, 1)
#define	__predict_false(exp)	__builtin_expect((exp) != 0, 0)
#else
#define	__predict_true(exp)	(exp)
#define	__predict_false(exp)	(exp)
#endif

/*
 * We define this here since <stddef.h>, <sys/queue.h>, and <sys/types.h>
 * require it.
 */
#if __GNUC_PREREQ__(4, 1)
#define __offsetof(type, field)  __builtin_offsetof(type, field)
#else
#ifndef __cplusplus
#define __offsetof(type, field) ((size_t)(&((type *)0)->field))
#else
#define __offsetof(type, field)                                 \
  (__offsetof__ (reinterpret_cast <size_t>                      \
                 (&reinterpret_cast <const volatile char &>     \
                  (static_cast<type *> (0)->field))))
#endif
#endif
#define __rangeof(type, start, end) \
        (__offsetof(type, end) - __offsetof(type, start))

/*
 * Compiler-dependent macros to declare that functions take printf-like
 * or scanf-like arguments.  They are null except for versions of gcc
 * that are known to support the features properly (old versions of gcc-2
 * didn't permit keeping the keywords out of the application namespace).
 */
#if __GNUC_PREREQ__(2, 7)
#define __printflike(fmtarg, firstvararg)	\
	    __attribute__((__format__ (__printf__, fmtarg, firstvararg)))
#define __scanflike(fmtarg, firstvararg)	\
	    __attribute__((__format__ (__scanf__, fmtarg, firstvararg)))
#define __format_arg(fmtarg)    __attribute__((__format_arg__ (fmtarg)))
#else
#define __printflike(fmtarg, firstvararg)	/* nothing */
#define __scanflike(fmtarg, firstvararg)	/* nothing */
#define __format_arg(fmtarg)			/* nothing */
#endif

/*
 * Macros for manipulating "link sets".  Link sets are arrays of pointers
 * to objects, which are gathered up by the linker.
 *
 * Object format-specific code has provided us with the following macros:
 *
 *	__link_set_add_text(set, sym)
 *		Add a reference to the .text symbol `sym' to `set'.
 *
 *	__link_set_add_rodata(set, sym)
 *		Add a reference to the .rodata symbol `sym' to `set'.
 *
 *	__link_set_add_data(set, sym)
 *		Add a reference to the .data symbol `sym' to `set'.
 *
 *	__link_set_add_bss(set, sym)
 *		Add a reference to the .bss symbol `sym' to `set'.
 *
 *	__link_set_decl(set, ptype)
 *		Provide an extern declaration of the set `set', which
 *		contains an array of the pointer type `ptype'.  This
 *		macro must be used by any code which wishes to reference
 *		the elements of a link set.
 *
 *	__link_set_start(set)
 *		This points to the first slot in the link set.
 *
 *	__link_set_end(set)
 *		This points to the (non-existent) slot after the last
 *		entry in the link set.
 *
 *	__link_set_count(set)
 *		Count the number of entries in link set `set'.
 *
 * In addition, we provide the following macros for accessing link sets:
 *
 *	__link_set_foreach(pvar, set)
 *		Iterate over the link set `set'.  Because a link set is
 *		an array of pointers, pvar must be declared as "type **pvar",
 *		and the actual entry accessed as "*pvar".
 *
 *	__link_set_entry(set, idx)
 *		Access the link set entry at index `idx' from set `set'.
 */
#define	__link_set_foreach(pvar, set)					\
	for (pvar = __link_set_start(set); pvar < __link_set_end(set); pvar++)

#define	__link_set_entry(set, idx)	(__link_set_begin(set)[idx])

/*
 * Return the number of elements in a statically-allocated array,
 * __x.
 */
#define	__arraycount(__x)	(sizeof(__x) / sizeof(__x[0]))

/* __BIT(n): nth bit, where __BIT(0) == 0x1. */
#define	__BIT(__n)	\
    (((uintmax_t)(__n) >= NBBY * sizeof(uintmax_t)) ? 0 : ((uintmax_t)1 << (uintmax_t)(__n)))

/* __BITS(m, n): bits m through n, m < n. */
#define	__BITS(__m, __n)	\
	((__BIT(MAX((__m), (__n)) + 1) - 1) ^ (__BIT(MIN((__m), (__n))) - 1))

/* find least significant bit that is set */
#define	__LOWEST_SET_BIT(__mask) ((((__mask) - 1) & (__mask)) ^ (__mask))

#define	__PRIuBIT	PRIuMAX
#define	__PRIuBITS	__PRIuBIT

#define	__PRIxBIT	PRIxMAX
#define	__PRIxBITS	__PRIxBIT

#define	__SHIFTOUT(__x, __mask)	(((__x) & (__mask)) / __LOWEST_SET_BIT(__mask))
#define	__SHIFTIN(__x, __mask) ((__x) * __LOWEST_SET_BIT(__mask))
#define	__SHIFTOUT_MASK(__mask) __SHIFTOUT((__mask), (__mask))

/*
 * Only to be used in other headers that are included from both c or c++
 * NOT to be used in code.
 */
#ifdef __cplusplus
#define __CAST(__dt, __st)	static_cast<__dt>(__st)
#else
#define __CAST(__dt, __st)	((__dt)(__st))
#endif

#endif /* !_SYS_CDEFS_H_ */
