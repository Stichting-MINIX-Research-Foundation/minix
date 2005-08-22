/*
ibm/mii.c

Created:	Nov 2004 by Philip Homburg <philip@f-mnx.phicoh.com>

Media Independent (Ethernet) Interface functions
*/

#include "../drivers.h"
#if __minix_vmd
#include "config.h"
#endif

#include "mii.h"

/*===========================================================================*
 *				mii_print_stat_speed			     *
 *===========================================================================*/
PUBLIC void mii_print_stat_speed(stat, extstat)
u16_t stat;
u16_t extstat;
{
	int fs, ft;

	fs= 1;
	if (stat & MII_STATUS_EXT_STAT)
	{
		if (extstat & (MII_ESTAT_1000XFD | MII_ESTAT_1000XHD |
			MII_ESTAT_1000TFD | MII_ESTAT_1000THD))
		{
			printf("1000 Mbps: ");
			fs= 0;
			ft= 1;
			if (extstat & (MII_ESTAT_1000XFD | MII_ESTAT_1000XHD))
			{
				ft= 0;
				printf("X-");
				switch(extstat &
					(MII_ESTAT_1000XFD|MII_ESTAT_1000XHD))
				{
				case MII_ESTAT_1000XFD:	printf("FD"); break;
				case MII_ESTAT_1000XHD:	printf("HD"); break;
				default:		printf("FD/HD"); break;
				}
			}
			if (extstat & (MII_ESTAT_1000TFD | MII_ESTAT_1000THD))
			{
				if (!ft)
					printf(", ");
				ft= 0;
				printf("T-");
				switch(extstat &
					(MII_ESTAT_1000TFD|MII_ESTAT_1000THD))
				{
				case MII_ESTAT_1000TFD:	printf("FD"); break;
				case MII_ESTAT_1000THD:	printf("HD"); break;
				default:		printf("FD/HD"); break;
				}
			}
		}
	}
	if (stat & (MII_STATUS_100T4 |
		MII_STATUS_100XFD | MII_STATUS_100XHD |
		MII_STATUS_100T2FD | MII_STATUS_100T2HD))
	{
		if (!fs)
			printf(", ");
		fs= 0;
		printf("100 Mbps: ");
		ft= 1;
		if (stat & MII_STATUS_100T4)
		{
			printf("T4");
			ft= 0;
		}
		if (stat & (MII_STATUS_100XFD | MII_STATUS_100XHD))
		{
			if (!ft)
				printf(", ");
			ft= 0;
			printf("TX-");
			switch(stat & (MII_STATUS_100XFD|MII_STATUS_100XHD))
			{
			case MII_STATUS_100XFD:	printf("FD"); break;
			case MII_STATUS_100XHD:	printf("HD"); break;
			default:		printf("FD/HD"); break;
			}
		}
		if (stat & (MII_STATUS_100T2FD | MII_STATUS_100T2HD))
		{
			if (!ft)
				printf(", ");
			ft= 0;
			printf("T2-");
			switch(stat & (MII_STATUS_100T2FD|MII_STATUS_100T2HD))
			{
			case MII_STATUS_100T2FD:	printf("FD"); break;
			case MII_STATUS_100T2HD:	printf("HD"); break;
			default:		printf("FD/HD"); break;
			}
		}
	}
	if (stat & (MII_STATUS_10FD | MII_STATUS_10HD))
	{
		if (!fs)
			printf(", ");
		printf("10 Mbps: ");
		fs= 0;
		printf("T-");
		switch(stat & (MII_STATUS_10FD|MII_STATUS_10HD))
		{
		case MII_STATUS_10FD:	printf("FD"); break;
		case MII_STATUS_10HD:	printf("HD"); break;
		default:		printf("FD/HD"); break;
		}
	}
}

/*===========================================================================*
 *				mii_print_techab			     *
 *===========================================================================*/
PUBLIC void mii_print_techab(techab)
u16_t techab;
{
	int fs, ft;

	if ((techab & MII_ANA_SEL_M) != MII_ANA_SEL_802_3)
	{
		printf("strange selector 0x%x, value 0x%x",
			techab & MII_ANA_SEL_M,
			(techab & MII_ANA_TAF_M) >> MII_ANA_TAF_S);
		return;
	}
	fs= 1;
	if (techab & (MII_ANA_100T4 | MII_ANA_100TXFD | MII_ANA_100TXHD))
	{
		printf("100 Mbps: ");
		fs= 0;
		ft= 1;
		if (techab & MII_ANA_100T4)
		{
			printf("T4");
			ft= 0;
		}
		if (techab & (MII_ANA_100TXFD | MII_ANA_100TXHD))
		{
			if (!ft)
				printf(", ");
			ft= 0;
			printf("TX-");
			switch(techab & (MII_ANA_100TXFD|MII_ANA_100TXHD))
			{
			case MII_ANA_100TXFD:	printf("FD"); break;
			case MII_ANA_100TXHD:	printf("HD"); break;
			default:		printf("FD/HD"); break;
			}
		}
	}
	if (techab & (MII_ANA_10TFD | MII_ANA_10THD))
	{
		if (!fs)
			printf(", ");
		printf("10 Mbps: ");
		fs= 0;
		printf("T-");
		switch(techab & (MII_ANA_10TFD|MII_ANA_10THD))
		{
		case MII_ANA_10TFD:	printf("FD"); break;
		case MII_ANA_10THD:	printf("HD"); break;
		default:		printf("FD/HD"); break;
		}
	}
	if (techab & MII_ANA_PAUSE_SYM)
	{
		if (!fs)
			printf(", ");
		fs= 0;
		printf("pause(SYM)");
	}
	if (techab & MII_ANA_PAUSE_ASYM)
	{
		if (!fs)
			printf(", ");
		fs= 0;
		printf("pause(ASYM)");
	}
	if (techab & MII_ANA_TAF_RES)
	{
		if (!fs)
			printf(", ");
		fs= 0;
		printf("0x%x", (techab & MII_ANA_TAF_RES) >> MII_ANA_TAF_S);
	}
}

/*
 * $PchId: mii.c,v 1.2 2005/01/31 22:17:26 philip Exp $
 */
