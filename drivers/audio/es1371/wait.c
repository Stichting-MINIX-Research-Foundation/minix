#include <minix/drivers.h>
#include <sys/types.h>
#include "pci_helper.h"


int WaitBitd (int paddr, int bitno, int state, long tmout)
{
        unsigned long mask;

	mask = 1UL << bitno;
	tmout *= 5000;

	if(state) {
    while(tmout-- > 0) {
	    if(pci_inl(paddr) & mask) {
	      return 0;
	    }
	  }
	}	else {
    while(tmout-- > 0) {
	    if(!(pci_inl(paddr) & mask)) {
	      return 0;
	    }
	  }
	}
	return 0;
}
