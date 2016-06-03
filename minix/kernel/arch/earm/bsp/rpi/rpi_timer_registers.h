#ifndef _RPI_TIMER_REGISTERS_H
#define _RPI_TIMER_REGISTERS_H

#define RPI2_QA7_BASE           0x40000000

#define QA7_CORE0TIMER          0x40
#define QA7_CORE0INT            0x60

#define RPI2_IRQ_ARMTIMER       3

#define RPI_ST_BASE             0x3f003000

#define RPI_ST_CS               0x0
#define RPI_ST_CLO              0x4
#define RPI_ST_CHI              0x8
#define RPI_ST_C0               0xc
#define RPI_ST_C1               0x10
#define RPI_ST_C2               0x14
#define RPI_ST_C3               0x18

#define RPI_ST_M0               0x1
#define RPI_ST_M1               0x2
#define RPI_ST_M2               0x4
#define RPI_ST_M3               0x8

#define RPI_IRQ_ST_C3           67

#define RPI_ST_FREQ             1000000

#endif /* _RPI_TIMER_REGISTERS_H */
