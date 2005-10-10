/*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*/

/* $Header$ */

/*
 *	these are the routines the routines to do 32 and  64-bit addition
 */

# ifdef	EXT_DEBUG
# include <stdio.h>
# endif

# include "FP_types.h"
# define	UNKNOWN -1
# define	TRUE	 1
# define	FALSE	 0
# define	MAXBIT	0x80000000L

	/*
	 *	add 64 bits
	 */
int
b64_add(e1,e2)
		/*
		 * pointers to 64 bit 'registers'
		 */
register	B64	*e1,*e2;
{
		register	int	overflow;
				int	carry;

			/* add higher pair of 32 bits */
	overflow = ((unsigned long) 0xFFFFFFFF - e1->h_32 < e2->h_32);
	e1->h_32 += e2->h_32;

			/* add lower pair of 32 bits */
	carry = ((unsigned long) 0xFFFFFFFF - e1->l_32 < e2->l_32);
	e1->l_32 += e2->l_32;
# ifdef	EXT_DEBUG
	printf("\t\t\t\t\tb64_add: overflow (%d); internal carry(%d)\n",
					overflow,carry);
	fflush(stdout);
# endif
	if ((carry) && (++e1->h_32 == 0))
		return(TRUE);		/* had a 64 bit overflow */
	return(overflow);		/* return status from higher add */
}
