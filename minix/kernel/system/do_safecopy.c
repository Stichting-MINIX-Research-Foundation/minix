/* The kernel call implemented in this file:
 *   m_type:	SYS_SAFECOPYFROM or SYS_SAFECOPYTO or SYS_VSAFECOPY
 *
 * The parameters for this kernel call are:
 *    	m_lsys_krn_safecopy.from_to	other endpoint
 *    	m_lsys_krn_safecopy.gid	grant id
 *    	m_lsys_krn_safecopy.offset	offset within granted space
 *	m_lsys_krn_safecopy.address	address in own address space
 *    	m_lsys_krn_safecopy.bytes	bytes to be copied
 *
 * For the vectored variant (do_vsafecopy):
 *      m_lsys_krn_vsafecopy.vec_addr   address of vector
 *      m_lsys_krn_vsafecopy.vec_size   number of significant elements in vector
 */

// #include <assert.h> // Replaced

#include "kernel/system.h"
#include "kernel/vm.h"

// Added kernel headers
#include <minix/kernel_types.h> // For k_size_t, k_errno_t
#include <klib/include/kprintf.h>
#include <klib/include/kstring.h>
#include <klib/include/kmemory.h>


#define MAX_INDIRECT_DEPTH 5	/* up to how many indirect grants to follow? */

#define MEM_TOP 0xFFFFFFFFUL

static int safecopy(struct proc *, endpoint_t, endpoint_t,
	cp_grant_id_t, k_size_t, vir_bytes, vir_bytes, int); // MODIFIED size_t

#define HASGRANTTABLE(gr) \
	(priv(gr) && priv(gr)->s_grant_table)

struct cp_sfinfo {		/* information for handling soft faults */
	int try;		/* if nonzero, try copy only, stop on fault */
	endpoint_t endpt;	/* endpoint owning grant with CPF_TRY flag */
	vir_bytes addr;		/* address to write mark upon soft fault */
	cp_grant_id_t value;	/* grant ID to use as mark value to write */
};

/*===========================================================================*
 *				verify_grant				     *
 *===========================================================================*/
int verify_grant(
  endpoint_t granter,		/* copyee */
  endpoint_t grantee,		/* copyer */
  cp_grant_id_t grant,		/* grant id */
  vir_bytes bytes,		/* copy size */
  int access,			/* direction (read/write) */
  vir_bytes offset_in,		/* copy offset within grant */
  vir_bytes *offset_result,	/* copy offset within virtual address space */
  endpoint_t *e_granter,	/* new granter (magic grants) */
  struct cp_sfinfo *sfinfo	/* storage for soft fault information */
)
{
	cp_grant_t g;
	int proc_nr;
	const struct proc *granter_proc;
	int grant_idx, grant_seq;
	int depth = 0;

	do {
		/* Get granter process slot (if valid), and check range of
		 * grant id.
		 */
		if(!isokendpt(granter, &proc_nr) ) {
			kprintf_stub( // MODIFIED
			"grant verify failed: invalid granter %d\n", (int) granter);
			return(EINVAL); // EINVAL might be undefined
		}
		if(!GRANT_VALID(grant)) {
			kprintf_stub( // MODIFIED
			"grant verify failed: invalid grant %d\n", (int) grant);
			return(EINVAL); // EINVAL might be undefined
		}
		granter_proc = proc_addr(proc_nr);

		/* If the granter has a temporary grant table, always allow
		 * requests with unspecified access and return ENOTREADY if
		 * no grant table is present or if the grantee's endpoint is not
		 * the endpoint the table belongs to. When ENOTREADY is returned
		 * the same verify_grant() request will be replayed again in a
		 * while until the grant table is final. This is necessary to
		 * avoid races at live update time.
		 */
		if(priv(granter_proc)->s_grant_endpoint != granter_proc->p_endpoint) {
			if(!access) {
				return OK;
			}
			else if(!HASGRANTTABLE(granter_proc) || grantee != priv(granter_proc)->s_grant_endpoint) {
				return ENOTREADY; // ENOTREADY might be undefined
			}
		}

		/* If there is no priv. structure, or no grant table in the
		 * priv. structure, or the grant table in the priv. structure
		 * is too small for the grant, return EPERM.
		 */
		if(!HASGRANTTABLE(granter_proc)) {
			kprintf_stub( // MODIFIED
			"grant verify failed: granter %d has no grant table\n",
			granter);
			return(EPERM); // EPERM might be undefined
		}

		grant_idx = GRANT_IDX(grant);
		grant_seq = GRANT_SEQ(grant);

		if(priv(granter_proc)->s_grant_entries <= grant_idx) {
				kprintf_stub( // MODIFIED
				"verify_grant: grant verify failed in ep %d "
				"proc %d: grant 0x%x (#%d) out of range "
				"for table size %d\n",
					granter, proc_nr, grant, grant_idx,
					priv(granter_proc)->s_grant_entries);
			return(EPERM); // EPERM might be undefined
		}

		/* Copy the grant entry corresponding to this ID's index to see
		 * what it looks like. If it fails, hide the fact that granter
		 * has (presumably) set an invalid grant table entry by
		 * returning EPERM, just like with an invalid grant id.
		 */
		if(data_copy(granter, priv(granter_proc)->s_grant_table +
			sizeof(g) * grant_idx,
			KERNEL, (vir_bytes) &g, sizeof(g)) != OK) {
			kprintf_stub( // MODIFIED
			"verify_grant: grant verify: data_copy failed\n");
			return EPERM; // EPERM might be undefined
		}

		/* Check validity: flags and sequence number. */
		if((g.cp_flags & (CPF_USED | CPF_VALID)) !=
			(CPF_USED | CPF_VALID)) {
			kprintf_stub("verify_grant: grant failed: invalid flags " // MODIFIED
			    "(0x%x, 0x%lx)\n", grant, g.cp_flags);
			return EPERM; // EPERM might be undefined
		}

		if (g.cp_seq != grant_seq) {
			kprintf_stub("verify_grant: grant failed: invalid sequence " // MODIFIED
			    "(0x%x, %d vs %d)\n", grant, grant_seq, g.cp_seq);
			return EPERM; // EPERM might be undefined
		}

		/* The given grant may be an indirect grant, that is, a grant
		 * that provides permission to use a grant given to the
		 * granter (i.e., for which it is the grantee). This can lead
		 * to a chain of indirect grants which must be followed back.
		 */
		if((g.cp_flags & CPF_INDIRECT)) {
			/* Stop after a few iterations. There may be a loop. */
			if (depth == MAX_INDIRECT_DEPTH) {
				kprintf_stub( // MODIFIED
					"verify grant: indirect grant verify "
					"failed: exceeded maximum depth\n");
				return ELOOP; // ELOOP might be undefined
			}
			depth++;

			/* Verify actual grantee. */
			if(g.cp_u.cp_indirect.cp_who_to != grantee &&
				grantee != ANY &&
				g.cp_u.cp_indirect.cp_who_to != ANY) {
				kprintf_stub( // MODIFIED
					"verify_grant: indirect grant verify "
					"failed: bad grantee\n");
				return EPERM; // EPERM might be undefined
			}

			/* Start over with new granter, grant, and grantee. */
			grantee = granter;
			granter = g.cp_u.cp_indirect.cp_who_from;
			grant = g.cp_u.cp_indirect.cp_grant;
		}
	} while(g.cp_flags & CPF_INDIRECT);

	/* Check access of grant. */
	if(((g.cp_flags & access) != access)) {
		kprintf_stub( // MODIFIED
	"verify_grant: grant verify failed: access invalid; want 0x%x, have 0x%x\n",
			access, g.cp_flags);
		return EPERM; // EPERM might be undefined
	}

	if((g.cp_flags & CPF_DIRECT)) {
		/* Don't fiddle around with grants that wrap, arithmetic
		 * below may be confused.
		 */
		if(MEM_TOP - g.cp_u.cp_direct.cp_len + 1 <
			g.cp_u.cp_direct.cp_start) {
			kprintf_stub( // MODIFIED
		"verify_grant: direct grant verify failed: len too long\n");
			return EPERM; // EPERM might be undefined
		}

		/* Verify actual grantee. */
		if(g.cp_u.cp_direct.cp_who_to != grantee && grantee != ANY
			&& g.cp_u.cp_direct.cp_who_to != ANY) {
			kprintf_stub( // MODIFIED
		"verify_grant: direct grant verify failed: bad grantee\n");
			return EPERM; // EPERM might be undefined
		}

		/* Verify actual copy range. */
		if((offset_in+bytes < offset_in) ||
		    offset_in+bytes > g.cp_u.cp_direct.cp_len) {
			kprintf_stub( // MODIFIED
		"verify_grant: direct grant verify failed: bad size or range. "
		"granted %d bytes @ 0x%lx; wanted %d bytes @ 0x%lx\n",
				g.cp_u.cp_direct.cp_len,
				g.cp_u.cp_direct.cp_start,
				bytes, offset_in);
			return EPERM; // EPERM might be undefined
		}

		/* Verify successful - tell caller what address it is. */
		*offset_result = g.cp_u.cp_direct.cp_start + offset_in;
		*e_granter = granter;
	} else if(g.cp_flags & CPF_MAGIC) {
		/* Currently, it is hardcoded that only VFS and MIB may do
		 * magic grants.  TODO: this should be a system.conf flag.
		 */
		if(granter != VFS_PROC_NR && granter != MIB_PROC_NR) {
			kprintf_stub( // MODIFIED
		"verify_grant: magic grant verify failed: granter (%d) "
		"not allowed\n", granter);
			return EPERM; // EPERM might be undefined
		}

		/* Verify actual grantee. */
		if(g.cp_u.cp_magic.cp_who_to != grantee && grantee != ANY
			&& g.cp_u.cp_direct.cp_who_to != ANY) {
			kprintf_stub( // MODIFIED
		"verify_grant: magic grant verify failed: bad grantee\n");
			return EPERM; // EPERM might be undefined
		}

		/* Verify actual copy range. */
		if((offset_in+bytes < offset_in) ||
		    offset_in+bytes > g.cp_u.cp_magic.cp_len) {
			kprintf_stub( // MODIFIED
		"verify_grant: magic grant verify failed: bad size or range. "
		"granted %d bytes @ 0x%lx; wanted %d bytes @ 0x%lx\n",
				g.cp_u.cp_magic.cp_len,
				g.cp_u.cp_magic.cp_start,
				bytes, offset_in);
			return EPERM; // EPERM might be undefined
		}

		/* Verify successful - tell caller what address it is. */
		*offset_result = g.cp_u.cp_magic.cp_start + offset_in;
		*e_granter = g.cp_u.cp_magic.cp_who_from;
	} else {
		kprintf_stub( // MODIFIED
		"verify_grant: grant verify failed: unknown grant type\n");
		return EPERM; // EPERM might be undefined
	}

	/* If requested, store information regarding soft faults. */
	if (sfinfo != NULL && (sfinfo->try = !!(g.cp_flags & CPF_TRY))) { // MODIFIED (NULL)
		sfinfo->endpt = granter;
		sfinfo->addr = priv(granter_proc)->s_grant_table +
		    sizeof(g) * grant_idx + K_OFFSETOF(cp_grant_t, cp_faulted);
		sfinfo->value = grant;
	}

	return OK;
}

/*===========================================================================*
 *				safecopy				     *
 *===========================================================================*/
static int safecopy(
  struct proc * caller,
  endpoint_t granter,
  endpoint_t grantee,
  cp_grant_id_t grantid,
  k_size_t bytes, // MODIFIED size_t
  vir_bytes g_offset,
  vir_bytes addr,
  int access			/* CPF_READ for a copy from granter to grantee, CPF_WRITE
				 * for a copy from grantee to granter.
				 */
)
{
	static struct vir_addr v_src, v_dst;
	static vir_bytes v_offset;
	endpoint_t new_granter, *src, *dst;
	int r;
	struct cp_sfinfo sfinfo;

	if(granter == NONE || grantee == NONE) {
		kprintf_stub("safecopy: nonsense processes\n"); // MODIFIED
		return EFAULT; // EFAULT might be undefined
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
	    g_offset, &v_offset, &new_granter, &sfinfo)) != OK) {
		if(r == ENOTREADY) return r; // ENOTREADY might be undefined
			kprintf_stub( // MODIFIED
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
	if (sfinfo.try) {
		/*
		 * Try copying without transparently faulting in pages.
		 * TODO: while CPF_TRY is meant to protect against deadlocks on
		 * memory-mapped files in file systems, it seems that this case
		 * triggers faults a whole lot more often, resulting in extra
		 * overhead due to retried file system operations.  It might be
		 * a good idea to go through VM even in this case, and have VM
		 * fail (only) if the affected page belongs to a file mapping.
		 */
		r = virtual_copy(&v_src, &v_dst, bytes);
		if (r == EFAULT_SRC || r == EFAULT_DST) { // EFAULT* might be undefined
			/*
			 * Mark the magic grant as having experienced a soft
			 * fault during its lifetime.  The exact value does not
			 * matter, but we use the grant ID (including its
			 * sequence number) as a form of protection in the
			 * light of CPU concurrency.
			 */
			r = data_copy(KERNEL, (vir_bytes)&sfinfo.value,
			    sfinfo.endpt, sfinfo.addr, sizeof(sfinfo.value));
			/*
			 * Failure means the creator of the magic grant messed
			 * up, which can only be unintentional, so report..
			 */
			if (r != OK)
				kprintf_stub("Kernel: writing soft fault marker %d " // MODIFIED
				    "into %d at 0x%lx failed (%d)\n",
				    sfinfo.value, sfinfo.endpt, sfinfo.addr,
				    r);

			return EFAULT; // EFAULT might be undefined
		}
		return r;
	}
	return virtual_copy_vmcheck(caller, &v_src, &v_dst, bytes);
}

/*===========================================================================*
 *				do_safecopy_to				     *
 *===========================================================================*/
int do_safecopy_to(struct proc * caller, message * m_ptr)
{
	return safecopy(caller, m_ptr->m_lsys_krn_safecopy.from_to, caller->p_endpoint,
		(cp_grant_id_t) m_ptr->m_lsys_krn_safecopy.gid,
		m_ptr->m_lsys_krn_safecopy.bytes, m_ptr->m_lsys_krn_safecopy.offset,
		(vir_bytes) m_ptr->m_lsys_krn_safecopy.address, CPF_WRITE);
}

/*===========================================================================*
 *				do_safecopy_from			     *
 *===========================================================================*/
int do_safecopy_from(struct proc * caller, message * m_ptr)
{
	return safecopy(caller, m_ptr->m_lsys_krn_safecopy.from_to, caller->p_endpoint,
		(cp_grant_id_t) m_ptr->m_lsys_krn_safecopy.gid,
		m_ptr->m_lsys_krn_safecopy.bytes, m_ptr->m_lsys_krn_safecopy.offset,
		(vir_bytes) m_ptr->m_lsys_krn_safecopy.address, CPF_READ);
}

/*===========================================================================*
 *				do_vsafecopy				     *
 *===========================================================================*/
int do_vsafecopy(struct proc * caller, message * m_ptr)
{
	static struct vscp_vec vec[SCPVEC_NR];
	static struct vir_addr src, dst;
	int r, i, els;
	k_size_t bytes; // MODIFIED size_t

	/* Set vector copy parameters. */
	src.proc_nr_e = caller->p_endpoint;
	KASSERT_PLACEHOLDER(src.proc_nr_e != NONE); // MODIFIED
	src.offset = (vir_bytes) m_ptr->m_lsys_krn_vsafecopy.vec_addr;
	dst.proc_nr_e = KERNEL;
	dst.offset = (vir_bytes) vec;

	/* No. of vector elements. */
	els = m_ptr->m_lsys_krn_vsafecopy.vec_size;
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
			kprintf_stub("vsafecopy: %d: element %d/%d: no SELF found\n", // MODIFIED
				caller->p_endpoint, i, els);
			return EINVAL; // EINVAL might be undefined
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
