/*
ethern2h.c

Created:	Nov 12, 1992 by Philip Homburg
*/

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <net/gen/ether.h>
#include <net/gen/if_ether.h>

int
ether_ntohost(hostname, e)
char *hostname;
struct ether_addr *e;
{
	FILE *etherf;
	char b[256];
	struct ether_addr e_tmp;

	etherf= fopen(_PATH_ETHERS, "r");
	if (etherf == NULL)
		return 1;

	while(fgets(b, sizeof(b), etherf) != NULL)
	{
		if (ether_line(b, &e_tmp, hostname) == 0 && 
		memcmp(&e_tmp, e, sizeof(e_tmp)) == 0)
		{
			fclose(etherf);
			return 0;
		}
	}
	fclose(etherf);
	return 1;
}
