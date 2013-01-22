/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.in by autoheader.  */
/* Id: acconfig.h,v 8.34 2002/01/19 23:01:35 skimo Exp  (Berkeley) Date: 2002/01/19 23:01:35  */

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef ssize_t */

/* Define if you want a debugging version. */
/* #undef DEBUG */

/* Define if you have a System V-style (broken) gettimeofday. */
/* #undef HAVE_BROKEN_GETTIMEOFDAY */

/* Define if you have a Ultrix-style (broken) vdisable. */
/* #undef HAVE_BROKEN_VDISABLE */

/* Define if you have a BSD version of curses. */
/* #undef HAVE_BSD_CURSES */

/* Define if you have wide ncurses(3). */
/* #undef HAVE_NCURSESW */

/* Define if you have the curses(3) waddnwstr function. */
/* #undef HAVE_CURSES_ADDNWSTR */

/* Define if you have the curses(3) waddnstr function. */
#define HAVE_CURSES_WADDNSTR 1

/* Define if you have the curses(3) beep function. */
#define HAVE_CURSES_BEEP 1

/* Define if you have the curses(3) flash function. */
#define HAVE_CURSES_FLASH 1

/* Define if you have the curses(3) idlok function. */
#define HAVE_CURSES_IDLOK 1

/* Define if you have the curses(3) keypad function. */
#define HAVE_CURSES_KEYPAD 1

/* Define if you have the curses(3) newterm function. */
#define HAVE_CURSES_NEWTERM 1

/* Define if you have the curses(3) setupterm function. */
#define HAVE_CURSES_SETUPTERM 1

/* Define if you have the curses(3) tigetstr/tigetnum functions. */
#define HAVE_CURSES_TIGETSTR 1

/* Define if you have the DB __hash_open call in the C library. */
/* #undef HAVE_DB_HASH_OPEN */

/* Define if you have the chsize(2) system call. */
/* #undef HAVE_FTRUNCATE_CHSIZE */

/* Define if you have the ftruncate(2) system call. */
#define HAVE_FTRUNCATE_FTRUNCATE 1

/* Define if you have gcc. */
#define HAVE_GCC 1

/* Define if you have fcntl(2) style locking. */
/* #undef HAVE_LOCK_FCNTL */

/* Define if you have flock(2) style locking. */
#define HAVE_LOCK_FLOCK 1

/* Define is appropriate pango is available */
/* #undef HAVE_PANGO */

/* Define if you want to compile in the Perl interpreter. */
/* #undef HAVE_PERL_INTERP */

/* Define if using pthread. */
/* #undef HAVE_PTHREAD */

#ifndef __minix
/* Define if you have the Berkeley style revoke(2) system call. */
#define HAVE_REVOKE 1
#endif

/* Define if you have the Berkeley style strsep(3) function. */
#define HAVE_STRSEP 1

/* Define if you have <sys/mman.h> */
#define HAVE_SYS_MMAN_H 1

/* Define if you have <sys/select.h> */
#define HAVE_SYS_SELECT_H 1

#ifndef __minix
/* Define if you have the System V style pty calls. */
#define HAVE_SYS5_PTY 1
#endif

/* Define if you want to compile in the Tcl interpreter. */
/* #undef HAVE_TCL_INTERP */

/* Define is appropriate zvt is available */
/* #undef HAVE_ZVT */

/* Define if your sprintf returns a pointer, not a length. */
/* #undef SPRINTF_RET_CHARPNT */

/* Define when using db1 */
#define USE_DB1 1

/* Define when using db4 logging */
/* #undef USE_DB4_LOGGING */

/* Define when dynamically loading DB 3 */
/* #undef USE_DYNAMIC_LOADING */

/* Define when iconv can be used */
#define USE_ICONV 1

/* Define when perl's setenv should be used */
/* #undef USE_PERL_SETENV */

/* Define when using S-Lang */
/* #undef USE_SLANG_CURSES */

/* Define when using wide characters */
/* #undef USE_WIDECHAR */

/* Define if you have <ncurses.h> */
/* #undef HAVE_NCURSES_H */

/* Define when fprintf prototype not in an obvious place */
/* #undef NEED_FPRINTF_PROTO */

/* Define to 1 if you have the `bsearch' function. */
#define HAVE_BSEARCH 1

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if you have the `fork' function. */
#define HAVE_FORK 1

/* Define to 1 if you have the `gethostname' function. */
#define HAVE_GETHOSTNAME 1

/* Define to 1 if you have the `getpagesize' function. */
#define HAVE_GETPAGESIZE 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the `iswblank' function. */
#define HAVE_ISWBLANK 1

/* Define to 1 if you have the `memcpy' function. */
#define HAVE_MEMCPY 1

/* Define to 1 if you have the `memchr' function. */
#define HAVE_MEMCHR 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the `memset' function. */
#define HAVE_MEMSET 1

/* Define to 1 if you have the `mkstemp' function. */
#define HAVE_MKSTEMP 1

/* Define to 1 if you have a working `mmap' system call. */
#define HAVE_MMAP 1

/* Define to 1 if you have the <ncursesw/ncurses.h> header file. */
/* #undef HAVE_NCURSESW_NCURSES_H */

/* Define to 1 if you have the <ncurses.h> header file. */
/* #undef HAVE_NCURSES_H */

/* Define to 1 if you have the `select' function. */
#define HAVE_SELECT 1

/* Define to 1 if you have the `setenv' function. */
#define HAVE_SETENV 1

/* Define to 1 if you have the `snprintf' function. */
#define HAVE_SNPRINTF 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `strdup' function. */
#define HAVE_STRDUP 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strpbrk' function. */
#define HAVE_STRPBRK 1

/* Define to 1 if you have the `strsep' function. */
#define HAVE_STRSEP 1

/* Define to 1 if `st_blksize' is member of `struct stat'. */
#define HAVE_STRUCT_STAT_ST_BLKSIZE 1

/* Define to 1 if your `struct stat' has `st_blksize'. Deprecated, use
   `HAVE_STRUCT_STAT_ST_BLKSIZE' instead. */
#define HAVE_ST_BLKSIZE 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the `unsetenv' function. */
#define HAVE_UNSETENV 1

/* Define to 1 if you have the `vfork' function. */
#define HAVE_VFORK 1

/* Define to 1 if you have the <vfork.h> header file. */
/* #undef HAVE_VFORK_H */

/* Define to 1 if you have the `vsnprintf' function. */
#define HAVE_VSNPRINTF 1

/* Define to 1 if `fork' works. */
#define HAVE_WORKING_FORK 1

/* Define to 1 if `vfork' works. */
#define HAVE_WORKING_VFORK 1

/* Name of package */
#define PACKAGE "vi"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME ""

/* Define to the full name and version of this package. */
#define PACKAGE_STRING ""

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME ""

/* Define to the version of this package. */
#define PACKAGE_VERSION ""

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define to 1 if your <sys/time.h> declares `struct tm'. */
/* #undef TM_IN_SYS_TIME */

/* Version number of package */
#define VERSION "1.81.6"

/* Define to 1 if your processor stores words with the most significant byte
   first (like Motorola and SPARC, unlike Intel and VAX). */
/* #undef WORDS_BIGENDIAN */

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define to `int' if <sys/types.h> does not define. */
/* #undef mode_t */

/* Define to `long int' if <sys/types.h> does not define. */
/* #undef off_t */

/* Define to `int' if <sys/types.h> does not define. */
/* #undef pid_t */

/* Define to `unsigned int' if <sys/types.h> does not define. */
/* #undef size_t */

/* Define to `int' if <sys/types.h> does not define. */
/* #undef ssize_t */

/* Define as `fork' if `vfork' does not work. */
/* #undef vfork */
