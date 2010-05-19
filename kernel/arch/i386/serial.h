
#ifndef _KERN_SERIAL_H
#define _KERN_SERIAL_H 1

#define THRREG  0
#define RBRREG  0
#define FICRREG 2
#define LSRREG  5
#define LCRREG  3
#define SPRREG  7

#define COM1_BASE       0x3F8
#define COM1_THR        (COM1_BASE + THRREG)
#define COM1_RBR (COM1_BASE + RBRREG)
#define COM1_LSR        (COM1_BASE + LSRREG)
#define         LSR_DR          0x01
#define         LSR_THRE        0x20
#define         LCR_DLA         0x80

#endif
