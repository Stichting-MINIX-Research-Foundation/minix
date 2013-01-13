/* Base configuration file for all FreeBSD targets.
   Copyright (C) 1999, 2000, 2001, 2004, 2005 Free Software Foundation, Inc.

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
   All MINIX architectures should include this file, which will specify
   their commonalities.
*/

/* In case we need to know.  */
#define USING_CONFIG_MINIX_SPEC 1

/* This defines which switch letters take arguments.  On FreeBSD, most of
   the normal cases (defined in gcc.c) apply, and we also have -h* and
   -z* options (for the linker) (coming from SVR4).
   We also have -R (alias --rpath), no -z, --soname (-h), --assert etc.  */

#define MINIX_SWITCH_TAKES_ARG(CHAR)					\
  (DEFAULT_SWITCH_TAKES_ARG (CHAR)					\
    || (CHAR) == 'h'							\
    || (CHAR) == 'z' /* ignored by ld */				\
    || (CHAR) == 'R')

/* This defines which multi-letter switches take arguments.  */

#define MINIX_WORD_SWITCH_TAKES_ARG(STR)					\
  (DEFAULT_WORD_SWITCH_TAKES_ARG (STR)					\
   || !strcmp ((STR), "rpath") || !strcmp ((STR), "rpath-link")		\
   || !strcmp ((STR), "soname") || !strcmp ((STR), "defsym") 		\
   || !strcmp ((STR), "assert") || !strcmp ((STR), "dynamic-linker"))

#define MINIX_TARGET_OS_CPP_BUILTINS()					\
  do									\
    {									\
		builtin_define ("__minix");				\
		MINIX_TARGET_CPU_CPP_BUILTINS();			\
    }									\
  while (0)

/* Define the default MINIX-specific per-CPU hook code.  */
#define MINIX_TARGET_CPU_CPP_BUILTINS() do {} while (0)

/* Provide a CPP_SPEC appropriate for MINIX.  We just deal with the GCC 
   option `-posix', and PIC issues.  */

#define MINIX_CPP_SPEC "							\
  %(cpp_cpu)								\
  %(cpp_arch)								\
  %{posix:-D_POSIX_SOURCE}"

/* Provide a STARTFILE_SPEC appropriate for MINIX.  Here we add
   the magical crtbegin.o file (see crtstuff.c) which provides part 
	of the support for getting C++ file-scope static object constructed 
	before entering `main'.  */
   
#define MINIX_STARTFILE_SPEC \
  "%{!shared: \
     %{pg:gcrt0%O%s} %{!pg:%{p:gcrt0%O%s} \
		       %{!p:%{profile:gcrt0%O%s} \
			 %{!profile:crt0%O%s}}}} \
   crti%O%s %{!shared:crtbegin%O%s} %{shared:crtbeginS%O%s}"

/* Provide a ENDFILE_SPEC appropriate for MINIX.  Here we tack on
   the magical crtend.o file (see crtstuff.c) which provides part of 
	the support for getting C++ file-scope static object constructed 
	before entering `main', followed by a normal "finalizer" file, 
	`crtn.o'.  */

#define MINIX_ENDFILE_SPEC \
  "%{!shared:crtend.o%s} %{shared:crtendS.o%s} crtn.o%s"

/* Provide a LIB_SPEC appropriate for MINIX.  Just select the appropriate
   libc, depending on whether we're doing profiling or need threads support.
   (similar to the default, except no -lg, and no -p).  */

#define MINIX_LIB_SPEC "							\
  %{pthread: %eThe -pthread option is only supported on MINIX when gcc \
is built with the --enable-threads configure-time option.}		\
  %{!shared:								\
    %{!pg: -lc}								\
    %{pg:  -lc_p}							\
  }"

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

#define MINIX_LINK_SPEC "							\
  %{p:%nconsider using `-pg' instead of `-p' with gprof(1) }		\
  %{v:-V}								\
  %{assert*} %{R*} %{rpath*} %{defsym*}					\
  %{shared:-Bshareable %{h*} %{soname*}}				\
  %{!shared:								\
    %{!static:								\
      %{rdynamic:-export-dynamic}					\
      %{!dynamic-linker:-dynamic-linker %(minix_dynamic_linker) }}	\
    %{static:-Bstatic}}							\
  %{symbolic:-Bsymbolic}"


/* Under MINIX, just like on NetBSD, the normal location of the various 
 *    *crt*.o files is the /usr/lib directory.  */

#undef STANDARD_STARTFILE_PREFIX
#define STANDARD_STARTFILE_PREFIX	"/usr/lib/"
#undef STANDARD_STARTFILE_PREFIX_1
#define STANDARD_STARTFILE_PREFIX_1	"/usr/lib/"

#define MINIX_DYNAMIC_LINKER "/libexec/ld-elf.so.1"

#define MINIX_SUBTARGET_EXTRA_SPECS \
  { "minix_dynamic_linker", MINIX_DYNAMIC_LINKER }
