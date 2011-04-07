/**
 * @file
 * Ethernet Interface Skeleton
 *
 */

/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
 * All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 * 
 * Author: Adam Dunkels <adam@sics.se>
 *
 */

/*
 * This file is a skeleton for developing Ethernet network interface
 * drivers for lwIP. Add code to the low_level functions and do a
 * search-and-replace for the word "ethernetif" to replace it with
 * something that better describes your network interface.
 */

#include <minix/sysutil.h>

#include "lwip/opt.h"

#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/sys.h"
#include <lwip/stats.h>
#include <lwip/snmp.h>
#include <netif/etharp.h>

#include <net/gen/ether.h>
#include <net/gen/eth_io.h>

#include "proto.h"
#include "driver.h"

extern endpoint_t lwip_ep;

static err_t low_level_output(__unused struct netif *netif, struct pbuf *pbuf)
{
	struct nic * nic;

	nic = (struct nic *) netif->state;
	assert(&nic->netif == netif);

	debug_print("device /dev/%s", nic->name);

	if (driver_tx_enqueue(nic, pbuf) != OK)
		return ERR_MEM;

	/* if the driver is idle, start transmitting the packet */
	if (nic->state == DRV_IDLE) {
		if (!driver_tx(nic))
			return ERR_MEM;
	}

	return ERR_OK;
}

static void low_level_init(struct netif *netif)
{
	message m;
	struct nic * nic = (struct nic *) netif->state;

	assert(nic);

	/* set MAC hardware address length */
	netif->hwaddr_len = ETHARP_HWADDR_LEN;

	/* maximum transfer unit */
	netif->mtu = 1500;
	nic->max_pkt_sz = ETH_MAX_PACK_SIZE;
	nic->min_pkt_sz = ETH_MIN_PACK_SIZE;

	/* device capabilities */
	netif->flags = NETIF_FLAG_ETHARP;

        m.DL_MODE = DL_NOMODE;
        if (nic->flags & NWEO_EN_BROAD)
                m.DL_MODE |= DL_BROAD_REQ;
        if (nic->flags & NWEO_EN_MULTI)
                m.DL_MODE |= DL_MULTI_REQ;
        if (nic->flags & NWEO_EN_PROMISC)
                m.DL_MODE |= DL_PROMISC_REQ;

        m.m_type = DL_CONF;

	if (asynsend(((struct nic *)netif->state)->drv_ep , &m) != OK)
		printf("LWIP : ERROR cannot send DL_CONF to driver\n");
}


err_t ethernetif_init(struct netif *netif)
{
	/*
	 * Initialize the snmp variables and counters inside the struct netif.
	 * The last argument should be replaced with your link speed, in units
	 * of bits per second.
	NETIF_INIT_SNMP(netif, snmp_ifType_ethernet_csmacd, LINK_SPEED_OF_YOUR_NETIF_IN_BPS);
	 */

	netif->output = etharp_output;
	netif->linkoutput = low_level_output;

	/* initialize the hardware */
	low_level_init(netif);

	return ERR_OK;
}
