#include <machine/asm.h>

#define LD_ST_REG(reg)							;\
ENTRY(ld_##reg)							;\
	movq	%rdi,	%reg						;\
	ret								;\
									;\
ENTRY(st_##reg)							;\
	movq	%reg,	%rax						;\
	ret

/*
 * void ld_dr0(phys_bytes value)
 * phys_bytes st_dr0(void)
 */
LD_ST_REG(dr0)
LD_ST_REG(dr1)
LD_ST_REG(dr2)
LD_ST_REG(dr3)

/*
 * void ld_dr6(unsigned long value)
 * unsigned long st_dr6(void)
 */
LD_ST_REG(dr6)
LD_ST_REG(dr7)
