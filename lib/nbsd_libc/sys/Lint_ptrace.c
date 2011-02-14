/* $NetBSD: Lint_ptrace.c,v 1.2 2000/06/14 06:49:10 cgd Exp $ */

/*
 * This file placed in the public domain.
 * Chris Demetriou, November 5, 1997.
 */

#include <unistd.h>

/*ARGSUSED*/
int
ptrace(request, pid, addr, data)
	int request;
	pid_t pid;
	caddr_t addr;
	int data;
{
	return (0);
}
