#include <assert.h>
#include <fenv.h>
#include <math.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/sigcontext.h>

#define MAX_ERROR 4
static int errct;

/* maximum allowed FP difference for our tests */ 
#define EPSILON 0.00000000023283064365386962890625 /* 2^(-32) */

static void quit(void)
{
	if (errct == 0) 
	{
		printf("ok\n");
		exit(0);
	} 
	else 
	{
		printf("%d errors\n", errct);
		exit(1);
	}
}

#define ERR(x, y) e(__LINE__, (x), (y))

static void e(int n, double x, double y)
{
	printf("Line %d, x=%.20g, y=%.20g\n", n, x, y);
	if (errct++ > MAX_ERROR) 
	{
		printf("Too many errors; test aborted\n");
		exit(1);
	}
}

static void signal_handler(int signum)
{
	struct sigframe *sigframe;
	
	/* report signal */
	sigframe = (struct sigframe *) ((char *) &signum - 
		(char *) &((struct sigframe *) NULL)->sf_signo);
	printf("Signal %d at 0x%x\n", signum, sigframe->sf_scp->sc_regs.pc);
	
	/* count as error */
	ERR(0, 0);
	fflush(stdout);
	
	/* handle signa again next time */
	signal(signum, signal_handler);
}

static void test_fpclassify(double value, int class, int test_sign)
{
	/* test fpclassify */
	if (fpclassify(value) != class) ERR(value, 0);
	if (test_sign) 
	{
		if (fpclassify(-value) != class) ERR(-value, 0);

		/* test signbit */
		if (signbit(value))   ERR(value, 0);
		if (!signbit(-value)) ERR(-value, 0);
	}
}

/* Maximum normal double: (2 - 2^(-53)) * 2^1023 */
#define NORMAL_DOUBLE_MAX 1.797693134862315708145274237317e+308

/* Minimum normal double: 2^(-1022) */
#define NORMAL_DOUBLE_MIN 2.2250738585072013830902327173324e-308

/* Maximum subnormal double: (1 - 2^(-53)) * 2^(-1022) */
#define SUBNORMAL_DOUBLE_MAX 2.2250738585072008890245868760859e-308

/* Minimum subnormal double: 2^(-52) * 2^(-1023) */
#define SUBNORMAL_DOUBLE_MIN 4.9406564584124654417656879286822e-324

static void test_fpclassify_values(void)
{
	double d;
	char negzero[] = { 0, 0, 0, 0, 0, 0, 0, 0x80 };

	/* test some corner cases for fpclassify and signbit */
	test_fpclassify(INFINITY,             FP_INFINITE,    1);
	test_fpclassify(NAN,                  FP_NAN,         0);
	test_fpclassify(0,                    FP_ZERO,        0);
	test_fpclassify(1,                    FP_NORMAL,      1);
	test_fpclassify(NORMAL_DOUBLE_MAX,    FP_NORMAL,      1);
	test_fpclassify(NORMAL_DOUBLE_MIN,    FP_NORMAL,      1);
	test_fpclassify(SUBNORMAL_DOUBLE_MAX, FP_SUBNORMAL,   1);
	test_fpclassify(SUBNORMAL_DOUBLE_MIN, FP_SUBNORMAL,   1);

	/* 
	 * unfortunately the minus operator does not change the sign of zero,
	 * so a special case is needed to test it
	 */
	assert(sizeof(negzero) == sizeof(double));
	test_fpclassify(*(double *) negzero, FP_ZERO, 0);
	if (!signbit(*(double *) negzero)) ERR(0, 0);

	/* test other small numbers for fpclassify and signbit */
	d = 1;
	while (d >= NORMAL_DOUBLE_MIN)
	{
		test_fpclassify(d, FP_NORMAL, 1);
		d /= 10;
	}
	while (d >= SUBNORMAL_DOUBLE_MIN)
	{
		test_fpclassify(d, FP_SUBNORMAL, 1);
		d /= 10;
	}
	test_fpclassify(d, FP_ZERO, 0);

	/* test other large numbers for fpclassify and signbit */
	d = 1;
	while (d <= NORMAL_DOUBLE_MAX / 10)
	{
		test_fpclassify(d, FP_NORMAL, 1);
		d *= 10;
	}
}

/* expected rounding: up, down or identical */
#define ROUND_EQ 1
#define ROUND_DN 2
#define ROUND_UP 3

static void test_round_value_mode_func(double value, int mode, double (*func)(double), int exp)
{
	int mode_old;
	double rounded;

	/* update and check rounding mode */
	mode_old = fegetround();
	fesetround(mode);
	if (fegetround() != mode) ERR(0, 0);

	/* perform rounding */
	rounded = func(value);
	
	/* check direction of rounding */
	switch (exp)
	{
		case ROUND_EQ: if (rounded != value) ERR(value, rounded); break;
		case ROUND_DN: if (rounded >= value) ERR(value, rounded); break;
		case ROUND_UP: if (rounded <= value) ERR(value, rounded); break;
		default:       assert(0); 
	}

	/* check whether the number is sufficiently close */
	if (fabs(value - rounded) >= 1) ERR(value, rounded);

	/* check whether the number is integer */
	if (remainder(rounded, 1)) ERR(value, rounded);

	/* re-check and restore rounding mode */
	if (fegetround() != mode) ERR(0, 0);
	fesetround(mode_old);
}

static void test_round_value_mode(double value, int mode, int exp_nearbyint,
	int exp_ceil, int exp_floor, int exp_trunc)
{
	/* test both nearbyint and trunc */
	test_round_value_mode_func(value, mode, nearbyint, exp_nearbyint);
	test_round_value_mode_func(value, mode, ceil,      exp_ceil);
	test_round_value_mode_func(value, mode, floor,     exp_floor);
	test_round_value_mode_func(value, mode, trunc,     exp_trunc);
}

static void test_round_value(double value, int exp_down, int exp_near, int exp_up, int exp_trunc)
{
	/* test each rounding mode */
	test_round_value_mode(value, FE_DOWNWARD,   exp_down,  exp_up, exp_down, exp_trunc);
	test_round_value_mode(value, FE_TONEAREST,  exp_near,  exp_up, exp_down, exp_trunc);
	test_round_value_mode(value, FE_UPWARD,     exp_up,    exp_up, exp_down, exp_trunc);
	test_round_value_mode(value, FE_TOWARDZERO, exp_trunc, exp_up, exp_down, exp_trunc);
}

static void test_round_values(void)
{
	int i;

	/* test various values */
	for (i = -100000; i < 100000; i++)
	{
		test_round_value(i + 0.00, ROUND_EQ, ROUND_EQ,                      ROUND_EQ, ROUND_EQ);
		test_round_value(i + 0.25, ROUND_DN, ROUND_DN,                      ROUND_UP, (i >= 0) ? ROUND_DN : ROUND_UP);
		test_round_value(i + 0.50, ROUND_DN, (i % 2) ? ROUND_UP : ROUND_DN, ROUND_UP, (i >= 0) ? ROUND_DN : ROUND_UP);
		test_round_value(i + 0.75, ROUND_DN, ROUND_UP,                      ROUND_UP, (i >= 0) ? ROUND_DN : ROUND_UP);
	}
}

static void test_remainder_value(double x, double y)
{
	int mode_old;
	double r1, r2, z;

	assert(y != 0);

	/* compute remainder using the function */
	r1 = remainder(x, y);

	/* compute remainder using alternative approach */
	mode_old = fegetround();
	fesetround(FE_TONEAREST);
	r2 = x - nearbyint(x / y) * y;
	fesetround(mode_old);

	/* Compare results */
	if (fabs(r1 - r2) > EPSILON && fabs(r1 + r2) > EPSILON) 
	{
		printf("%.20g != %.20g\n", r1, r2);
		ERR(x, y);
	}
}

static void test_remainder_values_y(double x)
{
	int i, j;

	/* try various rational and transcendental values for y (except zero) */
	for (i = -50; i <= 50; i++)
		if (i != 0)
			for (j = 1; j < 10; j++)
			{
				test_remainder_value(x, (double) i / (double) j);
				test_remainder_value(x, i * M_E + j * M_PI);
			}
}

static void test_remainder_values_xy(void)
{
	int i, j;

	/* try various rational and transcendental values for x */
	for (i = -50; i <= 50; i++)
		for (j = 1; j < 10; j++)
		{
			test_remainder_values_y((double) i / (double) j);
			test_remainder_values_y(i * M_E + j * M_PI);
		}
}

int main(int argc, char **argv)
{
	fenv_t fenv;
	int i;
	
	printf("Test 47 ");
	fflush(stdout);

	/* no FPU errors, please */
	if (feholdexcept(&fenv) < 0) ERR(0, 0);

	/* some signals count as errors */
	for (i = 0; i < _NSIG; i++)
		if (i != SIGINT && i != SIGTERM && i != SIGKILL)
			signal(i, signal_handler);
		
	/* test various floating point support functions */
	test_fpclassify_values();
	test_remainder_values_xy();
	test_round_values();
	quit();
	return -1;
}
