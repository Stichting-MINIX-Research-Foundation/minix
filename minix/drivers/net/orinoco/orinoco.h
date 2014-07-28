/* 
 * orinoco.h
 *
 * This file contains the most important structure for the driver: t_or
 * and some configurable definitions
 *
 * Created by Stevens Le Blond <slblond@few.vu.nl> 
 *	  and Michael Valkering <mjvalker@cs.vu.nl>
 */

#include		<net/gen/ether.h>
#include		<net/gen/eth_io.h>

#define 		NR_RX_BUFS 32

#define			LARGE_KEY_LENGTH 13
#define                 IW_ESSID_MAX_SIZE 32
#define			IOVEC_NR 16	
#define			OR_NAME "orinoco#n"

#define			IEEE802_11_HLEN		30
#define			IEEE802_11_DATA_LEN	(2304)
#define			IEEE802_11_FRAME_LEN	(IEEE802_11_DATA_LEN + IEEE802_11_HLEN + 3) 

typedef struct s_or
{
	int or_irq;
	int or_hook_id;
	int or_mode;
	int or_flags;
	char *or_model;
	int or_client;
	int or_link_up;
	int or_got_int;
	int or_tx_alive;
	int or_send_int;
	int or_clear_rx;
	u32_t or_base_port;
	int or_need_reset;
	int or_report_link;

	/* Events */
	int or_ev_rx;
	int or_ev_tx;
	int or_ev_info;
	int or_ev_txexc;
	int or_ev_alloc;
	int connected;
	u16_t channel_mask;
	u16_t channel;
	u16_t ap_density;
	u16_t rts_thresh;
	int bitratemode;
	int last_linkstatus;
	int max_data_len;
	int port_type;

	/* Rx */
	phys_bytes or_rx_buf;
	vir_bytes or_read_s;
	u16_t rxfid[NR_RX_BUFS];
	int rx_length[NR_RX_BUFS];
	u8_t rx_buf[NR_RX_BUFS][IEEE802_11_FRAME_LEN];
	u8_t rx_offset[NR_RX_BUFS];
	int rx_first;
	int rx_last;
	int rx_current;

	/* Tx */
	u16_t or_nicbuf_size;
	vir_bytes or_transm_s;
	int or_tx_head;
	int or_tx_tail;

	struct
	{
		int ret_busy;
		u16_t or_txfid;
	} or_tx;
	u32_t or_ertxth;	

	/* PCI related */
	int or_seen;		
	int devind;

	/* 'large' items */
	irq_hook_t or_hook;
	eth_stat_t or_stat;
	message or_rx_mess;
	message or_tx_mess;
	ether_addr_t or_address;
	iovec_t or_iovec[IOVEC_NR];
	iovec_s_t or_iovec_s[IOVEC_NR];
	char or_name[sizeof (OR_NAME)];
	hermes_t hw;
	char nick[IW_ESSID_MAX_SIZE + 1];


} t_or;
