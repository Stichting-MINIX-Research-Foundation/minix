#include <libgen.h>
#include <string.h>

/* based on http://www.opengroup.org/onlinepubs/009695399/functions/dirname.html */
char *dirname(char *path)
{
	char *pathend;

	/* remove leading slash(es) except root */
	pathend = path + strlen(path) - 1;
	while (pathend > path && *pathend == '/')
		pathend--;
	
	/* remove last path component */
	while (pathend >= path && *pathend != '/')
		pathend--;

	/* remove slash(es) before last path component except root */
	while (pathend > path && *pathend == '/')
		pathend--;

	/* special case: no slashes */
	if (pathend < path)
		return ".";

	/* truncate and return string */
	pathend[1] = 0;
	return path;
}
