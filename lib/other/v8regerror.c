/*	regerror() - Default regexp error report	Author: Kees J. Bot
 *								12 Jun 1999
 *
 * A better version of this routine should be supplied by the user in
 * the program using regexps.
 */
#include <stdio.h>
#define const		/* avoid "const poisoning" */
#include <regexp.h>
#undef const

void regerror(char *message)
{
	fprintf(stderr, "regexp error: %s\n", message);
}
