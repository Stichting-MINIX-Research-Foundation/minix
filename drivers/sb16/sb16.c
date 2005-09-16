#include "sb16.h"

/*===========================================================================*
 *				mixer_set
 *===========================================================================*/
PUBLIC int mixer_set(reg, data) 
int reg;
int data;
{
	int i;

	sb16_outb(MIXER_REG, reg);
	for(i = 0; i < 100; i++);
	sb16_outb(MIXER_DATA, data);

	return OK;
}


/*===========================================================================*
 *				sb16_inb
 *===========================================================================*/
PUBLIC int sb16_inb(port)
int port;
{	
	int s, value = -1;

	if ((s=sys_inb(port, &value)) != OK)
		panic("SB16DSP","sys_inb() failed", s);
	
	return value;
}


/*===========================================================================*
 *				sb16_outb
 *===========================================================================*/
PUBLIC void sb16_outb(port, value)
int port;
int value;
{
	int s;
	
	if ((s=sys_outb(port, value)) != OK)
		panic("SB16DSP","sys_outb() failed", s);
}