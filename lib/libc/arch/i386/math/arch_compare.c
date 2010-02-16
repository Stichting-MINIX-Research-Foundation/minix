#include <assert.h>
#include <math.h>

#include "fpu_cw.h"
#include "fpu_sw.h"

#define FPUSW_FLAG_MASK	\
	(FPUSW_CONDITION_C0 | FPUSW_CONDITION_C2 | FPUSW_CONDITION_C3)

#define DOUBLE_NORMAL_MIN 2.2250738585072013830902327173324e-308 /* 2^-1022 */

int fpclassify(double x)
{
	/* use status word returned by fpu_xam to determine type */
	switch (fpu_xam(x) & FPUSW_FLAG_MASK)
	{
		case FPUSW_CONDITION_C0:
			return FP_NAN;

		case FPUSW_CONDITION_C2:
			/* 
			 * unfortunately, fxam always operates on long doubles
			 * regardless of the precision setting. This means some
			 * subnormals are incorrectly classified as normals, 
			 * since they can be normalized using the additional
			 * exponent bits available. However, if we already know
			 * that the number is normal as a long double, finding
			 * out whether it would be subnormal as a double is just
			 * a simple comparison.
			 */
			if (-DOUBLE_NORMAL_MIN < x && x < DOUBLE_NORMAL_MIN)
				return FP_SUBNORMAL;
			else
				return FP_NORMAL;

		case FPUSW_CONDITION_C0 | FPUSW_CONDITION_C2:
			return FP_INFINITE;

		case FPUSW_CONDITION_C3:
			return FP_ZERO;

		case FPUSW_CONDITION_C3 | FPUSW_CONDITION_C2:
			return FP_SUBNORMAL;
	}

	/* we don't expect any other case: unsupported, emtpy or reserved */
	assert(0);
	return -1;
}

int signbit(double x)
{
	u16_t sw;

	/* examine and use status word to determine sign */
	sw = fpu_xam(x);
	return (sw & FPUSW_CONDITION_C1) ? 1 : 0;
}

#define FPUSW_GREATER	0
#define FPUSW_LESS	FPUSW_CONDITION_C0
#define FPUSW_EQUAL	FPUSW_CONDITION_C3
#define FPUSW_UNORDERED	\
	(FPUSW_CONDITION_C0 | FPUSW_CONDITION_C2 | FPUSW_CONDITION_C3)

static int fpcompare(double x, double y, u16_t sw1, u16_t sw2)
{
	u16_t sw;

	/* compare and check sanity */
	sw = fpu_compare(x, y) & FPUSW_FLAG_MASK;
	switch (sw)
	{
		case FPUSW_GREATER:
		case FPUSW_LESS:
		case FPUSW_EQUAL:
		case FPUSW_UNORDERED:
			break;

		default:
			/* other values are not possible (see IA32 Dev Man) */
			assert(0);
			return -1;
	}

	/* test whether FPUSW equals either sw1 or sw2 */
	return sw == sw1 || sw == sw2;
}

int isgreater(double x, double y)
{
	return fpcompare(x, y, FPUSW_GREATER, -1);
}

int isgreaterequal(double x, double y)
{
	return fpcompare(x, y, FPUSW_GREATER, FPUSW_EQUAL);
}

int isless(double x, double y)
{
	return fpcompare(x, y, FPUSW_LESS, -1);
}

int islessequal(double x, double y)
{
	return fpcompare(x, y, FPUSW_LESS, FPUSW_EQUAL);
}

int islessgreater(double x, double y)
{
	return fpcompare(x, y, FPUSW_LESS, FPUSW_GREATER);
}

int isunordered(double x, double y)
{
	return fpcompare(x, y, FPUSW_UNORDERED, -1);
}
