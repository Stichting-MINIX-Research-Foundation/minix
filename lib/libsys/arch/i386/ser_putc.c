#include "sysutil.h"

#define COM1_BASE       0x3F8
#define COM1_THR        (COM1_BASE + 0)
#define         LSR_THRE        0x20
#define COM1_LSR        (COM1_BASE + 5)

/*===========================================================================*
 *                               ser_putc			    	     *
 *===========================================================================*/
void ser_putc(char c)
{
        u32_t b;
        int i;
        int lsr, thr;
  
        lsr= COM1_LSR;
        thr= COM1_THR;
        for (i= 0; i<10000; i++)
        {
                if (sys_inb(lsr, &b) != OK)
			return;
                if (b & LSR_THRE)
                        break;
        }
        sys_outb(thr, c);
}
