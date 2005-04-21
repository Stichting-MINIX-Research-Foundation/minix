/*	Replacement for something BSD has in sys/cdefs.h. */

#ifndef _ASH_SYS_CDEFS
#define _ASH_SYS_CDEFS

#if __STDC__
#define	__P(params)			params
#else
#define	__P(params)			()
#endif

/*	Probably in sys/types.h. */
typedef void (*sig_t) __P(( int ));

#endif /* _ASH_SYS_CDEFS */
