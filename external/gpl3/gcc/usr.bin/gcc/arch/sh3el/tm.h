/* This file is automatically generated.  DO NOT EDIT! */
/* Generated from: NetBSD: mknative-gcc,v 1.70 2013/05/05 07:11:34 skrll Exp  */
/* Generated from: NetBSD: mknative.common,v 1.8 2006/05/26 19:17:21 mrg Exp  */

#ifndef GCC_TM_H
#define GCC_TM_H
#define TARGET_CPU_DEFAULT (SELECT_SH3)
#ifndef LIBC_GLIBC
# define LIBC_GLIBC 1
#endif
#ifndef LIBC_UCLIBC
# define LIBC_UCLIBC 2
#endif
#ifndef LIBC_BIONIC
# define LIBC_BIONIC 3
#endif
#ifndef NETBSD_ENABLE_PTHREADS
# define NETBSD_ENABLE_PTHREADS
#endif
#ifndef SH_MULTILIB_CPU_DEFAULT
# define SH_MULTILIB_CPU_DEFAULT "m3"
#endif
#ifndef SUPPORT_SH3
# define SUPPORT_SH3 1
#endif
#ifndef SUPPORT_SH3
# define SUPPORT_SH3 1
#endif
#ifndef SUPPORT_SH3E
# define SUPPORT_SH3E 1
#endif
#ifndef SUPPORT_SH4
# define SUPPORT_SH4 1
#endif
#ifdef IN_GCC
# include "options.h"
# include "insn-constants.h"
# include "config/sh/little.h"
# include "config/vxworks-dummy.h"
# include "config/sh/sh.h"
# include "config/dbxelf.h"
# include "config/elfos.h"
# include "config/sh/elf.h"
# include "config/netbsd.h"
# include "config/netbsd-stdint.h"
# include "config/netbsd-elf.h"
# include "config/sh/netbsd-elf.h"
# include "sysroot-suffix.h"
# include "config/initfini-array.h"
#endif
#if defined IN_GCC && !defined GENERATOR_FILE && !defined USED_FOR_TARGET
# include "insn-flags.h"
#endif
# include "defaults.h"
#endif /* GCC_TM_H */
