/*	$NetBSD: fmt.c,v 1.21 2007/12/12 22:55:43 lukem Exp $	*/

#include <sys/cdefs.h>
__RCSID("$NetBSD: fmt.c,v 1.21 2007/12/12 22:55:43 lukem Exp $");

#include <kvm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vis.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "ps.h"

void
fmt_puts(char *s, int *leftp)
{
	static char *v = 0;
	static int maxlen = 0;
	char *nv;
	int len, nlen;

	if (*leftp == 0)
		return;
	len = strlen(s) * 4 + 1;
	if (len > maxlen) {
		if (maxlen == 0)
			nlen = getpagesize();
		else
			nlen = maxlen;
		while (len > nlen)
			nlen *= 2;
		nv = realloc(v, nlen);
		if (nv == 0)
			return;
		v = nv;
		maxlen = nlen;
	}
	len = strvis(v, s, VIS_TAB | VIS_NL | VIS_CSTYLE);
	if (*leftp != -1) {
		if (len > *leftp) {
			v[*leftp] = '\0';
			*leftp = 0;
		} else
			*leftp -= len;
	}
	(void)printf("%s", v);
}

void
fmt_putc(int c, int *leftp)
{

	if (*leftp == 0)
		return;
	if (*leftp != -1)
		*leftp -= 1;
	putchar(c);
}
