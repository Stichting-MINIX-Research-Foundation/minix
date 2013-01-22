/*	$NetBSD: acconfig.h,v 1.1.1.2 2008/05/18 14:29:55 aymeric Exp $ */

/* Id: acconfig.h,v 8.34 2002/01/19 23:01:35 skimo Exp (Berkeley) Date: 2002/01/19 23:01:35 */

/* Define to `int' if <sys/types.h> doesn't define.  */
#undef ssize_t

/* Define if you want a debugging version. */
#undef DEBUG

/* Define if you have a System V-style (broken) gettimeofday. */
#undef HAVE_BROKEN_GETTIMEOFDAY

/* Define if you have a Ultrix-style (broken) vdisable. */
#undef HAVE_BROKEN_VDISABLE

/* Define if you have a BSD version of curses. */
#undef HAVE_BSD_CURSES

/* Define if you have wide ncurses(3). */
#undef HAVE_NCURSESW

/* Define if you have the curses(3) waddnwstr function. */
#undef HAVE_CURSES_ADDNWSTR

/* Define if you have the curses(3) waddnstr function. */
#undef HAVE_CURSES_WADDNSTR

/* Define if you have the curses(3) beep function. */
#undef HAVE_CURSES_BEEP

/* Define if you have the curses(3) flash function. */
#undef HAVE_CURSES_FLASH

/* Define if you have the curses(3) idlok function. */
#undef HAVE_CURSES_IDLOK

/* Define if you have the curses(3) keypad function. */
#undef HAVE_CURSES_KEYPAD

/* Define if you have the curses(3) newterm function. */
#undef HAVE_CURSES_NEWTERM

/* Define if you have the curses(3) setupterm function. */
#undef HAVE_CURSES_SETUPTERM

/* Define if you have the curses(3) tigetstr/tigetnum functions. */
#undef HAVE_CURSES_TIGETSTR

/* Define if you have the DB __hash_open call in the C library. */
#undef HAVE_DB_HASH_OPEN

/* Define if you have the chsize(2) system call. */
#undef HAVE_FTRUNCATE_CHSIZE

/* Define if you have the ftruncate(2) system call. */
#undef HAVE_FTRUNCATE_FTRUNCATE

/* Define if you have gcc. */
#undef HAVE_GCC

/* Define if you have fcntl(2) style locking. */
#undef HAVE_LOCK_FCNTL

/* Define if you have flock(2) style locking. */
#undef HAVE_LOCK_FLOCK

/* Define is appropriate pango is available */
#undef HAVE_PANGO

/* Define if you want to compile in the Perl interpreter. */
#undef HAVE_PERL_INTERP

/* Define if using pthread. */
#undef HAVE_PTHREAD

/* Define if you have the Berkeley style revoke(2) system call. */
#undef HAVE_REVOKE

/* Define if you have the Berkeley style strsep(3) function. */
#undef HAVE_STRSEP

/* Define if you have <sys/mman.h> */
#undef HAVE_SYS_MMAN_H

/* Define if you have <sys/select.h> */
#undef HAVE_SYS_SELECT_H

/* Define if you have the System V style pty calls. */
#undef HAVE_SYS5_PTY

/* Define if you want to compile in the Tcl interpreter. */
#undef HAVE_TCL_INTERP

/* Define is appropriate zvt is available */
#undef HAVE_ZVT

/* Define if your sprintf returns a pointer, not a length. */
#undef SPRINTF_RET_CHARPNT

/* Define when using db1 */
#undef USE_DB1

/* Define when using db4 logging */
#undef USE_DB4_LOGGING

/* Define when dynamically loading DB 3 */
#undef USE_DYNAMIC_LOADING

/* Define when iconv can be used */
#undef USE_ICONV

/* Define when perl's setenv should be used */
#undef USE_PERL_SETENV

/* Define when using S-Lang */
#undef USE_SLANG_CURSES

/* Define when using wide characters */
#undef USE_WIDECHAR

/* Define if you have <ncurses.h> */
#undef HAVE_NCURSES_H

/* Define when fprintf prototype not in an obvious place */
#undef NEED_FPRINTF_PROTO
