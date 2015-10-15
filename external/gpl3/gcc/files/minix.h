/* Base configuration file for all MINIX targets.
   Copyright (C) 1999, 2000, 2001 Free Software Foundation, Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GCC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to
the Free Software Foundation, 51 Franklin Street, Fifth Floor,
Boston, MA 02110-1301, USA.  */

/* Common MINIX configuration. 
 * This file maps GCC defines to MINIX defines
 *
 * For this to work properly, the order in the tm_file variable has
 * to be the following:
 *   minix-spec.h $arch/minix.h minix.h
 *
 * minix-spec.h	: specifies default arch-independent values
 * $arch/minix.h: redefines as needed default minix values
 * minix.h	: maps GCC defines to the minix defines.
 */

/* In case we need to know.  */
#define USING_CONFIG_MINIX 1

#undef  TARGET_OS_CPP_BUILTINS
#define TARGET_OS_CPP_BUILTINS() MINIX_TARGET_OS_CPP_BUILTINS()

#if defined(NETBSD_NATIVE) || defined(NETBSD_TOOLS)
#undef GPLUSPLUS_INCLUDE_DIR
#define GPLUSPLUS_INCLUDE_DIR MINIX_GPLUSPLUS_INCLUDE_DIR

#undef GPLUSPLUS_INCLUDE_DIR_ADD_SYSROOT
#define GPLUSPLUS_INCLUDE_DIR_ADD_SYSROOT MINIX_GPLUSPLUS_INCLUDE_DIR_ADD_SYSROOT

#undef GPLUSPLUS_BACKWARD_INCLUDE_DIR
#define GPLUSPLUS_BACKWARD_INCLUDE_DIR MINIX_GPLUSPLUS_BACKWARD_INCLUDE_DIR

#undef GCC_INCLUDE_DIR
#define GCC_INCLUDE_DIR MINIX_GCC_INCLUDE_DIR

#undef GCC_INCLUDE_DIR_ADD_SYSROOT
#define GCC_INCLUDE_DIR_ADD_SYSROOT MINIX_GCC_INCLUDE_DIR_ADD_SYSROOT

#undef STANDARD_STARTFILE_PREFIX
#define STANDARD_STARTFILE_PREFIX MINIX_STANDARD_STARTFILE_PREFIX

#undef STANDARD_STARTFILE_PREFIX_1
#define STANDARD_STARTFILE_PREFIX_1 MINIX_STANDARD_STARTFILE_PREFIX

#endif /* defined(NETBSD_NATIVE) || defined(NETBSD_TOOLS) */

#if defined(NETBSD_NATIVE)
/* Under NetBSD, the normal location of the compiler back ends is the
   /usr/libexec directory.  */

#undef STANDARD_EXEC_PREFIX
#define STANDARD_EXEC_PREFIX	MINIX_STANDARD_EXEC_PREFIX

#undef TOOLDIR_BASE_PREFIX
#define TOOLDIR_BASE_PREFIX	MINIX_TOOLDIR_BASE_PREFIX

#undef STANDARD_BINDIR_PREFIX
#define STANDARD_BINDIR_PREFIX	MINIX_STANDARD_BINDIR_PREFIX

#undef STANDARD_LIBEXEC_PREFIX
#define STANDARD_LIBEXEC_PREFIX	MINIX_STANDARD_EXEC_PREFIX

#endif /* NETBSD_NATIVE */

#undef  CPP_SPEC
#define CPP_SPEC MINIX_CPP_SPEC

#undef CC1_SPEC
#define CC1_SPEC MINIX_CC1_SPEC

#undef CC1PLUS_SPEC
#define CC1PLUS_SPEC MINIX_CC1PLUS_SPEC

#undef  STARTFILE_SPEC
#define STARTFILE_SPEC MINIX_STARTFILE_SPEC

#undef  ENDFILE_SPEC
#define ENDFILE_SPEC MINIX_ENDFILE_SPEC

#undef  LIB_SPEC
#define LIB_SPEC MINIX_LIB_SPEC

#undef	LINK_SPEC
#define LINK_SPEC MINIX_LINK_SPEC

#undef LINK_GCC_C_SEQUENCE_SPEC
#define LINK_GCC_C_SEQUENCE_SPEC MINIX_LINK_GCC_C_SEQUENCE_SPEC

/* This has to be here in order to allow architecture to define the default
 * content of the additional specs. */
#undef  SUBTARGET_EXTRA_SPECS
#define SUBTARGET_EXTRA_SPECS \
  { "subtarget_extra_asm_spec", MINIX_SUBTARGET_EXTRA_ASM_SPEC },	\
  { "subtarget_asm_float_spec", MINIX_SUBTARGET_ASM_FLOAT_SPEC },	\
  { "minix_dynamic_linker", MINIX_DYNAMIC_LINKER }

#undef  SUBTARGET_CPP_SPEC
#define SUBTARGET_CPP_SPEC MINIX_SUBTARGET_CPP_SPEC

/* All MINIX Architectures support the ELF object file format.  */
#undef  OBJECT_FORMAT_ELF
#define OBJECT_FORMAT_ELF

#undef TARGET_UNWIND_TABLES_DEFAULT
#define TARGET_UNWIND_TABLES_DEFAULT MINIX_TARGET_UNWIND_TABLES_DEFAULT

/* Use periods rather than dollar signs in special g++ assembler names.
   This ensures the configuration knows our system correctly so we can link
   with libraries compiled with the native cc.  */
#undef NO_DOLLAR_IN_LABEL

/* We always use gas here, so we don't worry about ECOFF assembler
   problems.  */
#undef TARGET_GAS
#define TARGET_GAS	1

/* Default to pcc-struct-return, because this is the ELF abi and
   we don't care about compatibility with older gcc versions.  */
#undef DEFAULT_PCC_STRUCT_RETURN
#define DEFAULT_PCC_STRUCT_RETURN 1

/* When building shared libraries, the initialization and finalization 
   functions for the library are .init and .fini respectively.  */

#define COLLECT_SHARED_INIT_FUNC(STREAM,FUNC)				\
  do {									\
    fprintf ((STREAM), "void __init() __asm__ (\".init\");");		\
    fprintf ((STREAM), "void __init() {\n\t%s();\n}\n", (FUNC));	\
  } while (0)

#define COLLECT_SHARED_FINI_FUNC(STREAM,FUNC)				\
  do {									\
    fprintf ((STREAM), "void __fini() __asm__ (\".fini\");");		\
    fprintf ((STREAM), "void __fini() {\n\t%s();\n}\n", (FUNC));	\
  } while (0)

#undef TARGET_POSIX_IO
#define TARGET_POSIX_IO

/* Don't assume anything about the header files.  */
#undef  NO_IMPLICIT_EXTERN_C
#define NO_IMPLICIT_EXTERN_C	1

/* Define some types that are the same on all NetBSD platforms,
   making them agree with <machine/ansi.h>.  */

#undef WCHAR_TYPE
#define WCHAR_TYPE "int"

#undef WCHAR_TYPE_SIZE
#define WCHAR_TYPE_SIZE 32

#undef WINT_TYPE
#define WINT_TYPE "int"

#define LINK_EH_SPEC "%{!static:--eh-frame-hdr} "

/* Use --as-needed -lgcc_s for eh support.  */
#ifdef HAVE_LD_AS_NEEDED
#define USE_LD_AS_NEEDED 1
#endif
