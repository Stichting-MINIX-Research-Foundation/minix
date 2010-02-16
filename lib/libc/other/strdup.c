/*
lib/other/strdup.c
*/

#include <stdlib.h>
#include <string.h>

char *strdup(s1)
const char *s1;
{
	size_t len;
	char *s2;

	len= strlen(s1)+1;

	s2= malloc(len);
	if (s2 == NULL)
		return NULL;
	strcpy(s2, s1);

	return s2;
}

