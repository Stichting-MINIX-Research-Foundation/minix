/*	asm86.c - 80X86 assembly intermediate		Author: Kees J. Bot
 *								24 Dec 1993
 */
#define nil 0
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include "asm86.h"
#include "asmconv.h"
#include "token.h"

expression_t *new_expr(void)
/* Make a new cell to build an expression. */
{
	expression_t *e;

	e= allocate(nil, sizeof(*e));
	e->operator= -1;
	e->left= e->middle= e->right= nil;
	e->name= nil;
	e->magic= 31624;
	return e;
}

void del_expr(expression_t *e)
/* Delete an expression tree. */
{
	if (e != nil) {
		assert(e->magic == 31624);
		e->magic= 0;
		deallocate(e->name);
		del_expr(e->left);
		del_expr(e->middle);
		del_expr(e->right);
		deallocate(e);
	}
}

asm86_t *new_asm86(void)
/* Make a new cell to hold an 80X86 instruction. */
{
	asm86_t *a;

	a= allocate(nil, sizeof(*a));
	a->opcode= -1;
	get_file(&a->file, &a->line);
	a->optype= -1;
	a->oaz= 0;
	a->rep= ONCE;
	a->seg= DEFSEG;
	a->args= nil;
	a->magic= 37937;
	return a;
}

void del_asm86(asm86_t *a)
/* Delete an 80X86 instruction. */
{
	assert(a != nil);
	assert(a->magic == 37937);
	a->magic= 0;
	del_expr(a->args);
	deallocate(a);
}

int isregister(const char *name)
/* True if the string is a register name.  Return its size. */
{
	static char *regs[] = {
		"al", "bl", "cl", "dl", "ah", "bh", "ch", "dh",
		"ax", "bx", "cx", "dx", "si", "di", "bp", "sp",
		"cs", "ds", "es", "fs", "gs", "ss",
		"eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "esp",
		"cr0", "cr1", "cr2", "cr3",
		"st",
	};
	int reg;

	for (reg= 0; reg < arraysize(regs); reg++) {
		if (strcmp(name, regs[reg]) == 0) {
			return reg < 8 ? 1 : reg < 22 ? 2 : 4;
		}
	}
	return 0;
}
