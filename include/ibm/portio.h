/*
ibm/portio.h

Created:	Jan 15, 1992 by Philip Homburg
*/

#ifndef _PORTIO_H_
#define _PORTIO_H_

#ifndef _TYPES_H
#include <sys/types.h>
#endif

unsigned inb(U16_t _port);
unsigned inw(U16_t _port);
unsigned inl(U32_t _port);
void outb(U16_t _port, U8_t _value);
void outw(U16_t _port, U16_t _value);
void outl(U16_t _port, U32_t _value);
void insb(U16_t _port, void *_buf, size_t _count);
void insw(U16_t _port, void *_buf, size_t _count);
void insl(U16_t _port, void *_buf, size_t _count);
void outsb(U16_t _port, void *_buf, size_t _count);
void outsw(U16_t _port, void *_buf, size_t _count);
void outsl(U16_t _port, void *_buf, size_t _count);
void intr_disable(void);
void intr_enable(void);

#endif /* _PORTIO_H_ */
