/* $DragonFly: src/gnu/usr.bin/cc41/cc_prep/config/dragonfly.h,v 1.2 2008/05/19 10:46:39 corecode Exp $ */

/* Base configuration file for all DragonFly targets.
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
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

/* Common DragonFly configuration. 
   All DragonFly architectures should include this file, which will specify
   their commonalities.

   Adapted from gcc/config/freebsd.h by
   Joerg Sonnenberger <joerg@bec.de>

   Adapted from gcc/config/i386/freebsd-elf.h by 
   David O'Brien <obrien@FreeBSD.org>.  
   Further work by David O'Brien <obrien@FreeBSD.org> and
   Loren J. Rittle <ljrittle@acm.org>.  */


/* This defines which switch letters take arguments.  On DragonFly, most of
   the normal cases (defined in gcc.c) apply, and we also have -h* and
   -z* options (for the linker) (coming from SVR4).
   We also have -R (alias --rpath), no -z, --soname (-h), --assert etc.  */

#undef  SWITCH_TAKES_ARG
#define SWITCH_TAKES_ARG(CHAR) (DFBSD_SWITCH_TAKES_ARG(CHAR))

#undef  WORD_SWITCH_TAKES_ARG
#define WORD_SWITCH_TAKES_ARG(STR) (DFBSD_WORD_SWITCH_TAKES_ARG(STR))

#undef  TARGET_OS_CPP_BUILTINS
#define TARGET_OS_CPP_BUILTINS() DFBSD_TARGET_OS_CPP_BUILTINS()

#undef  CPP_SPEC
#define CPP_SPEC DFBSD_CPP_SPEC

#undef  STARTFILE_SPEC
#define STARTFILE_SPEC DFBSD_STARTFILE_SPEC

#undef  ENDFILE_SPEC
#define ENDFILE_SPEC DFBSD_ENDFILE_SPEC

#undef  LIB_SPEC
#define LIB_SPEC DFBSD_LIB_SPEC

#if 0
#undef	LINK_COMMAND_SPEC
#define LINK_COMMAND_SPEC DFBSD_LINK_COMMAND_SPEC 
#endif

/************************[  Target stuff  ]***********************************/

/* All DragonFly Architectures support the ELF object file format.  */
#undef  OBJECT_FORMAT_ELF
#define OBJECT_FORMAT_ELF

/* Don't assume anything about the header files.  */
#undef  NO_IMPLICIT_EXTERN_C
#define NO_IMPLICIT_EXTERN_C	1

/* Make gcc agree with DragonFly's standard headers (<machine/stdint.h>, etc...)  */

#undef  WCHAR_TYPE
#define WCHAR_TYPE "int"

#define MATH_LIBRARY_PROFILE    "-lm_p"

/* Code generation parameters.  */

/* Use periods rather than dollar signs in special g++ assembler names.
   This ensures the configuration knows our system correctly so we can link
   with libraries compiled with the native cc.  */
#undef NO_DOLLAR_IN_LABEL

/* Define this so we can compile MS code for use with WINE.  */
#define HANDLE_PRAGMA_PACK_PUSH_POP

/* Used by libgcc2.c.  We support file locking with fcntl / F_SETLKW.
   This enables the test coverage code to use file locking when exiting a
   program, which avoids race conditions if the program has forked.  */
#define TARGET_POSIX_IO
