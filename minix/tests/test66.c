/* test66: test a whole bunch of basic comparisons.
 *
 * this test can be used both to generate test cases and run the test
 * case. all the results to be computed are in myresults[] on each iteration.
 * invoke the test with an argument, e.g. "./test66 gen" to generate 
 * test66expected.h. then recompile, and run the result. so all you
 * have to do to add a (integer-valued) test expression as a function
 * of an int, float or double, is increase SUBRESULTS, add the expression
 * to myresults[], and regenerate the desired results file.
 */

#include <stdio.h>
#include <assert.h>

int max_error = 10;
#include "common.h"



#define RESULTSNAME desired
#define SUBRESULTS 131

int desired[][SUBRESULTS] = {
#include "test66expected.h"
};

#define CASES (sizeof(desired)/sizeof(desired[0]))

int main(int argc, char *argv[])
{
	int a, b;
	int gen = 0;
	int n = 0;

	if(argc != 1) gen = 1;
	else start(66);

	for(a = -10; a < 10; a++) {
		for(b = -10; b < 10; b++) {
			float fa = a/4.0, fb = b/4.0;
			double da = a/4.0, db = b/4.0;
			signed long long a64s = a, b64s = b, ds, ms;
			unsigned long long a64u = a, b64u = b, mu;
			signed long a32s = a, b32s = b, ds32, ms32;
			unsigned long a32u = a, b32u = b, mu32;

			/* indicate no result */
			mu32 = ds32 = ms32 = mu = ds = ms = 31337;

			if(b64s != 0) {
				/* some 64-bit arithmetic */
				ds = a64s/b64s; ms = a64s%b64s;
				mu = a64u%b64u;
				ds32 = a32s/b32s; ms32 = a32s%b32s;
				mu32 = a32u%b32u;
			}

			int myresults[SUBRESULTS] = {
/* these results lots of combinations of float, double, int
 * and signed and unsigned 64-bit comparisons (and therefore
 * also conversions).
 */
a < b, a <= b, a == b, a >= b, a > b, a < fb, a <= fb, a == fb,
a >= fb, a > fb, a < db, a <= db, a == db, a >= db, a > db, a < b64s,
a <= b64s, a == b64s, a >= b64s, a > b64s, a < b64u, a <= b64u, a == b64u,
a >= b64u, a > b64u, fa < b, fa <= b, fa == b, fa >= b, fa > b, fa < fb,
fa <= fb, fa == fb, fa >= fb, fa > fb, fa < db, fa <= db, fa == db,
fa >= db, fa > db, fa < b64s, fa <= b64s, fa == b64s, fa >= b64s,
fa > b64s, fa < b64u, fa <= b64u, fa == b64u, fa >= b64u, fa > b64u,
da < b, da <= b, da == b, da >= b, da > b, da < fb, da <= fb, da == fb,
da >= fb, da > fb, da < db, da <= db, da == db, da >= db, da > db,
da < b64s, da <= b64s, da == b64s, da >= b64s, da > b64s, da < b64u,
da <= b64u, da == b64u, da >= b64u, da > b64u, a64s < b, a64s <= b, a64s == b,
a64s >= b, a64s > b, a64s < fb, a64s <= fb, a64s == fb, a64s >= fb,
a64s > fb, a64s < db, a64s <= db, a64s == db, a64s >= db, a64s > db,
a64s < b64s, a64s <= b64s, a64s == b64s, a64s >= b64s, a64s > b64s,
a64s < b64u, a64s <= b64u, a64s == b64u, a64s >= b64u, a64s > b64u, a64u < b,
a64u <= b, a64u == b, a64u >= b, a64u > b, a64u < fb, a64u <= fb,
a64u == fb, a64u >= fb, a64u > fb, a64u < db, a64u <= db, a64u == db,
a64u >= db, a64u > db, a64u < b64s, a64u <= b64s, a64u == b64s,
a64u >= b64s, a64u > b64s, a64u < b64u, a64u <= b64u, a64u == b64u,
a64u >= b64u, a64u > b64u,

/* 64-bit divison, modulo operations */
(int) ds, (int) ms, (int) mu,

/* 32-bit divison, modulo operations */
(int) ds32, (int) ms32, (int) mu32

};

			if(gen) {
				int r;
				printf("{ ");
				for(r = 0; r < SUBRESULTS; r++) {
					printf("%d, ", myresults[r]);
				}
				printf(" }, \n");
			} else {
				int subresults;
				int s;
				subresults = sizeof(desired[n])/
					sizeof(desired[n][0]);
				assert(subresults == SUBRESULTS);
				for(s = 0; s < subresults; s++) {
					if(desired[n][s] != myresults[s]) {
						printf("a, b = %d, %d: %d != %d\n",
							a, b, desired[n][s],
							myresults[s]);
						e(n);
					} else {
						assert(desired[n][s] == myresults[s]);
					}
				}
			}

			n++;
		}
	}

	if(!gen) {
		assert(n == CASES);
		quit();
	}

	return 0;
}

