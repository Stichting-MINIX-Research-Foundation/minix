#ifndef __FPU_SW__
#define __FPU_SW__

#include <stdint.h>

#define FPUSW_EXCEPTION_IE  0x0001
#define FPUSW_EXCEPTION_DE  0x0002
#define FPUSW_EXCEPTION_ZE  0x0004
#define FPUSW_EXCEPTION_OE  0x0008
#define FPUSW_EXCEPTION_UE  0x0010
#define FPUSW_EXCEPTION_PE  0x0020
#define FPUSW_STACK_FAULT   0x0040
#define FPUSW_ERROR_SUMMARY 0x0080
#define FPUSW_CONDITION_C0  0x0100
#define FPUSW_CONDITION_C1  0x0200
#define FPUSW_CONDITION_C2  0x0400
#define FPUSW_CONDITION_C3  0x4000
#define FPUSW_BUSY          0x8000

u16_t fpu_compare(double x, double y);
u16_t fpu_sw_get(void);
void fpu_sw_set(u16_t value);
u16_t fpu_xam(double value);

#endif /* !defined(__FPU_SW__) */

