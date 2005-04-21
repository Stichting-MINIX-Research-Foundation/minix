/* config.h.  Generated automatically by configure.  */
/* config.h.in.  Generated automatically from configure.in by autoheader.  */

/* Define if on AIX 3.
   System headers sometimes define this.
   We just want to avoid a redefinition error message.  */
#ifndef _ALL_SOURCE
/* #undef _ALL_SOURCE */
#endif

/* Define to empty if the keyword does not work.  */
/* #undef const */

/* Define if you have <sys/wait.h> that is POSIX.1 compatible.  */
#define HAVE_SYS_WAIT_H 1

/* Define as __inline if that's what the C compiler calls it.  */
#define inline 

/* Define if on MINIX.  */
#define _MINIX 1

/* Define if the system does not provide POSIX.1 features except
   with this defined.  */
#define _POSIX_1_SOURCE 2

/* Define if you need to in order for stat and other things to work.  */
#define _POSIX_SOURCE 1

/* Define as the return type of signal handlers (int or void).  */
#define RETSIGTYPE void

/* Define if the `setpgrp' function takes no argument.  */
#define SETPGRP_VOID 1

/* Define to `unsigned' if <sys/types.h> doesn't define.  */
/* #undef size_t */

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1

/* Define if you can safely include both <sys/time.h> and <time.h>.  */
/* #undef TIME_WITH_SYS_TIME */

/* Define if your <sys/time.h> declares struct tm.  */
/* #undef TM_IN_SYS_TIME */

/* Define if the X Window System is missing or not being used.  */
#define X_DISPLAY_MISSING 1

/* Define this if you want to use Xdf */
#define USE_XDF 1

/* Define this if you use mtools together with Solaris' vold */
/* #undef USING_VOLD */

/* Define this if you use mtools together with the new Solaris' vold
 * support */
/* #undef USING_NEW_VOLD */

/* Define for debugging messages */
/* #undef DEBUG */

/* Define on non Unix OS'es which don't have the concept of tty's */
/* #undef USE_RAWTERM */

/* Define when sys_errlist is defined in the standard include files */
/* #undef DECL_SYS_ERRLIST */

/* Define when you want to include floppyd support */
/* #undef USE_FLOPPYD */

/* Define when the compiler supports LOFF_T type */
/* #undef HAVE_LOFF_T */

/* Define when the compiler supports OFFSET_T type */
/* #undef HAVE_OFFSET_T */

/* Define when the compiler supports LONG_LONG type */
/* #undef HAVE_LONG_LONG */

/* Define when the system has a 64 bit off_t type */
/* #undef HAVE_OFF_T_64 */

/* Define when you have an LLSEEK prototype */
/* #undef HAVE_LLSEEK_PROTOTYPE */

/* Define if you have the atexit function.  */
#define HAVE_ATEXIT 1

/* Define if you have the basename function.  */
/* #undef HAVE_BASENAME */

/* Define if you have the fchdir function.  */
#ifdef __minix_vmd
#define HAVE_FCHDIR 1
#endif

/* Define if you have the flock function.  */
/* #undef HAVE_FLOCK */

/* Define if you have the getpass function.  */
#define HAVE_GETPASS 1

/* Define if you have the gettimeofday function.  */
#define HAVE_GETTIMEOFDAY 1

/* Define if you have the htons function.  */
/* #undef HAVE_HTONS */

/* Define if you have the llseek function.  */
/* #undef HAVE_LLSEEK */

/* Define if you have the lockf function.  */
#define HAVE_LOCKF 1

/* Define if you have the lseek64 function.  */
/* #undef HAVE_LSEEK64 */

/* Define if you have the media_oldaliases function.  */
/* #undef HAVE_MEDIA_OLDALIASES */

/* Define if you have the memcpy function.  */
#define HAVE_MEMCPY 1

/* Define if you have the memmove function.  */
#define HAVE_MEMMOVE 1

/* Define if you have the memset function.  */
#define HAVE_MEMSET 1

/* Define if you have the on_exit function.  */
/* #undef HAVE_ON_EXIT */

/* Define if you have the random function.  */
#define HAVE_RANDOM 1

/* Define if you have the seteuid function.  */
/* #undef HAVE_SETEUID */

/* Define if you have the setresuid function.  */
/* #undef HAVE_SETRESUID */

/* Define if you have the snprintf function.  */
#define HAVE_SNPRINTF 1

/* Define if you have the srandom function.  */
#define HAVE_SRANDOM 1

/* Define if you have the strcasecmp function.  */
#define HAVE_STRCASECMP 1

/* Define if you have the strchr function.  */
#define HAVE_STRCHR 1

/* Define if you have the strcspn function.  */
#define HAVE_STRCSPN 1

/* Define if you have the strdup function.  */
/* #undef HAVE_STRDUP */

/* Define if you have the strerror function.  */
#define HAVE_STRERROR 1

/* Define if you have the strncasecmp function.  */
#define HAVE_STRNCASECMP 1

/* Define if you have the strpbrk function.  */
#define HAVE_STRPBRK 1

/* Define if you have the strrchr function.  */
#define HAVE_STRRCHR 1

/* Define if you have the strspn function.  */
#define HAVE_STRSPN 1

/* Define if you have the strtol function.  */
#define HAVE_STRTOL 1

/* Define if you have the strtoul function.  */
#define HAVE_STRTOUL 1

/* Define if you have the tcflush function.  */
#define HAVE_TCFLUSH 1

/* Define if you have the tcsetattr function.  */
#define HAVE_TCSETATTR 1

/* Define if you have the tzset function.  */
#define HAVE_TZSET 1

/* Define if you have the utime function.  */
#define HAVE_UTIME 1

/* Define if you have the utimes function.  */
/* #undef HAVE_UTIMES */

/* Define if you have the <arpa/inet.h> header file.  */
/* #undef HAVE_ARPA_INET_H */

/* Define if you have the <fcntl.h> header file.  */
#define HAVE_FCNTL_H 1

/* Define if you have the <getopt.h> header file.  */
/* #undef HAVE_GETOPT_H */

/* Define if you have the <libc.h> header file.  */
/* #undef HAVE_LIBC_H */

/* Define if you have the <limits.h> header file.  */
#define HAVE_LIMITS_H 1

/* Define if you have the <linux/unistd.h> header file.  */
/* #undef HAVE_LINUX_UNISTD_H */

/* Define if you have the <malloc.h> header file.  */
/* #undef HAVE_MALLOC_H */

/* Define if you have the <memory.h> header file.  */
/* #undef HAVE_MEMORY_H */

/* Define if you have the <mntent.h> header file.  */
/* #undef HAVE_MNTENT_H */

/* Define if you have the <netdb.h> header file.  */
/* #undef HAVE_NETDB_H */

/* Define if you have the <netinet/in.h> header file.  */
/* #undef HAVE_NETINET_IN_H */

/* Define if you have the <sgtty.h> header file.  */
#define HAVE_SGTTY_H 1

/* Define if you have the <signal.h> header file.  */
#define HAVE_SIGNAL_H 1

/* Define if you have the <stdlib.h> header file.  */
#define HAVE_STDLIB_H 1

/* Define if you have the <string.h> header file.  */
#define HAVE_STRING_H 1

/* Define if you have the <strings.h> header file.  */
/* #undef HAVE_STRINGS_H */

/* Define if you have the <sys/file.h> header file.  */
/* #undef HAVE_SYS_FILE_H */

/* Define if you have the <sys/floppy.h> header file.  */
/* #undef HAVE_SYS_FLOPPY_H */

/* Define if you have the <sys/ioctl.h> header file.  */
#define HAVE_SYS_IOCTL_H 1

/* Define if you have the <sys/param.h> header file.  */
/* #undef HAVE_SYS_PARAM_H */

/* Define if you have the <sys/signal.h> header file.  */
/* #undef HAVE_SYS_SIGNAL_H */

/* Define if you have the <sys/socket.h> header file.  */
/* #undef HAVE_SYS_SOCKET_H */

/* Define if you have the <sys/stat.h> header file.  */
#define HAVE_SYS_STAT_H 1

/* Define if you have the <sys/sysmacros.h> header file.  */
/* #undef HAVE_SYS_SYSMACROS_H */

/* Define if you have the <sys/termio.h> header file.  */
/* #undef HAVE_SYS_TERMIO_H */

/* Define if you have the <sys/termios.h> header file.  */
/* #undef HAVE_SYS_TERMIOS_H */

/* Define if you have the <sys/time.h> header file.  */
/* #undef HAVE_SYS_TIME_H */

/* Define if you have the <termio.h> header file.  */
/* #undef HAVE_TERMIO_H */

/* Define if you have the <termios.h> header file.  */
#define HAVE_TERMIOS_H 1

/* Define if you have the <unistd.h> header file.  */
#define HAVE_UNISTD_H 1

/* Define if you have the <utime.h> header file.  */
#define HAVE_UTIME_H 1

/* Define if you have the cam library (-lcam).  */
/* #undef HAVE_LIBCAM */

/* Define if you have the nsl library (-lnsl).  */
/* #undef HAVE_LIBNSL */

/* Define if you have the socket library (-lsocket).  */
/* #undef HAVE_LIBSOCKET */

/* Define if you have the sun library (-lsun).  */
/* #undef HAVE_LIBSUN */
