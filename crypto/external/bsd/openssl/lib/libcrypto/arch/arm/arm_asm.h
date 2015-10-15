#if defined (_ARM_ARCH_4T)
# define RET		bx		lr
#else
# define RET		mov		pc, lr
#endif
