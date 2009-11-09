/*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*/

/*
  Module:	longint abs function
  Author:	Ceriel J.H. Jacobs
  Version:	$Header$
*/
long
absl(i)
	long i;
{
	return i >= 0 ? i : -i;
}
