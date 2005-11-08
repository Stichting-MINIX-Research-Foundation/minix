/*	queryparam.h - query program parameters		Author: Kees J. Bot
 *								22 Apr 1994
 */
#ifndef _MINIX__QUERYPARAM_H
#define _MINIX__QUERYPARAM_H

#include <ansi.h>

typedef size_t _mnx_size_t;

struct export_param_list {
	char	*name;		/* "variable", "[", ".field", or NULL. */
	void	*offset;	/* Address of a variable or field offset. */
	size_t	size;		/* Size of the resulting object. */
};

struct export_params {
	struct export_param_list *list;	/* List of exported parameters. */
	struct export_params	 *next;	/* Link several sets of parameters. */
};

#ifdef __STDC__
#define qp_stringize(var)	#var
#define qp_dotstringize(var)	"." #var
#else
#define qp_stringize(var)	"var"
#define qp_dotstringize(var)	".var"
#endif
#define QP_VARIABLE(var)	{ qp_stringize(var), &(var), sizeof(var) }
#define QP_ARRAY(var)		{ "[", 0, sizeof((var)[0]) }
#define QP_VECTOR(var,ptr,len)	{ qp_stringize(var), &(ptr), -1 },\
				{ "[", &(len), sizeof(*(ptr)) }
#define QP_FIELD(field, type)	{ qp_dotstringize(field), \
					(void *)offsetof(type, field), \
					sizeof(((type *)0)->field) }
#define QP_END()		{ 0, 0, 0 }

void qp_export _ARGS((struct export_params *_ex_params));
int queryparam _ARGS((int (*_qgetc) _ARGS((void)), void **_paddress,
							_mnx_size_t *_psize));
_mnx_size_t paramvalue _ARGS((char **_value, void *_address,
							_mnx_size_t _size));
#endif /* _MINIX__QUERYPARAM_H */

/* $PchId: queryparam.h,v 1.1 2005/06/28 14:31:26 philip Exp $ */
