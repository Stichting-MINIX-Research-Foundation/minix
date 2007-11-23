#ifndef PCI_HELPER
#define PCI_HELPER

_PROTOTYPE( unsigned pci_inb, (U16_t port) );
_PROTOTYPE( unsigned pci_inw, (U16_t port) );
_PROTOTYPE( unsigned pci_inl, (U16_t port) );

_PROTOTYPE( void pci_outb, (U16_t port, U8_t value) );
_PROTOTYPE( void pci_outw, (U16_t port, U16_t value) );
_PROTOTYPE( void pci_outl, (U16_t port, U32_t value) );

#endif
