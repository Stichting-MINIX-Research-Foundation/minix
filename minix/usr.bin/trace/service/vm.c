
#include "inc.h"

#include <sys/mman.h>
#include <sys/resource.h>

static int
vm_brk_out(struct trace_proc * proc, const message * m_out)
{

	put_ptr(proc, "addr", (vir_bytes)m_out->m_lc_vm_brk.addr);

	return CT_DONE;
}

static const struct flags mmap_prot[] = {
	FLAG_ZERO(PROT_NONE),
	FLAG(PROT_READ),
	FLAG(PROT_WRITE),
	FLAG(PROT_EXEC),
};

static const struct flags mmap_flags[] = {
	FLAG(MAP_SHARED),
	FLAG(MAP_PRIVATE),
	FLAG(MAP_FIXED),
	FLAG(MAP_RENAME),
	FLAG(MAP_NORESERVE),
	FLAG(MAP_INHERIT),
	FLAG(MAP_HASSEMAPHORE),
	FLAG(MAP_TRYFIXED),
	FLAG(MAP_WIRED),
	FLAG_MASK(MAP_ANON | MAP_STACK, MAP_FILE),
	FLAG(MAP_ANON),
	FLAG(MAP_STACK),
	FLAG(MAP_UNINITIALIZED),
	FLAG(MAP_PREALLOC),
	FLAG(MAP_CONTIG),
	FLAG(MAP_LOWER16M),
	FLAG(MAP_LOWER1M),
	FLAG(MAP_THIRDPARTY),
	/* TODO: interpret alignments for which there is no constant */
	FLAG_MASK(MAP_ALIGNMENT_MASK, MAP_ALIGNMENT_64KB),
	FLAG_MASK(MAP_ALIGNMENT_MASK, MAP_ALIGNMENT_16MB),
	FLAG_MASK(MAP_ALIGNMENT_MASK, MAP_ALIGNMENT_4GB),
	FLAG_MASK(MAP_ALIGNMENT_MASK, MAP_ALIGNMENT_1TB),
	FLAG_MASK(MAP_ALIGNMENT_MASK, MAP_ALIGNMENT_256TB),
	FLAG_MASK(MAP_ALIGNMENT_MASK, MAP_ALIGNMENT_64PB),
};

static int
vm_mmap_out(struct trace_proc * proc, const message * m_out)
{

	if (m_out->m_mmap.flags & MAP_THIRDPARTY)
		put_endpoint(proc, "forwhom", m_out->m_mmap.forwhom);
	put_ptr(proc, "addr", (vir_bytes)m_out->m_mmap.addr);
	put_value(proc, "len", "%zu", m_out->m_mmap.len);
	put_flags(proc, "prot", mmap_prot, COUNT(mmap_prot), "0x%x",
	    m_out->m_mmap.prot);
	put_flags(proc, "flags", mmap_flags, COUNT(mmap_flags), "0x%x",
	    m_out->m_mmap.flags);
	put_fd(proc, "fd", m_out->m_mmap.fd);
	put_value(proc, "offset", "%"PRId64, m_out->m_mmap.offset);

	return CT_DONE;
}

static void
vm_mmap_in(struct trace_proc * proc, const message * __unused m_out,
	const message * m_in, int failed)
{

	if (!failed)
		put_ptr(proc, NULL, (vir_bytes)m_in->m_mmap.retaddr);
	else
		/* TODO: consider printing MAP_FAILED in the right cases */
		put_result(proc);
}

static int
vm_munmap_out(struct trace_proc * proc, const message * m_out)
{

	put_ptr(proc, "addr", (vir_bytes)m_out->m_mmap.addr);
	put_value(proc, "len", "%zu", m_out->m_mmap.len);

	return CT_DONE;
}

#define VM_CALL(c) [((VM_ ## c) - VM_RQ_BASE)]

static const struct call_handler vm_map[] = {
	VM_CALL(BRK) = HANDLER("brk", vm_brk_out, default_in),
	VM_CALL(MMAP) = HANDLER("mmap", vm_mmap_out, vm_mmap_in),
	VM_CALL(MUNMAP) = HANDLER("munmap", vm_munmap_out, default_in),
};

const struct calls vm_calls = {
	.endpt = VM_PROC_NR,
	.base = VM_RQ_BASE,
	.map = vm_map,
	.count = COUNT(vm_map)
};
