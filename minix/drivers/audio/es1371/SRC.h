#ifndef SRC_H
#define SRC_H

#include "es1371.h"
#include "wait.h"

int SRCInit(DEV_STRUCT * DSP);
int SRCRegRead(DEV_STRUCT * DSP, u16_t reg, u16_t *data);
int SRCRegWrite(DEV_STRUCT * DSP, u16_t reg, u16_t val);
void SRCSetRate(DEV_STRUCT * DSP, char src_base, u16_t rate);


/* register/base and control equates for the SRC RAM */
#define SRC_SYNTH_FIFO      0x00
#define SRC_DAC_FIFO        0x20
#define SRC_ADC_FIFO        0x40
#define SRC_SYNTH_BASE      0x70
#define SRC_DAC_BASE        0x74
#define SRC_ADC_BASE        0x78
#define SRC_SYNTH_LVOL      0x7c
#define SRC_SYNTH_RVOL      0x7d
#define SRC_DAC_LVOL        0x7e
#define SRC_DAC_RVOL        0x7f
#define SRC_ADC_LVOL        0x6c
#define SRC_ADC_RVOL        0x6d

#define SRC_TRUNC_N_OFF     0x00
#define SRC_INT_REGS_OFF    0x01
#define SRC_ACCUM_FRAC_OFF  0x02
#define SRC_VFREQ_FRAC_OFF  0x03

/* miscellaneous control defines */
#define SRC_IOPOLL_COUNT    0x1000UL
#define SRC_WENABLE         (1UL << 24)
#define SRC_BUSY_BIT        23
#define SRC_BUSY            (1UL << SRC_BUSY_BIT)
#define SRC_DISABLE         (1UL << 22)
#define SRC_SYNTHFREEZE     (1UL << 21)
#define SRC_DACFREEZE       (1UL << 20)
#define SRC_ADCFREEZE       (1UL << 19)
#define SRC_CTLMASK         0x00780000UL

#endif /* SRC_H */
