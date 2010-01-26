/* The kernel call implemented in this file:
 *   m_type:	SYS_SAFECOPYFROM or SYS_SAFECOPYTO or SYS_VSAFECOPY
 *
 * The parameters for this kernel call are:
 *    	SCP_FROM_TO	other endpoint
 *    	SCP_SEG		segment in own address space
 *    	SCP_GID		grant id
 *    	SCP_OFFSET	offset within granted space
 *	SCP_ADDRESS	address in own address space
 *    	SCP_BYTES	bytes to be copied
 *
 * For the vectored variant (do_vsafecopy): 
 *      VSCP_VEC_ADDR   address of vector
 *      VSCP_VEC_SIZE   number of significant elements in vector
 */

#include <minix/type.h>
#include <minix/safecopies.h>

#include "../system.h"

#define MAX_INDIRECT_DEPTH 5	/* up to how many indirect grants to follow? */

#define MEM_TOP 0xFFFFFFFFUL

#define USE_COW_SAFECOPY 0

FORWARD _PROTOTYPE(int safecopy, (endpoint_t, endpoint_t, cp_grant_id_t, int, int, size_t, vir_bytes, vir_bytes, int));

#define HASGRANTTABLE(gr) \
	(!RTS_ISSET(gr, RTS_NO_PRIV) && priv(gr) && priv(gr)->s_grant_table > 0)

/*===========================================================================*
 *				verify_grant				     *
 *===========================================================================*/
PUBLIC int verify_grant(granter, grantee, grant, bytes, access,
	offset_in, offset_result, e_granter)
endpoint_t granter, grantee;	/* copyee, copyer */
cp_grant_id_t grant;		/* grant id */
vir_bytes bytes;		/* copy size */
int access;			/* direction (read/write) */
vir_bytes offset_in;		/* copy offset within grant */
vir_bytes *offset_result;	/* copy offset within virtual address space */
endpoint_t *e_granter;		/* new granter (magic grants) */
{
	static cp_grant_t g;
	static int proc_nr;
	static const struct proc *granter_proc;
	int r, depth = 0;

	do {
		/* Get granter process slot (if valid), and check range of
		 * grant id.
		 */
		if(!isokendpt(granter, &proc_nr) || !GRANT_VALID(grant)) {
			kprintf(
			"grant verify failed: invalid granter or grant\n");
			return(EINVAL);
		}
		granter_proc = proc_addr(proc_nr);

		/* If there is no priv. structure, or no grant table in the
		 * priv. structure, or the grant table in the priv. structure
		 * is too small for the grant, return EPERM.
		 */
		if(!HASGRANTTABLE(granter_proc)) return EPERM;

		if(priv(granter_proc)->s_grant_entries <= grant) {
				kprintf(
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
		if((r=data_copy(granter,
			priv(granter_proc)->s_grant_table + sizeof(g)*grant,
			SYSTEM, (vir_bytes) &g, sizeof(g))) != OK) {
			kprintf(
			"verify_grant: grant verify: data_copy failed\n");
			return EPERM;
		}

		/* Check validity. */
		if((g.cp_flags & (CPF_USED | CPF_VALID)) !=
			(CPF_USED | CPF_VALID)) {
			kprintf(
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
				kprintf(
					"verify grant: indirect grant verify "
					"failed: exceeded maximum depth\n");
				return ELOOP;
			}
			depth++;

			/* Verify actual grantee. */
			if(g.cp_u.cp_indirect.cp_who_to != grantee &&
				grantee != ANY &&
				g.cp_u.cp_indirect.cp_who_to != ANY) {
				kprintf(
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
		kprintf(
	"verify_grant: grant verify failed: access invalid; want 0x%x, have 0x%x\n",
			access, g.cp_flags);
		return EPERM;
	}

	if((g.cp_flags & CPF_DIRECT)) {
		/* Don't fiddle around with grants that wrap, arithmetic
		 * below may be confused.
		 */
		if(MEM_TOP - g.cp_u.cp_direct.cp_len <
			g.cp_u.cp_direct.cp_start - 1) {
			kprintf(
		"verify_grant: direct grant verify failed: len too long\n");
			return EPERM;
		}

		/* Verify actual grantee. */
		if(g.cp_u.cp_direct.cp_who_to != grantee && grantee != ANY
			&& g.cp_u.cp_direct.cp_who_to != ANY) {
			kprintf(
		"verify_grant: direct grant verify failed: bad grantee\n");
			return EPERM;
		}

		/* Verify actual copy range. */
		if((offset_in+bytes < offset_in) ||
		    offset_in+bytes > g.cp_u.cp_direct.cp_len) {
			kprintf(
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
		if(granter != FS_PROC_NR) {
			kprintf(
		"verify_grant: magic grant verify failed: granter (%d) "
		"is not FS (%d)\n", granter, FS_PROC_NR);
			return EPERM;
		}

		/* Verify actual grantee. */
		if(g.cp_u.cp_magic.cp_who_to != grantee && grantee != ANY
			&& g.cp_u.cp_direct.cp_who_to != ANY) {
			kprintf(
		"verify_grant: magic grant verify failed: bad grantee\n");
			return EPERM;
		}

		/* Verify actual copy range. */
		if((offset_in+bytes < offset_in) ||
		    offset_in+bytes > g.cp_u.cp_magic.cp_len) {
			kprintf(
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
		kprintf(
		"verify_grant: grant verify failed: unknown grant type\n");
		return EPERM;
	}

	return OK;
}

/*===========================================================================*
 *				safecopy				     *
 *===========================================================================*/
PRIVATE int safecopy(granter, grantee, grantid, src_seg, dst_seg, bytes,
	g_offset, addr, access)
endpoint_t granter, grantee;
cp_grant_id_t grantid;
int src_seg, dst_seg;
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
	vir_bytes size;
	int r;

	/* See if there is a reasonable grant table. */
	if(!(granter_p = endpoint_lookup(granter))) return EINVAL;
	if(!HASGRANTTABLE(granter_p)) return EPERM;

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
	    g_offset, &v_offset, &new_granter)) != OK) {
			kprintf(
		"grant %d verify to copy %d->%d by %d failed: err %d\n",
				grantid, *src, *dst, grantee, r);
		return r;
	}

	/* verify_grant() can redirect the grantee to someone else,
	 * meaning the source or destination changes.
	 */
	granter = new_granter;

	/* Now it's a regular copy. */
	v_src.segment = src_seg;
	v_dst.segment = dst_seg;
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
#if USE_COW_SAFECOPY
	if(v_offset % CLICK_SIZE != addr % CLICK_SIZE || bytes < CLICK_SIZE) {
		/* Give up on COW immediately when offsets are not aligned
		 * or we are copying less than a page.
		 */
		return virtual_copy_vmcheck(&v_src, &v_dst, bytes);
	}

	if((size = v_offset % CLICK_SIZE) != 0) {
		/* Normal copy for everything before the first page boundary. */
		size = CLICK_SIZE - size;
		r = virtual_copy_vmcheck(&v_src, &v_dst, size);
		if(r != OK)
			return r;
		v_src.offset += size;
		v_dst.offset += size;
		bytes -= size;
	}
	if((size = bytes / CLICK_SIZE) != 0) {
		/* Use COW optimization when copying entire pages. */
		size *= CLICK_SIZE;
		r = map_invoke_vm(VMPTYPE_COWMAP,
			v_dst.proc_nr_e, v_dst.segment, v_dst.offset,
			v_src.proc_nr_e, v_src.segment, v_src.offset,
			size, 0);
		if(r != OK)
			return r;
		v_src.offset += size;
		v_dst.offset += size;
		bytes -= size;
	}
	if(bytes != 0) {
		/* Normal copy for everything after the last page boundary. */
		r = virtual_copy_vmcheck(&v_src, &v_dst, bytes);
		if(r != OK)
			return r;
	}

	return OK;
#else
	return virtual_copy_vmcheck(&v_src, &v_dst, bytes);
#endif
}

/*===========================================================================*
 *				do_safecopy				     *
 *===========================================================================*/
PUBLIC int do_safecopy(m_ptr)
register message *m_ptr;	/* pointer to request message */
{
	static int access, src_seg, dst_seg;

	/* Set src and dst parameters. */
	if(sys_call_code == SYS_SAFECOPYFROM) {
		src_seg = D;
		dst_seg = m_ptr->SCP_SEG;
		access = CPF_READ;
	} else if(sys_call_code == SYS_SAFECOPYTO) {
		src_seg = m_ptr->SCP_SEG;
		dst_seg = D;
		access = CPF_WRITE;
	} else minix_panic("Impossible system call nr. ", sys_call_code);

	return safecopy(m_ptr->SCP_FROM_TO, who_e, m_ptr->SCP_GID,
		src_seg, dst_seg, m_ptr->SCP_BYTES, m_ptr->SCP_OFFSET,
		(vir_bytes) m_ptr->SCP_ADDRESS, access);
}

/*===========================================================================*
 *				do_vsafecopy				     *
 *===========================================================================*/
PUBLIC int do_vsafecopy(m_ptr)
register message *m_ptr;	/* pointer to request message */
{
	static struct vscp_vec vec[SCPVEC_NR];
	static struct vir_addr src, dst;
	int r, i, els;
	size_t bytes;

	/* Set vector copy parameters. */
	src.proc_nr_e = who_e;
	src.offset = (vir_bytes) m_ptr->VSCP_VEC_ADDR;
	src.segment = dst.segment = D;
	dst.proc_nr_e = SYSTEM;
	dst.offset = (vir_bytes) vec;

	/* No. of vector elements. */
	els = m_ptr->VSCP_VEC_SIZE;
	bytes = els * sizeof(struct vscp_vec);

	/* Obtain vector of copies. */
	if((r=virtual_copy_vmcheck(&src, &dst, bytes)) != OK)
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
			kprintf("vsafecopy: %d: element %d/%d: no SELF found\n",
				who_e, i, els);
			return EINVAL;
		}

		/* Do safecopy for this element. */
		if((r=safecopy(granter, who_e, vec[i].v_gid, D, D,
			vec[i].v_bytes, vec[i].v_offset,
			vec[i].v_addr, access)) != OK) {
			return r;
		}
	}

	return OK;
}

