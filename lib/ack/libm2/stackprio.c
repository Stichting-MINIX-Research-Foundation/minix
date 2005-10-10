/*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*/

/*
  Module:	Dummy priority routines
  Author:	Ceriel J.H. Jacobs
  Version:	$Header$
*/

static unsigned prio = 0;

stackprio(n)
	unsigned n;
{
	unsigned old = prio;

	if (n > prio) prio = n;
	return old;
}

unstackprio(n)
	unsigned n;
{
	prio = n;
}
