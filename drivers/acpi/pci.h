#ifndef __ACPI_PCI_H__
#define __ACPI_PCI_H__

#include <minix/ipc.h>

_PROTOTYPE(void do_map_bridge, (message *m));
_PROTOTYPE(void do_get_irq, (message *m));

_PROTOTYPE(void pci_scan_devices, (void));


#endif /* __ACPI_PCI_H__ */
