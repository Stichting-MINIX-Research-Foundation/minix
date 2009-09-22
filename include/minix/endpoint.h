
#ifndef _MINIX_ENDPOINT_H
#define _MINIX_ENDPOINT_H 1

#include <minix/sys_config.h>
#include <minix/com.h>
#include <limits.h>

/* The point of the padding in 'generation size' is to 
 * allow for certain bogus endpoint numbers such as NONE, ANY, etc.
 *
 * The _MAX_MAGIC_PROC is defined by <minix/com.h>. That include
 * file defines some magic process numbers such as ANY and NONE,
 * and must never be a valid endpoint number. Therefore we make sure
 * the generation size is big enough to start the next generation
 * above the highest magic number.
 */
#define _ENDPOINT_GENERATION_BITS	16
#define _ENDPOINT_PNUM_BITS		16
#define _ENDPOINT_MAX_GENERATION	((1 << _ENDPOINT_GENERATION_BITS)-1)
#define _ENDPOINT_MAX_PNUM		((1 << _ENDPOINT_PNUM_BITS) - 1)

/* Generation + Process slot number <-> endpoint. */
#define _ENDPOINT(g, p)	((endpoint_t)(((g) << _ENDPOINT_PNUM_BITS) | (p)))
#define _ENDPOINT_G(e) 	((u16_t)((e) >> _ENDPOINT_PNUM_BITS))
#define _ENDPOINT_P(e) 	((i16_t)((e) & _ENDPOINT_MAX_PNUM))

#endif
