#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define MAX_ERROR 4
static int errct;

/* test strtol */
#define	TYPE        long
#define	TYPEU       unsigned long
#define	TYPE_FUNC	strtol
#include "test45.h"
#undef	TYPE
#undef	TYPEU
#undef	TYPE_FUNC

/* test strtoul */
#define	TYPE        unsigned long
#define	TYPEU       unsigned long
#define	TYPE_FUNC	strtoul
#include "test45.h"
#undef	TYPE
#undef	TYPEU
#undef	TYPE_FUNC

#ifdef __LONG_LONG_SUPPORTED

/* test strtoll */
#define	TYPE        long long
#define	TYPEU       unsigned long long
#define	TYPE_FUNC	strtoll
#include "test45.h"
#undef	TYPE
#undef	TYPEU
#undef	TYPE_FUNC

/* test strtoull */
#define	TYPE        long long
#define	TYPEU       unsigned long long
#define	TYPE_FUNC	strtoull
#include "test45.h"
#undef	TYPE
#undef	TYPEU
#undef	TYPE_FUNC

#endif /* defined(__LONG_LONG_SUPPORTED) */

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

int main(int argc, char **argv)
{
#ifdef __LONG_LONG_SUPPORTED
	printf("Test 45 (GCC) ");
#else
	printf("Test 45 (ACK) ");
#endif
	fflush(stdout);

	/* run long/unsigned long tests */
	test_strtol();
	test_strtoul();

	/* run long long/unsigned long long tests (GCC only) */
#ifdef __LONG_LONG_SUPPORTED
	test_strtoll();
	test_strtoull();
#endif /* defined(__LONG_LONG_SUPPORTED) */

	quit();
	return -1; /* never happens */
}
