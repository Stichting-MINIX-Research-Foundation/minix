/*
tcp_send.c

Copyright 1995 Philip Homburg
*/

#include "inet.h"
#include "buf.h"
#include "clock.h"
#include "event.h"
#include "type.h"
#include "sr.h"

#include "assert.h"
#include "io.h"
#include "ip.h"
#include "tcp.h"
#include "tcp_int.h"

THIS_FILE

FORWARD acc_t *make_pack ARGS(( tcp_conn_t *tcp_conn ));
FORWARD void tcp_send_timeout ARGS(( int conn, struct timer *timer ));
FORWARD void do_snd_event ARGS(( event_t *ev, ev_arg_t arg ));

PUBLIC void tcp_conn_write (tcp_conn, enq)
tcp_conn_t *tcp_conn;
int enq;				/* Writes need to be enqueued. */
{
	tcp_port_t *tcp_port;
	ev_arg_t snd_arg;

	assert (tcp_conn->tc_flags & TCF_INUSE);

	tcp_port= tcp_conn->tc_port;
	if (tcp_conn->tc_flags & TCF_MORE2WRITE)
		return;

	/* Do we really have something to send here? */
	if (tcp_conn->tc_SND_UNA == tcp_conn->tc_SND_NXT &&
		!(tcp_conn->tc_flags & TCF_SEND_ACK) &&
		!tcp_conn->tc_frag2send)
	{
		return;
	}

	tcp_conn->tc_flags |= TCF_MORE2WRITE;
	tcp_conn->tc_send_link= NULL;
	if (!tcp_port->tp_snd_head)
	{
		tcp_port->tp_snd_head= tcp_conn;
		tcp_port->tp_snd_tail= tcp_conn;
		if (enq)
		{
			snd_arg.ev_ptr= tcp_port;
			if (!ev_in_queue(&tcp_port->tp_snd_event))
			{
				ev_enqueue(&tcp_port->tp_snd_event,
					do_snd_event, snd_arg);
			}
		}
		else
			tcp_port_write(tcp_port);
	}
	else
	{
		tcp_port->tp_snd_tail->tc_send_link= tcp_conn;
		tcp_port->tp_snd_tail= tcp_conn;
	}
}

PRIVATE void do_snd_event(ev, arg)
event_t *ev;
ev_arg_t arg;
{
	tcp_port_t *tcp_port;

	tcp_port= arg.ev_ptr;

	assert(ev == &tcp_port->tp_snd_event);
	tcp_port_write(tcp_port);
}

PUBLIC void tcp_port_write(tcp_port)
tcp_port_t *tcp_port;
{
	tcp_conn_t *tcp_conn;
	acc_t *pack2write;
	int r;

	assert (!(tcp_port->tp_flags & TPF_WRITE_IP));

	while(tcp_port->tp_snd_head)
	{
		tcp_conn= tcp_port->tp_snd_head;
		assert(tcp_conn->tc_flags & TCF_MORE2WRITE);

		for(;;)
		{
			if (tcp_conn->tc_frag2send)
			{
				pack2write= tcp_conn->tc_frag2send;
				tcp_conn->tc_frag2send= 0;
			}
			else
			{
				tcp_conn->tc_busy++;
				pack2write= make_pack(tcp_conn);
				tcp_conn->tc_busy--;
				if (!pack2write)
					break;
			}
			r= ip_send(tcp_port->tp_ipfd, pack2write,
				bf_bufsize(pack2write));
			if (r != NW_OK)
			{
				if (r == NW_WOULDBLOCK)
					break;
				if (r == EPACKSIZE)
				{
					tcp_mtu_exceeded(tcp_conn);
					continue;
				}
				if (r == EDSTNOTRCH)
				{
					tcp_notreach(tcp_conn);
					continue;
				}
				if (r == EBADDEST)
					continue;
			}
			assert(r == NW_OK ||
				(printf("ip_send failed, error %d\n", r),0));
		}

		if (pack2write)
		{
			tcp_port->tp_flags |= TPF_WRITE_IP;
			tcp_port->tp_pack= pack2write;

			r= ip_write (tcp_port->tp_ipfd,
				bf_bufsize(pack2write));
			if (r == NW_SUSPEND)
			{
				tcp_port->tp_flags |= TPF_WRITE_SP;
				return;
			}
			assert(r == NW_OK);
			tcp_port->tp_flags &= ~TPF_WRITE_IP;
			assert(!(tcp_port->tp_flags &
				(TPF_WRITE_IP|TPF_WRITE_SP)));
			continue;
		}
		tcp_conn->tc_flags &= ~TCF_MORE2WRITE;
		tcp_port->tp_snd_head= tcp_conn->tc_send_link;

	}
}

PRIVATE acc_t *make_pack(tcp_conn)
tcp_conn_t *tcp_conn;
{
	acc_t *pack2write, *tmp_pack, *tcp_pack;
	tcp_hdr_t *tcp_hdr;
	ip_hdr_t *ip_hdr;
	int tot_hdr_size, ip_hdr_len, no_push, head, more2write;
	u32_t seg_seq, seg_lo_data, queue_lo_data, seg_hi, seg_hi_data;
	u16_t seg_up, mss;
	u8_t seg_flags;
	size_t pack_size;
	clock_t curr_time, new_dis;
	u8_t *optptr;

	mss= tcp_conn->tc_mtu-IP_TCP_MIN_HDR_SIZE;

	assert(tcp_conn->tc_busy);
	curr_time= get_time();
	switch (tcp_conn->tc_state)
	{
	case TCS_CLOSED:
	case TCS_LISTEN:
		return NULL;
	case TCS_SYN_RECEIVED:
	case TCS_SYN_SENT:

		if (tcp_conn->tc_SND_TRM == tcp_conn->tc_SND_NXT &&
			!(tcp_conn->tc_flags & TCF_SEND_ACK))
		{
			return 0;
		}

		tcp_conn->tc_flags &= ~TCF_SEND_ACK;

		/* Advertise a mss based on the port mtu. The current mtu may
		 * be lower if the other side sends a smaller mss.
		 */
		mss= tcp_conn->tc_port->tp_mtu-IP_TCP_MIN_HDR_SIZE;

		/* Include a max segment size option. */
		assert(tcp_conn->tc_tcpopt == NULL);
		tcp_conn->tc_tcpopt= bf_memreq(4);
		optptr= (u8_t *)ptr2acc_data(tcp_conn->tc_tcpopt);
		optptr[0]= TCP_OPT_MSS;
		optptr[1]= 4;
		optptr[2]= mss >> 8;
		optptr[3]= mss & 0xFF;

		pack2write= tcp_make_header(tcp_conn, &ip_hdr, &tcp_hdr, 
			(acc_t *)0);

		bf_afree(tcp_conn->tc_tcpopt);
		tcp_conn->tc_tcpopt= NULL;

		if (!pack2write)
		{
			DBLOCK(1, printf("connection closed while inuse\n"));
			return 0;
		}
		tot_hdr_size= bf_bufsize(pack2write);
		seg_seq= tcp_conn->tc_SND_TRM;
		if (tcp_conn->tc_state == TCS_SYN_SENT)
			seg_flags= 0;
		else
			seg_flags= THF_ACK;	/* except for TCS_SYN_SENT
						 * ack is always present */

		if (seg_seq == tcp_conn->tc_ISS)
		{
			assert(tcp_conn->tc_transmit_timer.tim_active ||
				(tcp_print_conn(tcp_conn), printf("\n"), 0));
			seg_flags |= THF_SYN;
			tcp_conn->tc_SND_TRM++;
		}

		tcp_hdr->th_seq_nr= htonl(seg_seq);
		tcp_hdr->th_ack_nr= htonl(tcp_conn->tc_RCV_NXT);
		tcp_hdr->th_flags= seg_flags;
		tcp_hdr->th_window= htons(mss);
			/* Initially we allow one segment */

		ip_hdr->ih_length= htons(tot_hdr_size);

		pack2write->acc_linkC++;
		ip_hdr_len= (ip_hdr->ih_vers_ihl & IH_IHL_MASK) << 2;
		tcp_pack= bf_delhead(pack2write, ip_hdr_len);
		tcp_hdr->th_chksum= ~tcp_pack_oneCsum(ip_hdr, tcp_pack);
		bf_afree(tcp_pack);

		new_dis= curr_time + 2*HZ*tcp_conn->tc_ttl;
		if (new_dis > tcp_conn->tc_senddis)
			tcp_conn->tc_senddis= new_dis;
		return pack2write;

	case TCS_ESTABLISHED:
	case TCS_CLOSING:
		seg_seq= tcp_conn->tc_SND_TRM;

		seg_flags= 0;
		pack2write= 0;
		seg_up= 0;
		if (tcp_conn->tc_flags & TCF_SEND_ACK)
		{
			seg_flags= THF_ACK;
			tcp_conn->tc_flags &= ~TCF_SEND_ACK;

			pack2write= tcp_make_header (tcp_conn, &ip_hdr, 
				&tcp_hdr, (acc_t *)0);
			if (!pack2write)
			{
				return NULL;
			}
		}

		if (tcp_conn->tc_SND_UNA != tcp_conn->tc_SND_NXT)
		{
			assert(tcp_LEmod4G(seg_seq, tcp_conn->tc_SND_NXT));

			if (seg_seq == tcp_conn->tc_snd_cwnd)
			{
				DBLOCK(2,
					printf("no data: window is closed\n"));
				goto after_data;
			}

			/* Assert that our SYN has been ACKed. */
			assert(tcp_conn->tc_SND_UNA != tcp_conn->tc_ISS);

			seg_lo_data= seg_seq;
			queue_lo_data= tcp_conn->tc_SND_UNA;

			seg_hi= tcp_conn->tc_SND_NXT;
			seg_hi_data= seg_hi;
			if (tcp_conn->tc_flags & TCF_FIN_SENT)
			{
				if (seg_seq != seg_hi)
					seg_flags |= THF_FIN;
				if (queue_lo_data == seg_hi_data)
					queue_lo_data--;
				if (seg_lo_data == seg_hi_data)
					seg_lo_data--;
				seg_hi_data--;
			}

			if (!pack2write)
			{
				pack2write= tcp_make_header (tcp_conn,
					&ip_hdr, &tcp_hdr, (acc_t *)0);
				if (!pack2write)
				{
					return NULL;
				}
			}

			tot_hdr_size= bf_bufsize(pack2write);

			no_push= (tcp_LEmod4G(tcp_conn->tc_SND_PSH, seg_seq));
			head= (seg_seq == tcp_conn->tc_SND_UNA);
			if (no_push)
			{
				/* Shutdown sets SND_PSH */
				seg_flags &= ~THF_FIN;
				if (seg_hi_data-seg_lo_data <= 1)
				{
					/* Allways keep at least one byte
					 * for a future push.
					 */
					DBLOCK(0x20,
					    printf("no data: no push\n"));
					if (head)
					{
						DBLOCK(0x1, printf(
					"no data: setting TCF_NO_PUSH\n"));
						tcp_conn->tc_flags |=
							TCF_NO_PUSH;
					}
					goto after_data;
				}
				seg_hi_data--;
			}

			if (tot_hdr_size != IP_TCP_MIN_HDR_SIZE)
			{
				printf(
				"tcp_write`make_pack: tot_hdr_size = %d\n",
					tot_hdr_size);
				mss= tcp_conn->tc_mtu-tot_hdr_size;
			}
			if (seg_hi_data - seg_lo_data > mss)
			{
				/* Truncate to at most one segment */
				seg_hi_data= seg_lo_data + mss;
				seg_hi= seg_hi_data;
				seg_flags &= ~THF_FIN;
			}

			if (no_push &&
				seg_hi_data-seg_lo_data != mss)
			{
				DBLOCK(0x20, printf(
				"no data: no push for partial segment\n"));
				more2write= (tcp_conn->tc_fd &&
					(tcp_conn->tc_fd->tf_flags &
					TFF_WRITE_IP));
				DIFBLOCK(2, more2write, 
					printf(
			"tcp_send`make_pack: more2write -> !TCF_NO_PUSH\n");
				);
				if (head && !more2write)
				{
					DBLOCK(0x1, printf(
				"partial segment: setting TCF_NO_PUSH\n"));
					tcp_conn->tc_flags |= TCF_NO_PUSH;
					tcp_print_conn(tcp_conn);
					printf("\n");
				}
				goto after_data;
			}


			if (tcp_Gmod4G(seg_hi, tcp_conn->tc_snd_cwnd))
			{
				seg_hi_data= tcp_conn->tc_snd_cwnd;
				seg_hi= seg_hi_data;
				seg_flags &= ~THF_FIN;
			}

			if (!head &&
				seg_hi_data-seg_lo_data < mss)
			{
				if (tcp_conn->tc_flags & TCF_PUSH_NOW)
				{
					DBLOCK(0x20,
					printf("push: no Nagle\n"));
				}
				else
				{
				DBLOCK(0x20,
					printf("no data: partial packet\n"));
				seg_flags &= ~THF_FIN;
				goto after_data;
				}
			}

			if (seg_hi-seg_seq == 0)
			{
				DBLOCK(0x20,
				printf("no data: no data available\n"));
				goto after_data;
			}

			if (tcp_GEmod4G(tcp_conn->tc_SND_UP, seg_lo_data))
			{
				extern int killer_inet;

				if (tcp_GEmod4G(tcp_conn->tc_SND_UP,
					seg_hi_data))
				{
					seg_up= seg_hi_data-seg_seq;
				}
				else
				{
					seg_up= tcp_conn->tc_SND_UP-seg_seq;
				}
				seg_flags |= THF_URG;
				if (!killer_inet &&
					(tcp_conn->tc_flags & TCF_BSD_URG) &&
					seg_up == 0)
				{
					/* A zero urgent pointer doesn't mean
					 * anything when BSD semantics are
					 * used (urgent pointer points to the
					 * first no urgent byte). The use of
					 * a zero urgent pointer also crashes
					 * a Solaris 2.3 kernel. If urgent
					 * pointer doesn't have BSD semantics
					 * then an urgent pointer of zero
					 * simply indicates that there is one
					 * urgent byte.
					 */
					seg_flags &= ~THF_URG;
				}
			}
			else
				seg_up= 0;

			if (tcp_Gmod4G(tcp_conn->tc_SND_PSH, seg_lo_data) &&
				tcp_LEmod4G(tcp_conn->tc_SND_PSH, seg_hi_data))
			{
				seg_flags |= THF_PSH;
			}

			tcp_conn->tc_SND_TRM= seg_hi;

			assert(tcp_conn->tc_transmit_timer.tim_active ||
				(tcp_print_conn(tcp_conn), printf("\n"), 0));
			if (tcp_conn->tc_rt_seq == 0 && 
				tcp_Gmod4G(seg_seq, tcp_conn->tc_rt_threshold))
			{
				tcp_conn->tc_rt_time= curr_time;
				tcp_conn->tc_rt_seq=
					tcp_conn->tc_rt_threshold= seg_seq;
			}

			if (seg_hi_data-seg_lo_data)
			{
#if DEBUG & 0
				assert(tcp_check_conn(tcp_conn));
				assert((seg_hi_data-queue_lo_data <=
					bf_bufsize(tcp_conn->tc_send_data) &&
					seg_lo_data-queue_lo_data <=
					bf_bufsize(tcp_conn->tc_send_data) &&
					seg_hi_data>seg_lo_data)||
					(tcp_print_conn(tcp_conn),
					printf(
		" seg_hi_data= 0x%x, seg_lo_data= 0x%x, queue_lo_data= 0x%x\n",
					seg_hi_data, seg_lo_data,
					queue_lo_data), 0));
#endif

				tmp_pack= pack2write;
				while (tmp_pack->acc_next)
					tmp_pack= tmp_pack->acc_next;
				tmp_pack->acc_next=
					bf_cut(tcp_conn->tc_send_data, 
					(unsigned)(seg_lo_data-queue_lo_data), 
					(unsigned) (seg_hi_data-seg_lo_data));
			}
			seg_flags |= THF_ACK;
		}

after_data:
		if (!(seg_flags & THF_ACK))
		{
			if (pack2write)
				bf_afree(pack2write);
			return NULL;
		}

		tcp_hdr->th_seq_nr= htonl(seg_seq);
		tcp_hdr->th_ack_nr= htonl(tcp_conn->tc_RCV_NXT);
		tcp_hdr->th_flags= seg_flags;
		tcp_hdr->th_window= htons(tcp_conn->tc_RCV_HI -
			tcp_conn->tc_RCV_NXT);
		tcp_hdr->th_urgptr= htons(seg_up);

		pack_size= bf_bufsize(pack2write);
		ip_hdr->ih_length= htons(pack_size);

		pack2write->acc_linkC++;
		ip_hdr_len= (ip_hdr->ih_vers_ihl & IH_IHL_MASK) << 2;
		tcp_pack= bf_delhead(pack2write, ip_hdr_len);
		tcp_hdr->th_chksum= ~tcp_pack_oneCsum(ip_hdr, tcp_pack);
		bf_afree(tcp_pack);

		new_dis= curr_time + 2*HZ*tcp_conn->tc_ttl;
		if (new_dis > tcp_conn->tc_senddis)
			tcp_conn->tc_senddis= new_dis;

		return pack2write;
	default:
		DBLOCK(1, tcp_print_conn(tcp_conn); printf("\n"));
		ip_panic(( "Illegal state" ));
	}
	assert(0);
	return NULL;
}

/*
tcp_release_retrans
*/

PUBLIC void tcp_release_retrans(tcp_conn, seg_ack, new_win)
tcp_conn_t *tcp_conn;
u32_t seg_ack;
u16_t new_win;
{
	tcp_fd_t *tcp_fd;
	size_t size, offset;
	acc_t *pack;
	clock_t retrans_time, curr_time, rtt, artt, drtt, srtt;
	u32_t queue_lo, queue_hi;
	u16_t mss, cthresh;
	unsigned window;

	DBLOCK(0x10, printf("tcp_release_retrans, conn[%d]: ack %lu, win %u\n",
		tcp_conn-tcp_conn_table, (unsigned long)seg_ack, new_win););

	assert(tcp_conn->tc_busy);
	assert (tcp_GEmod4G(seg_ack, tcp_conn->tc_SND_UNA));
	assert (tcp_LEmod4G(seg_ack, tcp_conn->tc_SND_NXT));

	tcp_conn->tc_snd_dack= 0;
	mss= tcp_conn->tc_mtu-IP_TCP_MIN_HDR_SIZE;

	curr_time= get_time();
	if (tcp_conn->tc_rt_seq != 0 && 
		tcp_Gmod4G(seg_ack, tcp_conn->tc_rt_seq))
	{
		assert(curr_time >= tcp_conn->tc_rt_time);
		retrans_time= curr_time-tcp_conn->tc_rt_time;
		rtt= tcp_conn->tc_rtt;

		tcp_conn->tc_rt_seq= 0;

		if (rtt == TCP_RTT_GRAN*CLOCK_GRAN &&
			retrans_time <= TCP_RTT_GRAN*CLOCK_GRAN)
		{
			/* Common in fast networks. Nothing to do. */
		}
		else
		{
			srtt= retrans_time * TCP_RTT_SCALE;

			artt= tcp_conn->tc_artt;
			artt= ((TCP_RTT_SMOOTH-1)*artt+srtt)/TCP_RTT_SMOOTH;

			srtt -= artt;
			if (srtt < 0)
				srtt= -srtt;
			drtt= tcp_conn->tc_drtt;
			drtt= ((TCP_RTT_SMOOTH-1)*drtt+srtt)/TCP_RTT_SMOOTH;

			rtt= (artt+TCP_DRTT_MULT*drtt-1)/TCP_RTT_SCALE+1;
			if (rtt < TCP_RTT_GRAN*CLOCK_GRAN)
			{
				rtt= TCP_RTT_GRAN*CLOCK_GRAN;
			}
			else if (rtt > TCP_RTT_MAX)
			{
#if DEBUG
				static int warned /* = 0 */;

				if (!warned)
				{
					printf(
"tcp_release_retrans: warning retransmission time is limited to %d ms\n",
						TCP_RTT_MAX*1000/HZ);
					warned= 1;
				}
#endif
				rtt= TCP_RTT_MAX;
			}
			DBLOCK(0x10, printf(
	"tcp_release_retrans, conn[%d]: retrans_time= %ld ms, rtt = %ld ms\n",
				tcp_conn-tcp_conn_table,
				retrans_time*1000/HZ,
				rtt*1000/HZ));

			DBLOCK(0x10, printf(
	"tcp_release_retrans: artt= %ld -> %ld, drtt= %ld -> %ld\n",
				tcp_conn->tc_artt, artt,
				tcp_conn->tc_drtt, drtt));

			tcp_conn->tc_artt= artt;
			tcp_conn->tc_drtt= drtt;
			tcp_conn->tc_rtt= rtt;
		}

		if (tcp_conn->tc_mtu != tcp_conn->tc_max_mtu &&
			curr_time > tcp_conn->tc_mtutim+TCP_PMTU_INCR_IV)
		{
			tcp_mtu_incr(tcp_conn);
		}
	}

	/* Update the current window. */
	window= tcp_conn->tc_snd_cwnd-tcp_conn->tc_SND_UNA;
	assert(seg_ack != tcp_conn->tc_SND_UNA);

	/* For every real ACK we try to increase the current window
	 * with 1 mss.
	 */
	window += mss;

	/* If the window becomes larger than the current threshold,
	 * increment the threshold by a small amount and set the
	 * window to the threshold.
	 */
	cthresh= tcp_conn->tc_snd_cthresh;
	if (window > cthresh)
	{
		cthresh += tcp_conn->tc_snd_cinc;
		tcp_conn->tc_snd_cthresh= cthresh;
		window= cthresh;
	}

	/* If the window is larger than the window advertised by the
	 * receiver, set the window size to the advertisement.
	 */
	if (window > new_win)
		window= new_win;

	tcp_conn->tc_snd_cwnd= seg_ack+window;

	/* Release data queued for retransmissions. */
	queue_lo= tcp_conn->tc_SND_UNA;
	queue_hi= tcp_conn->tc_SND_NXT;

	tcp_conn->tc_SND_UNA= seg_ack;
	if (tcp_Lmod4G(tcp_conn->tc_SND_TRM, seg_ack))
	{
		tcp_conn->tc_SND_TRM= seg_ack;
	}
	assert(tcp_GEmod4G(tcp_conn->tc_snd_cwnd, seg_ack));

	/* Advance ISS every 0.5GB to avoid problem with wrap around */
	if (tcp_conn->tc_SND_UNA - tcp_conn->tc_ISS > 0x40000000)
	{
		tcp_conn->tc_ISS += 0x20000000;
		DBLOCK(1, printf(
			"tcp_release_retrans: updating ISS to 0x%lx\n",
			(unsigned long)tcp_conn->tc_ISS););
		if (tcp_Lmod4G(tcp_conn->tc_SND_UP, tcp_conn->tc_ISS))
		{
			tcp_conn->tc_SND_UP= tcp_conn->tc_ISS;
			DBLOCK(1, printf(
			"tcp_release_retrans: updating SND_UP to 0x%lx\n",
				(unsigned long)tcp_conn->tc_SND_UP););
		}
	}

	if (queue_lo == tcp_conn->tc_ISS)
		queue_lo++;

	if (tcp_conn->tc_flags & TCF_FIN_SENT)
	{
		if (seg_ack == queue_hi)
			seg_ack--;
		if (queue_lo == queue_hi)
			queue_lo--;
		queue_hi--;
	}

	offset= seg_ack - queue_lo;
	size= queue_hi - seg_ack;
	pack= tcp_conn->tc_send_data;
	tcp_conn->tc_send_data= 0;

	if (!size)
	{
		bf_afree(pack);
	}
	else
	{
		pack= bf_delhead(pack, offset);
		tcp_conn->tc_send_data= pack;
	}

	if (tcp_Gmod4G(tcp_conn->tc_SND_TRM, tcp_conn->tc_snd_cwnd))
		tcp_conn->tc_SND_TRM= tcp_conn->tc_snd_cwnd;

	/* Copy in new data if an ioctl is pending or if a write request is
	 * pending and either the write can be completed or at least one
	 * mss buffer space is available.
	 */
	tcp_fd= tcp_conn->tc_fd;
	if (tcp_fd)
	{
		if (tcp_fd->tf_flags & TFF_IOCTL_IP) 
		{
			tcp_fd_write(tcp_conn);
		}
		if ((tcp_fd->tf_flags & TFF_WRITE_IP) &&
			(size+tcp_fd->tf_write_count <= TCP_MAX_SND_WND_SIZE ||
			size <= TCP_MAX_SND_WND_SIZE-mss))
		{
			tcp_fd_write(tcp_conn);
		}
		if (tcp_fd->tf_flags & TFF_SEL_WRITE) 
			tcp_rsel_write(tcp_conn);
	}
	else
	{
		if (tcp_conn->tc_SND_UNA == tcp_conn->tc_SND_NXT)
		{
			assert(tcp_conn->tc_state == TCS_CLOSING);
			DBLOCK(0x10,
			printf("all data sent in abondoned connection\n"));
			tcp_close_connection(tcp_conn, ENOTCONN);
			return;
		}
	}

	if (!size && !tcp_conn->tc_send_data)
	{
		/* Reset window if a write is completed */
		tcp_conn->tc_snd_cwnd= tcp_conn->tc_SND_UNA + mss;
	}

	DIFBLOCK(2, (tcp_conn->tc_snd_cwnd == tcp_conn->tc_SND_TRM),
		printf("not sending: zero window\n"));

	if (tcp_conn->tc_snd_cwnd != tcp_conn->tc_SND_TRM &&
		tcp_conn->tc_SND_NXT != tcp_conn->tc_SND_TRM)
	{
		tcp_conn_write(tcp_conn, 1);
	}

}

/*
tcp_fast_retrans
*/

PUBLIC void tcp_fast_retrans(tcp_conn)
tcp_conn_t *tcp_conn;
{
	u16_t mss, mss2;

	/* Update threshold sequence number for retransmission calculation. */
	if (tcp_Gmod4G(tcp_conn->tc_SND_TRM, tcp_conn->tc_rt_threshold))
		tcp_conn->tc_rt_threshold= tcp_conn->tc_SND_TRM;

	tcp_conn->tc_SND_TRM= tcp_conn->tc_SND_UNA;

	mss= tcp_conn->tc_mtu-IP_TCP_MIN_HDR_SIZE;
	mss2= 2*mss;

	if (tcp_conn->tc_snd_cwnd == tcp_conn->tc_SND_UNA)
		tcp_conn->tc_snd_cwnd++;
	if (tcp_Gmod4G(tcp_conn->tc_snd_cwnd, tcp_conn->tc_SND_UNA + mss2))
	{
		tcp_conn->tc_snd_cwnd= tcp_conn->tc_SND_UNA + mss2;
		if (tcp_Gmod4G(tcp_conn->tc_SND_TRM, tcp_conn->tc_snd_cwnd))
			tcp_conn->tc_SND_TRM= tcp_conn->tc_snd_cwnd;

		tcp_conn->tc_snd_cthresh /= 2;
		if (tcp_conn->tc_snd_cthresh < mss2)
			tcp_conn->tc_snd_cthresh= mss2;
	}

	tcp_conn_write(tcp_conn, 1);
}

#if 0
PUBLIC void do_tcp_timeout(tcp_conn)
tcp_conn_t *tcp_conn;
{
	tcp_send_timeout(tcp_conn-tcp_conn_table,
		&tcp_conn->tc_transmit_timer);
}
#endif

/*
tcp_send_timeout
*/

PRIVATE void tcp_send_timeout(conn, timer)
int conn;
struct timer *timer;
{
	tcp_conn_t *tcp_conn;
	u16_t mss, mss2;
	u32_t snd_una, snd_nxt;
	clock_t curr_time, rtt, stt, timeout;
	acc_t *pkt;
	int new_ttl, no_push;

	DBLOCK(0x20, printf("tcp_send_timeout: conn[%d]\n", conn));

	curr_time= get_time();

	tcp_conn= &tcp_conn_table[conn];
	assert(tcp_conn->tc_flags & TCF_INUSE);
	assert(tcp_conn->tc_state != TCS_CLOSED);
	assert(tcp_conn->tc_state != TCS_LISTEN);

	snd_una= tcp_conn->tc_SND_UNA;
	snd_nxt= tcp_conn->tc_SND_NXT;
	no_push= (tcp_conn->tc_flags & TCF_NO_PUSH);
	if (snd_nxt == snd_una || no_push)
	{
		/* Nothing more to send */
		assert(tcp_conn->tc_SND_TRM == snd_una || no_push);

		/* A new write sets the timer if tc_transmit_seq == SND_UNA */
		tcp_conn->tc_transmit_seq= tcp_conn->tc_SND_UNA;
		tcp_conn->tc_stt= 0;
		tcp_conn->tc_0wnd_to= 0;
		assert(!tcp_conn->tc_fd ||
			!(tcp_conn->tc_fd->tf_flags & TFF_WRITE_IP) ||
			(tcp_print_conn(tcp_conn), printf("\n"), 0));

		if (snd_nxt != snd_una)
		{
			assert(no_push);
			DBLOCK(1, printf("not setting keepalive timer\n"););

			/* No point in setting the keepalive timer if we
			 * still have to send more data.
			 */
			return;
		}

		assert(tcp_conn->tc_send_data == NULL);
		DBLOCK(0x20, printf("keep alive timer\n"));
		if (tcp_conn->tc_ka_snd != tcp_conn->tc_SND_NXT ||
			tcp_conn->tc_ka_rcv != tcp_conn->tc_RCV_NXT)
		{
			tcp_conn->tc_ka_snd= tcp_conn->tc_SND_NXT;
			tcp_conn->tc_ka_rcv= tcp_conn->tc_RCV_NXT;
			DBLOCK(0x20, printf(
"tcp_send_timeout: conn[%d] setting keepalive timer (+%ld ms)\n",
				tcp_conn-tcp_conn_table,
				tcp_conn->tc_ka_time*1000/HZ));
			clck_timer(&tcp_conn->tc_transmit_timer,
				curr_time+tcp_conn->tc_ka_time,
				tcp_send_timeout,
				tcp_conn-tcp_conn_table);
			return;
		}
		DBLOCK(0x10, printf(
		"tcp_send_timeout, conn[%d]: triggering keep alive probe\n",
			tcp_conn-tcp_conn_table));
		tcp_conn->tc_ka_snd--;
		if (!(tcp_conn->tc_flags & TCF_FIN_SENT))
		{
			pkt= bf_memreq(1);
			*ptr2acc_data(pkt)= '\xff';	/* a random char */
			tcp_conn->tc_send_data= pkt; pkt= NULL;
		}
		tcp_conn->tc_SND_UNA--;
		if (tcp_conn->tc_SND_UNA == tcp_conn->tc_ISS)
		{
			/* We didn't send anything so far. Retrying the
			 * SYN is too hard. Decrement ISS and hope
			 * that the other side doesn't care.
			 */
			tcp_conn->tc_ISS--;
		}

		/* Set tc_transmit_seq and tc_stt to trigger packet */
		tcp_conn->tc_transmit_seq= tcp_conn->tc_SND_UNA;
		tcp_conn->tc_stt= curr_time;

		/* Set tc_rt_seq for round trip measurements */
		tcp_conn->tc_rt_time= curr_time;
		tcp_conn->tc_rt_seq= tcp_conn->tc_SND_UNA;

		/* Set PSH to make sure that data gets sent */
		tcp_conn->tc_SND_PSH= tcp_conn->tc_SND_NXT;
		assert(tcp_check_conn(tcp_conn));

		/* Fall through */
	}

	rtt= tcp_conn->tc_rtt;

	if (tcp_conn->tc_transmit_seq != tcp_conn->tc_SND_UNA)
	{
		/* Some data has been acknowledged since the last time the
		 * timer was set, set the timer again. */
		tcp_conn->tc_transmit_seq= tcp_conn->tc_SND_UNA;
		tcp_conn->tc_stt= 0;
		tcp_conn->tc_0wnd_to= 0;

		DBLOCK(0x20, printf(
	"tcp_send_timeout: conn[%d] setting timer to %ld ms (+%ld ms)\n",
			tcp_conn-tcp_conn_table,
			(curr_time+rtt)*1000/HZ, rtt*1000/HZ));

		clck_timer(&tcp_conn->tc_transmit_timer,
			curr_time+rtt, tcp_send_timeout,
			tcp_conn-tcp_conn_table);
		return;
	}

	stt= tcp_conn->tc_stt;
	if (stt == 0)
	{
		/* Some packet arrived but did not acknowledge any data.
		 * Apparently, the other side is still alive and has a
		 * reason to transmit. We can asume a zero window.
		 */

		DBLOCK(0x10, printf("conn[%d] setting zero window timer\n",
			tcp_conn-tcp_conn_table));

		if (tcp_conn->tc_0wnd_to < TCP_0WND_MIN)
			tcp_conn->tc_0wnd_to= TCP_0WND_MIN;
		else if (tcp_conn->tc_0wnd_to < rtt)
			tcp_conn->tc_0wnd_to= rtt;
		else
		{
			tcp_conn->tc_0wnd_to *= 2;
			if (tcp_conn->tc_0wnd_to > TCP_0WND_MAX)
				tcp_conn->tc_0wnd_to= TCP_0WND_MAX;
		}
		tcp_conn->tc_stt= curr_time;
		tcp_conn->tc_rt_seq= 0;

		DBLOCK(0x10, printf(
	"tcp_send_timeout: conn[%d] setting timer to %ld ms (+%ld ms)\n",
			tcp_conn-tcp_conn_table,
			(curr_time+tcp_conn->tc_0wnd_to)*1000/HZ,
			tcp_conn->tc_0wnd_to*1000/HZ));

		clck_timer(&tcp_conn->tc_transmit_timer,
			curr_time+tcp_conn->tc_0wnd_to,
			tcp_send_timeout, tcp_conn-tcp_conn_table);
		return;
	}
	assert(stt <= curr_time);

	DIFBLOCK(0x10, (tcp_conn->tc_fd == 0),
		printf("conn[%d] timeout in abondoned connection\n",
		tcp_conn-tcp_conn_table));

	/* At this point, we have do a retransmission, or send a zero window
	 * probe, which is almost the same.
	 */

	DBLOCK(0x20, printf("tcp_send_timeout: conn[%d] una= %lu, rtt= %ldms\n",
		tcp_conn-tcp_conn_table,
		(unsigned long)tcp_conn->tc_SND_UNA, rtt*1000/HZ));

	/* Update threshold sequence number for retransmission calculation. */
	if (tcp_Gmod4G(tcp_conn->tc_SND_TRM, tcp_conn->tc_rt_threshold))
		tcp_conn->tc_rt_threshold= tcp_conn->tc_SND_TRM;

	tcp_conn->tc_SND_TRM= tcp_conn->tc_SND_UNA;

	if (tcp_conn->tc_flags & TCF_PMTU &&
		curr_time > stt+TCP_PMTU_BLACKHOLE)
	{
		/* We can't tell the difference between a PMTU blackhole 
		 * and a broken link. Assume a PMTU blackhole, and switch
		 * off PMTU discovery.
		 */
		DBLOCK(1, printf(
			"tcp[%d]: PMTU blackhole (or broken link) on route to ",
			tcp_conn-tcp_conn_table);
			writeIpAddr(tcp_conn->tc_remaddr);
			printf(", max mtu = %u\n", tcp_conn->tc_max_mtu););
		tcp_conn->tc_flags &= ~TCF_PMTU;
		tcp_conn->tc_mtutim= curr_time;
		if (tcp_conn->tc_max_mtu > IP_DEF_MTU)
			tcp_conn->tc_mtu= IP_DEF_MTU;
	}

	mss= tcp_conn->tc_mtu-IP_TCP_MIN_HDR_SIZE;
	mss2= 2*mss;

	if (tcp_conn->tc_snd_cwnd == tcp_conn->tc_SND_UNA)
		tcp_conn->tc_snd_cwnd++;
	if (tcp_Gmod4G(tcp_conn->tc_snd_cwnd, tcp_conn->tc_SND_UNA + mss2))
	{
		tcp_conn->tc_snd_cwnd= tcp_conn->tc_SND_UNA + mss2;
		if (tcp_Gmod4G(tcp_conn->tc_SND_TRM, tcp_conn->tc_snd_cwnd))
			tcp_conn->tc_SND_TRM= tcp_conn->tc_snd_cwnd;

		tcp_conn->tc_snd_cthresh /= 2;
		if (tcp_conn->tc_snd_cthresh < mss2)
			tcp_conn->tc_snd_cthresh= mss2;
	}

	if (curr_time-stt > tcp_conn->tc_rt_dead)
	{
		tcp_close_connection(tcp_conn, ETIMEDOUT);
		return;
	}

	timeout= (curr_time-stt) >> 3;
	if (timeout < rtt)
		timeout= rtt;
	timeout += curr_time;

	DBLOCK(0x20, printf(
	"tcp_send_timeout: conn[%d] setting timer to %ld ms (+%ld ms)\n",
		tcp_conn-tcp_conn_table, timeout*1000/HZ,
		(timeout-curr_time)*1000/HZ));

	clck_timer(&tcp_conn->tc_transmit_timer, timeout,
		tcp_send_timeout, tcp_conn-tcp_conn_table);

#if 0
	if (tcp_conn->tc_rt_seq == 0)
	{
		printf("tcp_send_timeout: conn[%d]: setting tc_rt_time\n",
			tcp_conn-tcp_conn_table);
		tcp_conn->tc_rt_time= curr_time-rtt;
		tcp_conn->tc_rt_seq= tcp_conn->tc_SND_UNA;
	}
#endif

	if (tcp_conn->tc_state == TCS_SYN_SENT ||
		(curr_time-stt >= tcp_conn->tc_ttl*HZ))
	{
		new_ttl= tcp_conn->tc_ttl+1;
		if (new_ttl> IP_MAX_TTL)
			new_ttl= IP_MAX_TTL;
		tcp_conn->tc_ttl= new_ttl;
	}

	tcp_conn_write(tcp_conn, 0);
}


PUBLIC void tcp_fd_write(tcp_conn)
tcp_conn_t *tcp_conn;
{
	tcp_fd_t *tcp_fd;
	int urg, nourg, push;
	u32_t max_seq;
	size_t max_trans, write_count;
	acc_t *data, *send_data;

	assert(tcp_conn->tc_busy);
	tcp_fd= tcp_conn->tc_fd;

	if ((tcp_fd->tf_flags & TFF_IOCTL_IP) &&
		!(tcp_fd->tf_flags & TFF_WRITE_IP))
	{
		if (tcp_fd->tf_ioreq != NWIOTCPSHUTDOWN)
			return;
		DBLOCK(0x10, printf("NWIOTCPSHUTDOWN\n"));
		if (tcp_conn->tc_state == TCS_CLOSED)
		{
			tcp_reply_ioctl (tcp_fd, tcp_conn->tc_error);
			return;
		}
		if (!(tcp_conn->tc_flags & TCF_FIN_SENT))
		{
			DBLOCK(0x10, printf("calling tcp_shutdown\n"));
			tcp_shutdown (tcp_conn);
		}
		else
		{
			if (tcp_conn->tc_SND_UNA == tcp_conn->tc_SND_NXT)
			{
				tcp_reply_ioctl (tcp_fd, NW_OK);
				DBLOCK(0x10, printf("shutdown completed\n"));
			}
			else
			{
				DBLOCK(0x10,
					printf("shutdown still inprogress\n"));
			}
		}
		return;
	}

	assert (tcp_fd->tf_flags & TFF_WRITE_IP);
	if (tcp_conn->tc_state == TCS_CLOSED)
	{
		if (tcp_fd->tf_write_offset)
		{
			tcp_reply_write(tcp_fd,
				tcp_fd->tf_write_offset);
		}
		else
			tcp_reply_write(tcp_fd, tcp_conn->tc_error);
		return;
	}

	urg= (tcp_fd->tf_flags & TFF_WR_URG);
	push= (tcp_fd->tf_flags & TFF_PUSH_DATA);

	max_seq= tcp_conn->tc_SND_UNA + TCP_MAX_SND_WND_SIZE;
	max_trans= max_seq - tcp_conn->tc_SND_NXT;
	if (tcp_fd->tf_write_count <= max_trans)
		write_count= tcp_fd->tf_write_count;
	else
		write_count= max_trans;
	if (write_count)
	{
		if (tcp_conn->tc_flags & TCF_BSD_URG)
		{
			if (tcp_Gmod4G(tcp_conn->tc_SND_NXT,
				tcp_conn->tc_SND_UNA))
			{
				nourg= tcp_LEmod4G(tcp_conn->tc_SND_UP,
					tcp_conn->tc_SND_UNA);
				if ((urg && nourg) || (!urg && !nourg))
				{
					DBLOCK(0x20,
						printf("not sending\n"));
					return;
				}
			}
		}
		data= (*tcp_fd->tf_get_userdata)
			(tcp_fd->tf_srfd, tcp_fd->tf_write_offset,
			write_count, FALSE);

		if (!data)
		{
			if (tcp_fd->tf_write_offset)
			{
				tcp_reply_write(tcp_fd,
					tcp_fd->tf_write_offset);
			}
			else
				tcp_reply_write(tcp_fd, EFAULT);
			return;
		}
		tcp_fd->tf_write_offset += write_count;
		tcp_fd->tf_write_count -= write_count;

		send_data= tcp_conn->tc_send_data;
		tcp_conn->tc_send_data= 0;
		send_data= bf_append(send_data, data);
		tcp_conn->tc_send_data= send_data;
		tcp_conn->tc_SND_NXT += write_count;
		if (urg)
		{
			if (tcp_conn->tc_flags & TCF_BSD_URG)
				tcp_conn->tc_SND_UP= tcp_conn->tc_SND_NXT;
			else
				tcp_conn->tc_SND_UP= tcp_conn->tc_SND_NXT-1;
		}
		if (push && !tcp_fd->tf_write_count)
			tcp_conn->tc_SND_PSH= tcp_conn->tc_SND_NXT;
	}
	if (!tcp_fd->tf_write_count)
	{
		tcp_reply_write(tcp_fd, tcp_fd->tf_write_offset);
	}
}

PUBLIC unsigned tcp_sel_write(tcp_conn)
tcp_conn_t *tcp_conn;
{
	tcp_fd_t *tcp_fd;
	int urg, nourg;
	u32_t max_seq;
	size_t max_trans;

	tcp_fd= tcp_conn->tc_fd;

	if (tcp_conn->tc_state == TCS_CLOSED)
		return 1;
	
	urg= (tcp_fd->tf_flags & TFF_WR_URG);

	max_seq= tcp_conn->tc_SND_UNA + TCP_MAX_SND_WND_SIZE;
	max_trans= max_seq - tcp_conn->tc_SND_NXT;
	if (max_trans)
	{
		if (tcp_conn->tc_flags & TCF_BSD_URG)
		{
			if (tcp_Gmod4G(tcp_conn->tc_SND_NXT,
				tcp_conn->tc_SND_UNA))
			{
				nourg= tcp_LEmod4G(tcp_conn->tc_SND_UP,
					tcp_conn->tc_SND_UNA);
				if ((urg && nourg) || (!urg && !nourg))
				{
					DBLOCK(0x20,
						printf("not sending\n"));
					return 0;
				}
			}
		}
		return 1;
	}

	return 0;
}

PUBLIC void
tcp_rsel_write(tcp_conn)
tcp_conn_t *tcp_conn;
{
	tcp_fd_t *tcp_fd;

	if (tcp_sel_write(tcp_conn) == 0)
		return;

	tcp_fd= tcp_conn->tc_fd;
	tcp_fd->tf_flags &= ~TFF_SEL_WRITE;
	if (tcp_fd->tf_select_res)
		tcp_fd->tf_select_res(tcp_fd->tf_srfd, SR_SELECT_WRITE);
	else
		printf("tcp_rsel_write: no select_res\n");
}

/*
tcp_shutdown
*/

PUBLIC void tcp_shutdown(tcp_conn)
tcp_conn_t *tcp_conn;
{
	switch (tcp_conn->tc_state)
	{
	case TCS_CLOSED:
	case TCS_LISTEN:
	case TCS_SYN_SENT:
	case TCS_SYN_RECEIVED:
		tcp_close_connection(tcp_conn, ENOTCONN);
		return;
	}

	if (tcp_conn->tc_flags & TCF_FIN_SENT)
		return;
	tcp_conn->tc_flags |= TCF_FIN_SENT;
	tcp_conn->tc_flags &= ~TCF_NO_PUSH;
	tcp_conn->tc_SND_NXT++;
	tcp_conn->tc_SND_PSH= tcp_conn->tc_SND_NXT;

	assert (tcp_check_conn(tcp_conn) ||
		(tcp_print_conn(tcp_conn), printf("\n"), 0));

	tcp_conn_write(tcp_conn, 1);

	/* Start the timer */
	tcp_set_send_timer(tcp_conn);
}

PUBLIC void tcp_set_send_timer(tcp_conn)
tcp_conn_t *tcp_conn;
{
	clock_t curr_time;
	clock_t rtt;

	assert(tcp_conn->tc_state != TCS_CLOSED);
	assert(tcp_conn->tc_state != TCS_LISTEN);

	curr_time= get_time();
	rtt= tcp_conn->tc_rtt;

	DBLOCK(0x20, printf(
	"tcp_set_send_timer: conn[%d] setting timer to %ld ms (+%ld ms)\n",
		tcp_conn-tcp_conn_table,
		(curr_time+rtt)*1000/HZ, rtt*1000/HZ));

	/* Start the timer */
	clck_timer(&tcp_conn->tc_transmit_timer,
		curr_time+rtt, tcp_send_timeout, tcp_conn-tcp_conn_table);
	tcp_conn->tc_stt= curr_time;
}

/*
tcp_close_connection

*/

PUBLIC void tcp_close_connection(tcp_conn, error)
tcp_conn_t *tcp_conn;
int error;
{
	int i;
	tcp_port_t *tcp_port;
	tcp_fd_t *tcp_fd;
	tcp_conn_t *tc;

	assert (tcp_check_conn(tcp_conn) ||
		(tcp_print_conn(tcp_conn), printf("\n"), 0));
	assert (tcp_conn->tc_flags & TCF_INUSE);

	tcp_conn->tc_error= error;
	tcp_port= tcp_conn->tc_port;
	tcp_fd= tcp_conn->tc_fd;
	if (tcp_conn->tc_state == TCS_CLOSED)
		return;

	tcp_conn->tc_state= TCS_CLOSED;
	DBLOCK(0x10, tcp_print_state(tcp_conn); printf("\n"));

	if (tcp_fd && (tcp_fd->tf_flags & TFF_LISTENQ))
	{
		for (i= 0; i<TFL_LISTEN_MAX; i++)
		{
			if (tcp_fd->tf_listenq[i] == tcp_conn)
				break;
		}
		assert(i < TFL_LISTEN_MAX);
		tcp_fd->tf_listenq[i]= NULL;

		assert(tcp_conn->tc_connInprogress);
		tcp_conn->tc_connInprogress= 0;

		tcp_conn->tc_fd= NULL;
		tcp_fd= NULL;
	}
	else if (tcp_fd)
	{

		tcp_conn->tc_busy++;
		assert(tcp_fd->tf_conn == tcp_conn);

		if (tcp_fd->tf_flags & TFF_READ_IP)
			tcp_fd_read (tcp_conn, 1);
		assert (!(tcp_fd->tf_flags & TFF_READ_IP));
		if (tcp_fd->tf_flags & TFF_SEL_READ)
			tcp_rsel_read (tcp_conn);

		if (tcp_fd->tf_flags & TFF_WRITE_IP)
		{
			tcp_fd_write(tcp_conn);
			tcp_conn_write(tcp_conn, 1);
		}
		assert (!(tcp_fd->tf_flags & TFF_WRITE_IP));
		if (tcp_fd->tf_flags & TFF_IOCTL_IP)
		{
			tcp_fd_write(tcp_conn);
			tcp_conn_write(tcp_conn, 1);
		}
		if (tcp_fd->tf_flags & TFF_IOCTL_IP)
			assert(tcp_fd->tf_ioreq != NWIOTCPSHUTDOWN);
		if (tcp_fd->tf_flags & TFF_SEL_WRITE) 
			tcp_rsel_write(tcp_conn);

		if (tcp_conn->tc_connInprogress)
			tcp_restart_connect(tcp_conn);
		assert (!tcp_conn->tc_connInprogress);
		assert (!(tcp_fd->tf_flags & TFF_IOCTL_IP) ||
			(printf("req= 0x%lx\n",
			(unsigned long)tcp_fd->tf_ioreq), 0));
		tcp_conn->tc_busy--;
	}

	if (tcp_conn->tc_rcvd_data)
	{
		bf_afree(tcp_conn->tc_rcvd_data);
		tcp_conn->tc_rcvd_data= NULL;
	}
	tcp_conn->tc_flags &= ~TCF_FIN_RECV;
	tcp_conn->tc_RCV_LO= tcp_conn->tc_RCV_NXT;

	if (tcp_conn->tc_adv_data)
	{
		bf_afree(tcp_conn->tc_adv_data);
		tcp_conn->tc_adv_data= NULL;
	}

	if (tcp_conn->tc_send_data)
	{
		bf_afree(tcp_conn->tc_send_data);
		tcp_conn->tc_send_data= NULL;
		tcp_conn->tc_SND_TRM=
			tcp_conn->tc_SND_NXT= tcp_conn->tc_SND_UNA;
	}
	tcp_conn->tc_SND_TRM= tcp_conn->tc_SND_NXT= tcp_conn->tc_SND_UNA;

	if (tcp_conn->tc_remipopt)
	{
		bf_afree(tcp_conn->tc_remipopt);
		tcp_conn->tc_remipopt= NULL;
	}

	if (tcp_conn->tc_tcpopt)
	{
		bf_afree(tcp_conn->tc_tcpopt);
		tcp_conn->tc_tcpopt= NULL;
	}

	if (tcp_conn->tc_frag2send)
	{
		bf_afree(tcp_conn->tc_frag2send);
		tcp_conn->tc_frag2send= NULL;
	}
	if (tcp_conn->tc_flags & TCF_MORE2WRITE)
	{
		for (tc= tcp_port->tp_snd_head; tc; tc= tc->tc_send_link)
		{
			if (tc->tc_send_link == tcp_conn)
				break;
		}
		if (tc == NULL)
		{
			assert(tcp_port->tp_snd_head == tcp_conn);
			tcp_port->tp_snd_head= tcp_conn->tc_send_link;
		}
		else
		{
			tc->tc_send_link= tcp_conn->tc_send_link;
			if (tc->tc_send_link == NULL)
				tcp_port->tp_snd_tail= tc;
		}
		tcp_conn->tc_flags &= ~TCF_MORE2WRITE;
	}

	clck_untimer (&tcp_conn->tc_transmit_timer);
	tcp_conn->tc_transmit_seq= 0;

					/* clear all flags but TCF_INUSE */
	tcp_conn->tc_flags &= TCF_INUSE;
	assert (tcp_check_conn(tcp_conn));
}

/*
 * $PchId: tcp_send.c,v 1.32 2005/06/28 14:21:52 philip Exp $
 */
