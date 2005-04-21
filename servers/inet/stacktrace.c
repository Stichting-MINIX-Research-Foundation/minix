/*
stacktrace.c

Created:	Jan 19, 1993 by Philip Homburg

Copyright 1995 Philip Homburg
*/

#include "inet.h"

PUBLIC void stacktrace()
{
	typedef unsigned int reg_t;
	reg_t bp, pc, hbp;
	extern reg_t get_bp ARGS(( void ));

	bp= get_bp();
	while(bp)
	{
		pc= ((reg_t *)bp)[1];
		hbp= ((reg_t *)bp)[0];
		printf("0x%lx ", (unsigned long)pc);
		if (hbp != 0 && hbp <= bp)
		{
			printf("???");
			break;
		}
		bp= hbp;
	}
	printf("\n");
}

/*
 * $PchId: stacktrace.c,v 1.6 1996/05/07 21:11:34 philip Exp $
 */
