#ifndef __FPU_CW__
#define __FPU_CW__

#include <stdint.h>

/* 
 * see section 8.1.5 "x87 FPU Control Word" in "Intel 64 and IA-32 Architectures 
 * Software Developer's Manual Volume 1 Basic Architecture" 
 */
#define FPUCW_EXCEPTION_MASK    0x003f
#define FPUCW_EXCEPTION_MASK_IM 0x0001
#define FPUCW_EXCEPTION_MASK_DM 0x0002
#define FPUCW_EXCEPTION_MASK_ZM 0x0004
#define FPUCW_EXCEPTION_MASK_OM 0x0008
#define FPUCW_EXCEPTION_MASK_UM 0x0010
#define FPUCW_EXCEPTION_MASK_PM 0x0020

#define FPUCW_PRECISION_CONTROL         0x0300
#define FPUCW_PRECISION_CONTROL_SINGLE  0x0000
#define FPUCW_PRECISION_CONTROL_DOUBLE  0x0200
#define FPUCW_PRECISION_CONTROL_XDOUBLE 0x0300

#define FPUCW_ROUNDING_CONTROL         0x0c00
#define FPUCW_ROUNDING_CONTROL_NEAREST 0x0000
#define FPUCW_ROUNDING_CONTROL_DOWN    0x0400
#define FPUCW_ROUNDING_CONTROL_UP      0x0800
#define FPUCW_ROUNDING_CONTROL_TRUNC   0x0c00

/* get and set FPU control word */
u16_t fpu_cw_get(void);
void fpu_cw_set(u16_t fpu_cw);

#endif /* !defined(__FPU_CW__) */
