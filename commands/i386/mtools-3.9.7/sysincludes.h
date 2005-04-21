/* System includes for mtools */

#ifndef SYSINCLUDES_H
#define SYSINCLUDES_H

#include "config.h"

/* OS/2 needs __inline__, but for some reason is not autodetected */
#ifdef __EMX__
# ifndef inline
#  define inline __inline__
# endif
#endif

/***********************************************************************/
/*                                                                     */
/* OS dependancies which cannot be covered by the autoconfigure script */
/*                                                                     */
/***********************************************************************/


#ifdef OS_aux
/* A/UX needs POSIX_SOURCE, just as AIX does. Unlike SCO and AIX, it seems
 * to prefer TERMIO over TERMIOS */
#ifndef _POSIX_SOURCE
# define _POSIX_SOURCE
#endif
#ifndef POSIX_SOURCE
# define POSIX_SOURCE
#endif

#endif


/* On AIX, we have to prefer strings.h, as string.h lacks a prototype 
 * for strcasecmp. On most other architectures, it's string.h which seems
 * to be more complete */
#if (defined OS_aix && defined HAVE_STRINGS_H)
# undef HAVE_STRING_H
#endif


#ifdef OS_ultrix
/* on ultrix, if termios present, prefer it instead of termio */
# ifdef HAVE_TERMIOS_H
#  undef HAVE_TERMIO_H
# endif
#endif

#ifdef OS_linux_gnu
/* RMS strikes again */
# ifndef OS_linux
#  define OS_linux
# endif
#endif

#ifdef OS_Minix
typedef unsigned char *caddr_t;
#endif


/***********************************************************************/
/*                                                                     */
/* Compiler dependancies                                               */
/*                                                                     */
/***********************************************************************/


#if defined __GNUC__ && defined __STDC__
/* gcc -traditional doesn't have PACKED, UNUSED and NORETURN */
# define PACKED __attribute__ ((packed))
# if __GNUC__ == 2 && __GNUC_MINOR__ > 6 || __GNUC__ >= 3
/* gcc 2.6.3 doesn't have "unused" */		/* mool */
#  define UNUSED(x) x __attribute__ ((unused));x
# else
#  define UNUSED(x) x
# endif
# define NORETURN __attribute__ ((noreturn))
#else
# define UNUSED(x) x
# define PACKED /* */
# define NORETURN /* */
#endif


/***********************************************************************/
/*                                                                     */
/* Include files                                                       */
/*                                                                     */
/***********************************************************************/


#include <sys/types.h>

#ifdef OS_Minix
typedef unsigned long uoff_t;
#define off_t uoff_t
#endif

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#include <stdio.h>

#ifndef OS_Minix
# include <ctype.h>
#else
# ifdef __minix_vmd
#  include <bsd/asciictype.h>	/* Minix-vmd: Ignore locales on purpose. */
# else
#  include <ctype.h>		/* Minix: What's that "locale" thing? */
# endif
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_LINUX_UNISTD_H
# include <linux/unistd.h>
#endif

#ifdef HAVE_LIBC_H
# include <libc.h>
#endif

#ifdef HAVE_GETOPT_H
# include <getopt.h>
#else
# ifndef OS_Minix
int getopt();
extern char *optarg;
extern int optind, opterr;
# endif
#endif

#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif

#ifdef HAVE_LIMITS_H
# include <limits.h>
#endif

#ifdef HAVE_SYS_FILE_H
# include <sys/file.h>
#endif

#ifdef HAVE_SYS_IOCTL_H
# ifndef sunos
# include <sys/ioctl.h>
#endif
#endif
/* if we don't have sys/ioctl.h, we rely on unistd to supply a prototype
 * for it. If it doesn't, we'll only get a (harmless) warning. The idea
 * is to get mtools compile on as many platforms as possible, but to not
 * suppress warnings if the platform is broken, as long as these warnings do
 * not prevent compilation */

#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#ifndef NO_TERMIO
# ifdef HAVE_TERMIO_H
#  include <termio.h>
# elif defined HAVE_SYS_TERMIO_H
#  include <sys/termio.h>
# endif
# if !defined OS_ultrix || !(defined HAVE_TERMIO_H || defined HAVE_TERMIO_H)
/* on Ultrix, avoid double inclusion of both termio and termios */
#  ifdef HAVE_TERMIOS_H
#   include <termios.h>
#  elif defined HAVE_SYS_TERMIOS_H
#   include <sys/termios.h>
#  endif
# endif
# ifdef HAVE_STTY_H
#  include <sgtty.h>
# endif
#endif


#if defined(OS_aux) && !defined(_SYSV_SOURCE)
/* compiled in POSIX mode, this is left out unless SYSV */
#define	NCC	8
struct termio {
	unsigned short	c_iflag;	/* input modes */
	unsigned short	c_oflag;	/* output modes */
	unsigned short	c_cflag;	/* control modes */
	unsigned short	c_lflag;	/* line discipline modes */
	char	c_line;			/* line discipline */
	unsigned char	c_cc[NCC];	/* control chars */
};
extern int ioctl(int fildes, int request, void *arg);
#endif


#ifdef HAVE_MNTENT_H
# include <mntent.h>
#endif

#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif

/* Can only be done here, as BSD is defined in sys/param.h :-( */
#if defined BSD || defined __BEOS__
/* on BSD and on BEOS, we prefer gettimeofday, ... */
# ifdef HAVE_GETTIMEOFDAY
#  undef HAVE_TZSET
# endif
#else /* BSD */
/* ... elsewhere we prefer tzset */
# ifdef HAVE_TZSET
#  undef HAVE_GETTIMEOFDAY
# endif
#endif


#include <sys/stat.h>

#include <errno.h>
extern int errno;

#include <pwd.h>


#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif

#ifdef HAVE_MEMORY_H
# include <memory.h>
#endif

#ifdef HAVE_MALLOC_H
# include <malloc.h>
#endif

#ifdef HAVE_SIGNAL_H
# include <signal.h>
#else
# ifdef HAVE_SYS_SIGNAL_H
#  include <sys/signal.h>
# endif
#endif

#ifdef HAVE_UTIME_H
# include <utime.h>
#endif

#ifdef HAVE_SYS_WAIT_H
# ifndef DONT_NEED_WAIT
#  include <sys/wait.h>
# endif
#endif


#ifdef USE_FLOPPYD

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

#ifdef HAVE_X11_XAUTH_H
#include <X11/Xauth.h>
#endif

#ifdef HAVE_X11_XLIB_H
#include <X11/Xlib.h>
#endif

#endif

#ifndef INADDR_NONE
#define INADDR_NONE (-1)
#endif


#ifdef sgi
#define MSGIHACK __EXTENSIONS__
#undef __EXTENSIONS__
#endif
#include <math.h>
#ifdef sgi
#define __EXTENSIONS__ MSGIHACK
#undef MSGIHACK
#endif

/* missing functions */
#ifndef HAVE_SRANDOM
# define srandom srand48
#endif

#ifndef HAVE_RANDOM
# define random (long)lrand48
#endif

#if __minix && !__minix_vmd
# define srandom srand
# define random rand
#endif

#ifndef HAVE_STRCHR
# define strchr index
#endif

#ifndef HAVE_STRRCHR
# define strrchr rindex
#endif


#define SIG_CAST RETSIGTYPE(*)()

#ifndef HAVE_STRDUP
extern char *strdup(const char *str);
#endif /* HAVE_STRDUP */


#ifndef HAVE_MEMCPY
extern char *memcpy(char *s1, const char *s2, size_t n);
#endif

#ifndef HAVE_MEMSET
extern char *memset(char *s, char c, size_t n);
#endif /* HAVE_MEMSET */


#ifndef HAVE_STRPBRK
extern char *strpbrk(const char *string, const char *brkset);
#endif /* HAVE_STRPBRK */


#ifndef HAVE_STRTOUL
unsigned long strtoul(const char *string, char **eptr, int base);
#endif /* HAVE_STRTOUL */

#ifndef HAVE_STRSPN
size_t strspn(const char *s, const char *accept);
#endif /* HAVE_STRSPN */

#ifndef HAVE_STRCSPN
size_t strcspn(const char *s, const char *reject);
#endif /* HAVE_STRCSPN */

#ifndef HAVE_STRERROR
char *strerror(int errno);
#endif

#ifndef HAVE_ATEXIT
int atexit(void (*function)(void)); 

#ifndef HAVE_ON_EXIT
void myexit(int code) NORETURN;
#define exit myexit
#endif

#endif


#ifndef HAVE_MEMMOVE
# define memmove(DST, SRC, N) bcopy(SRC, DST, N)
#endif

#ifndef HAVE_STRCASECMP
int strcasecmp(const char *s1, const char *s2);
#endif

#ifndef HAVE_STRNCASECMP
int strncasecmp(const char *s1, const char *s2, size_t n);
#endif

#ifndef HAVE_GETPASS
char *getpass(const char *prompt);
#endif

#if 0
#ifndef HAVE_BASENAME
const char *basename(const char *filename);
#endif
#endif

const char *_basename(const char *filename);

#ifndef __STDC__
# ifndef signed
#  define signed /**/
# endif 
#endif /* !__STDC__ */



/***************************************************************************/
/*                                                                         */
/* Prototypes for systems where the functions exist but not the prototypes */
/*                                                                         */
/***************************************************************************/



/* prototypes which might be missing on some platforms, even if the functions
 * are present.  Do not declare argument types, in order to avoid conflict
 * on platforms where the prototypes _are_ correct.  Indeed, for most of
 * these, there are _several_ "correct" parameter definitions, and not all
 * platforms use the same.  For instance, some use the const attribute for
 * strings not modified by the function, and others do not.  By using just
 * the return type, which rarely changes, we avoid these problems.
 */

/* Correction:  Now it seems that even return values are not standardized :-(
  For instance  DEC-ALPHA, OSF/1 3.2d uses ssize_t as a return type for read
  and write.  NextStep uses a non-void return value for exit, etc.  With the
  advent of 64 bit system, we'll expect more of these problems in the future.
  Better uncomment the lot, except on SunOS, which is known to have bad
  incomplete files.  Add other OS'es with incomplete include files as needed
  */
#if (defined OS_sunos || defined OS_ultrix)
int read();
int write();
int fflush();
char *strdup();
int strcasecmp();
int strncasecmp();
char *getenv();
unsigned long strtoul();
int pclose();
void exit();
char *getpass();
int atoi();
FILE *fdopen();
FILE *popen();
#endif

#ifndef MAXPATHLEN
# ifdef PATH_MAX
#  define MAXPATHLEN PATH_MAX
# else
#  define MAXPATHLEN 1024
# endif
#endif


#ifndef OS_linux
# undef USE_XDF
#endif

#ifdef NO_XDF
# undef USE_XDF
#endif

#ifdef __EMX__
#define INCL_BASE
#define INCL_DOSDEVIOCTL
#include <os2.h>
#endif

#ifdef OS_nextstep
/* nextstep doesn't have this.  Unfortunately, we cannot test its presence
   using AC_EGREP_HEADER, as we don't know _which_ header to test, and in
   the general case utime.h might be non-existent */
struct utimbuf
{
  time_t actime,modtime;
};
#endif

/* NeXTStep doesn't have these */
#if !defined(S_ISREG) && defined (_S_IFMT) && defined (_S_IFREG)
#define S_ISREG(mode)   (((mode) & (_S_IFMT)) == (_S_IFREG))
#endif

#if !defined(S_ISDIR) && defined (_S_IFMT) && defined (_S_IFDIR)
#define S_ISDIR(mode)   (((mode) & (_S_IFMT)) == (_S_IFDIR))
#endif


#if 0

#define malloc(x) mymalloc(x)
#define calloc(x,y) mycalloc(x,y)
#define free(x) myfree(x)
#define realloc(x,y) myrealloc(x,y)
#define strdup(a) mystrdup(a)

void *mycalloc(size_t nmemb, size_t size);
void *mymalloc(size_t size);
void myfree(void *ptr);
void *myrealloc(void *ptr, size_t size);
char *mystrdup(char *a);

#endif

#endif
