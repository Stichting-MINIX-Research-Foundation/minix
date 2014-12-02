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
	int or_link_up;
	int or_got_int;
	int or_tx_alive;
	int or_send_int;
	int or_need_reset;
	int or_report_link;

	/* Events */
	int connected;
	int last_linkstatus;

	/* Rx */
	phys_bytes or_rx_buf;
	u16_t rxfid[NR_RX_BUFS];
	int rx_length[NR_RX_BUFS];
	u8_t rx_buf[NR_RX_BUFS][IEEE802_11_FRAME_LEN];
	int rx_first;
	int rx_last;
	int rx_current;

	/* Tx */
	u16_t or_nicbuf_size;
	int or_tx_head;
	int or_tx_tail;
	int or_tx_busy;

	struct
	{
		int ret_busy;
		u16_t or_txfid;
	} or_tx;

	eth_stat_t or_stat;
	char or_name[sizeof(OR_NAME)];
	hermes_t hw;
} t_or;
