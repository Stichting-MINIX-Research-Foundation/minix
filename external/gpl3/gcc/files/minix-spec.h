/* Base configuration file for all MINIX targets.
   Copyright (C) 1999, 2000, 2001, 2004, 2005 Free Software Foundation, Inc.
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

/* MINIX defines 
 * Default, architecture independent values for MINIX
 *
 * For this to work properly, the order in the tm_file variable has
 * to be the following:
 *   minix-spec.h $arch/minix.h minix.h
 *
 * minix-spec.h	: specifies default arch-independent values
 * $arch/minix.h: redefines as needed default minix values
 * minix.h	: maps GCC defines to the minix defines.
 *
 * WARNING:
 *   When changing any default, also check in the arch headers
 *   if the default is redefined and update them as required.
 */

/* In case we need to know.  */
#define USING_CONFIG_MINIX_SPEC 1

#define MINIX_TARGET_OS_CPP_BUILTINS()					\
  do									\
    {									\
		builtin_define ("__minix");				\
		builtin_define ("__minix__");				\
		builtin_define ("__unix__");				\
		builtin_assert ("system=bsd");				\
		builtin_assert ("system=unix");				\
		builtin_assert ("system=minix");			\
		MINIX_TARGET_CPU_CPP_BUILTINS();			\
    }									\
  while (0)

/* Define the default MINIX-specific per-CPU hook code.  */
#define MINIX_TARGET_CPU_CPP_BUILTINS() do {} while (0)

/* Look for the include files in the system-defined places.  */

#define MINIX_GPLUSPLUS_INCLUDE_DIR "/usr/include/g++"

#define MINIX_GPLUSPLUS_BACKWARD_INCLUDE_DIR "/usr/include/g++/backward"

#define MINIX_GPLUSPLUS_INCLUDE_DIR_ADD_SYSROOT 1

#define MINIX_GCC_INCLUDE_DIR_ADD_SYSROOT 1

/*
 * XXX figure out a better way to do this
 */
#define MINIX_GCC_INCLUDE_DIR "/usr/include/gcc-4.8"

/* Provide a CPP_SPEC appropriate for MINIX.  We just deal with the GCC 
   option `-posix'.  */
#define MINIX_CPP_SPEC "%{posix:-D_POSIX_SOURCE}"

/* Pass -cxx-isystem to cc1/cc1plus.  */
#define MINIX_CC1PLUS_SPEC "%{cxx-isystem}"
#define MINIX_CC1_SPEC "%{cxx-isystem}"


#define MINIX_SUBTARGET_CPP_SPEC ""
#define MINIX_SUBTARGET_EXTRA_ASM_SPEC ""
#define MINIX_SUBTARGET_ASM_FLOAT_SPEC ""

#define MINIX_DYNAMIC_LINKER "/usr/libexec/ld.elf_so"

/* Under MINIX, just like on NetBSD, the normal location of the various 
 *    *crt*.o files is the /usr/lib directory.  */
#define MINIX_STANDARD_STARTFILE_PREFIX	"/usr/lib/"

/* Under NetBSD, the normal location of the compiler back ends is the
   /usr/libexec directory.  */

#define MINIX_STANDARD_EXEC_PREFIX		"/usr/libexec/"
#define MINIX_TOOLDIR_BASE_PREFIX		"../"
#define MINIX_STANDARD_BINDIR_PREFIX		"/usr/bin"
#define MINIX_STANDARD_LIBEXEC_PREFIX		MINIX_STANDARD_EXEC_PREFIX

#define MINIX_LINK_GCC_C_SEQUENCE_SPEC \
	"%{static:--start-group} %G %L %{static:--end-group}%{!static:%G}"

/* Provide a STARTFILE_SPEC appropriate for MINIX.  Here we add
   the magical crtbegin.o file (see crtstuff.c) which provides part 
   of the support for getting C++ file-scope static object constructed
   before entering `main'.  */
#define MINIX_STARTFILE_SPEC	\
  "%{!shared:			\
     %{pg:gcrt0%O%s}		\
     %{!pg:			\
	 %{p:gcrt0%O%s}		\
	 %{!p:%{profile:gcrt0%O%s} \
	      %{!profile:crt0%O%s}}}} \
   %:if-exists(crti%O%s)	\
   %{static:%:if-exists-else(crtbeginT%O%s crtbegin%O%s)} \
   %{!static:                   \
     %{!shared:			\
	 %{!pie:crtbegin%O%s}	\
	 %{pie:crtbeginS%O%s}}	\
     %{shared:crtbeginS%O%s}}"

/* Provide an ENDFILE_SPEC appropriate for NetBSD ELF.  Here we
   add crtend.o, which provides part of the support for getting
   C++ file-scope static objects deconstructed after exiting "main".  */
#define MINIX_ENDFILE_SPEC	\
  "%{!shared:                   \
    %{!pie:crtend%O%s}          \
    %{pie:crtendS%O%s}}         \
   %{shared:crtendS%O%s}        \
   %:if-exists(crtn%O%s)"

/* Provide a LIB_SPEC appropriate for MINIX.  Just select the appropriate
   libc, depending on whether we're doing profiling or need threads support.
   (similar to the default, except no -lg, and no -p).  */
#define MINIX_LIB_SPEC "						\
%{pthread: %eThe -pthread option is only supported on MINIX when gcc	\
is built with the --enable-threads configure-time option.}		\
  %{shared:-lc}			\
  %{!shared:			\
    %{!symbolic:		\
      %{!p:			\
	%{!pg:-lc}}		\
      %{p:-lc_p}		\
      %{pg:-lc_p}}}"

/* Provide a LINK_SPEC appropriate for MINIX.  Here we provide support
   for the special GCC options -static and -shared, which allow us to
   link things in one of these three modes by applying the appropriate
   combinations of options at link-time. We like to support here for
   as many of the other GNU linker options as possible. But I don't
   have the time to search for those flags. I am sure how to add
   support for -soname shared_object_name. H.J.

   I took out %{v:%{!V:-V}}. It is too much :-(. They can use
   -Wl,-V.

   When the -shared link option is used a final link is not being
   done.  */
#define MINIX_LINK_SPEC "						\
  -X									\
  %{p:%nconsider using `-pg' instead of `-p' with gprof(1) }		\
  %{assert*} %{R*} %{rpath*}						\
  %{shared:-Bshareable %{h*} %{soname*}}				\
  %{symbolic:-Bsymbolic}						\
  %{!shared:								\
    -dc -dp								\
    %{!static:								\
      %{rdynamic:-export-dynamic}					\
      %{!dynamic-linker:-dynamic-linker %(minix_dynamic_linker) }}	\
    %{static:-Bstatic}}"

#define MINIX_TARGET_UNWIND_TABLES_DEFAULT true
