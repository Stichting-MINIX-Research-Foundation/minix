/*	getdomainname()					Author: Kees J. Bot
 *								2 Dec 1994
 */
#define nil 0
#include "namespace.h"
#include <sys/types.h>
#include <unistd.h>
#include <string.h>

#ifdef __weak_alias
__weak_alias(getdomainname, _getdomainname)
#endif

int getdomainname(char *result, size_t size)
{
	char nodename[256];
	char *domain;

	if (gethostname(nodename, sizeof(nodename)) < 0)
		return -1;
	nodename[sizeof(nodename)-1]= 0;
	if ((domain = strchr(nodename, '.')) != NULL)
		strncpy(result, domain+1, size);
	else
		result[0] = '\0';

	if (size > 0) result[size-1]= 0;
	return 0;
}
