/*
tcp_recv.c

Copyright 1995 Philip Homburg
*/

#include "inet.h"
#include "buf.h"
#include "clock.h"
#include "event.h"
#include "type.h"

#include "io.h"
#include "tcp_int.h"
#include "tcp.h"
#include "assert.h"

THIS_FILE

FORWARD void create_RST ARGS(( tcp_conn_t *tcp_conn,
	ip_hdr_t *ip_hdr, tcp_hdr_t *tcp_hdr, int data_len ));
FORWARD void process_data ARGS(( tcp_conn_t *tcp_conn,
	tcp_hdr_t *tcp_hdr, acc_t *tcp_data, int data_len ));
FORWARD void process_advanced_data ARGS(( tcp_conn_t *tcp_conn,
	tcp_hdr_t *tcp_hdr, acc_t *tcp_data, int data_len ));

PUBLIC void tcp_frag2conn(tcp_conn, ip_hdr, tcp_hdr, tcp_data, data_len)
tcp_conn_t *tcp_conn;
ip_hdr_t *ip_hdr;
tcp_hdr_t *tcp_hdr;
acc_t *tcp_data;
size_t data_len;
{
	tcp_fd_t *connuser;
	int tcp_hdr_flags;
	int ip_hdr_len, tcp_hdr_len;
	u32_t seg_ack, seg_seq, rcv_hi;
	u16_t seg_wnd;
	int acceptable_ACK, segm_acceptable;

	ip_hdr_len= (ip_hdr->ih_vers_ihl & IH_IHL_MASK) << 2;
	tcp_hdr_len= (tcp_hdr->th_data_off & TH_DO_MASK) >> 2;

	tcp_hdr_flags= tcp_hdr->th_flags & TH_FLAGS_MASK;
	seg_ack= ntohl(tcp_hdr->th_ack_nr);
	seg_seq= ntohl(tcp_hdr->th_seq_nr);
	seg_wnd= ntohs(tcp_hdr->th_window);

	switch (tcp_conn->tc_state)
	{
	case TCS_CLOSED:
/*
CLOSED:
	discard all data.
	!RST ?
		ACK ?
			<SEQ=SEG.ACK><CTL=RST>
			exit
		:
			<SEQ=0><ACK=SEG.SEQ+SEG.LEN><CTL=RST,ACK>
			exit
	:
		discard packet
		exit
*/

		if (!(tcp_hdr_flags & THF_RST))
		{
			create_RST(tcp_conn, ip_hdr, tcp_hdr, data_len);
			tcp_conn_write(tcp_conn, 1);
		}
		break;
	case TCS_LISTEN:
/*
LISTEN:
	RST ?
		discard packet
		exit
	ACK ?
		<SEQ=SEG.ACK><CTL=RST>
		exit
	SYN ?
		BUG: no security check
		RCV.NXT= SEG.SEQ+1
		IRS= SEG.SEQ
		ISS should already be selected
		<SEQ=ISS><ACK=RCV.NXT><CTL=SYN,ACK>
		SND.NXT=ISS+1
		SND.UNA=ISS
		state= SYN-RECEIVED
		exit
	:
		shouldnot occur
		discard packet
		exit
*/
		if (tcp_hdr_flags & THF_RST)
			break;
		if (tcp_hdr_flags & THF_ACK)
		{
			create_RST (tcp_conn, ip_hdr, tcp_hdr, data_len);
			tcp_conn_write(tcp_conn, 1);
			break;
		}
		if (tcp_hdr_flags & THF_SYN)
		{
			tcp_extract_ipopt(tcp_conn, ip_hdr);
			tcp_extract_tcpopt(tcp_conn, tcp_hdr);
			tcp_conn->tc_RCV_LO= seg_seq+1;
			tcp_conn->tc_RCV_NXT= seg_seq+1;
			tcp_conn->tc_RCV_HI= tcp_conn->tc_RCV_LO+
				tcp_conn->tc_rcv_wnd;
			tcp_conn->tc_RCV_UP= seg_seq;
			tcp_conn->tc_IRS= seg_seq;
			tcp_conn->tc_SND_UNA= tcp_conn->tc_ISS;
			tcp_conn->tc_SND_TRM= tcp_conn->tc_ISS;
			tcp_conn->tc_SND_NXT= tcp_conn->tc_ISS+1;
			tcp_conn->tc_SND_UP= tcp_conn->tc_ISS-1;
			tcp_conn->tc_SND_PSH= tcp_conn->tc_ISS-1;
			tcp_conn->tc_state= TCS_SYN_RECEIVED;
			tcp_conn->tc_stt= 0;
			assert (tcp_check_conn(tcp_conn));
			tcp_conn->tc_locaddr= ip_hdr->ih_dst;
			tcp_conn->tc_locport= tcp_hdr->th_dstport;
			tcp_conn->tc_remaddr= ip_hdr->ih_src;
			tcp_conn->tc_remport= tcp_hdr->th_srcport;
			tcp_conn_write(tcp_conn, 1);

			DIFBLOCK(0x10, seg_seq == 0,
				printf("warning got 0 IRS from ");
				writeIpAddr(tcp_conn->tc_remaddr);
				printf("\n"));

			/* Start the timer (if necessary) */
			tcp_set_send_timer(tcp_conn);

			break;
		}
		/* do nothing */
		break;
	case TCS_SYN_SENT:
/*
SYN-SENT:
	ACK ?
		SEG.ACK <= ISS || SEG.ACK > SND.NXT ?
			RST ?
				discard packet
				exit
			:
				<SEQ=SEG.ACK><CTL=RST>
				exit
		SND.UNA <= SEG.ACK && SEG.ACK <= SND.NXT ?
			ACK is acceptable
		:
			ACK is !acceptable
	:
		ACK is !acceptable
	RST ?
		ACK acceptable ?
			discard segment
			state= CLOSED
			error "connection refused"
			exit
		:
			discard packet
			exit
	BUG: no security check
	SYN ?
		IRS= SEG.SEQ
		RCV.NXT= IRS+1
		ACK ?
			SND.UNA= SEG.ACK
		SND.UNA > ISS ?
			state= ESTABLISHED
			<SEQ=SND.NXT><ACK= RCV.NXT><CTL=ACK>
			process ev. URG and text
			exit
		:
			state= SYN-RECEIVED
			SND.WND= SEG.WND
			SND.WL1= SEG.SEQ
			SND.WL2= SEG.ACK
			<SEQ=ISS><ACK=RCV.NXT><CTL=SYN,ACK>
			exit
	:
		discard segment
		exit
*/
		if (tcp_hdr_flags & THF_ACK)
		{
			if (tcp_LEmod4G(seg_ack, tcp_conn->tc_ISS) ||
				tcp_Gmod4G(seg_ack, tcp_conn->tc_SND_NXT))
				if (tcp_hdr_flags & THF_RST)
					break;
				else
				{
					create_RST (tcp_conn, ip_hdr,
						tcp_hdr, data_len);
					tcp_conn_write(tcp_conn, 1);
					break;
				}
			acceptable_ACK= (tcp_LEmod4G(tcp_conn->tc_SND_UNA,
				seg_ack) && tcp_LEmod4G(seg_ack,
				tcp_conn->tc_SND_NXT));
		}
		else
			acceptable_ACK= FALSE;
		if (tcp_hdr_flags & THF_RST)
		{
			if (acceptable_ACK)
			{
				DBLOCK(1, printf(
					"calling tcp_close_connection\n"));

				tcp_close_connection(tcp_conn,
					ECONNREFUSED);
			}
			break;
		}
		if (tcp_hdr_flags & THF_SYN)
		{
			tcp_conn->tc_RCV_LO= seg_seq+1;
			tcp_conn->tc_RCV_NXT= seg_seq+1;
			tcp_conn->tc_RCV_HI= tcp_conn->tc_RCV_LO +
				tcp_conn->tc_rcv_wnd;
			tcp_conn->tc_RCV_UP= seg_seq;
			tcp_conn->tc_IRS= seg_seq;
			if (tcp_hdr_flags & THF_ACK)
				tcp_conn->tc_SND_UNA= seg_ack;
			if (tcp_Gmod4G(tcp_conn->tc_SND_UNA,
				tcp_conn->tc_ISS))
			{
				tcp_conn->tc_state= TCS_ESTABLISHED;
				tcp_conn->tc_rt_dead= TCP_DEF_RT_DEAD;

				assert (tcp_check_conn(tcp_conn));
				assert(tcp_conn->tc_connInprogress);

				tcp_restart_connect(tcp_conn->tc_fd);

				tcp_conn->tc_flags |= TCF_SEND_ACK;
				tcp_conn_write(tcp_conn, 1);
				if (data_len != 0)
				{
					tcp_frag2conn(tcp_conn, ip_hdr,
						tcp_hdr, tcp_data, data_len);
					/* tcp_data is already freed */
					return;
				}
				break;
			}
			tcp_conn->tc_state= TCS_SYN_RECEIVED;

			assert (tcp_check_conn(tcp_conn));

			tcp_conn->tc_SND_TRM= tcp_conn->tc_ISS;
			tcp_conn_write(tcp_conn, 1);
		}
		break;

	case TCS_SYN_RECEIVED:
/*
SYN-RECEIVED:
	test if segment is acceptable:
	Segment	Receive	Test
	Length	Window
	0	0	SEG.SEQ == RCV.NXT
	0	>0	RCV.NXT <= SEG.SEQ && SEG.SEQ < RCV.NXT+RCV.WND
	>0	0	not acceptable
	>0	>0	(RCV.NXT <= SEG.SEQ && SEG.SEQ < RCV.NXT+RCV.WND)
			|| (RCV.NXT <= SEG.SEQ+SEG.LEN-1 &&
			SEG.SEQ+SEG.LEN-1 < RCV.NXT+RCV.WND)
	for urgent data: use RCV.WND+1 for RCV.WND
*/
		rcv_hi= tcp_conn->tc_RCV_HI;
		if (tcp_hdr_flags & THF_URG)
			rcv_hi++;
		if (!data_len)
		{
			if (rcv_hi == tcp_conn->tc_RCV_NXT)
				segm_acceptable= (seg_seq == rcv_hi);
			else
			{
				assert (tcp_Gmod4G(rcv_hi,
					tcp_conn->tc_RCV_NXT));
				segm_acceptable= (tcp_LEmod4G(tcp_conn->
					tc_RCV_NXT, seg_seq) &&
					tcp_Lmod4G(seg_seq, rcv_hi));
			}
		}
		else
		{
			if (tcp_Gmod4G(rcv_hi, tcp_conn->tc_RCV_NXT))
			{
				segm_acceptable= (tcp_LEmod4G(tcp_conn->
					tc_RCV_NXT, seg_seq) &&
					tcp_Lmod4G(seg_seq, rcv_hi)) ||
					(tcp_LEmod4G(tcp_conn->tc_RCV_NXT,
					seg_seq+data_len-1) &&
					tcp_Lmod4G(seg_seq+data_len-1,
					rcv_hi));
			}
			else
			{
				segm_acceptable= FALSE;
			}
		}
/*
	!segment acceptable ?
		RST ?
			discard packet
			exit
		:
			<SEG=SND.NXT><ACK=RCV.NXT><CTL=ACK>
			exit
*/
		if (!segm_acceptable)
		{
			if (!(tcp_hdr_flags & THF_RST))
			{
				tcp_conn->tc_flags |= TCF_SEND_ACK;
				tcp_conn_write(tcp_conn, 1);
			}
			break;
		}
/*
	RST ?
		initiated by a LISTEN ?
			state= LISTEN
			exit
		:
			state= CLOSED
			error "connection refused"
			exit
*/
		if (tcp_hdr_flags & THF_RST)
		{
			if (tcp_conn->tc_orglisten)
			{
				connuser= tcp_conn->tc_fd;

				tcp_conn->tc_connInprogress= 0;
				tcp_conn->tc_fd= NULL;

				tcp_close_connection (tcp_conn, ECONNREFUSED);
				if (connuser)
					(void)tcp_su4listen(connuser);
				break;
			}
			else
			{
				tcp_close_connection(tcp_conn, ECONNREFUSED);
				break;
			}
		}
/*
	SYN in window ?
		initiated by a LISTEN ?
			state= LISTEN
			exit
		:
			state= CLOSED
			error "connection reset"
			exit
*/
		if ((tcp_hdr_flags & THF_SYN) && tcp_GEmod4G(seg_seq,
			tcp_conn->tc_RCV_NXT))
		{
			if (tcp_conn->tc_orglisten)
			{
				connuser= tcp_conn->tc_fd;

				tcp_conn->tc_connInprogress= 0;
				tcp_conn->tc_fd= NULL;

				tcp_close_connection(tcp_conn, ECONNRESET);
				if (connuser)
					(void)tcp_su4listen(connuser);
				break;
			}
			tcp_close_connection(tcp_conn, ECONNRESET);
			break;
		}
/*
	!ACK ?
		discard packet
		exit
*/
		if (!(tcp_hdr_flags & THF_ACK))
			break;
/*
	SND.UNA < SEG.ACK <= SND.NXT ?
		state= ESTABLISHED
	:
		<SEG=SEG.ACK><CTL=RST>
		exit
*/
		if (tcp_Lmod4G(tcp_conn->tc_SND_UNA, seg_ack) &&
			tcp_LEmod4G(seg_ack, tcp_conn->tc_SND_NXT))
		{
			tcp_conn->tc_state= TCS_ESTABLISHED;
			tcp_conn->tc_rt_dead= TCP_DEF_RT_DEAD;

			tcp_release_retrans(tcp_conn, seg_ack, seg_wnd);

			assert (tcp_check_conn(tcp_conn));
			assert(tcp_conn->tc_connInprogress);

			tcp_restart_connect(tcp_conn->tc_fd);
			tcp_frag2conn(tcp_conn, ip_hdr, tcp_hdr, tcp_data,
				data_len);
			/* tcp_data is already freed */
			return;
		}
		else
		{
			create_RST (tcp_conn, ip_hdr, tcp_hdr, data_len);
			tcp_conn_write(tcp_conn, 1);
			break;
		}
		break;

	case TCS_ESTABLISHED:
	case TCS_CLOSING:
/*
ESTABLISHED:
FIN-WAIT-1:
FIN-WAIT-2:
CLOSE-WAIT:
CLOSING:
LAST-ACK:
TIME-WAIT:
	test if segment is acceptable:
	Segment	Receive	Test
	Length	Window
	0	0	SEG.SEQ == RCV.NXT
	0	>0	RCV.NXT <= SEG.SEQ && SEG.SEQ < RCV.NXT+RCV.WND
	>0	0	not acceptable
	>0	>0	(RCV.NXT <= SEG.SEQ && SEG.SEQ < RCV.NXT+RCV.WND)
			|| (RCV.NXT <= SEG.SEQ+SEG.LEN-1 &&
			SEG.SEQ+SEG.LEN-1 < RCV.NXT+RCV.WND)
	for urgent data: use RCV.WND+1 for RCV.WND
*/
		rcv_hi= tcp_conn->tc_RCV_HI;
		if (tcp_hdr_flags & THF_URG)
			rcv_hi++;
		if (!data_len)
		{
			if (rcv_hi == tcp_conn->tc_RCV_NXT)
				segm_acceptable= (seg_seq == rcv_hi);
			else
			{
				assert (tcp_Gmod4G(rcv_hi,
					tcp_conn->tc_RCV_NXT));
				segm_acceptable= (tcp_LEmod4G(tcp_conn->
					tc_RCV_NXT, seg_seq) &&
					tcp_Lmod4G(seg_seq, rcv_hi));
			}
		}
		else
		{
			if (tcp_Gmod4G(rcv_hi, tcp_conn->tc_RCV_NXT))
			{
				segm_acceptable= (tcp_LEmod4G(tcp_conn->
					tc_RCV_NXT, seg_seq) &&
					tcp_Lmod4G(seg_seq, rcv_hi)) ||
					(tcp_LEmod4G(tcp_conn->tc_RCV_NXT,
					seg_seq+data_len-1) &&
					tcp_Lmod4G(seg_seq+data_len-1,
					rcv_hi));
			}
			else
			{
				segm_acceptable= FALSE;
			}
		}
/*
	!segment acceptable ?
		RST ?
			discard packet
			exit
		:
			<SEG=SND.NXT><ACK=RCV.NXT><CTL=ACK>
			exit
*/
		if (!segm_acceptable)
		{
			if (!(tcp_hdr_flags & THF_RST))
			{
				DBLOCK(0x20,
					printf("segment is not acceptable\n");
					printf("\t");
					tcp_print_pack(ip_hdr, tcp_hdr);
					printf("\n\t");
					tcp_print_conn(tcp_conn);
					printf("\n"));
				tcp_conn->tc_flags |= TCF_SEND_ACK;
				tcp_conn_write(tcp_conn, 1);

				/* Sometimes, a retransmission sets the PSH
				 * flag (Solaris 2.4)
				 */
				if (tcp_conn->tc_rcvd_data != NULL &&
					(tcp_hdr_flags & THF_PSH))
				{
					tcp_conn->tc_flags |= TCF_RCV_PUSH;
					if (tcp_conn->tc_fd &&
						(tcp_conn->tc_fd->tf_flags &
						TFF_READ_IP))
					{
						tcp_fd_read(tcp_conn, 1);
					}
				}
			}
			break;
		}
/*
	RST ?
		state == CLOSING || state == LAST-ACK ||
			state == TIME-WAIT ?
			state= CLOSED
			exit
		:
			state= CLOSED
			error "connection reset"
			exit
*/
		if (tcp_hdr_flags & THF_RST)
		{
			if ((tcp_conn->tc_flags &
				(TCF_FIN_SENT|TCF_FIN_RECV)) ==
				(TCF_FIN_SENT|TCF_FIN_RECV) &&
				tcp_conn->tc_send_data == NULL)
			{
				/* Clean shutdown, but the other side
				 * doesn't want to ACK our FIN.
				 */
				tcp_close_connection (tcp_conn, 0);
			}
			else
				tcp_close_connection(tcp_conn, ECONNRESET);
			break;
		}
/*
	SYN in window ?
		state= CLOSED
		error "connection reset"
		exit
*/
		if ((tcp_hdr_flags & THF_SYN) && tcp_GEmod4G(seg_seq,
			tcp_conn->tc_RCV_NXT))
		{
			tcp_close_connection(tcp_conn, ECONNRESET);
			break;
		}
/*
	!ACK ?
		discard packet
		exit
*/
		if (!(tcp_hdr_flags & THF_ACK))
			break;

/*
	SND.UNA < SEG.ACK <= SND.NXT ?
		SND.UNA= SEG.ACK
		reply "send ok"
		SND.WL1 < SEG.SEQ || (SND.WL1 == SEG.SEQ &&
			SND.WL2 <= SEG.ACK ?
			SND.WND= SEG.WND
			SND.Wl1= SEG.SEQ
			SND.WL2= SEG.ACK
	SEG.ACK <= SND.UNA ?
		ignore ACK
	SEG.ACK > SND.NXT ?
		<SEQ=SND.NXT><ACK=RCV.NXT><CTL=ACK>
		discard packet
		exit
*/

		/* Always reset the send timer after a valid ack is
		 * received. The assumption is that either the ack really
		 * acknowledges some data (normal case), contains a zero
		 * window, or the remote host has another reason not
		 * to accept any data. In all cases, the remote host is
		 * alive, so the connection should stay alive too.
		 * Do not reset stt if the state is CLOSING, i.e. if
		 * the user closed the connection and we still have
		 * some data to deliver. We don't want a zero window
		 * to keep us from closing the connection.
		 */
		if (tcp_conn->tc_state != TCS_CLOSING)
			tcp_conn->tc_stt= 0;

		if (seg_ack == tcp_conn->tc_SND_UNA)
		{
			/* This ACK doesn't acknowledge any new data, this
			 * is a likely situation if we are only receiving
			 * data. We only update the window if we are
			 * actually sending or if we currently have a
			 * zero window.
			 */
			if (tcp_conn->tc_snd_cwnd == tcp_conn->tc_SND_UNA &&
				seg_wnd != 0)
			{
				DBLOCK(2, printf("zero window opened\n"));
				/* The other side opened up its receive
				 * window. */
				if (seg_wnd > 2*tcp_conn->tc_mss)
					seg_wnd= 2*tcp_conn->tc_mss;
				tcp_conn->tc_snd_cwnd=
					tcp_conn->tc_SND_UNA+seg_wnd;
				tcp_conn_write(tcp_conn, 1);
			}
			if (seg_wnd == 0)
			{
				tcp_conn->tc_snd_cwnd= tcp_conn->tc_SND_TRM=
					tcp_conn->tc_SND_UNA;
			}
		}
		else if (tcp_Lmod4G(tcp_conn->tc_SND_UNA, seg_ack)
			&& tcp_LEmod4G(seg_ack, tcp_conn->
			tc_SND_NXT))
		{
			tcp_release_retrans(tcp_conn, seg_ack, seg_wnd);
			if (tcp_conn->tc_state == TCS_CLOSED)
				break;
		}
		else if (tcp_Gmod4G(seg_ack,
			tcp_conn->tc_SND_NXT))
		{
			tcp_conn->tc_flags |= TCF_SEND_ACK;
			tcp_conn_write(tcp_conn, 1);
			DBLOCK(1, printf(
			"got an ack of something I haven't send\n");
				printf( "seg_ack= %lu, SND_NXT= %lu\n",
				seg_ack, tcp_conn->tc_SND_NXT));
			break;
		}

/*
	process data...
*/
		tcp_extract_ipopt(tcp_conn, ip_hdr);
		tcp_extract_tcpopt(tcp_conn, tcp_hdr);

		if (data_len)
		{
			if (tcp_LEmod4G(seg_seq, tcp_conn->tc_RCV_NXT))
			{
				process_data (tcp_conn, tcp_hdr,
					tcp_data, data_len);
			}
			else
			{
				process_advanced_data (tcp_conn,
					tcp_hdr, tcp_data, data_len);
			}
			tcp_conn->tc_flags |= TCF_SEND_ACK;
			tcp_conn_write(tcp_conn, 1);

			/* Don't process a FIN if we got new data */
			break;
		}
/*
	FIN ?
		reply pending receives
		advace RCV.NXT over the FIN
		<SEQ=SND.NXT><ACK=RCV.NXT><CTL=ACK>

		state == ESTABLISHED ?
			state= CLOSE-WAIT
		state == FIN-WAIT-1 ?
			state= CLOSING
		state == FIN-WAIT-2 ?
			state= TIME-WAIT
		state == TIME-WAIT ?
			restart the TIME-WAIT timer
	exit
*/
		if ((tcp_hdr_flags & THF_FIN) && tcp_LEmod4G(seg_seq,
			tcp_conn->tc_RCV_NXT))
		{
			if (!(tcp_conn->tc_flags & TCF_FIN_RECV) &&
				tcp_Lmod4G(tcp_conn->tc_RCV_NXT,
				tcp_conn->tc_RCV_HI))
			{
				tcp_conn->tc_RCV_NXT++;
				tcp_conn->tc_flags |= TCF_FIN_RECV;
			}
			tcp_conn->tc_flags |= TCF_SEND_ACK;
			tcp_conn_write(tcp_conn, 1);
			if (tcp_conn->tc_fd &&
				(tcp_conn->tc_fd->tf_flags & TFF_READ_IP))
			{
				tcp_fd_read(tcp_conn, 1);
			}
		}
		break;
	default:
#if !CRAMPED
		printf("tcp_frag2conn: unknown state ");
		tcp_print_state(tcp_conn);
#endif
		break;
	}
	if (tcp_data != NULL)
		bf_afree(tcp_data);
}


PRIVATE void
process_data(tcp_conn, tcp_hdr, tcp_data, data_len)
tcp_conn_t *tcp_conn;
tcp_hdr_t *tcp_hdr;
acc_t *tcp_data;
int data_len;
{
	u32_t lo_seq, hi_seq, urg_seq, seq_nr, adv_seq, nxt;
	u16_t urgptr;
	int tcp_hdr_flags;
	unsigned int offset;
	acc_t *tmp_data, *rcvd_data, *adv_data;
	int len_diff;

	assert(tcp_conn->tc_busy);

	/* Note, tcp_data will be freed by the caller. */
	assert (!(tcp_hdr->th_flags & THF_SYN));

	seq_nr= ntohl(tcp_hdr->th_seq_nr);
	urgptr= ntohs(tcp_hdr->th_urgptr);

	tcp_data->acc_linkC++;

	lo_seq= seq_nr;
	tcp_hdr_flags= tcp_hdr->th_flags & TH_FLAGS_MASK;

	if (tcp_hdr_flags & THF_URG)
	{
		if (urgptr > data_len)
			urgptr= data_len;
		urg_seq= lo_seq+ urgptr;

		if (tcp_GEmod4G(urg_seq, tcp_conn->tc_RCV_HI))
			urg_seq= tcp_conn->tc_RCV_HI;
		if (tcp_conn->tc_flags & TCF_BSD_URG)
		{
			if (tcp_Gmod4G(tcp_conn->tc_RCV_NXT,
				tcp_conn->tc_RCV_LO))
			{
				DBLOCK(1, printf(
					"ignoring urgent data\n"));

				bf_afree(tcp_data);
				/* Should set advertised window to
				 * zero */

				/* Flush */
				tcp_conn->tc_flags |= TCF_RCV_PUSH;
				if (tcp_conn->tc_fd &&
					(tcp_conn->tc_fd->tf_flags &
					TFF_READ_IP))
				{
					tcp_fd_read(tcp_conn, 1);
				}
				return;
			}
		}
		if (tcp_Gmod4G(urg_seq, tcp_conn->tc_RCV_UP))
			tcp_conn->tc_RCV_UP= urg_seq;
		if (urgptr < data_len)
		{
			data_len= urgptr;
			tmp_data= bf_cut(tcp_data, 0, data_len);
			bf_afree(tcp_data);
			tcp_data= tmp_data;
			tcp_hdr_flags &= ~THF_FIN;
		}
		tcp_conn->tc_flags |= TCF_RCV_PUSH;
	}
	else
	{
		/* Normal data. */
	}

	if (tcp_hdr_flags & THF_PSH)
	{
		tcp_conn->tc_flags |= TCF_RCV_PUSH;
	}

	if (tcp_Lmod4G(lo_seq, tcp_conn->tc_RCV_NXT))
	{
		DBLOCK(0x10,
			printf("segment is a retransmission\n"));
		offset= tcp_conn->tc_RCV_NXT-lo_seq;
		tcp_data= bf_delhead(tcp_data, offset);
		lo_seq += offset;
		data_len -= offset;
	}
	assert (lo_seq == tcp_conn->tc_RCV_NXT);

	hi_seq= lo_seq+data_len;
	if (tcp_Gmod4G(hi_seq, tcp_conn->tc_RCV_HI))
	{
		data_len= tcp_conn->tc_RCV_HI-lo_seq;
		tmp_data= bf_cut(tcp_data, 0, data_len);
		bf_afree(tcp_data);
		tcp_data= tmp_data;
		hi_seq= lo_seq+data_len;
		tcp_hdr_flags &= ~THF_FIN;
	}
	assert (tcp_LEmod4G (hi_seq, tcp_conn->tc_RCV_HI));

	rcvd_data= tcp_conn->tc_rcvd_data;
	tcp_conn->tc_rcvd_data= 0;
	tmp_data= bf_append(rcvd_data, tcp_data);
	tcp_conn->tc_rcvd_data= tmp_data;
	tcp_conn->tc_RCV_NXT= hi_seq;

	if ((tcp_hdr_flags & THF_FIN) && 
		tcp_Lmod4G(tcp_conn->tc_RCV_NXT, tcp_conn->tc_RCV_HI) &&
		!(tcp_conn->tc_flags & TCF_FIN_RECV))
	{
		tcp_conn->tc_RCV_NXT++;
		tcp_conn->tc_flags |= TCF_FIN_RECV;
	}

	if (tcp_conn->tc_fd && (tcp_conn->tc_fd->tf_flags & TFF_READ_IP))
		tcp_fd_read(tcp_conn, 1);

	DIFBLOCK(2, (tcp_conn->tc_RCV_NXT == tcp_conn->tc_RCV_HI),
		printf("conn[[%d] full receive buffer\n", 
		tcp_conn-tcp_conn_table));

	if (tcp_conn->tc_adv_data == NULL)
		return;
	if (tcp_hdr_flags & THF_FIN)
	{
#if !CRAMPED
		printf("conn[%d]: advanced data after FIN\n",
			tcp_conn-tcp_conn_table);
#endif
		tcp_data= tcp_conn->tc_adv_data;
		tcp_conn->tc_adv_data= NULL;
		bf_afree(tcp_data);
		return;
	}

	lo_seq= tcp_conn->tc_adv_seq;
	if (tcp_Gmod4G(lo_seq, tcp_conn->tc_RCV_NXT))
		return;		/* Not yet */

	tcp_data= tcp_conn->tc_adv_data;
	tcp_conn->tc_adv_data= NULL;

	data_len= bf_bufsize(tcp_data);
	if (tcp_Lmod4G(lo_seq, tcp_conn->tc_RCV_NXT))
	{
		offset= tcp_conn->tc_RCV_NXT-lo_seq;
		if (offset >= data_len)
		{
			bf_afree(tcp_data);
			return;
		}
		tcp_data= bf_delhead(tcp_data, offset);
		lo_seq += offset;
		data_len -= offset;
	}
	assert (lo_seq == tcp_conn->tc_RCV_NXT);

	hi_seq= lo_seq+data_len;
	assert (tcp_LEmod4G (hi_seq, tcp_conn->tc_RCV_HI));

	rcvd_data= tcp_conn->tc_rcvd_data;
	tcp_conn->tc_rcvd_data= 0;
	tmp_data= bf_append(rcvd_data, tcp_data);
	tcp_conn->tc_rcvd_data= tmp_data;
	tcp_conn->tc_RCV_NXT= hi_seq;

	assert (tcp_conn->tc_RCV_LO + bf_bufsize(tcp_conn->tc_rcvd_data) ==
		tcp_conn->tc_RCV_NXT ||
		(tcp_print_conn(tcp_conn), printf("\n"), 0));

	if (tcp_conn->tc_fd && (tcp_conn->tc_fd->tf_flags & TFF_READ_IP))
		tcp_fd_read(tcp_conn, 1);

	adv_data= tcp_conn->tc_adv_data;
	if (adv_data != NULL)
	{
		/* Try to use advanced data. */
		adv_seq= tcp_conn->tc_adv_seq;
		nxt= tcp_conn->tc_RCV_NXT;

		if (tcp_Gmod4G(adv_seq, nxt))
			return;		/* not yet */

		tcp_conn->tc_adv_data= NULL;
		data_len= bf_bufsize(adv_data);

		if (tcp_Lmod4G(adv_seq, nxt))
		{
			if (tcp_LEmod4G(adv_seq+data_len, nxt))
			{
				/* Data is not needed anymore. */
				bf_afree(adv_data);
				return;
			}

			len_diff= nxt-adv_seq;
			adv_data= bf_delhead(adv_data, len_diff);
			data_len -= len_diff;
		}

		DBLOCK(1, printf("using advanced data\n"));

		/* Append data to the input buffer */
		if (tcp_conn->tc_rcvd_data == NULL)
		{
			tcp_conn->tc_rcvd_data= adv_data;
		}
		else
		{
			tcp_conn->tc_rcvd_data=
				bf_append(tcp_conn->tc_rcvd_data, adv_data);
		}
		tcp_conn->tc_SND_NXT += data_len;
		assert(tcp_check_conn(tcp_conn));

		if (tcp_conn->tc_fd &&
			(tcp_conn->tc_fd->tf_flags & TFF_READ_IP))
		{
			tcp_fd_read(tcp_conn, 1);
		}
	}
}

PRIVATE void process_advanced_data(tcp_conn, tcp_hdr, tcp_data, data_len)
tcp_conn_t *tcp_conn;
tcp_hdr_t *tcp_hdr;
acc_t *tcp_data;
int data_len;
{
	u32_t seq, adv_seq;
	acc_t *adv_data;

	assert(tcp_conn->tc_busy);

	/* Note, tcp_data will be freed by the caller. */

	/* Always send an ACK, this allows the sender to do a fast
	 * retransmit.
	 */
	tcp_conn->tc_flags |= TCF_SEND_ACK;
	tcp_conn_write(tcp_conn, 1);

	if (tcp_hdr->th_flags & THF_URG)
		return;	/* Urgent data is to complicated */
	if (tcp_hdr->th_flags & THF_PSH)
		tcp_conn->tc_flags |= TCF_RCV_PUSH;
	seq= ntohl(tcp_hdr->th_seq_nr);

	/* Make sure that the packet doesn't fall outside of the window
	 * we offered.
	 */
	if (tcp_Gmod4G(seq+data_len, tcp_conn->tc_RCV_HI))
		return;

	adv_data= tcp_conn->tc_adv_data;
	adv_seq= tcp_conn->tc_adv_seq;
	tcp_conn->tc_adv_data= NULL;

	tcp_data->acc_linkC++;
	if (adv_data == NULL)
	{
		adv_seq= seq;
		adv_data= tcp_data;
	}
	else if (seq + data_len == adv_seq)
	{
		/* New data fits right before exiting data. */
		adv_data= bf_append(tcp_data, adv_data);
		adv_seq= seq;
	}
	else if (adv_seq + bf_bufsize(adv_data) == seq)
	{
		/* New data fits right after exiting data. */
		adv_data= bf_append(adv_data, tcp_data);
	}
	else
	{
		/* New data doesn't fit. */
		bf_afree(tcp_data);
	}
	tcp_conn->tc_adv_data= adv_data;
	tcp_conn->tc_adv_seq= adv_seq;
}
				
PRIVATE void create_RST(tcp_conn, ip_hdr, tcp_hdr, data_len)
tcp_conn_t *tcp_conn;
ip_hdr_t *ip_hdr;
tcp_hdr_t *tcp_hdr;
int data_len;
{
	acc_t *tmp_ipopt, *tmp_tcpopt, *tcp_pack;
	ip_hdropt_t ip_hdropt;
	tcp_hdropt_t tcp_hdropt;
	acc_t *RST_acc;
	ip_hdr_t *RST_ip_hdr;
	tcp_hdr_t *RST_tcp_hdr;
	char *ptr2RSThdr;
	size_t pack_size, ip_hdr_len;

	DBLOCK(0x10, printf("in create_RST, bad pack is:\n"); 
		tcp_print_pack(ip_hdr, tcp_hdr); tcp_print_state(tcp_conn);
		printf("\n"));

	assert(tcp_conn->tc_busy);

	/* Only send RST packets in reponse to actual data (or SYN, FIN)
	 * this solves a problem during connection shutdown. The problem
	 * is the follow senario: a senders closes the connection instead
	 * of doing a shutdown and waiting for the receiver to shutdown.
	 * The receiver is slow in processing the last data. After the
	 * sender has completely closed the connection, the receiver
	 * sends a window update which triggers the sender to send a
	 * RST. The receiver closes the connection in reponse to the RST.
	 */
	if ((tcp_hdr->th_flags & (THF_FIN|THF_SYN)) == 0 &&
		data_len == 0)
	{
#if DEBUG
 { printf("tcp_recv`create_RST: no data, no RST\n"); }
#endif
		return;
	}

	tmp_ipopt= tcp_conn->tc_remipopt;
	if (tmp_ipopt)
		tmp_ipopt->acc_linkC++;
	tmp_tcpopt= tcp_conn->tc_tcpopt;
	if (tmp_tcpopt)
		tmp_tcpopt->acc_linkC++;

	tcp_extract_ipopt (tcp_conn, ip_hdr);
	tcp_extract_tcpopt (tcp_conn, tcp_hdr);

	RST_acc= tcp_make_header (tcp_conn, &RST_ip_hdr, &RST_tcp_hdr,
		(acc_t *)0);

	if (tcp_conn->tc_remipopt)
		bf_afree(tcp_conn->tc_remipopt);
	tcp_conn->tc_remipopt= tmp_ipopt;
	if (tcp_conn->tc_tcpopt)
		bf_afree(tcp_conn->tc_tcpopt);
	tcp_conn->tc_tcpopt= tmp_tcpopt;

	RST_ip_hdr->ih_src= ip_hdr->ih_dst;
	RST_ip_hdr->ih_dst= ip_hdr->ih_src;

	RST_tcp_hdr->th_srcport= tcp_hdr->th_dstport;
	RST_tcp_hdr->th_dstport= tcp_hdr->th_srcport;
	if (tcp_hdr->th_flags & THF_ACK)
	{
		RST_tcp_hdr->th_seq_nr= tcp_hdr->th_ack_nr;
		RST_tcp_hdr->th_flags= THF_RST;
	}
	else
	{
		RST_tcp_hdr->th_seq_nr= 0;
		RST_tcp_hdr->th_ack_nr=
			htonl(
				ntohl(tcp_hdr->th_seq_nr)+
				data_len +
				(tcp_hdr->th_flags & THF_SYN ? 1 : 0) +
				(tcp_hdr->th_flags & THF_FIN ? 1 : 0));
		RST_tcp_hdr->th_flags= THF_RST|THF_ACK;
	}

	pack_size= bf_bufsize(RST_acc);
	RST_ip_hdr->ih_length= htons(pack_size);
	RST_tcp_hdr->th_window= htons(tcp_conn->tc_rcv_wnd);
	RST_tcp_hdr->th_chksum= 0;

	RST_acc->acc_linkC++;
	ip_hdr_len= (ip_hdr->ih_vers_ihl & IH_IHL_MASK) << 2;
	tcp_pack= bf_delhead(RST_acc, ip_hdr_len);
	RST_tcp_hdr->th_chksum= ~tcp_pack_oneCsum (RST_ip_hdr, tcp_pack);
	bf_afree(tcp_pack);
	
	DBLOCK(2, tcp_print_pack(ip_hdr, tcp_hdr); printf("\n");
		tcp_print_pack(RST_ip_hdr, RST_tcp_hdr); printf("\n"));

	if (tcp_conn->tc_frag2send)
		bf_afree(tcp_conn->tc_frag2send);
	tcp_conn->tc_frag2send= RST_acc;
	tcp_conn_write(tcp_conn, 1);
}

PUBLIC void
tcp_fd_read(tcp_conn, enq)
tcp_conn_t *tcp_conn;
int enq;					/* Enqueue writes. */
{
	tcp_fd_t *tcp_fd;
	size_t data_size, read_size;
	acc_t *data;
	int fin_recv, urg, push, result;
	i32_t old_window, new_window;

	assert(tcp_conn->tc_busy);

	tcp_fd= tcp_conn->tc_fd;

	assert (tcp_fd->tf_flags & TFF_READ_IP);
	if (tcp_conn->tc_state == TCS_CLOSED)
	{
		if (tcp_fd->tf_read_offset)
			tcp_reply_read (tcp_fd, tcp_fd->tf_read_offset);
		else
			tcp_reply_read (tcp_fd, tcp_conn->tc_error);
		return;
	}

	urg= tcp_Gmod4G(tcp_conn->tc_RCV_UP, tcp_conn->tc_RCV_LO);
	push= (tcp_conn->tc_flags & TCF_RCV_PUSH);
	fin_recv= (tcp_conn->tc_flags & TCF_FIN_RECV);

	data_size= tcp_conn->tc_RCV_NXT-tcp_conn->tc_RCV_LO;
	if (fin_recv)
		data_size--;
	if (urg)
		read_size= tcp_conn->tc_RCV_UP-tcp_conn->tc_RCV_LO;
	else
		read_size= data_size;

	if (read_size >= tcp_fd->tf_read_count)
		read_size= tcp_fd->tf_read_count;
	else if (!push && !fin_recv && !urg &&
		data_size < TCP_MIN_RCV_WND_SIZE)
	{
		/* Defer the copy out until later. */
		return;
	}
	else if (data_size == 0 && !fin_recv)
	{
		/* No data, and no end of file. */
		return;
	}

	if (read_size)
	{
		if (urg && !(tcp_fd->tf_flags & TFF_RECV_URG))
		{
			if (tcp_fd->tf_read_offset)
			{
				tcp_reply_read (tcp_fd,
					tcp_fd->tf_read_offset);
			}
			else
			{
				tcp_reply_read (tcp_fd, EURG);
			}
			return;
		}
		else if (!urg && (tcp_fd->tf_flags & TFF_RECV_URG))
		{
			if (tcp_fd->tf_read_offset)
			{
				tcp_reply_read (tcp_fd,
					tcp_fd->tf_read_offset);
			}
			else
			{
				tcp_reply_read(tcp_fd, ENOURG);
			}
			return;
		}

		if (read_size == data_size)
		{
			data= tcp_conn->tc_rcvd_data;
			data->acc_linkC++;
		}
		else
		{
			data= bf_cut(tcp_conn->tc_rcvd_data, 0, read_size);
		}
		result= (*tcp_fd->tf_put_userdata) (tcp_fd->tf_srfd,
			tcp_fd->tf_read_offset, data, FALSE);
		if (result<0)
		{
			if (tcp_fd->tf_read_offset)
				tcp_reply_read(tcp_fd, tcp_fd->
					tf_read_offset);
			else
				tcp_reply_read(tcp_fd, result);
			return;
		}
		tcp_fd->tf_read_offset += read_size;
		tcp_fd->tf_read_count -= read_size;

		if (data_size == read_size)
		{
			bf_afree(tcp_conn->tc_rcvd_data);
			tcp_conn->tc_rcvd_data= 0;
		}
		else
		{
			tcp_conn->tc_rcvd_data=
				bf_delhead(tcp_conn->tc_rcvd_data,
				read_size);
		}
		tcp_conn->tc_RCV_LO += read_size;
		data_size -= read_size;
	}
	if (tcp_conn->tc_RCV_HI-tcp_conn->tc_RCV_LO <= (tcp_conn->
		tc_rcv_wnd-tcp_conn->tc_mss))
	{
		old_window= tcp_conn->tc_RCV_HI-tcp_conn->tc_RCV_NXT;
		tcp_conn->tc_RCV_HI= tcp_conn->tc_RCV_LO + 
			tcp_conn->tc_rcv_wnd;
		new_window= tcp_conn->tc_RCV_HI-tcp_conn->tc_RCV_NXT;
		assert(old_window >=0 && new_window >= old_window);
		if (old_window < tcp_conn->tc_mss &&
			new_window >= tcp_conn->tc_mss)
		{
			tcp_conn->tc_flags |= TCF_SEND_ACK;
			DBLOCK(2, printf("opening window\n"));
			tcp_conn_write(tcp_conn, 1);
		}
	}
	if (tcp_conn->tc_rcvd_data == NULL &&
		tcp_conn->tc_adv_data == NULL)
	{
		/* Out of data, clear PUSH flag and reply to a read. */
		tcp_conn->tc_flags &= ~TCF_RCV_PUSH;
	}
	if (fin_recv || urg || !tcp_fd->tf_read_count)
	{
		tcp_reply_read (tcp_fd, tcp_fd->tf_read_offset);
		return;
	}
	if (tcp_fd->tf_read_offset)
	{
		tcp_reply_read (tcp_fd, tcp_fd->tf_read_offset);
		return;
	}
}

/*
 * $PchId: tcp_recv.c,v 1.13.2.1 2000/05/02 18:53:06 philip Exp $
 */
