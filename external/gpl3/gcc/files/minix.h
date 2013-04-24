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

#undef  SWITCH_TAKES_ARG
#define SWITCH_TAKES_ARG(CHAR) (MINIX_SWITCH_TAKES_ARG(CHAR))

#undef  WORD_SWITCH_TAKES_ARG
#define WORD_SWITCH_TAKES_ARG(STR) (MINIX_WORD_SWITCH_TAKES_ARG(STR))

#undef  TARGET_OS_CPP_BUILTINS
#define TARGET_OS_CPP_BUILTINS() MINIX_TARGET_OS_CPP_BUILTINS()

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

#undef STANDARD_STARTFILE_PREFIX
#define STANDARD_STARTFILE_PREFIX MINIX_STANDARD_STARTFILE_PREFIX

#undef STANDARD_STARTFILE_PREFIX_1
#define STANDARD_STARTFILE_PREFIX_1 MINIX_STANDARD_STARTFILE_PREFIX

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

/* Use periods rather than dollar signs in special g++ assembler names.
   This ensures the configuration knows our system correctly so we can link
   with libraries compiled with the native cc.  */
#undef NO_DOLLAR_IN_LABEL

/* Don't assume anything about the header files.  */
#undef  NO_IMPLICIT_EXTERN_C
#define NO_IMPLICIT_EXTERN_C	1

/* Handle #pragma weak and #pragma pack.  */
#undef HANDLE_SYSV_PRAGMA
#define HANDLE_SYSV_PRAGMA 1

/* Don't default to pcc-struct-return, we want to retain compatibility with
   older gcc versions AND pcc-struct-return is nonreentrant.
   (even though the SVR4 ABI for the i386 says that records and unions are
   returned in memory).  */

#undef  DEFAULT_PCC_STRUCT_RETURN
#define DEFAULT_PCC_STRUCT_RETURN 0

/* Use --as-needed -lgcc_s for eh support.  */
#ifdef HAVE_LD_AS_NEEDED
#define USE_LD_AS_NEEDED 1
#endif

#if defined(HAVE_LD_EH_FRAME_HDR)
#define LINK_EH_SPEC "--eh-frame-hdr "
#endif
