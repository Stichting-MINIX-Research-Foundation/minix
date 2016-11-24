#ifndef _IO_H
#define _IO_H

#include <sys/types.h>
#include <minix/syslib.h>
#include "vt6105.h"

/* I/O function */
static u8_t my_inb(u32_t port) {
	u32_t value;
	int r;
#ifdef DMA_REG_MODE
	value = *(u8_t *)(port);
#else
	if ((r = sys_inb(port, &value)) != OK)
		printf("vt6105: sys_inb failed: %d\n", r);
#endif
	return (u8_t)value;
}
#define vt_in8(port, offset) (my_inb((port) + (offset)))

static u16_t my_inw(u32_t port) {
	u32_t value;
	int r;
#ifdef DMA_REG_MODE
	value = *(u16_t *)(port);
#else
	if ((r = sys_inw(port, &value)) != OK)
		printf("vt6105: sys_inw failed: %d\n", r);
#endif
	return (u16_t)value;
}
#define vt_in16(port, offset) (my_inw((port) + (offset)))

static u32_t my_inl(u32_t port) {
	u32_t value;
	int r;
#ifdef DMA_REG_MODE
	value = *(u32_t *)(port);
#else
	if ((r = sys_inl(port, &value)) != OK)
		printf("vt6105: sys_inl failed: %d\n", r);
#endif
	return value;
}
#define vt_in32(port, offset) (my_inl((port) + (offset)))

static void my_outb(u32_t port, u8_t value) {
	int r;
#ifdef DMA_REG_MODE
	*(u8_t *)(port) = value;
#else
	if ((r = sys_outb(port, value)) != OK)
		printf("vt6105: sys_outb failed: %d\n", r);
#endif
}
#define vt_out8(port, offset, value) \
				(my_outb(((port) + (offset)), (value)))

static void my_outw(u32_t port, u16_t value) {
	int r;
#ifdef DMA_REG_MODE
	*(u16_t *)(port) = value;
#else
	if ((r = sys_outw(port, value)) != OK)
		printf("vt6105: sys_outw failed: %d\n", r);
#endif
}
#define vt_out16(port, offset, value) \
				(my_outw(((port) + (offset)), (value)))

static void my_outl(u16_t port, u32_t value) {
	int r;
#ifdef DMA_REG_MODE
	*(u32_t *)(port) = value;
#else
	if ((r = sys_outl(port, value)) != OK)
		printf("vt6105: sys_outl failed: %d\n", r);
#endif
}
#define vt_out32(port, offset, value) \
				(my_outl(((port) + (offset)), (value)))

#endif
