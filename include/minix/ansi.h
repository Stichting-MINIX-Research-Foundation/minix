#ifndef _MINIX_ANSI_H
#define _MINIX_ANSI_H

#if __STDC__ == 1
#define _ANSI		31459	/* compiler claims full ANSI conformance */
#endif

#ifdef __GNUC__
#define _ANSI		31459	/* gcc conforms enough even in non-ANSI mode */
#endif

#if defined(__GNUC__) && !defined(__STRICT_ANSI__) || __STDC_VERSION__ >= 199901
#define __LONG_LONG_SUPPORTED 1
#endif

/* Setting of _POSIX_SOURCE (or _NETBSD_SOURCE) in NBSD headers is 
 * done in <sys/featuretest.h> */
#include <sys/featuretest.h>

#endif /* _MINIX_ANSI_H */
