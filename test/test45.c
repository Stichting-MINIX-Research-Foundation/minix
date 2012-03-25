#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#if defined(__clang__)
#pragma clang diagnostic ignored "-Wtautological-compare"
#endif

#define MAX_ERROR 4
#include "common.c"

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

int main(int argc, char **argv)
{
	start(45);

	/* run long/unsigned long tests */
	test_strtol();
	test_strtoul();

	/* run long long/unsigned long long tests */
	test_strtoll();
	test_strtoull();

	quit();
	return -1; /* never happens */
}
