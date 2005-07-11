/* The <math.h> header contains prototypes for mathematical functions. */

#ifndef _MATH_H
#define _MATH_H

#ifndef _ANSI_H
#include <ansi.h>
#endif

#define HUGE_VAL	(__huge_val())		/* may be infinity */

/* Function Prototypes. */
_PROTOTYPE( double __huge_val,	(void)					);
_PROTOTYPE( int __IsNan,	(double _x)				);

_PROTOTYPE( double acos,  (double _x)					);
_PROTOTYPE( double asin,  (double _x)					);
_PROTOTYPE( double atan,  (double _x)					);
_PROTOTYPE( double atan2, (double _y, double _x)			);
_PROTOTYPE( double ceil,  (double _x)					);
_PROTOTYPE( double cos,   (double _x)					);
_PROTOTYPE( double cosh,  (double _x)					);
_PROTOTYPE( double exp,   (double _x)					);
_PROTOTYPE( double fabs,  (double _x)					);
_PROTOTYPE( double floor, (double _x)					);
_PROTOTYPE( double fmod,  (double _x, double _y)			);
_PROTOTYPE( double frexp, (double _x, int *_exp)			);
_PROTOTYPE( double ldexp, (double _x, int _exp)				);
_PROTOTYPE( double log,   (double _x)					);
_PROTOTYPE( double log10, (double _x)					);
_PROTOTYPE( double modf,  (double _x, double *_iptr)			);
_PROTOTYPE( double pow,   (double _x, double _y)			);
_PROTOTYPE( double sin,   (double _x)					);
_PROTOTYPE( double sinh,  (double _x)					);
_PROTOTYPE( double sqrt,  (double _x)					);
_PROTOTYPE( double tan,   (double _x)					);
_PROTOTYPE( double tanh,  (double _x)					);

#ifdef _POSIX_SOURCE	/* STD-C? */
#include <mathconst.h>
#endif

#endif /* _MATH_H */
