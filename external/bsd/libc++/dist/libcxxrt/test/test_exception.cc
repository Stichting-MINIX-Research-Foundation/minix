#include "test.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <exception>

#define fprintf(...)

void log(void* ignored)
{
	//printf("Cleanup called on %s\n", *(char**)ignored);
}
#define CLEANUP\
	__attribute__((cleanup(log))) __attribute__((unused))\
		const char *f = __func__;

/**
 * Simple struct to test throwing.
 */
struct foo
{
	int i;
};

struct bar : foo
{
	float bar;
};


/**
 * Non-pod type to test throwing
 */
class non_pod {
public:
    non_pod(int i): x(i) {}
    int x;
};


static int cleanup_count;
/**
 * Simple structure declared with a destructor.  Destroying this object will
 * increment cleanup count.  The destructor should be called automatically if
 * an instance of cl is allocated with automatic storage.
 */
struct cl
{
	int i;
	~cl() { fprintf(stderr, "cl destroyed: %d\n", i); cleanup_count++; }
};
/**
 * Test that one cl was destroyed when running the argument.
 */
#define TEST_CLEANUP(x) do {\
		int cleanups = cleanup_count;\
		{ x; }\
		TEST(cleanup_count == cleanups+1, "Cleanup ran correctly");\
	} while(0)

int inner(int i)
{
	CLEANUP
	switch (i)
	{
		case 0: throw (int)1.0;
		case 1: throw (float)1.0;
		case 2: fprintf(stderr, "Throwing int64_t\n");throw (int64_t)1;
		case 3: { foo f = {2} ; throw f; }
		case 4: { bar f; f.i = 2 ; f.bar=1 ; throw f; }
        case 5: throw non_pod(3);
	}
	return -1;
}

int outer(int i) throw(float, int, foo, non_pod)
{
	//CLEANUP
	inner(i);
	return 1;
}

static void test_const(void)
{
	int a = 1;
	try
	{
		throw a;
	}
	catch (const int b)
	{
		TEST(a == b, "Caught int as const int");
	}
	catch(...)
	{
		TEST(0, "Failed to catch int as const int");
	}
	try
	{
		throw &a;
	}
	catch (const int *b)
	{
		TEST(&a == b, "Caught int* as const int*");
	}
	catch(...)
	{
		TEST(0, "Failed to catch int* as const int*");
	}
}

static void test_catch(int s) 
{
	cl c;
	c.i = 12;
	fprintf(stderr, "Entering try\n");
	try
	{
		outer(s);
	}
	catch(int i)
	{
		fprintf(stderr, "Caught int %d in test %d\n", i, s);
		TEST((s == 0 && i == 1) || (s == 2 && i == 0), "Caught int");
		return;
	}
	catch (float f)
	{
		fprintf(stderr, "Caught float %f!\n", f);
		TEST(s == 1 && f == 1, "Caught float");
		return;
	}
	catch (foo f)
	{
		fprintf(stderr, "Caught struct {%d}!\n", f.i);
		TEST((s == 3 || s == 4) && f.i == 2, "Caught struct");
		return;
	}
    catch (non_pod np) {
        fprintf(stderr, "Caught non_pod {%d}!\n", np.x);
        TEST(s == 5 && np.x == 3, "Caught non_pod");
        return;
    }
	//abort();
	TEST(0, "Unreachable line reached");
}

void test_nested1(void)
{
	CLEANUP;
	cl c;
	c.i = 123;
	try 
	{
		outer(0);
	}
	catch (int a)
	{
		try
		{
			TEST(a == 1, "Caught int");
			outer(1);
		}
		catch (float f)
		{
			TEST(f == 1, "Caught float inside outer catch block");
			throw;
		}
	}
}

void test_nested()
{
	try
	{
		test_nested1();
	}
	catch (float f)
	{
		fprintf(stderr, "Caught re-thrown float\n");
		TEST(f == 1, "Caught re-thrown float");
	}
}

static int violations = 0;
static void throw_zero()
{
	violations++;
	fprintf(stderr, "Throwing 0\n");
	throw 0;
}

extern "C" void __cxa_bad_cast();

void test_exceptions(void)
{
	std::set_unexpected(throw_zero);
	TEST_CLEANUP(test_catch(0));
	TEST_CLEANUP(test_catch(1));
	TEST_CLEANUP(test_catch(3));
	TEST_CLEANUP(test_catch(4));
	TEST_CLEANUP(test_catch(5));
	TEST_CLEANUP(test_nested());
	try{
		test_catch(2);
		TEST(violations == 1, "Exactly one exception spec violation");
	}
	catch (int64_t i) {
		TEST(0, "Caught int64_t, but that violates an exception spec");
	}
	int a;
	try {
		throw &a;
	}
	catch (const int *b)
	{
		TEST(&a==b, "Caught const int from thrown int");
	}
	try {
		throw &a;
	}
	catch (int *b)
	{
		TEST(&a==b, "Caught int from thrown int");
	}
	try
	{
		__cxa_bad_cast();
	}
	catch (std::exception b)
	{
		TEST(1, "Caught bad cast");
	}
	catch (...)
	{
		TEST(0, "Bad cast was not caught correctly");
	}
	test_const();


	//printf("Test: %s\n",
}
