.text

#define LD_ST_REG(reg)							;\
.globl	_ld_##reg							;\
.globl	_st_##reg							;\
									;\
_ld_##reg:								;\
	mov	4(%esp), %eax						;\
	mov	%eax,	%reg						;\
	ret								;\
									;\
_st_##reg:								;\
	mov	%reg,	%eax						;\
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
 * void ld_dr0(unsigned long value)
 * unsigned long st_dr0(void)
 */
LD_ST_REG(dr6)
LD_ST_REG(dr7)
