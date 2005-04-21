/*
domainname.c
*/

#include <stdio.h>
#include <string.h>
#include <net/netlib.h>

int getdomainname(domain, size)
char *domain;
size_t size;
{
	FILE *domainfile;
	char *line;

	domainfile= fopen("/etc/domainname", "r");
	if (!domainfile)
	{
		return -1;
	}

	line= fgets(domain, size, domainfile);
	fclose(domainfile);
	if (!line)
		return -1;
	line= strchr(domain, '\n');
	if (line)
		*line= '\0';
	return 0;
}
