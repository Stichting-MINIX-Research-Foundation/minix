/* The <stddef.h> header defines certain commonly used macros. */

#ifndef _STDDEF_H
#define _STDDEF_H

#include <sys/null.h>

/* The following is not portable, but the compiler accepts it. */
#ifdef __GNUC__
#define offsetof(type, ident)	__builtin_offsetof (type, ident)
#else
#define offsetof(type, ident)	((size_t) (unsigned long) &((type *)0)->ident)
#endif

#if _EM_PSIZE == _EM_WSIZE
typedef int ptrdiff_t;		/* result of subtracting two pointers */
#else	/* _EM_PSIZE == _EM_LSIZE */
typedef long ptrdiff_t;
#endif

#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned int size_t;	/* type returned by sizeof */
#endif

#ifndef _WCHAR_T
#define _WCHAR_T
typedef char wchar_t;		/* type expanded character set */
#endif

#endif /* _STDDEF_H */
