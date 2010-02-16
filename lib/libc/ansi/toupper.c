#include	<ctype.h>

int toupper(int c) {
	return islower(c) ? c - 'a' + 'A' : c ;
}
