/* Base configuration file for all FreeBSD targets.
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
   All FreeBSD architectures should include this file, which will specify
   their commonalities.
*/

/* In case we need to know.  */
#define USING_CONFIG_MINIX 1

/* This defines which switch letters take arguments.  On MINIX, most of
   the normal cases (defined in gcc.c) apply, and we also have -h* and
   -z* options (for the linker) (coming from SVR4).
   We also have -R (alias --rpath), no -z, --soname (-h), --assert etc.  */

#undef  SWITCH_TAKES_ARG
#define SWITCH_TAKES_ARG(CHAR) (MINIX_SWITCH_TAKES_ARG(CHAR))

#undef  WORD_SWITCH_TAKES_ARG
#define WORD_SWITCH_TAKES_ARG(STR) (MINIX_WORD_SWITCH_TAKES_ARG(STR))

#undef  TARGET_OS_CPP_BUILTINS
#define TARGET_OS_CPP_BUILTINS() MINIX_TARGET_OS_CPP_BUILTINS()

#undef  CPP_SPEC
#define CPP_SPEC MINIX_CPP_SPEC

#undef  STARTFILE_SPEC
#define STARTFILE_SPEC MINIX_STARTFILE_SPEC

#undef  ENDFILE_SPEC
#define ENDFILE_SPEC MINIX_ENDFILE_SPEC

#undef  LIB_SPEC
#define LIB_SPEC MINIX_LIB_SPEC

#undef	LINK_SPEC
#define LINK_SPEC MINIX_LINK_SPEC

#undef  SUBTARGET_EXTRA_SPECS
#define SUBTARGET_EXTRA_SPECS MINIX_SUBTARGET_EXTRA_SPECS

#undef  SUBTARGET_CPP_SPEC
#define SUBTARGET_CPP_SPEC MINIX_CPP_SPEC

/************************[  Target stuff  ]***********************************/

/* All MINIX Architectures support the ELF object file format.  */
#undef  OBJECT_FORMAT_ELF
#define OBJECT_FORMAT_ELF

/* Don't assume anything about the header files.  */
#undef  NO_IMPLICIT_EXTERN_C
#define NO_IMPLICIT_EXTERN_C	1

/* Code generation parameters.  */

/* Use periods rather than dollar signs in special g++ assembler names.
   This ensures the configuration knows our system correctly so we can link
   with libraries compiled with the native cc.  */
#undef NO_DOLLAR_IN_LABEL

