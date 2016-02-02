/*
 * This file contains the main routine for retrieval of the kernel information
 * page, as well as abstraction routines for retrieval of specific values from
 * this kernel-mapped user information structure.  These routines may be used
 * from both userland and system services, and their accesses are considered to
 * establish part of the userland ABI.  Do not add routines here that are not
 * for retrieval of userland ABI fields (e.g., clock information)!  Also, since
 * these functions are MINIX3 specific, their names should contain - preferably
 * be prefixed with - "minix_".
 */

#define _MINIX_SYSTEM

#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>
#include <minix/param.h>
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

/*
 * Obtain the initial stack pointer for a new userland process.  This value
 * is used by routines that set up the stack when executing a new program.
 * It is used for userland exec(2) and in various system services.
 */
vir_bytes
minix_get_user_sp(void)
{
	struct minix_kerninfo *ki;

	/* All information is obtained from the kernel information page. */
	ki = get_minix_kerninfo();

	/*
	 * Check whether we can retrieve the user stack pointer value from the
	 * kuserinfo structure.  In general, this test is the correct one to
	 * see whether the kuserinfo structure has a certain field.
	 */
	if ((ki->ki_flags & MINIX_KIF_USERINFO) &&
	    KUSERINFO_HAS_FIELD(ki->kuserinfo, kui_user_sp)) {
		return ki->kuserinfo->kui_user_sp;
	}

	/*
	 * Otherwise, fall back to legacy support: retrieve the value from the
	 * kinfo structure.  This field will eventually be removed.
	 */
	return ki->kinfo->user_sp;
}
