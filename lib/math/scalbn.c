#include <math.h>

double scalbln(double x, long n)
{
	return ldexp(x, n);
}

float scalblnf(float x, long n)
{
	return ldexp(x, n);
}

double scalbn(double x, int n)
{
	return ldexp(x, n);
}

float scalbnf(float x, int n)
{
	return ldexp(x, n);
}

