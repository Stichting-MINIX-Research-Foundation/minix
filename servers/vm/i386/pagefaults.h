
#ifndef _PAGEFAULTS_H
#define _PAGEFAULTS_H 1

#include <machine/vm.h>

#define PFERR_NOPAGE(e)	(!((e) & I386_VM_PFE_P))
#define PFERR_PROT(e)	(((e) & I386_VM_PFE_P))
#define PFERR_WRITE(e)	((e) & I386_VM_PFE_W)
#define PFERR_READ(e)	(!((e) & I386_VM_PFE_W))

#endif

