/*
 * Whatever is commonly used throughout USBD code
 */

#ifndef _USBD_COMMON_H_
#define _USBD_COMMON_H_

/* For commonly used: NULL, EXIT_*, and stuff like that */
#include <stdlib.h>

/* Current printf implementation for dumping important messages */
#include <stdio.h>

/* In case of verbose debug output, enable this: */
#if 0
#define DEBUG
#endif

/* This allows us to analyze thread context in
 * consecutive function calls (DEBUG_DUMP) */
#include <ddekit/thread.h>

/* Represents current thread's name string */
#define HCD_THREAD_NAME ddekit_thread_get_name(ddekit_thread_myself())


/*===========================================================================*
 *    Standard output message                                                *
 *===========================================================================*/
#define USB_MSG(fmt, ...)						\
	do {								\
		printf("USBD: ");					\
		printf(fmt, ##__VA_ARGS__);				\
		printf("\n");						\
	} while(0)


/*===========================================================================*
 *    Debug helpers                                                          *
 *===========================================================================*/
#ifdef DEBUG
#define DEBUG_DUMP							\
	do {								\
		printf("USBD: [%s -> %s]\n", HCD_THREAD_NAME, __func__);\
	} while(0)

#define USB_DBG(fmt, ...)						\
	do {								\
		printf("USBD: [%s -> %s] ", HCD_THREAD_NAME, __func__);	\
		printf(fmt, ##__VA_ARGS__);				\
		printf("\n");						\
	} while(0)

#else
#define DEBUG_DUMP		((void)0)
#define USB_DBG(fmt, ...)	((void)0)
#endif


/*===========================================================================*
 *    Assert for USB code                                                    *
 *===========================================================================*/
#define USB_ASSERT(cond, otherwise)					\
	do {								\
		if (!(cond)) {						\
			USB_MSG("ASSERTION ERROR (%s -> %s:%d) - "	\
				otherwise, HCD_THREAD_NAME,		\
				__func__, __LINE__);			\
			exit(EXIT_FAILURE);				\
		}							\
	} while(0)


#endif /* !_USBD_COMMON_H_ */
