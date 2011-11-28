#include <assert.h>
#include <minix/u64.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

#define ERR err(__LINE__)
#define MAX_ERROR 4
#define TIMED 0

#include "common.c"

static volatile int expect_SIGFPE;
static u64_t i, j, k;
static jmp_buf jmpbuf_SIGFPE, jmpbuf_main;

static void err(int line)
{
	/* print error information */
	printf("error line %d; i=0x%.8lx%.8lx; j=0x%.8lx%.8lx; k=0x%.8lx%.8lx\n", 
		line, 
		ex64hi(i), ex64lo(i), 
		ex64hi(j), ex64lo(j),
		ex64hi(k), ex64lo(k));

	/* quit after too many errors */
	if (errct++ > MAX_ERROR) {
		printf("Too many errors; test aborted\n");
		quit();
	}
}

#define LENGTHOF(arr) (sizeof(arr) / sizeof(arr[0]))

static u64_t getargval(int index, int *done)
{
	u32_t values[] = { 
		/* corner cases */
		0, 
		1,
		0x7fffffff,
		0x80000000,
		0x80000001,
		0xffffffff,
		/* random values */
		0xa9, 
		0x0d88, 
		0x242811, 
		0xeb44d1bc, 
		0x5b, 
		0xfb50, 
		0x569c02, 
		0xb23c8f7d, 
		0xc3, 
		0x2366, 
		0xfabb73, 
		0xcb4e8aef, 
		0xe9, 
		0xffdc, 
		0x05842d, 
		0x3fff902d};

	assert(done);

	/* values with corner case and random 32-bit components */
	if (index < LENGTHOF(values) * LENGTHOF(values))
		return make64(values[index / LENGTHOF(values)], values[index % LENGTHOF(values)]);

	index -= LENGTHOF(values) * LENGTHOF(values);

	/* small numbers */
	if (index < 16) return make64(index + 2, 0);
	index -= 16;

	/* big numbers */
	if (index < 16) return make64(-index - 2, -1);
	index -= 16;

	/* powers of two */
	if (index < 14) return make64(1 << (index * 2 + 5), 0);
	index -= 14;
	if (index < 16) return make64(0, 1 << (index * 2 + 1));
	index -= 16;

	/* done */
	*done = 1;
	return make64(0, 0);
}

static void handler_SIGFPE(int signum)
{
	assert(signum == SIGFPE);

	/* restore the signal handler */
	if (signal(SIGFPE, handler_SIGFPE) == SIG_ERR) ERR;

	/* division by zero occurred, was this expected? */
	if (expect_SIGFPE) {
		/* expected: jump back to test */
		expect_SIGFPE = 0;
		longjmp(jmpbuf_SIGFPE, -1);
	} else {
		/* not expected: error and jump back to main */
		longjmp(jmpbuf_main, -1);
	}

	/* not reachable */
	assert(0);
	exit(-1);
}

static void testmul(void)
{
	int kdone, kidx;
	u32_t ilo = ex64lo(i), jlo = ex64lo(j);
	u64_t prod = mul64(i, j);
	int prodbits;
		
	/* compute maximum index of highest-order bit */
	prodbits = bsr64(i) + bsr64(j) + 1;
	if (cmp64u(i, 0) == 0 || cmp64u(j, 0) == 0) prodbits = -1;
	if (bsr64(prod) > prodbits) ERR;

	/* compare to 32-bit multiplication if possible */	
	if (ex64hi(i) == 0 && ex64hi(j) == 0) {
		if (cmp64(prod, mul64u(ilo, jlo)) != 0) ERR;
		
		/* if there is no overflow we can check against pure 32-bit */
		if (prodbits < 32 && cmp64u(prod, ilo * jlo) != 0) ERR;
	}

	/* in 32-bit arith low-order DWORD matches regardless of overflow */
	if (ex64lo(prod) != ilo * jlo) ERR;

	/* multiplication by zero yields zero */
	if (prodbits < 0 && cmp64u(prod, 0) != 0) ERR;

	/* if there is no overflow, check absence of zero divisors */
	if (prodbits >= 0 && prodbits < 64 && cmp64u(prod, 0) == 0) ERR;

	/* commutativity */
	if (cmp64(prod, mul64(j, i)) != 0) ERR;

	/* loop though all argument value combinations for third argument */
	for (kdone = 0, kidx = 0; k = getargval(kidx, &kdone), !kdone; kidx++) {
		/* associativity */
		if (cmp64(mul64(mul64(i, j), k), mul64(i, mul64(j, k))) != 0) ERR;

		/* left and right distributivity */
		if (cmp64(mul64(add64(i, j), k), add64(mul64(i, k), mul64(j, k))) != 0) ERR;
		if (cmp64(mul64(i, add64(j, k)), add64(mul64(i, j), mul64(i, k))) != 0) ERR;
	}
}

static void testdiv0(void)
{
	int funcidx;

	assert(cmp64u(j, 0) == 0);

	/* loop through the 5 different division functions */
	for (funcidx = 0; funcidx < 5; funcidx++) {
		expect_SIGFPE = 1;
		if (setjmp(jmpbuf_SIGFPE) == 0) {
			/* divide by zero using various functions */
			switch (funcidx) {
				case 0: div64(i, j);		ERR; break;
				case 1: div64u64(i, ex64lo(j));	ERR; break;
				case 2: div64u(i, ex64lo(j));	ERR; break;
				case 3: rem64(i, j);		ERR; break;
				case 4: rem64u(i, ex64lo(j));	ERR; break;
				default: assert(0);		ERR; break;
			}

			/* if we reach this point there was no signal and an
			 * error has been recorded
			 */
			expect_SIGFPE = 0;
		} else {
			/* a signal has been received and expect_SIGFPE has
			 * been reset; all is ok now
			 */
			assert(!expect_SIGFPE);
		}
	}
}

static void testdiv(void)
{
	u64_t q, r;
#if TIMED
	struct timeval tvstart, tvend;

	printf("i=0x%.8x%.8x; j=0x%.8x%.8x\n", 
		ex64hi(i), ex64lo(i), 
		ex64hi(j), ex64lo(j));
	fflush(stdout);
	if (gettimeofday(&tvstart, NULL) < 0) ERR;
#endif

	/* division by zero has a separate test */
	if (cmp64u(j, 0) == 0) {
		testdiv0();
		return;
	}

	/* perform division, store q in k to make ERR more informative */
	q = div64(i, j);
	r = rem64(i, j);
	k = q;

#if TIMED
	if (gettimeofday(&tvend, NULL) < 0) ERR;
	tvend.tv_sec -= tvstart.tv_sec;
	tvend.tv_usec -= tvstart.tv_usec;
	if (tvend.tv_usec < 0) {
		tvend.tv_sec -= 1;
		tvend.tv_usec += 1000000;
	}
	printf("q=0x%.8x%.8x; r=0x%.8x%.8x; time=%d.%.6d\n", 
		ex64hi(q), ex64lo(q), 
		ex64hi(r), ex64lo(r), 
		tvend.tv_sec, tvend.tv_usec);
	fflush(stdout);
#endif

	/* compare to 64/32-bit division if possible */
	if (!ex64hi(j)) {
		if (cmp64(q, div64u64(i, ex64lo(j))) != 0) ERR;
		if (!ex64hi(q)) {
			if (cmp64u(q, div64u(i, ex64lo(j))) != 0) ERR;
		}
		if (cmp64u(r, rem64u(i, ex64lo(j))) != 0) ERR;

		/* compare to 32-bit division if possible */
		if (!ex64hi(i)) {
			if (cmp64u(q, ex64lo(i) / ex64lo(j)) != 0) ERR;
			if (cmp64u(r, ex64lo(i) % ex64lo(j)) != 0) ERR;
		}
	}

	/* check results using i = q j + r and r < j */
	if (cmp64(i, add64(mul64(q, j), r)) != 0) ERR;
	if (cmp64(r, j) >= 0) ERR;
}

static void test(void)
{
	int idone, jdone, iidx, jidx;

	/* loop though all argument value combinations */
	for (idone = 0, iidx = 0; i = getargval(iidx, &idone), !idone; iidx++)
	for (jdone = 0, jidx = 0; j = getargval(jidx, &jdone), !jdone; jidx++) {
		testmul();
		testdiv();
	}
}

int main(void)
{
	start(53);

	/* set up signal handler to deal with div by zero */
	if (setjmp(jmpbuf_main) == 0) {
		if (signal(SIGFPE, handler_SIGFPE) == SIG_ERR) ERR;
		
		/* perform tests */
		test();
	} else {
		/* an unexpected SIGFPE has occurred */
		ERR;
	}

	/* this was all */
	quit();

	return(-1);	/* Unreachable */
}
