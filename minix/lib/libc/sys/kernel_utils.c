/*
 * This file contains the main routine for retrieval of the kernel information
 * page.
 */

#define _MINIX_SYSTEM

#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>
#include <assert.h>

extern struct minix_kerninfo *_minix_kerninfo;

/*
 * Get a pointer to the kernel information page.
 */
struct minix_kerninfo *
get_minix_kerninfo(void)
{

	assert(_minix_kerninfo != NULL);

	return _minix_kerninfo;
}
