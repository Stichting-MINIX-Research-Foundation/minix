
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *
fgetln(FILE *fp, size_t *lenp)
{
#define EXTRA 80
	char *buf = NULL;
	int used = 0, len = 0, remain = 0, final = 0;
	while(!final) {
		char *b;
		int r;
		if(remain < EXTRA) {
			int newlen;
			char *newbuf;
			newlen = len + EXTRA;
			if(!(newbuf = realloc(buf, newlen))) {
				if(buf) free(buf);
				return NULL;
			}
			buf = newbuf;
			len = newlen;
			remain += EXTRA;
		}
		buf[used] = '\0';
		if(!fgets(buf + used, remain, fp))
			break;
		r = strlen(buf+used);
		used += r;
		remain -= r;
		len += r;
	}
	*lenp = len;
	return buf;
}

