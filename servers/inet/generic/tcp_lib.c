/*
tcp_lib.c

Copyright 1995 Philip Homburg
*/

#include "inet.h"
#include "buf.h"
#include "clock.h"
#include "event.h"
#include "io.h"
#include "type.h"

#include "assert.h"
#include "tcp_int.h"

THIS_FILE

#if you_want_to_be_complete

#undef tcp_LEmod4G
PUBLIC int tcp_LEmod4G(n1, n2)
u32_t n1;
u32_t n2;
{
	return !((u32_t)(n2-n1) & 0x80000000L);
}

#undef tcp_GEmod4G
PUBLIC int tcp_GEmod4G(n1, n2)
u32_t n1;
u32_t n2;
{
	return !((u32_t)(n1-n2) & 0x80000000L);
}

#undef tcp_Lmod4G
PUBLIC int tcp_Lmod4G(n1, n2)
u32_t n1;
u32_t n2;
{
	return !!((u32_t)(n1-n2) & 0x80000000L);
}

#undef tcp_Gmod4G
PUBLIC int tcp_Gmod4G(n1, n2)
u32_t n1;
u32_t n2;
{
	return !!((u32_t)(n2-n1) & 0x80000000L);
}
#endif

PUBLIC void tcp_extract_ipopt(tcp_conn, ip_hdr)
tcp_conn_t *tcp_conn;
ip_hdr_t *ip_hdr;
{
	int ip_hdr_len;

	ip_hdr_len= (ip_hdr->ih_vers_ihl & IH_IHL_MASK) << 2;
	if (ip_hdr_len == IP_MIN_HDR_SIZE)
		return;

	DBLOCK(1, printf("ip_hdr options NOT supported (yet?)\n"));
}

PUBLIC void tcp_extract_tcpopt(tcp_conn, tcp_hdr)
tcp_conn_t *tcp_conn;
tcp_hdr_t *tcp_hdr;
{
	int tcp_hdr_len;

	tcp_hdr_len= (tcp_hdr->th_data_off & TH_DO_MASK) >> 2;
	if (tcp_hdr_len == TCP_MIN_HDR_SIZE)
		return;

	DBLOCK(2, printf("tcp_hdr options NOT supported (yet?)\n"));
}

PUBLIC u16_t tcp_pack_oneCsum(ip_hdr, tcp_pack)
ip_hdr_t *ip_hdr;
acc_t *tcp_pack;
{
	size_t ip_hdr_len;
	acc_t *pack;
	u16_t sum;
	u16_t word_buf[6];
	int odd_length;
	char *data_ptr;
	int length;

	ip_hdr_len= (ip_hdr->ih_vers_ihl & IH_IHL_MASK) << 2;
	word_buf[0]= ip_hdr->ih_src & 0xffff;
	word_buf[1]= (ip_hdr->ih_src >> 16) & 0xffff;
	word_buf[2]= ip_hdr->ih_dst & 0xffff;
	word_buf[3]= (ip_hdr->ih_dst >> 16) & 0xffff;
	word_buf[4]= HTONS(IPPROTO_TCP);
	word_buf[5]= htons(ntohs(ip_hdr->ih_length)-ip_hdr_len);
	sum= oneC_sum(0, word_buf, sizeof(word_buf));

	pack= tcp_pack;
	odd_length= 0;
	for (; pack; pack= pack->acc_next)
	{
		
		data_ptr= ptr2acc_data(pack);
		length= pack->acc_length;

		if (!length)
			continue;
		sum= oneC_sum (sum, (u16_t *)data_ptr, length);
		if (length & 1)
		{
			odd_length= !odd_length;
			sum= ((sum >> 8) & 0xff) | ((sum & 0xff) << 8);
		}
	}
	if (odd_length)
	{
		/* Undo the last swap */
		sum= ((sum >> 8) & 0xff) | ((sum & 0xff) << 8);
	}
	return sum;
}

PUBLIC void tcp_get_ipopt(tcp_conn, ip_hdropt)
tcp_conn_t *tcp_conn;
ip_hdropt_t *ip_hdropt;
{
	if (!tcp_conn->tc_remipopt)
	{
		ip_hdropt->iho_opt_siz= 0;
		return;
	}
	DBLOCK(1, printf("ip_hdr options NOT supported (yet?)\n"));
	ip_hdropt->iho_opt_siz= 0;
	return;
}

PUBLIC void tcp_get_tcpopt(tcp_conn, tcp_hdropt)
tcp_conn_t *tcp_conn;
tcp_hdropt_t *tcp_hdropt;
{
	int optsiz;

	if (!tcp_conn->tc_tcpopt)
	{
		tcp_hdropt->tho_opt_siz= 0;
		return;
	}
	tcp_conn->tc_tcpopt= bf_pack(tcp_conn->tc_tcpopt);
	optsiz= bf_bufsize(tcp_conn->tc_tcpopt);
	memcpy(tcp_hdropt->tho_data, ptr2acc_data(tcp_conn->tc_tcpopt),
		optsiz);
	if ((optsiz & 3) != 0)
	{
		tcp_hdropt->tho_data[optsiz]= TCP_OPT_EOL;
		optsiz= (optsiz+3) & ~3;
	}
	tcp_hdropt->tho_opt_siz= optsiz;

	return;
}

PUBLIC acc_t *tcp_make_header(tcp_conn, ref_ip_hdr, ref_tcp_hdr, data)
tcp_conn_t *tcp_conn;
ip_hdr_t **ref_ip_hdr;
tcp_hdr_t **ref_tcp_hdr;
acc_t *data;
{
	ip_hdropt_t ip_hdropt;
	tcp_hdropt_t tcp_hdropt;
	ip_hdr_t *ip_hdr;
	tcp_hdr_t *tcp_hdr;
	acc_t *hdr_acc;
	char *ptr2hdr;
	int closed_connection;

	closed_connection= (tcp_conn->tc_state == TCS_CLOSED);

	if (tcp_conn->tc_remipopt || tcp_conn->tc_tcpopt)
	{
		tcp_get_ipopt (tcp_conn, &ip_hdropt);
		tcp_get_tcpopt (tcp_conn, &tcp_hdropt);
		assert (!(ip_hdropt.iho_opt_siz & 3));
		assert (!(tcp_hdropt.tho_opt_siz & 3));

		hdr_acc= bf_memreq(IP_MIN_HDR_SIZE+
			ip_hdropt.iho_opt_siz+TCP_MIN_HDR_SIZE+
			tcp_hdropt.tho_opt_siz);
		ptr2hdr= ptr2acc_data(hdr_acc);

		ip_hdr= (ip_hdr_t *)ptr2hdr;
		ptr2hdr += IP_MIN_HDR_SIZE;

		if (ip_hdropt.iho_opt_siz)
		{
			memcpy(ptr2hdr, (char *)ip_hdropt.iho_data,
				ip_hdropt.iho_opt_siz);
		}
		ptr2hdr += ip_hdropt.iho_opt_siz;

		tcp_hdr= (tcp_hdr_t *)ptr2hdr;
		ptr2hdr += TCP_MIN_HDR_SIZE;

		if (tcp_hdropt.tho_opt_siz)
		{
			memcpy (ptr2hdr, (char *)tcp_hdropt.tho_data,
				tcp_hdropt.tho_opt_siz);
		}
		hdr_acc->acc_next= data;

		ip_hdr->ih_vers_ihl= (IP_MIN_HDR_SIZE+
			ip_hdropt.iho_opt_siz) >> 2;
		tcp_hdr->th_data_off= (TCP_MIN_HDR_SIZE+
			tcp_hdropt.tho_opt_siz) << 2;
	}
	else
	{
		hdr_acc= bf_memreq(IP_MIN_HDR_SIZE+TCP_MIN_HDR_SIZE);
		ip_hdr= (ip_hdr_t *)ptr2acc_data(hdr_acc);
		tcp_hdr= (tcp_hdr_t *)&ip_hdr[1];
		hdr_acc->acc_next= data;

		ip_hdr->ih_vers_ihl= IP_MIN_HDR_SIZE >> 2;
		tcp_hdr->th_data_off= TCP_MIN_HDR_SIZE << 2;
	}

	if (!closed_connection && (tcp_conn->tc_state == TCS_CLOSED))
	{
		DBLOCK(1, printf("connection closed while inuse\n"));
		bf_afree(hdr_acc);
		return 0;
	}

	ip_hdr->ih_tos= tcp_conn->tc_tos;
	ip_hdr->ih_ttl= tcp_conn->tc_ttl;
	ip_hdr->ih_proto= IPPROTO_TCP;
	ip_hdr->ih_src= tcp_conn->tc_locaddr;
	ip_hdr->ih_dst= tcp_conn->tc_remaddr;
	ip_hdr->ih_flags_fragoff= 0;

	tcp_hdr->th_srcport= tcp_conn->tc_locport;
	tcp_hdr->th_dstport= tcp_conn->tc_remport;
	tcp_hdr->th_seq_nr= tcp_conn->tc_RCV_NXT;
	tcp_hdr->th_flags= 0;
	tcp_hdr->th_window= htons(tcp_conn->tc_RCV_HI-tcp_conn->tc_RCV_LO);
	tcp_hdr->th_chksum= 0;
	*ref_ip_hdr= ip_hdr;
	*ref_tcp_hdr= tcp_hdr;
	return hdr_acc;
}

#if !CRAMPED
PUBLIC void tcp_print_state (tcp_conn)
tcp_conn_t *tcp_conn;
{
	printf("tcp_conn_table[%d]->tc_state= ", tcp_conn-
		tcp_conn_table);
	if (!(tcp_conn->tc_flags & TCF_INUSE))
	{
		printf("not inuse\n");
		return;
	}
	switch (tcp_conn->tc_state)
	{
	case TCS_CLOSED: printf("CLOSED"); break;
	case TCS_LISTEN: printf("LISTEN"); break;
	case TCS_SYN_RECEIVED: printf("SYN_RECEIVED"); break;
	case TCS_SYN_SENT: printf("SYN_SENT"); break;
	case TCS_ESTABLISHED: printf("ESTABLISHED"); break;
	case TCS_CLOSING: printf("CLOSING"); break;
	default: printf("unknown (=%d)", tcp_conn->tc_state); break;
	}
}
#endif

PUBLIC int tcp_check_conn(tcp_conn)
tcp_conn_t *tcp_conn;
{
	int allright;
	u32_t lo_queue, hi_queue;
	int size;

	allright= TRUE;
	if (tcp_conn->tc_inconsistent)
	{
		assert(tcp_conn->tc_inconsistent == 1);
#if !CRAMPED
		printf("tcp_check_conn: connection is inconsistent\n");
#endif
		return allright;
	}

	/* checking receive queue */
	lo_queue= tcp_conn->tc_RCV_LO;
	if (lo_queue == tcp_conn->tc_IRS)
		lo_queue++;
	if (lo_queue == tcp_conn->tc_RCV_NXT && (tcp_conn->tc_flags &
		TCF_FIN_RECV))
		lo_queue--;
	hi_queue= tcp_conn->tc_RCV_NXT;
	if (hi_queue == tcp_conn->tc_IRS)
		hi_queue++;
	if (tcp_conn->tc_flags & TCF_FIN_RECV)
		hi_queue--;

	size= hi_queue-lo_queue;
	if (size<0)
	{
#if !CRAMPED
		printf("rcv hi_queue-lo_queue < 0\n");
		printf("SND_NXT= 0x%x, SND_UNA= 0x%x\n", 
			tcp_conn->tc_SND_NXT, tcp_conn->tc_SND_UNA);
		printf("lo_queue= 0x%x, hi_queue= 0x%x\n", 
			lo_queue, hi_queue);
		printf("size= %d\n", size);
#endif
		allright= FALSE;
	}
	else if (!tcp_conn->tc_rcvd_data)
	{
		if (size)
		{
#if !CRAMPED
			printf("RCV_NXT-RCV_LO != 0\n");
			tcp_print_conn(tcp_conn);
			printf("lo_queue= %lu, hi_queue= %lu\n",
				lo_queue, hi_queue);
#endif
			allright= FALSE;
		}
	}
	else if (size != bf_bufsize(tcp_conn->tc_rcvd_data))
	{
#if !CRAMPED
		printf("RCV_NXT-RCV_LO != sizeof tc_rcvd_data\n");
		tcp_print_conn(tcp_conn);
		printf(
		"lo_queue= %lu, hi_queue= %lu, sizeof tc_rcvd_data= %d\n",
			lo_queue, hi_queue, bf_bufsize(tcp_conn->tc_rcvd_data));
#endif
		allright= FALSE;
	}
	else if (size != 0 && (tcp_conn->tc_state == TCS_CLOSED ||
		tcp_conn->tc_state == TCS_LISTEN ||
		tcp_conn->tc_state == TCS_SYN_RECEIVED ||
		tcp_conn->tc_state ==  TCS_SYN_SENT))
	{
#if !CRAMPED
		printf("received data but not connected\n");
		tcp_print_conn(tcp_conn);
#endif
		allright= FALSE;
	}
	if (tcp_Lmod4G(tcp_conn->tc_RCV_HI, tcp_conn->tc_RCV_NXT))
	{
#if !CRAMPED
		printf("tc_RCV_HI (%d) < tc_RCV_NXT (%d)\n", 
			tcp_conn->tc_RCV_HI, tcp_conn->tc_RCV_NXT);
#endif
		allright= FALSE;
	}

	/* checking send data */
	lo_queue= tcp_conn->tc_SND_UNA;
	if (lo_queue == tcp_conn->tc_ISS)
		lo_queue++;
	if (lo_queue == tcp_conn->tc_SND_NXT &&
		(tcp_conn->tc_flags & TCF_FIN_SENT))
	{
		lo_queue--;
	}
	hi_queue= tcp_conn->tc_SND_NXT;
	if (hi_queue == tcp_conn->tc_ISS)
		hi_queue++;
	if (tcp_conn->tc_flags & TCF_FIN_SENT)
		hi_queue--;

	size= hi_queue-lo_queue;
	if (size<0)
	{
#if !CRAMPED
		printf("snd hi_queue-lo_queue < 0\n");
		printf("SND_ISS= 0x%x, SND_UNA= 0x%x, SND_NXT= 0x%x\n",
			tcp_conn->tc_ISS, tcp_conn->tc_SND_UNA,
			tcp_conn->tc_SND_NXT);
		printf("hi_queue= 0x%x, lo_queue= 0x%x, size= %d\n",
			hi_queue, lo_queue, size);
#endif
		allright= FALSE;
	}
	else if (!tcp_conn->tc_send_data)
	{
		if (size)
		{
#if !CRAMPED
			printf("SND_NXT-SND_UNA != 0\n");
			printf("SND_NXT= %d, SND_UNA= %d\n", 
				tcp_conn->tc_SND_NXT, tcp_conn->tc_SND_UNA);
			printf("lo_queue= %d, hi_queue= %d\n", 
				lo_queue, hi_queue);
#endif
			allright= FALSE;
		}
	}
	else if (size != bf_bufsize(tcp_conn->tc_send_data))
	{
#if !CRAMPED
		printf("SND_NXT-SND_UNA != sizeof tc_send_data\n");
		printf("SND_NXT= %d, SND_UNA= %d\n", 
			tcp_conn->tc_SND_NXT, tcp_conn->tc_SND_UNA);
		printf("lo_queue= %d, lo_queue= %d\n", 
			lo_queue, hi_queue);
		printf("bf_bufsize(data)= %d\n", 
			bf_bufsize(tcp_conn->tc_send_data));
#endif
		allright= FALSE;
	}

	/* checking counters */
	if (!tcp_GEmod4G(tcp_conn->tc_SND_UNA, tcp_conn->tc_ISS))
	{
#if !CRAMPED
		printf("SND_UNA < ISS\n");
#endif
		allright= FALSE;
	}
	if (!tcp_GEmod4G(tcp_conn->tc_SND_NXT, tcp_conn->tc_SND_UNA))
	{
#if !CRAMPED
		printf("SND_NXT<SND_UNA\n");
#endif
		allright= FALSE;
	}
	if (!tcp_GEmod4G(tcp_conn->tc_SND_TRM, tcp_conn->tc_SND_UNA))
	{
#if !CRAMPED
		printf("SND_TRM<SND_UNA\n");
#endif
		allright= FALSE;
	}
	if (!tcp_GEmod4G(tcp_conn->tc_SND_NXT, tcp_conn->tc_SND_TRM))
	{
#if !CRAMPED
		printf("SND_NXT<SND_TRM\n");
#endif
		allright= FALSE;
	}

	DIFBLOCK(1, (!allright), printf("tcp_check_conn: not allright\n"));
	return allright;
}

#if !CRAMPED
PUBLIC void tcp_print_pack(ip_hdr, tcp_hdr)
ip_hdr_t *ip_hdr;
tcp_hdr_t *tcp_hdr;
{
	int tcp_hdr_len;

	assert(tcp_hdr);
	if (ip_hdr)
		writeIpAddr(ip_hdr->ih_src);
	else
		printf("???");
	printf(",%u ", ntohs(tcp_hdr->th_srcport));
	if (ip_hdr)
		writeIpAddr(ip_hdr->ih_dst);
	else
		printf("???");
	printf(",%u ", ntohs(tcp_hdr->th_dstport));
	printf(" 0x%lx", ntohl(tcp_hdr->th_seq_nr));
	if (tcp_hdr->th_flags & THF_FIN)
		printf(" <FIN>");
	if (tcp_hdr->th_flags & THF_SYN)
		printf(" <SYN>");
	if (tcp_hdr->th_flags & THF_RST)
		printf(" <RST>");
	if (tcp_hdr->th_flags & THF_PSH)
		printf(" <PSH>");
	if (tcp_hdr->th_flags & THF_ACK)
		printf(" <ACK 0x%x %u>", ntohl(tcp_hdr->th_ack_nr),
			ntohs(tcp_hdr->th_window));
	if (tcp_hdr->th_flags & THF_URG)
		printf(" <URG %u>", tcp_hdr->th_urgptr);
	tcp_hdr_len= (tcp_hdr->th_data_off & TH_DO_MASK) >> 2;
	if (tcp_hdr_len != TCP_MIN_HDR_SIZE)
		printf(" <options %d>", tcp_hdr_len-TCP_MIN_HDR_SIZE);
}
#endif

#if !CRAMPED
PUBLIC void tcp_print_conn(tcp_conn)
tcp_conn_t *tcp_conn;
{
	int iss, irs;

	iss= tcp_conn->tc_ISS;
	irs= tcp_conn->tc_IRS;

	tcp_print_state (tcp_conn);
	printf(
	" ISS 0x%lx UNA +0x%lx(0x%lx) TRM +0x%lx(0x%lx) NXT +0x%lx(0x%lx)",
		iss, tcp_conn->tc_SND_UNA-iss, tcp_conn->tc_SND_UNA, 
		tcp_conn->tc_SND_TRM-iss, tcp_conn->tc_SND_TRM,
		tcp_conn->tc_SND_NXT-iss, tcp_conn->tc_SND_NXT);
	printf(" snd_cwnd +0x%lx(0x%lx)",
		tcp_conn->tc_snd_cwnd-tcp_conn->tc_SND_UNA,
		tcp_conn->tc_snd_cwnd);
	printf(" transmit_seq ");
	if (tcp_conn->tc_transmit_seq == 0)
		printf("0");
	else
	{
		printf("+0x%lx(0x%lx)", tcp_conn->tc_transmit_seq-iss,
			tcp_conn->tc_transmit_seq);
	}
	printf(" IRS 0x%lx LO +0x%x(0x%lx) NXT +0x%x(0x%lx) HI +0x%x(0x%lx)",
		irs, tcp_conn->tc_RCV_LO-irs, tcp_conn->tc_RCV_LO,
		tcp_conn->tc_RCV_NXT-irs, tcp_conn->tc_RCV_NXT,
		tcp_conn->tc_RCV_HI-irs, tcp_conn->tc_RCV_HI);
	if (tcp_conn->tc_flags & TCF_INUSE)
		printf(" TCF_INUSE");
	if (tcp_conn->tc_flags & TCF_FIN_RECV)
		printf(" TCF_FIN_RECV");
	if (tcp_conn->tc_flags & TCF_RCV_PUSH)
		printf(" TCF_RCV_PUSH");
	if (tcp_conn->tc_flags & TCF_MORE2WRITE)
		printf(" TCF_MORE2WRITE");
	if (tcp_conn->tc_flags & TCF_SEND_ACK)
		printf(" TCF_SEND_ACK");
	if (tcp_conn->tc_flags & TCF_FIN_SENT)
		printf(" TCF_FIN_SENT");
}
#endif

/*
 * $PchId: tcp_lib.c,v 1.7 1995/11/21 06:45:27 philip Exp $
 */
