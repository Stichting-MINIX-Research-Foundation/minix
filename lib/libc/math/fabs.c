/*
 * (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 *
 * Author: Ceriel J.H. Jacobs
 */
/* $Header$ */

float
fabsf(float x)
{
	return  x < 0 ? -x : x;
}

double
fabs(double x)
{
	return  x < 0 ? -x : x;
}

