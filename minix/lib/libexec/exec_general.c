#define _SYSTEM 1

#include <minix/type.h>
#include <minix/const.h>
#include <sys/param.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <libexec.h>
#include <string.h>
#include <assert.h>
#include <minix/ipc.h>
#include <minix/com.h>
#include <minix/callnr.h>
#include <minix/vm.h>
#include <minix/ipc.h>
#include <minix/syslib.h>
#include <sys/mman.h>
#include <machine/elf.h>

int libexec_alloc_mmap_prealloc_junk(struct exec_info *execi, vir_bytes vaddr, size_t len)
{
	if(minix_mmap_for(execi->proc_e, (void *) vaddr, len,
		PROT_READ|PROT_WRITE|PROT_EXEC,
		MAP_ANON|MAP_PREALLOC|MAP_UNINITIALIZED|MAP_FIXED, -1, 0) == MAP_FAILED) {
		return ENOMEM;
	}

	return OK;
}

int libexec_alloc_mmap_prealloc_cleared(struct exec_info *execi, vir_bytes vaddr, size_t len)
{
	if(minix_mmap_for(execi->proc_e, (void *) vaddr, len,
		PROT_READ|PROT_WRITE|PROT_EXEC,
		MAP_ANON|MAP_PREALLOC|MAP_FIXED, -1, 0) == MAP_FAILED) {
		return ENOMEM;
	}

	return OK;
}

int libexec_alloc_mmap_ondemand(struct exec_info *execi, vir_bytes vaddr, size_t len)
{
	if(minix_mmap_for(execi->proc_e, (void *) vaddr, len,
		PROT_READ|PROT_WRITE|PROT_EXEC,
		MAP_ANON|MAP_FIXED, -1, 0) == MAP_FAILED) {
		return ENOMEM;
	}

	return OK;
}

int libexec_clearproc_vm_procctl(struct exec_info *execi)
{
	return vm_procctl_clear(execi->proc_e);
}

int libexec_clear_sys_memset(struct exec_info *execi, vir_bytes vaddr, size_t len)
{
	return sys_memset(execi->proc_e, 0, vaddr, len);
}

int libexec_copy_memcpy(struct exec_info *execi,
	off_t off, vir_bytes vaddr, size_t len)
{
	assert(off + len <= execi->hdr_len);
	memcpy((char *) vaddr, (char *) execi->hdr + off, len);
	return OK;
}

int libexec_clear_memset(struct exec_info *execi, vir_bytes vaddr, size_t len)
{
	memset((char *) vaddr, 0, len);
	return OK;
}

int libexec_pm_newexec(endpoint_t proc_e, struct exec_info *e)
{
  int r;
  message m;

  memset(&m, 0, sizeof(m));
  m.m_type = PM_EXEC_NEW;
  m.m_lexec_pm_exec_new.endpt = proc_e;
  m.m_lexec_pm_exec_new.ptr = (vir_bytes)e;
  if ((r = ipc_sendrec(PM_PROC_NR, &m)) != OK) return(r);

  e->allow_setuid = !!m.m_pm_lexec_exec_new.suid;

  return(m.m_type);
}
