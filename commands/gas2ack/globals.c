/* 
 * Table of all global definitions. Since the ack convention is to prepend
 * syms with '_' for C interfacing, we need to know about them and add/remove
 * teh '_' as neccessary
 */

#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#include "asm86.h"

/* this should be fine for common minix assembly files */
#define SYM_MAX		1024
#define SYM_MAX_LEN	64

struct sym {
	char 	name[SYM_MAX_LEN];
	int	gl;
};

static struct sym syms[SYM_MAX];

static int syms_num = 0;

static struct sym * sym_exists(const char * n)
{
	int i;

	for (i = 0; i < syms_num; i++) {
		if (strcmp(syms[i].name, n) == 0)
			return &syms[i];
	}

	return NULL;
}

static int is_local_label_ref(const char *n)
{
	int i;
	int l = strlen(n);

	for(i = 0; i < l - 1; i++)
		if (!isdigit(n[i]))
			return 0;
	if (n[l-1] != 'b' && n[l-1] != 'f')
		return 0;

	return 1;
}

static int is_hex(const char *n)
{
	int i;
	for(i = 0; n[i]; i++)
		if (!isxdigit(n[i]))
			return 0;
	return 1;
}

static int is_dec(const char *n)
{
	int i;
	for(i = 0; n[i]; i++)
		if (!isdigit(n[i]))
			return 0;
	return 1;
}

static int is_number(const char * n)
{
	if (n[0] == '0' && n[1] == 'x')
		return is_hex(n + 2);
	else
		return is_dec(n);
}

int syms_is_global(const char * n)
{
	struct sym *s;

	if (!n || is_number(n) || is_local_label_ref(n) || isregister(n))
		return 0;
	
	/* if not found, it must be extern -> global */
	if (!(s = sym_exists(n)))
		return 1;

	return s->gl;
}

static int add(const char * n, int isgl)
{
	if (syms_num >= SYM_MAX)
		return -ENOMEM;
	if (!n || strlen(n) >= SYM_MAX_LEN)
		return -EINVAL;

	/* ignore numbers */
	if (is_number(n))
		return 0;

	strcpy(syms[syms_num].name, n);
	syms[syms_num].gl = isgl;
	syms_num++;

	return 0;
}

int syms_add(const char *n)
{
	return add(n, 0);
}

int syms_add_global(const char *n)
{
	return add(n, 1);
}

void syms_add_global_csl(expression_t * exp)
{
	if (!exp)
		return;

	if (exp->operator == ',') {
		syms_add_global_csl(exp->left);
		syms_add_global_csl(exp->right);
	}
	else {
		syms_add_global(exp->name);
	}
}

