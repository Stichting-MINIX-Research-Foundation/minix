/*
eth_int.h

Copyright 1995 Philip Homburg
*/

#ifndef ETH_INT_H
#define ETH_INT_H

#define ETH_TYPE_HASH_NR	16
#define ETH_VLAN_HASH_NR	16

/* Assume that the arguments are a local variable */
#define ETH_HASH_VLAN(v,t)	\
	((t)= (((v) >> 8) ^ (v)), \
	(t)= (((t) >> 4) ^ (t)), \
	(t) & (ETH_VLAN_HASH_NR-1))
	
typedef struct eth_port
{
	int etp_flags;
	ether_addr_t etp_ethaddr;
	acc_t *etp_wr_pack, *etp_rd_pack;
	struct eth_fd *etp_sendq_head;
	struct eth_fd *etp_sendq_tail;
	struct eth_fd *etp_type_any;
	struct eth_fd *etp_type[ETH_TYPE_HASH_NR];
	event_t etp_sendev;

	/* VLAN support */
	u16_t etp_vlan;
	struct eth_port *etp_vlan_port;
	struct eth_port *etp_vlan_tab[ETH_VLAN_HASH_NR];
	struct eth_port *etp_vlan_next;

	osdep_eth_port_t etp_osdep;
} eth_port_t;

#define EPF_EMPTY	 0x0
#define EPF_ENABLED	 0x1
#define EPF_READ_IP	0x20
#define EPF_READ_SP	0x40

extern eth_port_t *eth_port_table;

extern int no_ethWritePort;	/* debug, consistency check */

void osdep_eth_init ARGS(( void ));
int eth_get_stat ARGS(( eth_port_t *eth_port, eth_stat_t *eth_stat ));
void eth_write_port ARGS(( eth_port_t *eth_port, acc_t *pack ));
void eth_arrive ARGS(( eth_port_t *port, acc_t *pack, size_t pack_size ));
void eth_set_rec_conf ARGS(( eth_port_t *eth_port, u32_t flags ));
void eth_restart_write ARGS(( eth_port_t *eth_port ));
void eth_loop_ev ARGS(( event_t *ev, ev_arg_t ev_arg ));
void eth_reg_vlan ARGS(( eth_port_t *eth_port, eth_port_t *vlan_port ));

#endif /* ETH_INT_H */

/*
 * $PchId: eth_int.h,v 1.9 2001/04/23 08:04:06 philip Exp $
 */
