/* best viewed with tabsize 4 */

#include <minix/drivers.h>

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>


#include "pci_helper.h"

#include "es1370.h"

/*===========================================================================*
 *			helper functions for I/O										 *
 *===========================================================================*/
u32_t pci_inb(u16_t port) {
	u32_t value;
	int s;
	if ((s=sys_inb(port, &value)) !=OK)
		printf("%s: warning, sys_inb failed: %d\n", DRIVER_NAME, s);
	return value;
}


u32_t pci_inw(u16_t port) {
	u32_t value;
	int s;
	if ((s=sys_inw(port, &value)) !=OK)
		printf("%s: warning, sys_inw failed: %d\n", DRIVER_NAME, s);
	return value;
}


u32_t pci_inl(u16_t port) {
	u32_t value;
	int s;
	if ((s=sys_inl(port, &value)) !=OK)
		printf("%s: warning, sys_inl failed: %d\n", DRIVER_NAME, s);
	return value;
}


void pci_outb(u16_t port, u8_t value) {
	int s;
	if ((s=sys_outb(port, value)) !=OK)
		printf("%s: warning, sys_outb failed: %d\n", DRIVER_NAME, s);
}


void pci_outw(u16_t port, u16_t value) {
	int s;
	if ((s=sys_outw(port, value)) !=OK)
		printf("%s: warning, sys_outw failed: %d\n", DRIVER_NAME, s);
}


void pci_outl(u16_t port, u32_t value) {
	int s;
	if ((s=sys_outl(port, value)) !=OK)
		printf("%s: warning, sys_outl failed: %d\n", DRIVER_NAME, s);
}

