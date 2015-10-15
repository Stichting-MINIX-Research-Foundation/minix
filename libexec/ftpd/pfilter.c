#include <stdio.h>
#include <blacklist.h>

#include "pfilter.h"

static struct blacklist *blstate;

void
pfilter_open(void)
{
	if (blstate == NULL)
		blstate = blacklist_open();
}

void
pfilter_notify(int what, const char *msg)
{
	pfilter_open();

	if (blstate == NULL)
		return;

	blacklist_r(blstate, what, 0, msg);
}
