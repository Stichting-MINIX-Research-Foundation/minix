
#ifndef _MINIX_ENDPOINT_H
#define _MINIX_ENDPOINT_H 1

#include <minix/sys_config.h>
#include <minix/com.h>
#include <limits.h>
#include <minix/type.h>

/* The point of the padding in 'generation size' is to 
 * allow for certain bogus endpoint numbers such as NONE, ANY, etc.
 *
 * The _MAX_MAGIC_PROC is defined by <minix/com.h>. That include
 * file defines some magic process numbers such as ANY and NONE,
 * and must never be a valid endpoint number. Therefore we make sure
 * the generation size is big enough to start the next generation
 * above the highest magic number.
 */
#define _ENDPOINT_GENERATION_SIZE (MAX_NR_TASKS+_MAX_MAGIC_PROC+1)
#define _ENDPOINT_MAX_GENERATION  (INT_MAX/_ENDPOINT_GENERATION_SIZE-1)

/* Generation + Process slot number <-> endpoint. */
#define _ENDPOINT(g, p) ((endpoint_t)((g) * _ENDPOINT_GENERATION_SIZE + (p)))
#define _ENDPOINT_G(e) (((e)+MAX_NR_TASKS) / _ENDPOINT_GENERATION_SIZE)
#define _ENDPOINT_P(e) \
	((((e)+MAX_NR_TASKS) % _ENDPOINT_GENERATION_SIZE) - MAX_NR_TASKS)

#endif
