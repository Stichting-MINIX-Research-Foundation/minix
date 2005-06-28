/*
eth.c

Copyright 1995 Philip Homburg
*/

#include "inet.h"
#include "buf.h"
#include "clock.h"
#include "event.h"
#include "osdep_eth.h"
#include "type.h"

#include "assert.h"
#include "buf.h"
#include "eth.h"
#include "eth_int.h"
#include "io.h"
#include "sr.h"

THIS_FILE

#define ETH_FD_NR	(4*IP_PORT_MAX)
#define EXPIRE_TIME	60*HZ	/* seconds */

typedef struct eth_fd
{
	int ef_flags;
	nwio_ethopt_t ef_ethopt;
	eth_port_t *ef_port;
	struct eth_fd *ef_type_next;
	struct eth_fd *ef_send_next;
	int ef_srfd;
	acc_t *ef_rdbuf_head;
	acc_t *ef_rdbuf_tail;
	get_userdata_t ef_get_userdata;
	put_userdata_t ef_put_userdata;
	put_pkt_t ef_put_pkt;
	time_t ef_exp_time;
	size_t ef_write_count;
} eth_fd_t;

#define EFF_FLAGS	0xf
#	define EFF_EMPTY	0x0
#	define EFF_INUSE	0x1
#	define EFF_BUSY		0x6
#		define	EFF_READ_IP	0x2
#		define 	EFF_WRITE_IP	0x4
#	define EFF_OPTSET       0x8

/* Note that the vh_type field is normally considered part of the ethernet
 * header.
 */
typedef struct 
{
	u16_t vh_type;
	u16_t vh_vlan;
} vlan_hdr_t;

FORWARD int eth_checkopt ARGS(( eth_fd_t *eth_fd ));
FORWARD void hash_fd ARGS(( eth_fd_t *eth_fd ));
FORWARD void unhash_fd ARGS(( eth_fd_t *eth_fd ));
FORWARD void eth_buffree ARGS(( int priority ));
#ifdef BUF_CONSISTENCY_CHECK
FORWARD void eth_bufcheck ARGS(( void ));
#endif
FORWARD void packet2user ARGS(( eth_fd_t *fd, acc_t *pack, time_t exp_time ));
FORWARD void reply_thr_get ARGS(( eth_fd_t *eth_fd,
	size_t result, int for_ioctl ));
FORWARD void reply_thr_put ARGS(( eth_fd_t *eth_fd,
	size_t result, int for_ioctl ));
FORWARD void do_rec_conf ARGS(( eth_port_t *eth_port ));
FORWARD u32_t compute_rec_conf ARGS(( eth_port_t *eth_port ));
FORWARD acc_t *insert_vlan_hdr ARGS(( eth_port_t *eth_port, acc_t *pack ));

PUBLIC eth_port_t *eth_port_table;
PUBLIC int no_ethWritePort= 0;

PRIVATE eth_fd_t eth_fd_table[ETH_FD_NR];
PRIVATE ether_addr_t broadcast= { { 255, 255, 255, 255, 255, 255 } };

PUBLIC void eth_prep()
{
	eth_port_table= alloc(eth_conf_nr * sizeof(eth_port_table[0]));
}

PUBLIC void eth_init()
{
	int i, j;

	assert (BUF_S >= sizeof(nwio_ethopt_t));
	assert (BUF_S >= ETH_HDR_SIZE);	/* these are in fact static assertions,
					   thus a good compiler doesn't
					   generate any code for this */


	for (i=0; i<ETH_FD_NR; i++)
		eth_fd_table[i].ef_flags= EFF_EMPTY;
	for (i=0; i<eth_conf_nr; i++)
	{
		eth_port_table[i].etp_flags= EFF_EMPTY;
		eth_port_table[i].etp_sendq_head= NULL;
		eth_port_table[i].etp_sendq_tail= NULL;
		eth_port_table[i].etp_type_any= NULL;
		ev_init(&eth_port_table[i].etp_sendev);
		for (j= 0; j<ETH_TYPE_HASH_NR; j++)
			eth_port_table[i].etp_type[j]= NULL;
		for (j= 0; j<ETH_VLAN_HASH_NR; j++)
			eth_port_table[i].etp_vlan_tab[j]= NULL;
	}

#ifndef BUF_CONSISTENCY_CHECK
	bf_logon(eth_buffree);
#else
	bf_logon(eth_buffree, eth_bufcheck);
#endif

	osdep_eth_init();
}

PUBLIC int eth_open(port, srfd, get_userdata, put_userdata, put_pkt,
	select_res)
int port, srfd;
get_userdata_t get_userdata;
put_userdata_t put_userdata;
put_pkt_t put_pkt;
select_res_t select_res;
{
	int i;
	eth_port_t *eth_port;
	eth_fd_t *eth_fd;

	DBLOCK(0x20, printf("eth_open (%d, %d, %lx, %lx)\n", port, srfd, 
		(unsigned long)get_userdata, (unsigned long)put_userdata));
	eth_port= &eth_port_table[port];
	if (!(eth_port->etp_flags & EPF_ENABLED))
		return EGENERIC;

	for (i=0; i<ETH_FD_NR && (eth_fd_table[i].ef_flags & EFF_INUSE);
		i++);

	if (i>=ETH_FD_NR)
	{
		DBLOCK(1, printf("out of fds\n"));
		return EAGAIN;
	}

	eth_fd= &eth_fd_table[i];

	eth_fd->ef_flags= EFF_INUSE;
	eth_fd->ef_ethopt.nweo_flags=NWEO_DEFAULT;
	eth_fd->ef_port= eth_port;
	eth_fd->ef_srfd= srfd;
	assert(eth_fd->ef_rdbuf_head == NULL);
	eth_fd->ef_get_userdata= get_userdata;
	eth_fd->ef_put_userdata= put_userdata;
	eth_fd->ef_put_pkt= put_pkt;
	return i;
}

PUBLIC int eth_ioctl(fd, req)
int fd;
ioreq_t req;
{
	acc_t *data;
	eth_fd_t *eth_fd;
	eth_port_t *eth_port;

	DBLOCK(0x20, printf("eth_ioctl (%d, 0x%lx)\n", fd, (unsigned long)req));
	eth_fd= &eth_fd_table[fd];
	eth_port= eth_fd->ef_port;

	assert (eth_fd->ef_flags & EFF_INUSE);

	switch (req)
	{
	case NWIOSETHOPT:
		{
			nwio_ethopt_t *ethopt;
			nwio_ethopt_t oldopt, newopt;
			int result;
			u32_t new_en_flags, new_di_flags,
				old_en_flags, old_di_flags;

			data= (*eth_fd->ef_get_userdata)(eth_fd->
				ef_srfd, 0, sizeof(nwio_ethopt_t), TRUE);

                        ethopt= (nwio_ethopt_t *)ptr2acc_data(data);
			oldopt= eth_fd->ef_ethopt;
			newopt= *ethopt;

			old_en_flags= oldopt.nweo_flags & 0xffff;
			old_di_flags= (oldopt.nweo_flags >> 16) & 0xffff;
			new_en_flags= newopt.nweo_flags & 0xffff;
			new_di_flags= (newopt.nweo_flags >> 16) & 0xffff;
			if (new_en_flags & new_di_flags)
			{
				bf_afree(data);
				reply_thr_get (eth_fd, EBADMODE, TRUE);
				return NW_OK;
			}	

			/* NWEO_ACC_MASK */
			if (new_di_flags & NWEO_ACC_MASK)
			{
				bf_afree(data);
				reply_thr_get (eth_fd, EBADMODE, TRUE);
				return NW_OK;
			}	
					/* you can't disable access modes */

			if (!(new_en_flags & NWEO_ACC_MASK))
				new_en_flags |= (old_en_flags & NWEO_ACC_MASK);


			/* NWEO_LOC_MASK */
			if (!((new_en_flags | new_di_flags) & NWEO_LOC_MASK))
			{
				new_en_flags |= (old_en_flags & NWEO_LOC_MASK);
				new_di_flags |= (old_di_flags & NWEO_LOC_MASK);
			}

			/* NWEO_BROAD_MASK */
			if (!((new_en_flags | new_di_flags) & NWEO_BROAD_MASK))
			{
				new_en_flags |= (old_en_flags & NWEO_BROAD_MASK);
				new_di_flags |= (old_di_flags & NWEO_BROAD_MASK);
			}

			/* NWEO_MULTI_MASK */
			if (!((new_en_flags | new_di_flags) & NWEO_MULTI_MASK))
			{
				new_en_flags |= (old_en_flags & NWEO_MULTI_MASK);
				new_di_flags |= (old_di_flags & NWEO_MULTI_MASK);
				newopt.nweo_multi= oldopt.nweo_multi;
			}

			/* NWEO_PROMISC_MASK */
			if (!((new_en_flags | new_di_flags) & NWEO_PROMISC_MASK))
			{
				new_en_flags |= (old_en_flags & NWEO_PROMISC_MASK);
				new_di_flags |= (old_di_flags & NWEO_PROMISC_MASK);
			}

			/* NWEO_REM_MASK */
			if (!((new_en_flags | new_di_flags) & NWEO_REM_MASK))
			{
				new_en_flags |= (old_en_flags & NWEO_REM_MASK);
				new_di_flags |= (old_di_flags & NWEO_REM_MASK);
				newopt.nweo_rem= oldopt.nweo_rem;
			}

			/* NWEO_TYPE_MASK */
			if (!((new_en_flags | new_di_flags) & NWEO_TYPE_MASK))
			{
				new_en_flags |= (old_en_flags & NWEO_TYPE_MASK);
				new_di_flags |= (old_di_flags & NWEO_TYPE_MASK);
				newopt.nweo_type= oldopt.nweo_type;
			}

			/* NWEO_RW_MASK */
			if (!((new_en_flags | new_di_flags) & NWEO_RW_MASK))
			{
				new_en_flags |= (old_en_flags & NWEO_RW_MASK);
				new_di_flags |= (old_di_flags & NWEO_RW_MASK);
			}

			if (eth_fd->ef_flags & EFF_OPTSET)
				unhash_fd(eth_fd);

			newopt.nweo_flags= ((unsigned long)new_di_flags << 16) |
				new_en_flags;
			eth_fd->ef_ethopt= newopt;

			result= eth_checkopt(eth_fd);

			if (result<0)
				eth_fd->ef_ethopt= oldopt;
			else
			{
				unsigned long opt_flags;
				unsigned changes;
				opt_flags= oldopt.nweo_flags ^
					eth_fd->ef_ethopt.nweo_flags;
				changes= ((opt_flags >> 16) | opt_flags) &
					0xffff;
				if (changes & (NWEO_BROAD_MASK |
					NWEO_MULTI_MASK | NWEO_PROMISC_MASK))
				{
					do_rec_conf(eth_port);
				}
			}

			if (eth_fd->ef_flags & EFF_OPTSET)
				hash_fd(eth_fd);

			bf_afree(data);
			reply_thr_get (eth_fd, result, TRUE);
			return NW_OK;	
		}

	case NWIOGETHOPT:
		{
			nwio_ethopt_t *ethopt;
			acc_t *acc;
			int result;

			acc= bf_memreq(sizeof(nwio_ethopt_t));

			ethopt= (nwio_ethopt_t *)ptr2acc_data(acc);

			*ethopt= eth_fd->ef_ethopt;

			result= (*eth_fd->ef_put_userdata)(eth_fd->
				ef_srfd, 0, acc, TRUE);
			if (result >= 0)
				reply_thr_put(eth_fd, NW_OK, TRUE);
			return result;
		}
	case NWIOGETHSTAT:
		{
			nwio_ethstat_t *ethstat;
			acc_t *acc;
			int result;

			assert (sizeof(nwio_ethstat_t) <= BUF_S);

			eth_port= eth_fd->ef_port;
			if (!(eth_port->etp_flags & EPF_ENABLED))
			{
				reply_thr_put(eth_fd, EGENERIC, TRUE);
				return NW_OK;
			}

			acc= bf_memreq(sizeof(nwio_ethstat_t));
			compare (bf_bufsize(acc), ==, sizeof(*ethstat));

			ethstat= (nwio_ethstat_t *)ptr2acc_data(acc);
			ethstat->nwes_addr= eth_port->etp_ethaddr;

			if (!eth_port->etp_vlan)
			{
				result= eth_get_stat(eth_port,
					&ethstat->nwes_stat);
				assert (result == 0);
			}
			else
			{
				/* No statistics */
				memset(&ethstat->nwes_stat, '\0',
					sizeof(ethstat->nwes_stat));
			}

			result= (*eth_fd->ef_put_userdata)(eth_fd->
				ef_srfd, 0, acc, TRUE);
			if (result >= 0)
				reply_thr_put(eth_fd, NW_OK, TRUE);
			return result;
		}
	default:
		break;
	}
	reply_thr_put(eth_fd, EBADIOCTL, TRUE);
	return NW_OK;
}

PUBLIC int eth_write(fd, count)
int fd;
size_t count;
{
	eth_fd_t *eth_fd;
	eth_port_t *eth_port, *rep;
	acc_t *user_data;
	int r;

	eth_fd= &eth_fd_table[fd];
	eth_port= eth_fd->ef_port;

	if (!(eth_fd->ef_flags & EFF_OPTSET))
	{
		reply_thr_get (eth_fd, EBADMODE, FALSE);
		return NW_OK;
	}

	assert (!(eth_fd->ef_flags & EFF_WRITE_IP));

	eth_fd->ef_write_count= count;
	if (eth_fd->ef_ethopt.nweo_flags & NWEO_RWDATONLY)
		count += ETH_HDR_SIZE;

	if (count<ETH_MIN_PACK_SIZE || count>ETH_MAX_PACK_SIZE)
	{
		DBLOCK(1, printf("illegal packetsize (%d)\n",count));
		reply_thr_get (eth_fd, EPACKSIZE, FALSE);
		return NW_OK;
	}
	eth_fd->ef_flags |= EFF_WRITE_IP;

	/* Enqueue at the real ethernet port */
	rep= eth_port->etp_vlan_port;
	if (!rep)
		rep= eth_port;
	if (rep->etp_wr_pack)
	{
		eth_fd->ef_send_next= NULL;
		if (rep->etp_sendq_head)
			rep->etp_sendq_tail->ef_send_next= eth_fd;
		else
			rep->etp_sendq_head= eth_fd;
		rep->etp_sendq_tail= eth_fd;
		return NW_SUSPEND;
	}

	user_data= (*eth_fd->ef_get_userdata)(eth_fd->ef_srfd, 0,
		eth_fd->ef_write_count, FALSE);
	if (!user_data)
	{
		eth_fd->ef_flags &= ~EFF_WRITE_IP;
		reply_thr_get (eth_fd, EFAULT, FALSE);
		return NW_OK;
	}
	r= eth_send(fd, user_data, eth_fd->ef_write_count);
	assert(r == NW_OK);

	eth_fd->ef_flags &= ~EFF_WRITE_IP;
	reply_thr_get(eth_fd, eth_fd->ef_write_count, FALSE);
	return NW_OK;
}

PUBLIC int eth_send(fd, data, data_len)
int fd;
acc_t *data;
size_t data_len;
{
	eth_fd_t *eth_fd;
	eth_port_t *eth_port, *rep;
	eth_hdr_t *eth_hdr;
	acc_t *eth_pack;
	unsigned long nweo_flags;
	size_t count;
	ev_arg_t ev_arg;

	eth_fd= &eth_fd_table[fd];
	eth_port= eth_fd->ef_port;

	if (!(eth_fd->ef_flags & EFF_OPTSET))
		return EBADMODE;

	count= data_len;
	if (eth_fd->ef_ethopt.nweo_flags & NWEO_RWDATONLY)
		count += ETH_HDR_SIZE;

	if (count<ETH_MIN_PACK_SIZE || count>ETH_MAX_PACK_SIZE)
	{
		DBLOCK(1, printf("illegal packetsize (%d)\n",count));
		return EPACKSIZE;
	}
	rep= eth_port->etp_vlan_port;
	if (!rep)
		rep= eth_port;

	if (rep->etp_wr_pack)
		return NW_WOULDBLOCK;
	
	nweo_flags= eth_fd->ef_ethopt.nweo_flags;
	if (nweo_flags & NWEO_RWDATONLY)
	{
		eth_pack= bf_memreq(ETH_HDR_SIZE);
		eth_pack->acc_next= data;
	}
	else
		eth_pack= bf_packIffLess(data, ETH_HDR_SIZE);

	eth_hdr= (eth_hdr_t *)ptr2acc_data(eth_pack);

	if (nweo_flags & NWEO_REMSPEC)
		eth_hdr->eh_dst= eth_fd->ef_ethopt.nweo_rem;

	if (!(nweo_flags & NWEO_EN_PROMISC))
		eth_hdr->eh_src= eth_port->etp_ethaddr;

	if (nweo_flags & NWEO_TYPESPEC)
		eth_hdr->eh_proto= eth_fd->ef_ethopt.nweo_type;

	if (eth_addrcmp(eth_hdr->eh_dst, eth_port->etp_ethaddr) == 0)
	{
		/* Local loopback. */
		eth_port->etp_wr_pack= eth_pack;
		ev_arg.ev_ptr= eth_port;
		ev_enqueue(&eth_port->etp_sendev, eth_loop_ev, ev_arg);
		return NW_OK;
	}

	if (rep != eth_port)
	{
		eth_pack= insert_vlan_hdr(eth_port, eth_pack);
		if (!eth_pack)
		{
			/* Packet is silently discarded */
			return NW_OK;
		}
	}

	eth_write_port(rep, eth_pack);
	return NW_OK;
}

PUBLIC int eth_read (fd, count)
int fd;
size_t count;
{
	eth_fd_t *eth_fd;
	acc_t *pack;

	eth_fd= &eth_fd_table[fd];
	if (!(eth_fd->ef_flags & EFF_OPTSET))
	{
		reply_thr_put(eth_fd, EBADMODE, FALSE);
		return NW_OK;
	}
	if (count < ETH_MAX_PACK_SIZE)
	{
		reply_thr_put(eth_fd, EPACKSIZE, FALSE);
		return NW_OK;
	}

	assert(!(eth_fd->ef_flags & EFF_READ_IP));
	eth_fd->ef_flags |= EFF_READ_IP;

	while (eth_fd->ef_rdbuf_head)
	{
		pack= eth_fd->ef_rdbuf_head;
		eth_fd->ef_rdbuf_head= pack->acc_ext_link;
		if (get_time() <= eth_fd->ef_exp_time)
		{
			packet2user(eth_fd, pack, eth_fd->ef_exp_time);
			if (!(eth_fd->ef_flags & EFF_READ_IP))
				return NW_OK;
		}
		else
			bf_afree(pack);
	}
	return NW_SUSPEND;
}

PUBLIC int eth_cancel(fd, which_operation)
int fd;
int which_operation;
{
	eth_fd_t *eth_fd;

	DBLOCK(2, printf("eth_cancel (%d)\n", fd));
	eth_fd= &eth_fd_table[fd];

	switch (which_operation)
	{
	case SR_CANCEL_READ:
		assert (eth_fd->ef_flags & EFF_READ_IP);
		eth_fd->ef_flags &= ~EFF_READ_IP;
		reply_thr_put(eth_fd, EINTR, FALSE);
		break;
	case SR_CANCEL_WRITE:
		assert (eth_fd->ef_flags & EFF_WRITE_IP);
		eth_fd->ef_flags &= ~EFF_WRITE_IP;
		reply_thr_get(eth_fd, EINTR, FALSE);
		break;
	default:
		ip_panic(( "got unknown cancel request" ));
	}
	return NW_OK;
}

PUBLIC int eth_select(fd, operations)
int fd;
unsigned operations;
{
	printf("eth_select: not implemented\n");
	return 0;
}

PUBLIC void eth_close(fd)
int fd;
{
	eth_fd_t *eth_fd;
	eth_port_t *eth_port;
	acc_t *pack;

	eth_fd= &eth_fd_table[fd];

	assert ((eth_fd->ef_flags & EFF_INUSE) &&
		!(eth_fd->ef_flags & EFF_BUSY));

	if (eth_fd->ef_flags & EFF_OPTSET)
		unhash_fd(eth_fd);
	while (eth_fd->ef_rdbuf_head != NULL)
	{
		pack= eth_fd->ef_rdbuf_head;
		eth_fd->ef_rdbuf_head= pack->acc_ext_link;
		bf_afree(pack);
	}
	eth_fd->ef_flags= EFF_EMPTY;

	eth_port= eth_fd->ef_port;
	do_rec_conf(eth_port);
}

PUBLIC void eth_loop_ev(ev, ev_arg)
event_t *ev;
ev_arg_t ev_arg;
{
	acc_t *pack;
	eth_port_t *eth_port;

	eth_port= ev_arg.ev_ptr;
	assert(ev == &eth_port->etp_sendev);

	pack= eth_port->etp_wr_pack;

	assert(!no_ethWritePort);
	no_ethWritePort= 1;
	eth_arrive(eth_port, pack, bf_bufsize(pack));
	assert(no_ethWritePort);
	no_ethWritePort= 0;

	eth_port->etp_wr_pack= NULL;
	eth_restart_write(eth_port);
}

PRIVATE int eth_checkopt (eth_fd)
eth_fd_t *eth_fd;
{
/* bug: we don't check access modes yet */

	unsigned long flags;
	unsigned int en_di_flags;
	eth_port_t *eth_port;
	acc_t *pack;

	eth_port= eth_fd->ef_port;
	flags= eth_fd->ef_ethopt.nweo_flags;
	en_di_flags= (flags >>16) | (flags & 0xffff);

	if ((en_di_flags & NWEO_ACC_MASK) &&
		(en_di_flags & NWEO_LOC_MASK) &&
		(en_di_flags & NWEO_BROAD_MASK) &&
		(en_di_flags & NWEO_MULTI_MASK) &&
		(en_di_flags & NWEO_PROMISC_MASK) &&
		(en_di_flags & NWEO_REM_MASK) &&
		(en_di_flags & NWEO_TYPE_MASK) &&
		(en_di_flags & NWEO_RW_MASK))
	{
		eth_fd->ef_flags |= EFF_OPTSET;
	}
	else
		eth_fd->ef_flags &= ~EFF_OPTSET;

	while (eth_fd->ef_rdbuf_head != NULL)
	{
		pack= eth_fd->ef_rdbuf_head;
		eth_fd->ef_rdbuf_head= pack->acc_ext_link;
		bf_afree(pack);
	}

	return NW_OK;
}

PRIVATE void hash_fd(eth_fd)
eth_fd_t *eth_fd;
{
	eth_port_t *eth_port;
	int hash;

	eth_port= eth_fd->ef_port;
	if (eth_fd->ef_ethopt.nweo_flags & NWEO_TYPEANY)
	{
		eth_fd->ef_type_next= eth_port->etp_type_any;
		eth_port->etp_type_any= eth_fd;
	}
	else
	{
		hash= eth_fd->ef_ethopt.nweo_type;
		hash ^= (hash >> 8);
		hash &= (ETH_TYPE_HASH_NR-1);

		eth_fd->ef_type_next= eth_port->etp_type[hash];
		eth_port->etp_type[hash]= eth_fd;
	}
}

PRIVATE void unhash_fd(eth_fd)
eth_fd_t *eth_fd;
{
	eth_port_t *eth_port;
	eth_fd_t *prev, *curr, **eth_fd_p;
	int hash;

	eth_port= eth_fd->ef_port;
	if (eth_fd->ef_ethopt.nweo_flags & NWEO_TYPEANY)
	{
		eth_fd_p= &eth_port->etp_type_any;
	}
	else
	{
		hash= eth_fd->ef_ethopt.nweo_type;
		hash ^= (hash >> 8);
		hash &= (ETH_TYPE_HASH_NR-1);

		eth_fd_p= &eth_port->etp_type[hash];
	}
	for (prev= NULL, curr= *eth_fd_p; curr;
		prev= curr, curr= curr->ef_type_next)
	{
		if (curr == eth_fd)
			break;
	}
	assert(curr);
	if (prev)
		prev->ef_type_next= curr->ef_type_next;
	else
		*eth_fd_p= curr->ef_type_next;
}

PUBLIC void eth_restart_write(eth_port)
eth_port_t *eth_port;
{
	eth_fd_t *eth_fd;
	int r;

	assert(eth_port->etp_wr_pack == NULL);
	while (eth_fd= eth_port->etp_sendq_head, eth_fd != NULL)
	{
		if (eth_port->etp_wr_pack)
			return;
		eth_port->etp_sendq_head= eth_fd->ef_send_next;

		eth_fd->ef_flags &= ~EFF_WRITE_IP;
		r= eth_write(eth_fd-eth_fd_table, eth_fd->ef_write_count);
		assert(r == NW_OK);
	}
}

PUBLIC void eth_arrive (eth_port, pack, pack_size)
eth_port_t *eth_port;
acc_t *pack;
size_t pack_size;
{

	eth_hdr_t *eth_hdr;
	ether_addr_t *dst_addr;
	int pack_stat;
	ether_type_t type;
	eth_fd_t *eth_fd, *first_fd, *share_fd;
	int hash, i;
	u16_t vlan, temp;
	time_t exp_time;
	acc_t *vlan_pack, *hdr_acc, *tmp_acc;
	eth_port_t *vp;
	vlan_hdr_t vh;
	u32_t *p;

	exp_time= get_time() + EXPIRE_TIME;

	pack= bf_packIffLess(pack, ETH_HDR_SIZE);

	eth_hdr= (eth_hdr_t*)ptr2acc_data(pack);
	dst_addr= &eth_hdr->eh_dst;

	DIFBLOCK(0x20, dst_addr->ea_addr[0] != 0xFF &&
		(dst_addr->ea_addr[0] & 0x1),
		printf("got multicast packet\n"));

	if (dst_addr->ea_addr[0] & 0x1)
	{
		/* multi cast or broadcast */
		if (eth_addrcmp(*dst_addr, broadcast) == 0)
			pack_stat= NWEO_EN_BROAD;
		else
			pack_stat= NWEO_EN_MULTI;
	}
	else
	{
		if (eth_addrcmp (*dst_addr, eth_port->etp_ethaddr) == 0)
			pack_stat= NWEO_EN_LOC;
		else
			pack_stat= NWEO_EN_PROMISC;
	}
	type= eth_hdr->eh_proto;
	hash= type;
	hash ^= (hash >> 8);
	hash &= (ETH_TYPE_HASH_NR-1);

	if (type == HTONS(ETH_VLAN_PROTO))
	{
		/* VLAN packet. Extract original ethernet packet */

		vlan_pack= pack;
		vlan_pack->acc_linkC++;
		hdr_acc= bf_cut(vlan_pack, 0, 2*sizeof(ether_addr_t));
		vlan_pack= bf_delhead(vlan_pack, 2*sizeof(ether_addr_t));
		vlan_pack= bf_packIffLess(vlan_pack, sizeof(vh));
		vh= *(vlan_hdr_t *)ptr2acc_data(vlan_pack);
		vlan_pack= bf_delhead(vlan_pack, sizeof(vh));
		hdr_acc= bf_append(hdr_acc, vlan_pack);
		vlan_pack= hdr_acc; hdr_acc= NULL;
		if (bf_bufsize(vlan_pack) < ETH_MIN_PACK_SIZE)
		{
			tmp_acc= bf_memreq(sizeof(vh));

			/* Clear padding */
			assert(sizeof(vh) <= sizeof(*p));
			p= (u32_t *)ptr2acc_data(tmp_acc);
			*p= 0xdeadbeef;

			vlan_pack= bf_append(vlan_pack, tmp_acc);
			tmp_acc= NULL;
		}
		vlan= ntohs(vh.vh_vlan);
		if (vlan & ETH_TCI_CFI)
		{
			/* No support for extended address formats */
			bf_afree(vlan_pack); vlan_pack= NULL;
		}
		vlan &= ETH_TCI_VLAN_MASK;
	}
	else
	{
		/* No VLAN processing */
		vlan_pack= NULL;
		vlan= 0;	/* lint */
	}

	first_fd= NULL;
	for (i= 0; i<2; i++)
	{
		share_fd= NULL;

		eth_fd= (i == 0) ? eth_port->etp_type_any :
			eth_port->etp_type[hash];
		for (; eth_fd; eth_fd= eth_fd->ef_type_next)
		{
			if (i && eth_fd->ef_ethopt.nweo_type != type)
				continue;
			if (!(eth_fd->ef_ethopt.nweo_flags & pack_stat))
				continue;
			if (eth_fd->ef_ethopt.nweo_flags & NWEO_REMSPEC &&
				eth_addrcmp(eth_hdr->eh_src,
				eth_fd->ef_ethopt.nweo_rem) != 0)
			{
					continue;
			}
			if ((eth_fd->ef_ethopt.nweo_flags & NWEO_ACC_MASK) ==
				NWEO_SHARED)
			{
				if (!share_fd)
				{
					share_fd= eth_fd;
					continue;
				}
				if (!eth_fd->ef_rdbuf_head)
					share_fd= eth_fd;
				continue;
			}
			if (!first_fd)
			{
				first_fd= eth_fd;
				continue;
			}
			pack->acc_linkC++;
			packet2user(eth_fd, pack, exp_time);
		}
		if (share_fd)
		{
			pack->acc_linkC++;
			packet2user(share_fd, pack, exp_time);
		}
	}
	if (first_fd)
	{
		if (first_fd->ef_put_pkt &&
			(first_fd->ef_flags & EFF_READ_IP) &&
			!(first_fd->ef_ethopt.nweo_flags & NWEO_RWDATONLY))
		{
			(*first_fd->ef_put_pkt)(first_fd->ef_srfd, pack,
				pack_size);
		}
		else
			packet2user(first_fd, pack, exp_time);
	}
	else
	{
		if (pack_stat == NWEO_EN_LOC)
		{
			DBLOCK(0x01,
			printf("eth_arrive: dropping packet for proto 0x%x\n",
				ntohs(type)));
		}
		else
		{
			DBLOCK(0x20, printf("dropping packet for proto 0x%x\n",
				ntohs(type)));
		}			
		bf_afree(pack);
	}
	if (vlan_pack)
	{
		hash= ETH_HASH_VLAN(vlan, temp);
		for (vp= eth_port->etp_vlan_tab[hash]; vp;
			vp= vp->etp_vlan_next)
		{
			if (vp->etp_vlan == vlan)
				break;
		}
		if (vp)
		{
			eth_arrive(vp, vlan_pack, pack_size-sizeof(vh));
			vlan_pack= NULL;
		}
		else
		{
			/* No device for VLAN */
			bf_afree(vlan_pack);
			vlan_pack= NULL;
		}
	}
}

PUBLIC void eth_reg_vlan(eth_port, vlan_port)
eth_port_t *eth_port;
eth_port_t *vlan_port;
{
	u16_t t, vlan;
	int h;

	vlan= vlan_port->etp_vlan;
	h= ETH_HASH_VLAN(vlan, t);
	vlan_port->etp_vlan_next= eth_port->etp_vlan_tab[h];
	eth_port->etp_vlan_tab[h]= vlan_port;
}

PRIVATE void packet2user (eth_fd, pack, exp_time)
eth_fd_t *eth_fd;
acc_t *pack;
time_t exp_time;
{
	int result;
	acc_t *tmp_pack;
	size_t size;

	assert (eth_fd->ef_flags & EFF_INUSE);
	if (!(eth_fd->ef_flags & EFF_READ_IP))
	{
		if (pack->acc_linkC != 1)
		{
			tmp_pack= bf_dupacc(pack);
			bf_afree(pack);
			pack= tmp_pack;
			tmp_pack= NULL;
		}
		pack->acc_ext_link= NULL;
		if (eth_fd->ef_rdbuf_head == NULL)
		{
			eth_fd->ef_rdbuf_head= pack;
			eth_fd->ef_exp_time= exp_time;
		}
		else
			eth_fd->ef_rdbuf_tail->acc_ext_link= pack;
		eth_fd->ef_rdbuf_tail= pack;
		return;
	}

	if (eth_fd->ef_ethopt.nweo_flags & NWEO_RWDATONLY)
		pack= bf_delhead(pack, ETH_HDR_SIZE);

	size= bf_bufsize(pack);

	if (eth_fd->ef_put_pkt)
	{
		(*eth_fd->ef_put_pkt)(eth_fd->ef_srfd, pack, size);
		return;
	}

	eth_fd->ef_flags &= ~EFF_READ_IP;
	result= (*eth_fd->ef_put_userdata)(eth_fd->ef_srfd, (size_t)0, pack,
		FALSE);
	if (result >=0)
		reply_thr_put(eth_fd, size, FALSE);
	else
		reply_thr_put(eth_fd, result, FALSE);
}

PRIVATE void eth_buffree (priority)
int priority;
{
	int i;
	eth_fd_t *eth_fd;
	acc_t *pack;

	if (priority == ETH_PRI_FDBUFS_EXTRA)
	{
		for (i= 0, eth_fd= eth_fd_table; i<ETH_FD_NR; i++, eth_fd++)
		{
			while (eth_fd->ef_rdbuf_head &&
				eth_fd->ef_rdbuf_head->acc_ext_link)
			{
				pack= eth_fd->ef_rdbuf_head;
				eth_fd->ef_rdbuf_head= pack->acc_ext_link;
				bf_afree(pack);
			}
		}
	}
	if (priority == ETH_PRI_FDBUFS)
	{
		for (i= 0, eth_fd= eth_fd_table; i<ETH_FD_NR; i++, eth_fd++)
		{
			while (eth_fd->ef_rdbuf_head)
			{
				pack= eth_fd->ef_rdbuf_head;
				eth_fd->ef_rdbuf_head= pack->acc_ext_link;
				bf_afree(pack);
			}
		}
	}
}

#ifdef BUF_CONSISTENCY_CHECK
PRIVATE void eth_bufcheck()
{
	int i;
	eth_fd_t *eth_fd;
	acc_t *pack;

	for (i= 0; i<eth_conf_nr; i++)
	{
		bf_check_acc(eth_port_table[i].etp_rd_pack);
		bf_check_acc(eth_port_table[i].etp_wr_pack);
	}
	for (i= 0, eth_fd= eth_fd_table; i<ETH_FD_NR; i++, eth_fd++)
	{
		for (pack= eth_fd->ef_rdbuf_head; pack;
			pack= pack->acc_ext_link)
		{
			bf_check_acc(pack);
		}
	}
}
#endif

PRIVATE void do_rec_conf(eth_port)
eth_port_t *eth_port;
{
	int i;
	u32_t flags;
	eth_port_t *vp;

	if (eth_port->etp_vlan)
	{
		/* Configure underlying device */
		eth_port= eth_port->etp_vlan_port;
	}
	flags= compute_rec_conf(eth_port);
	for (i= 0; i<ETH_VLAN_HASH_NR; i++)
	{
		for (vp= eth_port->etp_vlan_tab[i]; vp; vp= vp->etp_vlan_next)
			flags |= compute_rec_conf(vp);
	}
	eth_set_rec_conf(eth_port, flags);
}

PRIVATE u32_t compute_rec_conf(eth_port)
eth_port_t *eth_port;
{
	eth_fd_t *eth_fd;
	u32_t flags;
	int i;

	flags= NWEO_NOFLAGS;
	for (i=0, eth_fd= eth_fd_table; i<ETH_FD_NR; i++, eth_fd++)
	{
		if ((eth_fd->ef_flags & (EFF_INUSE|EFF_OPTSET)) !=
			(EFF_INUSE|EFF_OPTSET))
		{
			continue;
		}
		if (eth_fd->ef_port != eth_port)
			continue;
		flags |= eth_fd->ef_ethopt.nweo_flags;
	}
	return flags;
}

PRIVATE void reply_thr_get (eth_fd, result, for_ioctl)
eth_fd_t *eth_fd;
size_t result;
int for_ioctl;
{
	acc_t *data;

	data= (*eth_fd->ef_get_userdata)(eth_fd->ef_srfd, result, 0, for_ioctl);
	assert (!data);	
}

PRIVATE void reply_thr_put (eth_fd, result, for_ioctl)
eth_fd_t *eth_fd;
size_t result;
int for_ioctl;
{
	int error;

	error= (*eth_fd->ef_put_userdata)(eth_fd->ef_srfd, result, (acc_t *)0,
		for_ioctl);
	assert(error == NW_OK);
}

PRIVATE acc_t *insert_vlan_hdr(eth_port, pack)
eth_port_t *eth_port;
acc_t *pack;
{
	acc_t *head_acc, *vh_acc;
	u16_t type, vlan;
	vlan_hdr_t *vp;

	head_acc= bf_cut(pack, 0, 2*sizeof(ether_addr_t));
	pack= bf_delhead(pack, 2*sizeof(ether_addr_t));
	pack= bf_packIffLess(pack, sizeof(type));
	type= *(u16_t *)ptr2acc_data(pack);
	if (type == HTONS(ETH_VLAN_PROTO))
	{
		/* Packeted is already tagged. Should update vlan number.
		 * For now, just discard packet.
		 */
		printf("insert_vlan_hdr: discarding vlan packet\n");
		bf_afree(head_acc); head_acc= NULL;
		bf_afree(pack); pack= NULL;
		return NULL;
	}
	vlan= eth_port->etp_vlan;	/* priority and CFI are zero */

	vh_acc= bf_memreq(sizeof(vlan_hdr_t));
	vp= (vlan_hdr_t *)ptr2acc_data(vh_acc);
	vp->vh_type= HTONS(ETH_VLAN_PROTO);
	vp->vh_vlan= htons(vlan);

	head_acc= bf_append(head_acc, vh_acc); vh_acc= NULL;
	head_acc= bf_append(head_acc, pack); pack= NULL;
	pack= head_acc; head_acc= NULL;
	return pack;
}

/*
 * $PchId: eth.c,v 1.23 2005/06/28 14:15:58 philip Exp $
 */
