/*
 * Whatever is commonly used throughout USB code
 */

#ifndef _USB_COMMON_H_
#define _USB_COMMON_H_

/* For commonly used: NULL, EXIT_*, and stuff like that */
#include <stdlib.h>

/* Current printf implementation for dumping important messages */
#include <stdio.h>

#if 1
/* TODO: should be elsewhere */
#define DEBUG
#endif


/*===========================================================================*
 *    Standard output message                                                *
 *===========================================================================*/
#define USB_MSG(fmt, ...)						\
		do {							\
			printf("USBD: ");				\
			printf(fmt, ##__VA_ARGS__);			\
			printf("\n");					\
		} while(0)


/*===========================================================================*
 *    Debug helpers                                                          *
 *===========================================================================*/
#ifdef DEBUG
#define DEBUG_DUMP							\
		do {							\
			printf("USBD (DEBUG %s)\n", __func__);		\
		} while(0)

#define USB_DBG(fmt, ...)						\
		do {							\
			printf("USBD (DEBUG %s): ", __func__);		\
			printf(fmt, ##__VA_ARGS__);			\
			printf("\n");					\
		} while(0)

#else
#define DEBUG_DUMP
#define USB_DBG(fmt, ...)
#endif


/*===========================================================================*
 *    Assert for USB code                                                    *
 *===========================================================================*/
#define USB_ASSERT(cond, otherwise)					\
		do {							\
			if(!(cond)) {					\
				USB_MSG("ERROR - "otherwise);		\
				exit(EXIT_FAILURE);			\
			}						\
		} while(0)


#endif /* !_USB_COMMON_H_ */
