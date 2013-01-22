/*	$NetBSD: util2.c,v 1.1.1.2 2008/05/18 14:29:52 aymeric Exp $ */

#include "config.h"

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "multibyte.h"

int
ucs2utf8(const CHAR_T *src, size_t len, char *dst)
{
    int i, j;

    for (i = 0, j = 0; i < len; ++i) {
	if (src[i] < 0x80)
	    dst[j++] = src[i];
	else if (src[i] < 0x800) {
	    dst[j++] = (src[i] >> 6) | 0xc0;
	    dst[j++] = (src[i] & 0x3f) | 0x80;
	} else {
	    dst[j++] = (src[i] >> 12) | 0xe0;
	    dst[j++] = ((src[i] >> 6) & 0x3f) | 0x80;
	    dst[j++] = (src[i] & 0x3f) | 0x80;
	}
    }

    return j;
}
