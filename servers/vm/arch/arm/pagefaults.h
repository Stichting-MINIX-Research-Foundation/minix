
#ifndef _PAGEFAULTS_H
#define _PAGEFAULTS_H 1

#include <machine/vm.h>

#define PFERR_PROT(e)	((ARM_VM_PFE_FS(e) == ARM_VM_PFE_L1PERM) \
			 || (ARM_VM_PFE_FS(e) == ARM_VM_PFE_L2PERM))
#define PFERR_NOPAGE(e) (!PFERR_PROT(e))
#define PFERR_WRITE(e)	((e) & ARM_VM_PFE_W)
#define PFERR_READ(e)	(!((e) & ARM_VM_PFE_W))

#endif

