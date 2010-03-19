.text

#define LD_ST_REG(reg)							;\
.globl	ld_##reg							;\
.globl	st_##reg							;\
									;\
ld_##reg:								;\
	mov	4(%esp), %eax						;\
	mov	%eax,	%reg						;\
	ret								;\
									;\
st_##reg:								;\
	mov	%reg,	%eax						;\
	ret

/*
 * void ld_dr0(u32_t value)
 * u32_t st_dr0(void)
 */
LD_ST_REG(dr0)
LD_ST_REG(dr1)
LD_ST_REG(dr2)
LD_ST_REG(dr3)
LD_ST_REG(dr6)
LD_ST_REG(dr7)

