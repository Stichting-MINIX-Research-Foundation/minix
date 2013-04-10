/*	$NetBSD: system.h,v 1.11 2009/03/06 17:03:48 apb Exp $	*/

/* system.h: system-dependent declarations; include this first.
   Id: system.h,v 1.12 2004/04/26 13:56:57 karl Exp

   Copyright (C) 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004 Free Software
   Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef TEXINFO_SYSTEM_H
#define TEXINFO_SYSTEM_H

#define _GNU_SOURCE

#include <config.h>

#ifdef MIKTEX
#include <gnu-miktex.h>
#define S_ISDIR(x) ((x)&_S_IFDIR) 
#else
/* MiKTeX defines substring() in a separate DLL, where it has its
   own __declspec declaration.  We don't want to try to duplicate 
   this Microsoft-ism here.  */
extern char *substring (const char *, const char *);
#endif

/* We follow the order of header inclusion from Autoconf's
   ac_includes_default, more or less.  */
#include <stdio.h>
#include <sys/types.h>
#include <ctype.h>

/* All systems nowadays probably have these functions, but ... */
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#ifndef HAVE_SETLOCALE
#define setlocale(category,locale) /* empty */
#endif

/* For gettext (NLS).  */
#define const
#include "gettext.h"
#undef const

#define _(String) gettext (String)
#define N_(String) (String)

#ifdef STDC_HEADERS
#define getopt system_getopt
#include <stdlib.h>
#undef getopt
#else
extern char *getenv ();
#endif

/* Don't use bcopy!  Use memmove if source and destination may overlap,
   memcpy otherwise.  */
#if HAVE_STRING_H
# if !STDC_HEADERS && HAVE_MEMORY_H
#  include <memory.h>
# endif
# include <string.h>
#endif

#if HAVE_STRINGS_H
/* Always include <strings.h> if we have it.  This is because that's
   what Autoconf's AC_CHECK_DECL does.  On IBM AIX 4.2, strncasecmp is
   only declared in strings.h.  */
# include <strings.h>
#endif

#if !HAVE_STRNCASECMP || !HAVE_STRCASECMP
# include "strcase.h"
#endif

#if !HAVE_DECL_MEMCHR
char *memchr ();
#endif

/* <unistd.h> defines _POSIX_VERSION, but Paul Eggert points out that is
   only supposed to be used in user code, not other system headers.  */
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include <errno.h>
#ifndef errno
extern int errno;
#endif
#ifdef VMS
#include <perror.h>
#endif

#ifndef HAVE_DECL_STRERROR
extern char *strerror ();
#endif

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifndef PATH_MAX
#ifndef _POSIX_PATH_MAX
# define _POSIX_PATH_MAX 255
#endif
#define PATH_MAX _POSIX_PATH_MAX
#endif

#ifndef HAVE_DECL_STRCASECMP
extern int strcasecmp ();
#endif

#ifndef HAVE_DECL_STRNCASECMP
extern int strncasecmp ();
#endif

#ifndef HAVE_DECL_STRCOLL
extern int strcoll ();
#endif

#include <sys/stat.h>
#if STAT_MACROS_BROKEN
# undef S_ISDIR
#endif
#if !defined(S_ISDIR) && defined(S_IFDIR)
# define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
#endif

#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif /* HAVE_SYS_FILE_H */

#ifndef O_RDONLY
/* Since <fcntl.h> is POSIX, prefer that to <sys/fcntl.h>.
   This also avoids some useless warnings on (at least) Linux.  */
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#else /* not HAVE_FCNTL_H */
#ifdef HAVE_SYS_FCNTL_H
#include <sys/fcntl.h>
#endif /* not HAVE_SYS_FCNTL_H */
#endif /* not HAVE_FCNTL_H */
#endif /* not O_RDONLY */

/* MS-DOS and similar non-Posix systems have some peculiarities:
    - they distinguish between binary and text files;
    - they use both `/' and `\\' as directory separator in file names;
    - they can have a drive letter X: prepended to a file name;
    - they have a separate root directory on each drive;
    - their filesystems are case-insensitive;
    - directories in environment variables (like INFOPATH) are separated
        by `;' rather than `:';
    - text files can have their lines ended either with \n or with \r\n pairs;
   These are all parameterized here except the last, which is
   handled by the source code as appropriate (mostly, in info/).  */
#ifndef O_BINARY
# ifdef _O_BINARY
#  define O_BINARY _O_BINARY
# else
#  define O_BINARY 0
# endif
#endif /* O_BINARY */

/* We'd like to take advantage of _doprnt if it's around, a la error.c,
   but then we'd have no VA_SPRINTF.  */
#if HAVE_VPRINTF
# if __STDC__
#  include <stdarg.h>
#  define VA_START(args, lastarg) va_start(args, lastarg)
# else
#  include <varargs.h>
#  define VA_START(args, lastarg) va_start(args)
# endif
# define VA_FPRINTF(file, fmt, ap) vfprintf (file, fmt, ap)
# define VA_SPRINTF(str, fmt, ap) vsprintf (str, fmt, ap)
#else /* not HAVE_VPRINTF */
# define VA_START(args, lastarg)
# define va_alist a1, a2, a3, a4, a5, a6, a7, a8
# define va_dcl char *a1, *a2, *a3, *a4, *a5, *a6, *a7, *a8;
# define va_end(args)
#endif

#if O_BINARY
# ifdef HAVE_IO_H
#  include <io.h>
# endif
# ifdef __MSDOS__
#  include <limits.h>
#  ifdef __DJGPP__
#   define HAVE_LONG_FILENAMES(dir)  (pathconf (dir, _PC_NAME_MAX) > 12)
#   define NULL_DEVICE	"/dev/null"
#   define DEFAULT_INFOPATH "c:/djgpp/info;/usr/local/info;/usr/info;."
    /* DJGPP supports /dev/null, which is okay for Unix aficionados,
       shell scripts and Makefiles, but interactive DOS die-hards
       would probably want to have NUL as well.  */
#   define ALSO_NULL_DEVICE  "NUL"
#  else  /* O_BINARY && !__DJGPP__ */
#   define HAVE_LONG_FILENAMES(dir)  (0)
#   define NULL_DEVICE	"NUL"
#  endif /* O_BINARY && !__DJGPP__ */
#  define SET_SCREEN_SIZE_HELPER terminal_prep_terminal()
#  define DEFAULT_INFO_PRINT_COMMAND ">PRN"
# else   /* O_BINARY && !__MSDOS__ */
#  define setmode(f,m)  _setmode(f,m)
#  define HAVE_LONG_FILENAMES(dir)   (1)
#  define NULL_DEVICE	"NUL"
# endif  /* O_BINARY && !__MSDOS__ */
# ifdef __CYGWIN__
#  define DEFAULT_TMPDIR	"/tmp/"
#  define PATH_SEP	":"
# else  /* O_BINARY && !__CYGWIN__ */
#  define DEFAULT_TMPDIR	"c:/"
#  define PATH_SEP	";"
# endif /* O_BINARY && !__CYGWIN__ */
  /* Back to any O_BINARY system.  */
# define FILENAME_CMP	strcasecmp
# define FILENAME_CMPN	strncasecmp
# define FOPEN_RBIN	"rb"
# define FOPEN_WBIN	"wb"
# define HAVE_DRIVE(n)	((n)[0] && (n)[1] == ':')
# define IS_SLASH(c)	((c) == '/' || (c) == '\\')
# define IS_ABSOLUTE(n)	(IS_SLASH((n)[0]) || ((n)[0] && (n)[1] == ':'))
# define PIPE_USE_FORK	0
# define SET_BINARY(f)  do {if (!isatty(f)) setmode(f,O_BINARY);} while(0)
# define STRIP_DOT_EXE	1

#else  /* not O_BINARY, i.e., Unix */
# define SET_BINARY(f)	(void)0
# define FOPEN_RBIN	"r"
# define FOPEN_WBIN	"w"
# define IS_SLASH(c)	((c) == '/')
# define HAVE_DRIVE(n)	(0)
# define IS_ABSOLUTE(n)	((n)[0] == '/')
# define FILENAME_CMP	strcmp
# define FILENAME_CMPN	strncmp
# define HAVE_LONG_FILENAMES(dir)   (1)
# define PATH_SEP	":"
# define STRIP_DOT_EXE	0
# ifdef VMS
#  define DEFAULT_TMPDIR "sys$scratch:"
# else
#  define DEFAULT_TMPDIR "/tmp/"
# endif
# define NULL_DEVICE	"/dev/null"
# define PIPE_USE_FORK	1
#endif /* not O_BINARY */

/* Everything but DJGPP.  */
#ifndef ALSO_NULL_DEVICE
# define ALSO_NULL_DEVICE  ""
#endif

#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
/* Some systems don't declare this function in pwd.h. */
struct passwd *getpwnam (const char *name);

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

/* Our library routines not included in any system library.  */
extern void *xmalloc (size_t), *xrealloc (void *, size_t);
extern char *xstrdup (const char *);
extern void xexit (int);

/* For convenience.  */
#define STREQ(s1,s2) (strcmp (s1, s2) == 0)
#define STRCASEEQ(s1,s2) (strcasecmp (s1, s2) == 0)
#define STRNCASEEQ(s1,s2,n) (strncasecmp (s1, s2, n) == 0)

/* We don't need anything fancy.  If we did need something fancy, gnulib
   has it.  */
#ifdef MIN
#undef MIN
#endif
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#ifdef MAX
#undef MAX
#endif
#define MAX(a,b) ((a) > (b) ? (a) : (b))

#endif /* TEXINFO_SYSTEM_H */
