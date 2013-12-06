/* $NetBSD: namespace.h,v 1.10 2013/11/19 19:24:34 joerg Exp $ */

#define atan2 _atan2
#define atan2f _atan2f
#define atan2l _atan2l
#define hypot _hypot
#define hypotf _hypotf
#define hypotl _hypotl

#define exp _exp
#define expf _expf
#define expl _expl
#define log _log
#define logf _logf
#define logl _logl

#if 0 /* not yet - need to review use in machdep code first */
#define sin _sin
#define sinf _sinf
#define cos _cos
#define cosf _cosf
#define finite _finite
#define finitef _finitef
#endif /* notyet */
#define sinh _sinh
#define sinhf _sinhf
#define sinhl _sinhl
#define cosh _cosh
#define coshf _coshf
#define coshl _coshl
#define asin _asin
#define asinf _asinf
#define asinl _asinl

#define casin _casin
#define casinf _casinf
#define casinl _casinl
#define catan _catan
#define catanf _catanf
#define catanl _catanl

#define scalbn _scalbn
#define scalbnf _scalbnf
#define scalbnl _scalbnl
#define scalbln _scalbln
#define scalblnf _scalblnf
#define scalblnl _scalblnl

#define sqrtl _sqrtl
#define cbrtl _cbrtl
#define ceill _ceill
#define floorl _floorl
#define roundl _roundl
#define fmodl _fmodl
#define truncl _truncl

#define exp2l _exp2l
#define cosl _cosl
#define sinl _sinl
#define tanl _tanl
#define powl _powl
#define coshl _coshl
#define sinhl _sinhl
#define acosl _acosl
#define atanl _atanl
#define asinhl _asinhl
#define acoshl _acoshl
#define tanhl _tanhl
#define atanhl _atanhl
#define log10l _log10l
