
#include	<stdlib.h>
#include	<string.h>

int
setenv(const char *name, const char *val, int overwrite)
{
	char *bf;
	int r;

	if(!overwrite && getenv(name))
		return 0;

	if(!(bf=malloc(strlen(name)+strlen(val)+2)))
		return -1;

	strcpy(bf, name);
	strcat(bf, "=");
	strcat(bf, val);

	r = putenv(bf);

	return r == 0 ? 0 : -1;
}

