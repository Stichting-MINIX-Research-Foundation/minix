/*
tcp_send.c

Copyright 1995 Philip Homburg
*/

#include "inet.h"
#include "buf.h"
#include "clock.h"
#include "event.h"
#include "type.h"

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

	/* XXX - do we really have something to send here? */

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
				if (r == EDSTNOTRCH)
				{
					tcp_notreach(tcp_conn);
					continue;
				}
				else if (r == EBADDEST)
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
	int tot_hdr_size, ip_hdr_len;
	u32_t seg_seq, seg_lo_data, queue_lo_data, seg_hi, seg_hi_data;
	u16_t seg_up;
	u8_t seg_flags;
	time_t new_dis;
	size_t pack_size;
	time_t curr_time;
	u8_t *optptr;

	assert(tcp_conn->tc_busy);
	curr_time= get_time();
	switch (tcp_conn->tc_state)
	{
	case TCS_CLOSED:
		return 0;
	case TCS_SYN_RECEIVED:
	case TCS_SYN_SENT:

		if (tcp_conn->tc_SND_TRM == tcp_conn->tc_SND_NXT &&
			!(tcp_conn->tc_flags & TCF_SEND_ACK))
		{
			return 0;
		}

		tcp_conn->tc_flags &= ~TCF_SEND_ACK;

		/* Include a max segment size option. */
		assert(tcp_conn->tc_tcpopt == NULL);
		tcp_conn->tc_tcpopt= bf_memreq(4);
		optptr= (u8_t *)ptr2acc_data(tcp_conn->tc_tcpopt);
		optptr[0]= TCP_OPT_MSS;
		optptr[1]= 4;
		optptr[2]= tcp_conn->tc_mss >> 8;
		optptr[3]= tcp_conn->tc_mss & 0xFF;

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
		tcp_hdr->th_window= htons(tcp_conn->tc_mss);
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
			if (seg_hi_data - seg_lo_data > tcp_conn->tc_mss -
				tot_hdr_size)
			{
				seg_hi_data= seg_lo_data + tcp_conn->tc_mss -
					tot_hdr_size;
				seg_hi= seg_hi_data;
				seg_flags &= ~THF_FIN;
			}

			if (tcp_Gmod4G(seg_hi, tcp_conn->tc_snd_cwnd))
			{
				seg_hi_data= tcp_conn->tc_snd_cwnd;
				seg_hi= seg_hi_data;
				seg_flags &= ~THF_FIN;
			}

			if (seg_hi-seg_seq == 0)
			{
				DBLOCK(0x20,
				printf("no data: no data available\n"));
				goto after_data;
			}

			if (seg_seq != tcp_conn->tc_SND_UNA &&
				seg_hi_data-seg_lo_data+tot_hdr_size < 
				tcp_conn->tc_mss)
			{
				DBLOCK(0x20,
					printf("no data: partial packet\n"));
				seg_flags &= ~THF_FIN;
				goto after_data;
			}

			if (tcp_GEmod4G(tcp_conn->tc_SND_UP, seg_lo_data))
			{
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
				if ((tcp_conn->tc_flags & TCF_BSD_URG) &&
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
#if !CRAMPED
	default:
		DBLOCK(1, tcp_print_conn(tcp_conn); printf("\n"));
		ip_panic(( "Illegal state" ));
#endif
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
	size_t size, offset;
	acc_t *pack;
	time_t retrans_time, curr_time, rtt;
	u32_t queue_lo, queue_hi;
	u16_t mss, cthresh;
	unsigned window;

	assert(tcp_conn->tc_busy);
	assert (tcp_GEmod4G(seg_ack, tcp_conn->tc_SND_UNA));
	assert (tcp_LEmod4G(seg_ack, tcp_conn->tc_SND_NXT));

	curr_time= get_time();
	if (tcp_conn->tc_rt_seq != 0 && 
		tcp_Gmod4G(seg_ack, tcp_conn->tc_rt_seq))
	{
		assert(curr_time >= tcp_conn->tc_rt_time);
		retrans_time= curr_time-tcp_conn->tc_rt_time;
		rtt= tcp_conn->tc_rtt;

		DBLOCK(0x20, printf(
		"tcp_release_retrans, conn[%d]: retrans_time= %ld ms\n",
			tcp_conn-tcp_conn_table, retrans_time*1000/HZ));


		tcp_conn->tc_rt_seq= 0;

		if (rtt == TCP_RTT_GRAN*CLOCK_GRAN &&
			retrans_time <= TCP_RTT_GRAN*CLOCK_GRAN)
		{
			/* Common in fast networks. Nothing to do. */
		}
		else if (rtt >= retrans_time && rtt <= 2*retrans_time)
		{
			/* Nothing to do. We assume that a factor 2 for
			 * variance is enough.
			 */
		}
		else if (retrans_time > rtt)
		{
			/* Retrans time is really too small. */

			tcp_conn->tc_rtt= rtt*2;
			if (tcp_conn->tc_rtt > TCP_RTT_MAX)
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
				tcp_conn->tc_rtt= TCP_RTT_MAX;
			}
			assert (tcp_conn->tc_rtt);

			DBLOCK(0x10, printf(
"tcp_release_retrans, conn[%d]: (was too small) retrans_time= %ld ms, rtt= %ld ms\n",
				tcp_conn-tcp_conn_table, retrans_time*1000/HZ,
				tcp_conn->tc_rtt*1000/HZ));


		}
		else if (seg_ack - tcp_conn->tc_rt_seq == tcp_conn->tc_mss)
		{
			/* Retrans time is really too big. */
			rtt= (rtt*3)>>2;
			if (rtt < TCP_RTT_GRAN*CLOCK_GRAN)
				rtt= TCP_RTT_GRAN*CLOCK_GRAN;
			tcp_conn->tc_rtt= rtt;
			assert (tcp_conn->tc_rtt);

			DBLOCK(0x10, printf(
"tcp_release_retrans, conn[%d]: (was too big) retrans_time= %ld ms, rtt= %ld ms\n",
				tcp_conn-tcp_conn_table, retrans_time*1000/HZ,
				tcp_conn->tc_rtt*1000/HZ));
		}
		else
		{
			/* Retrans time might be too big. Try a bit smaller. */
			rtt= (rtt*31)>>5;
			if (rtt < TCP_RTT_GRAN*CLOCK_GRAN)
				rtt= TCP_RTT_GRAN*CLOCK_GRAN;
			tcp_conn->tc_rtt= rtt;
			assert (tcp_conn->tc_rtt);

			DBLOCK(0x20, printf(
"tcp_release_retrans, conn[%d]: (maybe too big) retrans_time= %ld ms, rtt= %ld ms\n",
				tcp_conn-tcp_conn_table, retrans_time*1000/HZ,
				tcp_conn->tc_rtt*1000/HZ));
		}
	}

	/* Update the current window. */
	window= tcp_conn->tc_snd_cwnd-tcp_conn->tc_SND_UNA;
	mss= tcp_conn->tc_mss;
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

		/* Reset window if a write is completed */
		tcp_conn->tc_snd_cwnd= tcp_conn->tc_SND_UNA +
			2*tcp_conn->tc_mss;
	}
	else
	{
		pack= bf_delhead(pack, offset);
		tcp_conn->tc_send_data= pack;
	}

	if (tcp_Gmod4G(tcp_conn->tc_SND_TRM, tcp_conn->tc_snd_cwnd))
		tcp_conn->tc_SND_TRM= tcp_conn->tc_snd_cwnd;

	/* Copy in new data if a write request is pending and
	 * SND_NXT-SND_TRM is less than 1 mss.
	 */
	if (tcp_conn->tc_fd)
	{
		if ((tcp_conn->tc_fd->tf_flags &
			(TFF_WRITE_IP|TFF_IOCTL_IP)) &&
			tcp_conn->tc_SND_NXT-tcp_conn->tc_SND_TRM <
			tcp_conn->tc_mss)
		{
			tcp_fd_write(tcp_conn);
		}
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

	DIFBLOCK(2, (tcp_conn->tc_snd_cwnd == tcp_conn->tc_SND_TRM),
		printf("not sending: zero window\n"));

	if (tcp_conn->tc_snd_cwnd != tcp_conn->tc_SND_TRM &&
		tcp_conn->tc_SND_NXT != tcp_conn->tc_SND_TRM)
	{
		tcp_conn_write(tcp_conn, 1);
	}

}

/*
tcp_send_timeout
*/

PRIVATE void tcp_send_timeout(conn, timer)
int conn;
struct timer *timer;
{
	tcp_conn_t *tcp_conn;
	u16_t mss, mss2;
	time_t curr_time, stt, timeout;

	curr_time= get_time();

	tcp_conn= &tcp_conn_table[conn];
	assert(tcp_conn->tc_flags & TCF_INUSE);
	assert(tcp_conn->tc_state != TCS_CLOSED);
	assert(tcp_conn->tc_state != TCS_LISTEN);

	if (tcp_conn->tc_SND_NXT == tcp_conn->tc_SND_UNA)
	{
		/* Nothing to do */
		assert(tcp_conn->tc_SND_TRM == tcp_conn->tc_SND_UNA);

		/* A new write sets the timer if tc_transmit_seq == SND_UNA */
		tcp_conn->tc_transmit_seq= tcp_conn->tc_SND_UNA;
		tcp_conn->tc_stt= 0;
		tcp_conn->tc_0wnd_to= 0;
		assert(!tcp_conn->tc_fd ||
			!(tcp_conn->tc_fd->tf_flags & TFF_WRITE_IP));
		return;
	}

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
			(curr_time+tcp_conn->tc_rtt)*1000/HZ,
			tcp_conn->tc_rtt*1000/HZ));

		clck_timer(&tcp_conn->tc_transmit_timer,
			curr_time+tcp_conn->tc_rtt,
			tcp_send_timeout, tcp_conn-tcp_conn_table);
		return;
	}

	if (tcp_conn->tc_stt == 0)
	{
		/* Some packet arrived but did not acknowledge any data.
		 * Apparently, the other side is still alive and has a
		 * reason to transmit. We can asume a zero window.
		 */

		DBLOCK(0x10, printf("conn[%d] setting zero window timer\n",
			tcp_conn-tcp_conn_table));

		if (tcp_conn->tc_0wnd_to < TCP_0WND_MIN)
			tcp_conn->tc_0wnd_to= TCP_0WND_MIN;
		else if (tcp_conn->tc_0wnd_to < tcp_conn->tc_rtt)
			tcp_conn->tc_0wnd_to= tcp_conn->tc_rtt;
		else
		{
			tcp_conn->tc_0wnd_to *= 2;
			if (tcp_conn->tc_0wnd_to > TCP_0WND_MAX)
				tcp_conn->tc_0wnd_to= TCP_0WND_MAX;
		}
		tcp_conn->tc_stt= curr_time;
		
		tcp_conn->tc_rt_seq= 0;

		DBLOCK(0x20, printf(
	"tcp_send_timeout: conn[%d] setting timer to %ld ms (+%ld ms)\n",
			tcp_conn-tcp_conn_table,
			(curr_time+tcp_conn->tc_0wnd_to)*1000/HZ,
			tcp_conn->tc_0wnd_to*1000/HZ));

		clck_timer(&tcp_conn->tc_transmit_timer,
			curr_time+tcp_conn->tc_0wnd_to,
			tcp_send_timeout, tcp_conn-tcp_conn_table);
		return;
	}

	DIFBLOCK(0x10, (tcp_conn->tc_fd == 0),
		printf("conn[%d] timeout in abondoned connection\n",
		tcp_conn-tcp_conn_table));

	/* At this point, we have do a retransmission, or send a zero window
	 * probe, which is almost the same.
	 */

	DBLOCK(0x20, printf("tcp_send_timeout: conn[%d] una= %u, rtt= %dms\n",
		tcp_conn-tcp_conn_table,
		tcp_conn->tc_SND_UNA, tcp_conn->tc_rtt*1000/HZ));

	/* Update threshold sequence number for retransmission calculation. */
	if (tcp_Gmod4G(tcp_conn->tc_SND_TRM, tcp_conn->tc_rt_threshold))
		tcp_conn->tc_rt_threshold= tcp_conn->tc_SND_TRM;

	tcp_conn->tc_SND_TRM= tcp_conn->tc_SND_UNA;

	mss= tcp_conn->tc_mss;
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

	stt= tcp_conn->tc_stt;
	assert(stt <= curr_time);
	if (curr_time-stt > tcp_conn->tc_rt_dead)
	{
		tcp_close_connection(tcp_conn, ETIMEDOUT);
		return;
	}

	timeout= (curr_time-stt) >> 3;
	if (timeout < tcp_conn->tc_rtt)
		timeout= tcp_conn->tc_rtt;
	timeout += curr_time;

	DBLOCK(0x20, printf(
	"tcp_send_timeout: conn[%d] setting timer to %ld ms (+%ld ms)\n",
		tcp_conn-tcp_conn_table, timeout*1000/HZ,
		(timeout-curr_time)*1000/HZ));

	clck_timer(&tcp_conn->tc_transmit_timer, timeout,
		tcp_send_timeout, tcp_conn-tcp_conn_table);

	if (tcp_conn->tc_rt_seq == 0)
	{
		tcp_conn->tc_rt_time= curr_time-tcp_conn->tc_rtt;
		tcp_conn->tc_rt_seq= tcp_conn->tc_SND_UNA;
	}

	tcp_conn_write(tcp_conn, 0);
}


PUBLIC void tcp_fd_write(tcp_conn)
tcp_conn_t *tcp_conn;
{
	tcp_fd_t *tcp_fd;
	int urg, nourg, push;
	u32_t max_seq;
	size_t max_count, max_trans, write_count, send_count;
	acc_t *data, *tmp_acc, *send_data;

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

	max_seq= tcp_conn->tc_SND_UNA + tcp_conn->tc_snd_wnd;
	if (urg)
		max_seq++;
	max_count= max_seq - tcp_conn->tc_SND_UNA;
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
	tcp_conn->tc_SND_NXT++;

	assert (tcp_check_conn(tcp_conn) ||
		(tcp_print_conn(tcp_conn), printf("\n"), 0));

	tcp_conn_write(tcp_conn, 1);

	/* Start the timer (if necessary) */
	tcp_set_send_timer(tcp_conn);
}

PUBLIC void tcp_set_send_timer(tcp_conn)
tcp_conn_t *tcp_conn;
{
	time_t curr_time;

	assert(tcp_conn->tc_state != TCS_CLOSED);
	assert(tcp_conn->tc_state != TCS_LISTEN);

	curr_time= get_time();

	/* Start the timer */

	DBLOCK(0x20, printf(
	"tcp_set_send_timer: conn[%d] setting timer to %ld ms (+%ld ms)\n",
		tcp_conn-tcp_conn_table,
		(curr_time+tcp_conn->tc_rtt)*1000/HZ,
		tcp_conn->tc_rtt*1000/HZ));

	clck_timer(&tcp_conn->tc_transmit_timer,
		curr_time+tcp_conn->tc_rtt,
		tcp_send_timeout, tcp_conn-tcp_conn_table);
		tcp_conn->tc_stt= curr_time;

	tcp_conn->tc_stt= curr_time;
}

/*
tcp_close_connection

*/

PUBLIC void tcp_close_connection(tcp_conn, error)
tcp_conn_t *tcp_conn;
int error;
{
	tcp_port_t *tcp_port;
	tcp_fd_t *tcp_fd;
	tcp_conn_t *tc;

	assert (tcp_check_conn(tcp_conn));
	assert (tcp_conn->tc_flags & TCF_INUSE);

	tcp_conn->tc_error= error;
	tcp_port= tcp_conn->tc_port;
	tcp_fd= tcp_conn->tc_fd;
	if (tcp_conn->tc_state == TCS_CLOSED)
		return;

	tcp_conn->tc_state= TCS_CLOSED;
	DBLOCK(0x10, tcp_print_state(tcp_conn); printf("\n"));

	if (tcp_fd)
	{
		tcp_conn->tc_busy++;
		assert(tcp_fd->tf_conn == tcp_conn);

		if (tcp_fd->tf_flags & TFF_READ_IP)
			tcp_fd_read (tcp_conn, 1);
		assert (!(tcp_fd->tf_flags & TFF_READ_IP));

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

		if (tcp_conn->tc_connInprogress)
			tcp_restart_connect(tcp_conn->tc_fd);
		assert (!tcp_conn->tc_connInprogress);
		assert (!(tcp_fd->tf_flags & TFF_IOCTL_IP) ||
			(printf("req= 0x%lx\n", tcp_fd->tf_ioreq), 0));
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
 * $PchId: tcp_send.c,v 1.12 1996/12/17 07:57:11 philip Exp $
 */
