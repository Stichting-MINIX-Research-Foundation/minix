
#ifndef _I386_TYPES_H
#define _I386_TYPES_H

#include <minix/sys_config.h>
#include <machine/stackframe.h>
#include <machine/fpu.h>
#include <sys/cdefs.h>

struct segdesc_s {		/* segment descriptor for protected mode */
  u16_t limit_low;
  u16_t base_low;
  u8_t base_middle;
  u8_t access;		/* |P|DL|1|X|E|R|A| */
  u8_t granularity;	/* |G|X|0|A|LIMT| */
  u8_t base_high;
} __attribute__((packed));

struct gatedesc_s {
  u16_t offset_low;
  u16_t selector;
  u8_t pad;                     /* |000|XXXXX| ig & trpg, |XXXXXXXX| task g */
  u8_t p_dpl_type;              /* |P|DL|0|TYPE| */
  u16_t offset_high;
} __attribute__((packed));

struct desctableptr_s {
  u16_t limit;
  u32_t base;
} __attribute__((packed));

typedef struct segframe {
	reg_t	p_cr3;		/* page table root */
	u32_t	*p_cr3_v;
	char	*fpu_state;
	int	p_kern_trap_style;
} segframe_t;

struct cpu_info {
	u8_t	vendor;
	u8_t	family;
	u8_t	model;
	u8_t	stepping;
	u32_t	freq;		/* in MHz */
	u32_t	flags[2];
};

typedef u32_t atomic_t;	/* access to an aligned 32bit value is atomic on i386 */

#endif /* #ifndef _I386_TYPES_H */

