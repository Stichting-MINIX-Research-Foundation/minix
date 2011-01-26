/* $NetBSD: complex.h,v 1.1 2007/08/20 16:01:29 drochner Exp $ */

#if __STDC_VERSION__ >= 199901L

#define complex _Complex
#define _Complex_I 1.0fi
#define I _Complex_I

#include <sys/cdefs.h>

__BEGIN_DECLS

double creal(double complex);
double cimag(double complex);
double complex conj(double complex);
float crealf(float complex);
float cimagf(float complex);
float complex conjf(float complex);

#ifndef __minix
#ifndef __LIBM0_SOURCE__
/* avoid conflict with historical cabs(struct complex) */
double cabs(double complex) __RENAME(__c99_cabs);
float cabsf(float complex) __RENAME(__c99_cabsf);
#endif
#endif
double carg(double complex);
float cargf(float complex);

double complex csqrt(double complex);
double complex cexp(double complex);
double complex clog(double complex);
double complex cpow(double complex, double complex);

double complex csin(double complex);
double complex ccos(double complex);
double complex ctan(double complex);
double complex csinh(double complex);
double complex ccosh(double complex);
double complex ctanh(double complex);

double complex casin(double complex);
double complex cacos(double complex);
double complex catan(double complex);
double complex casinh(double complex);
double complex cacosh(double complex);
double complex catanh(double complex);

float complex csqrtf(float complex);
float complex cexpf(float complex);
float complex clogf(float complex);
float complex cpowf(float complex, float complex);

float complex csinf(float complex);
float complex ccosf(float complex);
float complex ctanf(float complex);
float complex csinhf(float complex);
float complex ccoshf(float complex);
float complex ctanhf(float complex);

float complex casinf(float complex);
float complex cacosf(float complex);
float complex catanf(float complex);
float complex casinhf(float complex);
float complex cacoshf(float complex);
float complex catanhf(float complex);

__END_DECLS
#endif
