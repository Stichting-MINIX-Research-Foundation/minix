/* The kernel call implemented in this file:
 *   m_type:	SYS_SAFEMEMSET
 *
 * The parameters for this kernel call are:
 *	SMS_DST		dst endpoint
 *	SMS_GID		grant id
 *	SMS_OFFSET	offset within grant
 *	SMS_PATTERN     memset pattern byte
 *	SMS_BYTES	bytes from offset
 */
// #include <assert.h> // Removed (assert was not used)

#include <minix/safecopies.h> // Kept

#include "kernel/system.h"

// Added kernel headers
#include <minix/kernel_types.h> // For k_size_t, k_errno_t
#include <klib/include/kprintf.h>
#include <klib/include/kstring.h>
#include <klib/include/kmemory.h>


/*===========================================================================*
 *                              do_safememset                                *
 *===========================================================================*/
int do_safememset(struct proc *caller, message *m_ptr) {
	/* Implementation of the do_safememset() kernel call */

	/* Extract parameters */
	endpoint_t dst_endpt = m_ptr->SMS_DST;
	endpoint_t caller_endpt = caller->p_endpoint;
	cp_grant_id_t grantid = m_ptr->SMS_GID;
	vir_bytes g_offset = m_ptr->SMS_OFFSET;
	int pattern = m_ptr->SMS_PATTERN;
	k_size_t len = (k_size_t)m_ptr->SMS_BYTES; // MODIFIED size_t

	struct proc *dst_p;
	endpoint_t new_granter;
	static vir_bytes v_offset;
	int r;

	if (dst_endpt == NONE || caller_endpt == NONE)
		return EFAULT; // EFAULT might be undefined

	if (!(dst_p = endpoint_lookup(dst_endpt)))
		return EINVAL; // EINVAL might be undefined

	if (!(priv(dst_p) && priv(dst_p)->s_grant_table)) {
		kprintf_stub("safememset: dst %d has no grant table\n", dst_endpt); // MODIFIED
		return EINVAL; // EINVAL might be undefined
	}

	/* Verify permission exists, memset always requires CPF_WRITE */
	r = verify_grant(dst_endpt, caller_endpt, grantid, len, CPF_WRITE,
			 g_offset, &v_offset, &new_granter, NULL); // MODIFIED (NULL)

	if (r != OK) {
		kprintf_stub("safememset: grant %d verify failed %d", grantid, r); // MODIFIED
		return r;
	}

	return vm_memset(caller, new_granter, v_offset, pattern, len);
}
