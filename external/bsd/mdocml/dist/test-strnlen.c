#include <string.h>

int
main(void)
{
	const char s[1] = { 'a' };
	return(1 != strnlen(s, 1));
}
