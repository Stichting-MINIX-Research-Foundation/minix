/*
 * Whatever is commonly used in hub driver, should be here
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
#define THIS_EXEC_NAME "usb_hub"
#define HUB_MSG(...) do {						\
	printf(THIS_EXEC_NAME": ");					\
	printf(__VA_ARGS__);						\
	printf("; %s:%d\n", __func__, __LINE__);			\
	} while(0)

/*---------------------------*
 *  debug helpers:           *
 *---------------------------*/
#ifdef HUB_DEBUG
#define HUB_DEBUG_MSG		HUB_MSG
#define HUB_DEBUG_DUMP		printf("%s():%d\n", __func__, __LINE__)
#else
#define HUB_DEBUG_MSG(...)
#define HUB_DEBUG_DUMP
#endif

#endif /* !_COMMON_H_ */
