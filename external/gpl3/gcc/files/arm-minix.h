/* Definitions for ARM running MINIX using the ELF format
   Copyright (C) 2001, 2004, 2007 Free Software Foundation, Inc.
   Contributed by David E. O'Brien <obrien@FreeBSD.org> and BSDi.
   Adapted for MINIX by Lionel Sambuc <lionel@minix3.org>

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

/* Define the actual types of some ANSI-mandated types.  
   Needs to agree with <machine/ansi.h>.  GCC defaults come from c-decl.c,
   c-common.c, and config/<arch>/<arch>.h. */
#undef  SIZE_TYPE
#define SIZE_TYPE	"unsigned int"

#undef  PTRDIFF_TYPE
#define PTRDIFF_TYPE	"int"

#undef WCHAR_TYPE
#define WCHAR_TYPE	"int"

/* VERY BIG NOTE: Change of structure alignment for NetBSD/arm.
   There are consequences you should be aware of...

   Normally GCC/arm uses a structure alignment of 32 for compatibility
   with armcc.  This means that structures are padded to a word
   boundary.  However this causes problems with bugged NetBSD kernel
   code (possibly userland code as well - I have not checked every
   binary).  The nature of this bugged code is to rely on sizeof()
   returning the correct size of various structures rounded to the  
   nearest byte (SCSI and ether code are two examples, the vm system
   is another).  This code breaks when the structure alignment is 32
   as sizeof() will report a word=rounded size.  By changing the        
   structure alignment to 8. GCC will conform to what is expected by
   NetBSD.
   
   This has several side effects that should be considered.
   1. Structures will only be aligned to the size of the largest member.
      i.e. structures containing only bytes will be byte aligned.
           structures containing shorts will be half word aligned.          
           structures containing ints will be word aligned.                 
  
      This means structures should be padded to a word boundary if
      alignment of 32 is required for byte structures etc.
       
   2. A potential performance penalty may exist if strings are no longer
      word aligned.  GCC will not be able to use word load/stores to copy
      short strings.

   This modification is not encouraged but with the present state of the
   NetBSD source tree it is currently the only solution that meets the
   requirements.  */

#undef DEFAULT_STRUCTURE_SIZE_BOUNDARY
#define DEFAULT_STRUCTURE_SIZE_BOUNDARY 8

/* Fixed-sized enum by default (-fno-short-enums) */
#undef MINIX_CC1_SPEC
#define MINIX_CC1_SPEC	"%{!fshort-enums:%{!fno-short-enums:-fno-short-enums}} "

/* Use by default the new abi and calling standard */
#undef ARM_DEFAULT_ABI
#define ARM_DEFAULT_ABI ARM_ABI_AAPCS

/* LSC: FIXME: When activated, some programs crash on qemu with an illegal 
 *             instruction.
 *             The cause is unknown (Missing support on MINIX, missing support
 *             on the emulator, library error...).
 */
#if 0
/* Make sure we use hard-floating point ABI by default */
#undef TARGET_DEFAULT_FLOAT_ABI
#define TARGET_DEFAULT_FLOAT_ABI ARM_FLOAT_ABI_HARD
#endif

/* Default to full VFP if -mhard-float is specified.  */
#undef MINIX_SUBTARGET_ASM_FLOAT_SPEC
#define MINIX_SUBTARGET_ASM_FLOAT_SPEC					\
	"%{mhard-float:{!mfpu=*:-mfpu=vfpv3-d16}}			\
	 %{mfloat-abi=hard:{!mfpu=*:-mfpu=vfpv3-d16}}"

#undef MINIX_SUBTARGET_EXTRA_ASM_SPEC
#define MINIX_SUBTARGET_EXTRA_ASM_SPEC					\
	"%{mabi=apcs-gnu|mabi=atpcs:-meabi=gnu;:-meabi=5}"		\
	TARGET_FIX_V4BX_SPEC						\
	"%{fpic|fpie:-k} %{fPIC|fPIE:-k}"

/* Little endian by default */
#undef TARGET_ENDIAN_DEFAULT
#define TARGET_ENDIAN_DEFAULT 0

#undef  SUBTARGET_CPU_DEFAULT
#define SUBTARGET_CPU_DEFAULT	TARGET_CPU_cortexa8

#undef TARGET_VERSION
#define TARGET_VERSION fputs (" (MINIX/arm ELF EABI)", stderr);
