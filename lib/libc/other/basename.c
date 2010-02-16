/*
basename.c
*/

#include <libgen.h>
#include <string.h>

char *basename(path)
char *path;
{
	size_t len;
	char *cp;

	if (path == NULL)
		return ".";
	len= strlen(path);
	if (len == 0)
		return ".";
	while (path[len-1] == '/')
	{
		if (len == 1)
			return path;	/* just "/" */
		len--;
		path[len]= '\0';
	}
	cp= strrchr(path, '/');
	if (cp != NULL)
		return cp+1;
	return path;
}
