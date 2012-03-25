#include "common.h"
#include <ddekit/printf.h>

/****************************************************************************/
/*     ddekit_print                                                         */
/****************************************************************************/
int ddekit_print(const char* c)
{
	return ddekit_printf(c);
}

/****************************************************************************/
/*     ddekit_printf                                                        */
/****************************************************************************/
int ddekit_printf(const char* fmt, ...)
{
	int r;
	va_list va;

	va_start(va,fmt);
	r = vprintf(fmt, va);
	va_end(va);
	
	return r;
}

/****************************************************************************/
/*     ddekit_vprintf                                                       */
/****************************************************************************/
int ddekit_vprintf(const char *fmt, va_list va) 
{
	return vprintf(fmt, va);
}
