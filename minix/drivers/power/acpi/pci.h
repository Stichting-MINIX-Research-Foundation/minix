#ifndef __ACPI_PCI_H__
#define __ACPI_PCI_H__

#include <minix/ipc.h>

void do_map_bridge(message *m);
void do_get_irq(message *m);

void pci_scan_devices(void);


#endif /* __ACPI_PCI_H__ */
