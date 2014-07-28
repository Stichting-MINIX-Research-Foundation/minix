/* The kernel call implemented in this file:
 *   m_type:	SYS_SAFECOPYFROM or SYS_SAFECOPYTO or SYS_VSAFECOPY
 *
 * The parameters for this kernel call are:
 *    	m_lsys_kern_safecopy.from_to	other endpoint
 *    	m_lsys_kern_safecopy.gid	grant id
 *    	m_lsys_kern_safecopy.offset	offset within granted space
 *	m_lsys_kern_safecopy.address	address in own address space
 *    	m_lsys_kern_safecopy.bytes	bytes to be copied
 *
 * For the vectored variant (do_vsafecopy): 
 *      m_lsys_kern_vsafecopy.vec_addr   address of vector
 *      m_lsys_kern_vsafecopy.vec_size   number of significant elements in vector
 */

#include <assert.h>

#include "kernel/system.h"
#include "kernel/kernel.h"
#include "kernel/vm.h"

#define MAX_INDIRECT_DEPTH 5	/* up to how many indirect grants to follow? */

#define MEM_TOP 0xFFFFFFFFUL

static int safecopy(struct proc *, endpoint_t, endpoint_t,
	cp_grant_id_t, size_t, vir_bytes, vir_bytes, int);

#define HASGRANTTABLE(gr) \
	(priv(gr) && priv(gr)->s_grant_table)

/*===========================================================================*
 *				verify_grant				     *
 *===========================================================================*/
int verify_grant(granter, grantee, grant, bytes, access,
	offset_in, offset_result, e_granter, flags)
endpoint_t granter, grantee;	/* copyee, copyer */
cp_grant_id_t grant;		/* grant id */
vir_bytes bytes;		/* copy size */
int access;			/* direction (read/write) */
vir_bytes offset_in;		/* copy offset within grant */
vir_bytes *offset_result;	/* copy offset within virtual address space */
endpoint_t *e_granter;		/* new granter (magic grants) */
u32_t *flags;			/* CPF_* */
{
	static cp_grant_t g;
	static int proc_nr;
	static const struct proc *granter_proc;
	int depth = 0;

	do {
		/* Get granter process slot (if valid), and check range of
		 * grant id.
		 */
		if(!isokendpt(granter, &proc_nr) ) {
			printf(
			"grant verify failed: invalid granter %d\n", (int) granter);
			return(EINVAL);
		}
		if(!GRANT_VALID(grant)) {
			printf(
			"grant verify failed: invalid grant %d\n", (int) grant);
			return(EINVAL);
		}
		granter_proc = proc_addr(proc_nr);

		/* If there is no priv. structure, or no grant table in the
		 * priv. structure, or the grant table in the priv. structure
		 * is too small for the grant, return EPERM.
		 */
		if(!HASGRANTTABLE(granter_proc)) {
			printf(
			"grant verify failed: granter %d has no grant table\n",
			granter);
			return(EPERM);
		}

		if(priv(granter_proc)->s_grant_entries <= grant) {
				printf(
				"verify_grant: grant verify failed in ep %d "
				"proc %d: grant %d out of range "
				"for table size %d\n",
					granter, proc_nr, grant,
					priv(granter_proc)->s_grant_entries);
			return(EPERM);
		}

		/* Copy the grant entry corresponding to this id to see what it
		 * looks like. If it fails, hide the fact that granter has
		 * (presumably) set an invalid grant table entry by returning
		 * EPERM, just like with an invalid grant id.
		 */
		if(data_copy(granter,
			priv(granter_proc)->s_grant_table + sizeof(g)*grant,
			KERNEL, (vir_bytes) &g, sizeof(g)) != OK) {
			printf(
			"verify_grant: grant verify: data_copy failed\n");
			return EPERM;
		}

		if(flags) *flags = g.cp_flags;

		/* Check validity. */
		if((g.cp_flags & (CPF_USED | CPF_VALID)) !=
			(CPF_USED | CPF_VALID)) {
			printf(
			"verify_grant: grant failed: invalid (%d flags 0x%lx)\n",
				grant, g.cp_flags);
			return EPERM;
		}

		/* The given grant may be an indirect grant, that is, a grant
		 * that provides permission to use a grant given to the
		 * granter (i.e., for which it is the grantee). This can lead
		 * to a chain of indirect grants which must be followed back.
		 */
		if((g.cp_flags & CPF_INDIRECT)) {
			/* Stop after a few iterations. There may be a loop. */
			if (depth == MAX_INDIRECT_DEPTH) {
				printf(
					"verify grant: indirect grant verify "
					"failed: exceeded maximum depth\n");
				return ELOOP;
			}
			depth++;

			/* Verify actual grantee. */
			if(g.cp_u.cp_indirect.cp_who_to != grantee &&
				grantee != ANY &&
				g.cp_u.cp_indirect.cp_who_to != ANY) {
				printf(
					"verify_grant: indirect grant verify "
					"failed: bad grantee\n");
				return EPERM;
			}

			/* Start over with new granter, grant, and grantee. */
			grantee = granter;
			granter = g.cp_u.cp_indirect.cp_who_from;
			grant = g.cp_u.cp_indirect.cp_grant;
		}
	} while(g.cp_flags & CPF_INDIRECT);

	/* Check access of grant. */
	if(((g.cp_flags & access) != access)) {
		printf(
	"verify_grant: grant verify failed: access invalid; want 0x%x, have 0x%x\n",
			access, g.cp_flags);
		return EPERM;
	}

	if((g.cp_flags & CPF_DIRECT)) {
		/* Don't fiddle around with grants that wrap, arithmetic
		 * below may be confused.
		 */
		if(MEM_TOP - g.cp_u.cp_direct.cp_len + 1 <
			g.cp_u.cp_direct.cp_start) {
			printf(
		"verify_grant: direct grant verify failed: len too long\n");
			return EPERM;
		}

		/* Verify actual grantee. */
		if(g.cp_u.cp_direct.cp_who_to != grantee && grantee != ANY
			&& g.cp_u.cp_direct.cp_who_to != ANY) {
			printf(
		"verify_grant: direct grant verify failed: bad grantee\n");
			return EPERM;
		}

		/* Verify actual copy range. */
		if((offset_in+bytes < offset_in) ||
		    offset_in+bytes > g.cp_u.cp_direct.cp_len) {
			printf(
		"verify_grant: direct grant verify failed: bad size or range. "
		"granted %d bytes @ 0x%lx; wanted %d bytes @ 0x%lx\n",
				g.cp_u.cp_direct.cp_len,
				g.cp_u.cp_direct.cp_start,
				bytes, offset_in);
			return EPERM;
		}

		/* Verify successful - tell caller what address it is. */
		*offset_result = g.cp_u.cp_direct.cp_start + offset_in;
		*e_granter = granter;
	} else if(g.cp_flags & CPF_MAGIC) {
		/* Currently, it is hardcoded that only FS may do
		 * magic grants.
		 */
		if(granter != VFS_PROC_NR) {
			printf(
		"verify_grant: magic grant verify failed: granter (%d) "
		"is not FS (%d)\n", granter, VFS_PROC_NR);
			return EPERM;
		}

		/* Verify actual grantee. */
		if(g.cp_u.cp_magic.cp_who_to != grantee && grantee != ANY
			&& g.cp_u.cp_direct.cp_who_to != ANY) {
			printf(
		"verify_grant: magic grant verify failed: bad grantee\n");
			return EPERM;
		}

		/* Verify actual copy range. */
		if((offset_in+bytes < offset_in) ||
		    offset_in+bytes > g.cp_u.cp_magic.cp_len) {
			printf(
		"verify_grant: magic grant verify failed: bad size or range. "
		"granted %d bytes @ 0x%lx; wanted %d bytes @ 0x%lx\n",
				g.cp_u.cp_magic.cp_len,
				g.cp_u.cp_magic.cp_start,
				bytes, offset_in);
			return EPERM;
		}

		/* Verify successful - tell caller what address it is. */
		*offset_result = g.cp_u.cp_magic.cp_start + offset_in;
		*e_granter = g.cp_u.cp_magic.cp_who_from;
	} else {
		printf(
		"verify_grant: grant verify failed: unknown grant type\n");
		return EPERM;
	}

	return OK;
}

/*===========================================================================*
 *				safecopy				     *
 *===========================================================================*/
static int safecopy(caller, granter, grantee, grantid, bytes,
	g_offset, addr, access)
struct proc * caller;
endpoint_t granter, grantee;
cp_grant_id_t grantid;
size_t bytes;
vir_bytes g_offset, addr;
int access;			/* CPF_READ for a copy from granter to grantee, CPF_WRITE
				 * for a copy from grantee to granter.
				 */
{
	static struct vir_addr v_src, v_dst;
	static vir_bytes v_offset;
	endpoint_t new_granter, *src, *dst;
	struct proc *granter_p;
	int r;
	u32_t flags;
#if PERF_USE_COW_SAFECOPY
	vir_bytes size;
#endif

	if(granter == NONE || grantee == NONE) {
		printf("safecopy: nonsense processes\n");
		return EFAULT;
	}

	/* See if there is a reasonable grant table. */
	if(!(granter_p = endpoint_lookup(granter))) return EINVAL;
	if(!HASGRANTTABLE(granter_p)) {
		printf(
		"safecopy failed: granter %d has no grant table\n", granter);
		return(EPERM);
	}

	/* Decide who is src and who is dst. */
	if(access & CPF_READ) {
		src = &granter;
		dst = &grantee;
	} else {
		src = &grantee;
		dst = &granter;
	}

	/* Verify permission exists. */
	if((r=verify_grant(granter, grantee, grantid, bytes, access,
	    g_offset, &v_offset, &new_granter, &flags)) != OK) {
			printf(
		"grant %d verify to copy %d->%d by %d failed: err %d\n",
				grantid, *src, *dst, grantee, r);
		return r;
	}

	/* verify_grant() can redirect the grantee to someone else,
	 * meaning the source or destination changes.
	 */
	granter = new_granter;

	/* Now it's a regular copy. */
	v_src.proc_nr_e = *src;
	v_dst.proc_nr_e = *dst;

	/* Now the offset in virtual addressing is known in 'offset'.
	 * Depending on the access, this is the source or destination
	 * address.
	 */
	if(access & CPF_READ) {
		v_src.offset = v_offset;
		v_dst.offset = (vir_bytes) addr;
	} else {
		v_src.offset = (vir_bytes) addr;
		v_dst.offset = v_offset;
	}

	/* Do the regular copy. */
	if(flags & CPF_TRY) {
		int r;
		/* Try copy without transparently faulting in pages. */
		r = virtual_copy(&v_src, &v_dst, bytes);
		if(r == EFAULT_SRC || r == EFAULT_DST) return EFAULT;
		return r;
	}
	return virtual_copy_vmcheck(caller, &v_src, &v_dst, bytes);
}

/*===========================================================================*
 *				do_safecopy_to				     *
 *===========================================================================*/
int do_safecopy_to(struct proc * caller, message * m_ptr)
{
	return safecopy(caller, m_ptr->m_lsys_kern_safecopy.from_to, caller->p_endpoint,
		(cp_grant_id_t) m_ptr->m_lsys_kern_safecopy.gid,
		m_ptr->m_lsys_kern_safecopy.bytes, m_ptr->m_lsys_kern_safecopy.offset,
		(vir_bytes) m_ptr->m_lsys_kern_safecopy.address, CPF_WRITE);
}

/*===========================================================================*
 *				do_safecopy_from			     *
 *===========================================================================*/
int do_safecopy_from(struct proc * caller, message * m_ptr)
{
	return safecopy(caller, m_ptr->m_lsys_kern_safecopy.from_to, caller->p_endpoint,
		(cp_grant_id_t) m_ptr->m_lsys_kern_safecopy.gid, 
		m_ptr->m_lsys_kern_safecopy.bytes, m_ptr->m_lsys_kern_safecopy.offset,
		(vir_bytes) m_ptr->m_lsys_kern_safecopy.address, CPF_READ);
}

/*===========================================================================*
 *				do_vsafecopy				     *
 *===========================================================================*/
int do_vsafecopy(struct proc * caller, message * m_ptr)
{
	static struct vscp_vec vec[SCPVEC_NR];
	static struct vir_addr src, dst;
	int r, i, els;
	size_t bytes;

	/* Set vector copy parameters. */
	src.proc_nr_e = caller->p_endpoint;
	assert(src.proc_nr_e != NONE);
	src.offset = (vir_bytes) m_ptr->m_lsys_kern_vsafecopy.vec_addr;
	dst.proc_nr_e = KERNEL;
	dst.offset = (vir_bytes) vec;

	/* No. of vector elements. */
	els = m_ptr->m_lsys_kern_vsafecopy.vec_size;
	bytes = els * sizeof(struct vscp_vec);

	/* Obtain vector of copies. */
	if((r=virtual_copy_vmcheck(caller, &src, &dst, bytes)) != OK)
		return r;

	/* Perform safecopies. */
	for(i = 0; i < els; i++) {
		int access;
		endpoint_t granter;
		if(vec[i].v_from == SELF) {
			access = CPF_WRITE;
			granter = vec[i].v_to;
		} else if(vec[i].v_to == SELF) {
			access = CPF_READ;
			granter = vec[i].v_from;
		} else {
			printf("vsafecopy: %d: element %d/%d: no SELF found\n",
				caller->p_endpoint, i, els);
			return EINVAL;
		}

		/* Do safecopy for this element. */
		if((r=safecopy(caller, granter, caller->p_endpoint,
			vec[i].v_gid,
			vec[i].v_bytes, vec[i].v_offset,
			vec[i].v_addr, access)) != OK) {
			return r;
		}
	}

	return OK;
}

