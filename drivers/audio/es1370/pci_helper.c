/* best viewed with tabsize 4 */

#include "../../drivers.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>


#include "pci_helper.h"

#include "es1370.h"

/*===========================================================================*
 *			helper functions for I/O										 *
 *===========================================================================*/
PUBLIC unsigned pci_inb(U16_t port) {
	u32_t value;
	int s;
	if ((s=sys_inb(port, &value)) !=OK)
		printf("%s: warning, sys_inb failed: %d\n", DRIVER_NAME, s);
	return value;
}


PUBLIC unsigned pci_inw(U16_t port) {
	u32_t value;
	int s;
	if ((s=sys_inw(port, &value)) !=OK)
		printf("%s: warning, sys_inw failed: %d\n", DRIVER_NAME, s);
	return value;
}


PUBLIC unsigned pci_inl(U16_t port) {
	U32_t value;
	int s;
	if ((s=sys_inl(port, &value)) !=OK)
		printf("%s: warning, sys_inl failed: %d\n", DRIVER_NAME, s);
	return value;
}


PUBLIC void pci_outb(U16_t port, U8_t value) {
	int s;
	if ((s=sys_outb(port, value)) !=OK)
		printf("%s: warning, sys_outb failed: %d\n", DRIVER_NAME, s);
}


PUBLIC void pci_outw(U16_t port, U16_t value) {
	int s;
	if ((s=sys_outw(port, value)) !=OK)
		printf("%s: warning, sys_outw failed: %d\n", DRIVER_NAME, s);
}


PUBLIC void pci_outl(U16_t port, U32_t value) {
	int s;
	if ((s=sys_outl(port, value)) !=OK)
		printf("%s: warning, sys_outl failed: %d\n", DRIVER_NAME, s);
}

