
#include <lib.h>
#define vm_adddma	_vm_adddma
#define vm_deldma	_vm_deldma
#define vm_getdma	_vm_getdma
#include <minix/vm.h>
#include <unistd.h>
#include <stdarg.h>

int vm_adddma(req_proc_e, proc_e, start, size)
endpoint_t req_proc_e;
endpoint_t proc_e;
phys_bytes start;
phys_bytes size;
{
  message m;

  m.VMAD_REQ= req_proc_e;
  m.VMAD_EP= proc_e;
  m.VMAD_START= start;
  m.VMAD_SIZE= size;

  return _syscall(VM_PROC_NR, VM_ADDDMA, &m);
}

int vm_deldma(req_proc_e, proc_e, start, size)
endpoint_t req_proc_e;
endpoint_t proc_e;
phys_bytes start;
phys_bytes size;
{
  message m;

  m.VMDD_REQ= proc_e;
  m.VMDD_EP= proc_e;
  m.VMDD_START= start;
  m.VMDD_SIZE= size;

  return _syscall(VM_PROC_NR, VM_DELDMA, &m);
}

int vm_getdma(req_proc_e, procp, basep, sizep)
endpoint_t req_proc_e;
endpoint_t *procp;
phys_bytes *basep;
phys_bytes *sizep;
{
  int r;
  message m;

  m.VMGD_REQ = req_proc_e;

  r= _syscall(VM_PROC_NR, VM_GETDMA, &m);
  if (r == 0)
  {
	*procp= m.VMGD_PROCP;
	*basep= m.VMGD_BASEP;
	*sizep= m.VMGD_SIZEP;
  }
  return r;
}

