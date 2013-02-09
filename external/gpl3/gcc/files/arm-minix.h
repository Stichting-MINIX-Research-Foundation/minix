/* Definitions for StrongARM running FreeBSD using the ELF format
   Copyright (C) 2001, 2004, 2007 Free Software Foundation, Inc.
   Contributed by David E. O'Brien <obrien@FreeBSD.org> and BSDi.

   This file is part of GCC.

   GCC is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published
   by the Free Software Foundation; either version 3, or (at your
   option) any later version.

   GCC is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License
   along with GCC; see the file COPYING3.  If not see
   <http://www.gnu.org/licenses/>.  */


#undef MINIX_TARGET_CPU_CPP_BUILTINS
#define MINIX_TARGET_CPU_CPP_BUILTINS()	\
  do					\
    {					\
      TARGET_BPABI_CPP_BUILTINS();	\
    }					\
  while (0)

/************************[  Target stuff  ]***********************************/

/* Define the actual types of some ANSI-mandated types.  
   Needs to agree with <machine/ansi.h>.  GCC defaults come from c-decl.c,
   c-common.c, and config/<arch>/<arch>.h.  */

/* arm.h gets this wrong for FreeBSD.  We use the GCC defaults instead.  */

#undef  SIZE_TYPE
#define SIZE_TYPE	"unsigned int"

#undef  PTRDIFF_TYPE
#define PTRDIFF_TYPE	"int"

#undef WCHAR_TYPE
#define WCHAR_TYPE	"int"

/* Little endian by default */
#undef TARGET_ENDIAN_DEFAULT
#define TARGET_ENDIAN_DEFAULT 0

/* Use by default the new abi and calling standard */
#undef ARM_DEFAULT_ABI
#define ARM_DEFAULT_ABI ARM_ABI_AAPCS

/* Fixed-sized enum by default (-fno-short-enums) */
#undef CC1_SPEC
#define CC1_SPEC                                                \
	"%{!fshort-enums:%{!fno-short-enums:-fno-short-enums}} "

/* This defaults us to little-endian.  */
#ifndef TARGET_ENDIAN_DEFAULT
#define TARGET_ENDIAN_DEFAULT 0
#endif

#undef  SUBTARGET_CPU_DEFAULT
#define SUBTARGET_CPU_DEFAULT	TARGET_CPU_cortexa8

#undef TARGET_VERSION
#define TARGET_VERSION fputs (" (MINIX/arm ELF EABI)", stderr);

/* suppress -lgcc - don't include %G (-lgcc) in the libraries */
#undef LINK_GCC_C_SEQUENCE_SPEC
#define LINK_GCC_C_SEQUENCE_SPEC "%L"
