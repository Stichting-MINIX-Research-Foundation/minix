/* The <math.h> header contains prototypes for mathematical functions. */

#ifndef _MATH_H
#define _MATH_H

#ifndef _ANSI_H
#include <ansi.h>
#endif

#define INFINITY	(__infinity())
#define NAN		(__qnan())
#define HUGE_VAL	INFINITY

/* Function Prototypes. */
_PROTOTYPE( double __infinity,	(void)					);
_PROTOTYPE( double __qnan,	(void)					);

_PROTOTYPE( double acos,  (double _x)					);
_PROTOTYPE( double asin,  (double _x)					);
_PROTOTYPE( double atan,  (double _x)					);
_PROTOTYPE( double atan2, (double _y, double _x)			);
_PROTOTYPE( double ceil,  (double _x)					);
_PROTOTYPE( double cos,   (double _x)					);
_PROTOTYPE( double cosh,  (double _x)					);
_PROTOTYPE( double exp,   (double _x)					);
_PROTOTYPE( double fabs,  (double _x)					);
_PROTOTYPE( float fabsf, (float _x)					);
_PROTOTYPE( double floor, (double _x)					);
_PROTOTYPE( double fmod,  (double _x, double _y)			);
_PROTOTYPE( double frexp, (double _x, int *_exp)			);
_PROTOTYPE( double ldexp, (double _x, int _exp)				);
_PROTOTYPE( double log,   (double _x)					);
_PROTOTYPE( double log10, (double _x)					);
_PROTOTYPE( double modf,  (double _x, double *_iptr)			);
_PROTOTYPE( double pow,   (double _x, double _y)			);
_PROTOTYPE( double rint,  (double _x)					);
_PROTOTYPE( double scalbn, (double _x, int _exp)			);
_PROTOTYPE( float scalbnf, (float _x, int _exp)				);
_PROTOTYPE( double scalbln, (double _x, long _exp)			);
_PROTOTYPE( float scalblnf, (float _x, long _exp)			);
_PROTOTYPE( double sin,   (double _x)					);
_PROTOTYPE( double sinh,  (double _x)					);
_PROTOTYPE( double sqrt,  (double _x)					);
_PROTOTYPE( double tan,   (double _x)					);
_PROTOTYPE( double tanh,  (double _x)					);
_PROTOTYPE( double hypot, (double _x, double _y)			);

#ifdef _POSIX_SOURCE	/* STD-C? */
#include <mathconst.h>

#define FP_INFINITE  1
#define FP_NAN       2
#define FP_NORMAL    3
#define FP_SUBNORMAL 4
#define FP_ZERO      5

_PROTOTYPE( int fpclassify,     (double x)				);
_PROTOTYPE( int isfinite,       (double x)				);
_PROTOTYPE( int isinf,          (double x)				);
_PROTOTYPE( int isnan,          (double x)				);
_PROTOTYPE( int isnormal,       (double x)				);
_PROTOTYPE( int signbit,        (double x)				);
_PROTOTYPE( int isgreater,      (double x, double y)	);
_PROTOTYPE( int isgreaterequal, (double x, double y)	);
_PROTOTYPE( int isless,         (double x, double y)	);
_PROTOTYPE( int islessequal,    (double x, double y)	);
_PROTOTYPE( int islessgreater,  (double x, double y)	);
_PROTOTYPE( int isunordered,    (double x, double y)	);
_PROTOTYPE( double nearbyint,   (double x)		);
_PROTOTYPE( double remainder,   (double x, double y)	);
_PROTOTYPE( double trunc,       (double x)		);
#endif

/* The following definitions are not implemented in the ACK math lib.
 * We allow them in the GNU compiler because they are builtins there.
 */
#ifdef __GNUC__
_PROTOTYPE( float powf,		(float, float)		);
_PROTOTYPE( float fmodf,	(float, float)		);
_PROTOTYPE( double erf,		(double)		);
_PROTOTYPE( double erfc,	(double)		);
_PROTOTYPE( double gamma,	(double)		);
_PROTOTYPE( double j0,		(double)		);
_PROTOTYPE( double j1,		(double)		);
_PROTOTYPE( double jn,		(int, double)		);
_PROTOTYPE( double lgamma,	(double)		);
_PROTOTYPE( double y0,		(double)		);
_PROTOTYPE( double y1,		(double)		);
_PROTOTYPE( double yn,		(int, double)		);
_PROTOTYPE( double acosh,	(double)		);
_PROTOTYPE( double asinh,	(double)		);
_PROTOTYPE( double atanh,	(double)		);
_PROTOTYPE( double cbrt,	(double)		);
_PROTOTYPE( double expm1,	(double)		);
_PROTOTYPE( int    ilogb,	(double)		);
_PROTOTYPE( double log1p,	(double)		);
_PROTOTYPE( double logb,	(double)		);
_PROTOTYPE( double nextafter,	(double, double)	);
_PROTOTYPE( double scalb,	(double, double)	);
#endif

#endif /* _MATH_H */
