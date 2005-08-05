/*
local.h
*/

#define ENABLE_WDETH 1
#define ENABLE_NE2000 1
#define ENABLE_3C503 1
#define ENABLE_PCI 1

struct dpeth;

/* 3c503.c */
_PROTOTYPE( int el2_probe, (struct dpeth* dep)				);

/* dp8390.c */
_PROTOTYPE( u8_t inb, (port_t port)					);
_PROTOTYPE( u16_t inw, (port_t port)					);
_PROTOTYPE( void outb, (port_t port, u8_t v)				);
_PROTOTYPE( void outw, (port_t port, u16_t v)				);

/* ne2000.c */
_PROTOTYPE( int ne_probe, (struct dpeth *dep)				);
_PROTOTYPE( void ne_init, (struct dpeth *dep)				);

/* rtl8029.c */
_PROTOTYPE( int rtl_probe, (struct dpeth *dep)				);

/* wdeth.c */
_PROTOTYPE( int wdeth_probe, (struct dpeth* dep)				);

