/* The <stddef.h> header defines certain commonly used macros. */

#ifndef _STDDEF_H
#define _STDDEF_H

#define NULL   ((void *)0)

/* The following is not portable, but the compiler accepts it. */
#define offsetof(type, ident)	((size_t) (unsigned long) &((type *)0)->ident)

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
