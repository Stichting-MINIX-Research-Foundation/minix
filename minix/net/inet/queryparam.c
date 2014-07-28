/*	queryparam() - allow program parameters to be queried
 *							Author: Kees J. Bot
 *								21 Apr 1994
 */
#define nil 0
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "queryparam.h"

#if EXAMPLE
struct stat st[2];

struct export_param_list ex_st_list[]= {
	QP_VARIABLE(st),
	QP_ARRAY(st),
	QP_FIELD(st_dev, struct stat),
	QP_FIELD(st_ino, struct stat),
	...
	QP_END()
};

struct buf { block_t b_blocknr; ... } *buf;
size_t nr_bufs;

struct export_param_list ex_buf_list[]=
	QP_VECTOR(buf, buf, nr_bufs),
	QP_FIELD(b_blocknr),
	...
	QP_END()
};

struct export_params ex_st= { ex_st_list, 0 };
struct export_params ex_buf= { ex_buf_list, 0 };
#endif

#define between(a, c, z)    ((unsigned) ((c) - (a)) <= (unsigned) ((z) - (a)))

static int isvar(int c)
{
	return between('a', c, 'z') || between('A', c, 'Z')
				|| between('0', c, '9') || c == '_';
}

static struct export_params *params;

void qp_export(struct export_params *ex_params)
{
	/* Add a set of exported parameters. */

	if (ex_params->next == nil) {
		ex_params->next= params;
		params= ex_params;
	}
}

int queryparam(int qgetc(void), void **poffset, size_t *psize)
{
	char *prefix;
	struct export_params *ep;
	struct export_param_list *epl;
	size_t offset= 0;
	size_t size= -1;
	size_t n;
	static size_t retval;
	int c, firstc;

	firstc= c= (*qgetc)();
	if (c == '&' || c == '$') c= (*qgetc)();
	if (!isvar(c)) goto fail;

	if ((ep= params) == nil) goto fail;
	epl= ep->list;

	while (c != 0 && c != ',') {
		prefix= (char *) "x";
		n= 0;

		for (;;) {
			while (epl->name == nil) {
				if ((ep= ep->next) == nil) goto fail;
				epl= ep->list;
			}
			if (strncmp(prefix, epl->name, n) == 0) {
				prefix= epl->name;
				while (prefix[n] != 0 && c == prefix[n]) {
					n++;
					c= (*qgetc)();
				}
			}
			if (prefix[n] == 0 && (!isvar(c) || prefix[0] == '[')) {
				/* Got a match. */
				break;
			}
			epl++;
		}

		if (prefix[0] == '[') {
			/* Array reference. */
			size_t idx= 0, cnt= 1, max= size / epl->size;

			while (between('0', c, '9')) {
				idx= idx * 10 + (c - '0');
				if (idx > max) goto fail;
				c= (*qgetc)();
			}
			if (c == ':') {
				cnt= 0;
				while (between('0', (c= (*qgetc)()), '9')) {
					cnt= cnt * 10 + (c - '0');
				}
			}
			if (c != ']') goto fail;
			if (idx + cnt > max) cnt= max - idx;
			offset+= idx * epl->size;
			size= cnt * epl->size;
			c= (*qgetc)();
		} else
		if (epl->size == -1) {
			/* Vector. */
			offset= (size_t) * (void **) epl->offset;
			size= (* (size_t *) epl[1].offset) * epl[1].size;
		} else {
			/* Variable or struct field. */
			offset+= (size_t) epl->offset;
			if ((size_t) epl->offset > size) goto fail;
			size-= (size_t) epl->offset;
			if (size < epl->size) goto fail;
			size= epl->size;
		}
	}
	if (firstc == '&' || firstc == '$') {
		retval= firstc == '&' ? offset : size;
		offset= (size_t) &retval;
		size= sizeof(retval);
	}
	if (c != 0 && c != ',') goto fail;
	*poffset= (void *) offset;
	*psize= size;
	return c != 0;
fail:
	while (c != 0 && c != ',') c= (*qgetc)();
	*poffset= nil;
	*psize= 0;
	return c != 0;
}

/*
 * $PchId: queryparam.c,v 1.1 2005/06/28 14:30:56 philip Exp $
 */
