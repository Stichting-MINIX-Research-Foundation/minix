/*
 * Whatever is commonly used in mass_storage driver, should be here
 */

#ifndef _COMMON_H_
#define _COMMON_H_

/*---------------------------*
 *  commonly used headers:   *
 *---------------------------*/
#include <stdlib.h> /* For things, like EXIT_*, NULL, ... */
#include <stdio.h>

/*---------------------------*
 *  commonly used defines:   *
 *---------------------------*/
#define THIS_EXEC_NAME "usb_storage"
#define MASS_MSG(...) do {						\
	printf(THIS_EXEC_NAME": ");					\
	printf(__VA_ARGS__);						\
	printf("; %s:%d\n", __func__, __LINE__);			\
	} while(0)

/*---------------------------*
 *  debug helpers:           *
 *---------------------------*/
#ifdef MASS_DEBUG
#define MASS_DEBUG_MSG		MASS_MSG
#define MASS_DEBUG_DUMP		printf("%s():%d\n", __func__, __LINE__)
#else
#define MASS_DEBUG_MSG(...)
#define MASS_DEBUG_DUMP
#endif

#endif /* !_COMMON_H_ */
