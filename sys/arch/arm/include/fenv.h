/*	$NetBSD: fenv.h,v 1.1 2013/04/23 05:41:12 matt Exp $	*/

/* 
 * Based on ieeefp.h written by J.T. Conklin, Apr 28, 1995
 * Public domain.
 */

#ifndef _ARM_FENV_H_
#define _ARM_FENV_H_

typedef int fenv_t;
typedef int fexcept_t;

#define	FE_INVALID	0x01	/* invalid operation exception */
#define	FE_DIVBYZERO	0x02	/* divide-by-zero exception */
#define	FE_OVERFLOW	0x04	/* overflow exception */
#define	FE_UNDERFLOW	0x08	/* underflow exception */
#define	FE_INEXACT	0x10	/* imprecise (loss of precision; "inexact") */

#define	FE_ALL_EXCEPT	0x1f

#define	FE_TONEAREST	0	/* round to nearest representable number */
#define	FE_UPWARD	1	/* round toward positive infinity */
#define	FE_DOWNWARD	2	/* round toward negative infinity */
#define	FE_TOWARDZERO	3	/* round to zero (truncate) */

__BEGIN_DECLS

/* Default floating-point environment */
extern const fenv_t	__fe_dfl_env;
#define FE_DFL_ENV	(&__fe_dfl_env)

__END_DECLS

#endif /* _ARM_FENV_H_ */
