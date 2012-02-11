#include <sys/cdefs.h>
#include "namespace.h"

#include <sys/types.h>
#include <paths.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <lib.h>

#ifdef __weak_alias
__weak_alias(getloadavg, _getloadavg)
#endif

/* Retrieve system load average information. */
int getloadavg(double *loadavg, int nelem)
{
  FILE *fp;
  int i;

  if(nelem < 1) {
	errno = ENOSPC;
	return -1;
  }

  if((fp = fopen(_PATH_PROC "loadavg", "r")) == NULL)
	return -1;

  for(i = 0; i < nelem; i++)
	if(fscanf(fp, "%lf", &loadavg[i]) != 1)
		break;

  fclose(fp);

  if (i == 0) {
	errno = ENOENT;
	return -1;
  }

  return i;
}
