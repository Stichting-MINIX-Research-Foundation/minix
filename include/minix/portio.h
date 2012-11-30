/*
minix/portio.h

Created:	Jan 15, 1992 by Philip Homburg
*/

#ifndef _PORTIO_H_
#define _PORTIO_H_

#include <sys/types.h>

unsigned inb(u16_t _port);
unsigned inw(u16_t _port);
unsigned inl(u16_t _port);
void outb(u16_t _port, u8_t _value);
void outw(u16_t _port, u16_t _value);
void outl(u16_t _port, u32_t _value);
void intr_disable(void);
void intr_enable(void);

#endif /* _PORTIO_H_ */
