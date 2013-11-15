/* include/mcld/Config/Config.h.  Generated from Config.h.in by configure.  */
/* include/mcld/Config/Config.h.in.  Generated from configure.ac by autoheader.  */


//===- Config.h.in --------------------------------------------------------===//
//
//                     The MCLinker Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#ifndef MCLD_CONFIG_H
#define MCLD_CONFIG_H


/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define if you have POSIX threads libraries and header files. */
#define HAVE_PTHREAD 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/mman.h> header file. */
#define HAVE_SYS_MMAN_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to the sub-directory in which libtool stores uninstalled libraries.
   */
#define LT_OBJDIR ".libs/"

/* Target triple MCLinker will generate code for by default */
#define MCLD_DEFAULT_TARGET_TRIPLE "x86_64--netbsd"

/* Define if this is Unixish platform */
#define MCLD_ON_UNIX 1

/* Define if this is Win32ish platform */
/* #undef MCLD_ON_WIN32 */

/* MCLINKER version */
#define MCLD_VERSION "2.1.0.0-NanHu"

/* Name of package */
#define PACKAGE "mclinker"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "mclinker@googlegroups.com"

/* Define to the full name of this package. */
#define PACKAGE_NAME "MCLinker"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "MCLinker NanHu"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "mclinker"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "NanHu"

/* Define to necessary symbol if this constant uses a non-standard name on
   your system. */
/* #undef PTHREAD_CREATE_JOINABLE */

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define for standalone Android linker */
/* #undef TARGET_BUILD */

/* Version number of package */
#define VERSION "NanHu"


#define MCLD_REGION_CHUNK_SIZE 32
#define MCLD_NUM_OF_INPUTS 32
#define MCLD_SECTIONS_PER_INPUT 16
#define MCLD_SYMBOLS_PER_INPUT 128
#define MCLD_RELOCATIONS_PER_INPUT 1024

#endif

