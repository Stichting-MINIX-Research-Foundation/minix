#include	<ctype.h>

int tolower(int c) {
	return isupper(c) ? c - 'A' + 'a' : c ;
}
