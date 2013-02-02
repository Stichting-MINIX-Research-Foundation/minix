#include "common.h"
#include <ddekit/panic.h>
#include <ddekit/printf.h>

/****************************************************************************/
/*      ddekit_panic                                                        */
/****************************************************************************/
void ddekit_panic(char *fmt, ...)
{ 
	
	int r;
    va_list va;

	printf("%c[31;1mPANIC: \033[0m\n",0x1b);
    va_start(va,fmt);
    r = vprintf(fmt, va);
    va_end(va);
	panic("panicced");	

	while(1)
		;
}

/****************************************************************************/
/*      ddekit_debug                                                        */
/****************************************************************************/
void ddekit_debug(char *fmt, ...)
{
    int r; 
    va_list va;
    va_start(va,fmt);
    r = vprintf(fmt, va);
    va_end(va); 
}
