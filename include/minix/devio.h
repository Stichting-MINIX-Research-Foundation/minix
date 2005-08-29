/* This file provides basic types and some constants for the 
 * SYS_DEVIO and SYS_VDEVIO system calls, which allow user-level 
 * processes to perform device I/O. 
 *
 * Created: 
 *	Apr 08, 2004 by Jorrit N. Herder
 */

#ifndef _DEVIO_H
#define _DEVIO_H

#include <minix/sys_config.h>     /* needed to include <minix/type.h> */
#include <sys/types.h>        /* u8_t, u16_t, u32_t needed */

typedef u16_t port_t;
typedef U16_t Port_t;

/* We have different granularities of port I/O: 8, 16, 32 bits.
 * Also see <ibm/portio.h>, which has functions for bytes, words,  
 * and longs. Hence, we need different (port,value)-pair types. 
 */
typedef struct { u16_t port;  u8_t value; } pvb_pair_t;
typedef struct { u16_t port; u16_t value; } pvw_pair_t;
typedef struct { u16_t port; u32_t value; } pvl_pair_t;

/* Macro shorthand to set (port,value)-pair. */
#define pv_set(pv, p, v) ((pv).port = (p), (pv).value = (v))
#define pv_ptr_set(pv_ptr, p, v) ((pv_ptr)->port = (p), (pv_ptr)->value = (v))

#if 0	/* no longer in use !!! */
/* Define a number of flags to indicate granularity we are using. */
#define MASK_GRANULARITY 0x000F  /* not in use! does not match flags */
#define PVB_FLAG 'b'
#define PVW_FLAG 'w'
#define PVL_FLAG 'l'

/* Flags indicating whether request wants to do input or output. */
#define MASK_IN_OR_OUT 0x00F0
#define DEVIO_INPUT 0x0010
#define DEVIO_OUTPUT 0x0020
#endif	/* 0 */

#if 0	/* no longer used !!! */
/* Define how large the (port,value)-pair buffer in the kernel is. 
 * This buffer is used to copy the (port,value)-pairs in kernel space.
 */
#define PV_BUF_SIZE  64      /* creates char pv_buf[PV_BUF_SIZE] */

/* Note that SYS_VDEVIO sends a pointer to a vector of (port,value)-pairs, 
 * whereas SYS_DEVIO includes a single (port,value)-pair in the messages.
 * Calculate maximum number of (port,value)-pairs that can be handled 
 * in a single SYS_VDEVIO system call with above struct definitions. 
 */
#define MAX_PVB_PAIRS ((PV_BUF_SIZE * sizeof(char)) / sizeof(pvb_pair_t))
#define MAX_PVW_PAIRS ((PV_BUF_SIZE * sizeof(char)) / sizeof(pvw_pair_t))
#define MAX_PVL_PAIRS ((PV_BUF_SIZE * sizeof(char)) / sizeof(pvl_pair_t))
#endif /* 0 */
	

#endif  /* _DEVIO_H */
