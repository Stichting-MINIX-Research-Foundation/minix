/*
ethera2n.c

Convert an ASCII string with an ethernet address into a struct ether_addr.

Created:	Nov 17, 1992 by Philip Homburg
*/

#include <sys/types.h>
#include <stdlib.h>
#include <net/netlib.h>
#include <net/gen/ether.h>
#include <net/gen/if_ether.h>

struct ether_addr *ether_aton(s)
_CONST char *s;
{
	static struct ether_addr ea;

	int i;
	long v;
	char *check;

	if (s == NULL)
		return NULL;

	for (i=0; i<6; i++)
	{
		v= strtol(s, &check, 16);
		if (v<0 || v>255)
			return NULL;
		if ((i<5 && check[0] != ':') || (i == 5 && check[0] != '\0'))
			return NULL;
		ea.ea_addr[i]= v;
		s= check+1;
	}
	return &ea;
}

/*
 * $PchId: ethera2n.c,v 1.3 1996/02/22 21:10:01 philip Exp $
 */
