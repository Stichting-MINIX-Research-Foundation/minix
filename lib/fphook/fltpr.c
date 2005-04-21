#include	<stdio.h>
#include	<stdlib.h>
#include	"../stdio/loc_incl.h"

int _fp_hook = 1;

char *
_f_print(va_list *ap, int flags, char *s, char c, int precision)
{
	fprintf(stderr,"cannot print floating point\n");
	exit(EXIT_FAILURE);
}
