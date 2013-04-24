/* Definitions for Intel 386 running MINIX with ELF format
   Copyright (C) 1996, 2000, 2002, 2004 Free Software Foundation, Inc.
   Contributed by Eric Youngdale.
   Modified for stabs-in-ELF by H.J. Lu.
   Adapted from GNU/Linux version by John Polstra.
   Continued development by David O'Brien <obrien@freebsd.org>
   Adapted for MINIX by Lionel Sambuc <lionel@minix3.org>

   This file is part of GCC.

   GCC is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published
   by the Free Software Foundation; either version 2, or (at your
   option) any later version.

   GCC is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License
   along with GCC; see the file COPYING.  If not, write to the 
   Free Software Foundation, 51 Franklin Street, Fifth Floor, Boston,
   MA 02110-1301, USA.  */

#undef  MINIX_TARGET_CPU_CPP_BUILTINS
#define MINIX_TARGET_CPU_CPP_BUILTINS()          \
  do                                            \
    {                                           \
		builtin_define ("__i386");	\
		builtin_define_with_int_value ("_EM_WSIZE", 4);	\
		builtin_define_with_int_value ("_EM_PSIZE", 4);	\
		builtin_define_with_int_value ("_EM_SSIZE", 2);	\
		builtin_define_with_int_value ("_EM_LSIZE", 4);	\
		builtin_define_with_int_value ("_EM_FSIZE", 4);	\
		builtin_define_with_int_value ("_EM_DSIZE", 8);	\
    }                                           \
  while (0)

/* Define the actual types of some ANSI-mandated types.
   Needs to agree with <machine/ansi.h>.  GCC defaults come from c-decl.c,
   c-common.c, and config/<arch>/<arch>.h. */
#undef  SIZE_TYPE
#define SIZE_TYPE	(TARGET_64BIT ? "long unsigned int" : "unsigned int")
 
#undef  PTRDIFF_TYPE
#define PTRDIFF_TYPE	(TARGET_64BIT ? "long int" : "int")
  
#undef  WCHAR_TYPE_SIZE
#define WCHAR_TYPE_SIZE	(TARGET_64BIT ? 32 : BITS_PER_WORD)

/* Override the default comment-starter of "/".  */
#undef  ASM_COMMENT_START
#define ASM_COMMENT_START "#"

#undef  ASM_APP_ON
#define ASM_APP_ON "#APP\n"

#undef  ASM_APP_OFF
#define ASM_APP_OFF "#NO_APP\n"

#undef  DBX_REGISTER_NUMBER
#define DBX_REGISTER_NUMBER(n) \
  (TARGET_64BIT ? dbx64_register_map[n] : svr4_dbx_register_map[n])

#undef  NO_PROFILE_COUNTERS
#define NO_PROFILE_COUNTERS	1

/* Tell final.c that we don't need a label passed to mcount.  */
#undef  MCOUNT_NAME
#define MCOUNT_NAME ".mcount"

/* A C statement to output to the stdio stream FILE an assembler
   command to advance the location counter to a multiple of 1<<LOG
   bytes if it is within MAX_SKIP bytes.

   This is used to align code labels according to Intel recommendations.  */
#ifdef HAVE_GAS_MAX_SKIP_P2ALIGN
#undef  ASM_OUTPUT_MAX_SKIP_ALIGN
#define ASM_OUTPUT_MAX_SKIP_ALIGN(FILE, LOG, MAX_SKIP)					\
  if ((LOG) != 0) {														\
    if ((MAX_SKIP) == 0) fprintf ((FILE), "\t.p2align %d\n", (LOG));	\
    else fprintf ((FILE), "\t.p2align %d,,%d\n", (LOG), (MAX_SKIP));	\
  }
#endif

#undef SUBTARGET32_DEFAULT_CPU
#define SUBTARGET32_DEFAULT_CPU "i586"

#undef TARGET_VERSION
#define TARGET_VERSION fprintf (stderr, " (MINIX/i586 ELF)");
