#ifndef PCI_HELPER
#define PCI_HELPER

_PROTOTYPE( unsigned pci_inb, (u16_t port) );
_PROTOTYPE( unsigned pci_inw, (u16_t port) );
_PROTOTYPE( unsigned pci_inl, (u16_t port) );

_PROTOTYPE( void pci_outb, (u16_t port, u8_t value) );
_PROTOTYPE( void pci_outw, (u16_t port, u16_t value) );
_PROTOTYPE( void pci_outl, (u16_t port, u32_t value) );

#endif
