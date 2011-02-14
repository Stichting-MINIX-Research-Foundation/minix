/*
server/ip/gen/icmp_hdr.h
*/

#ifndef __SERVER__IP__GEN__ICMP_HDR_H__
#define __SERVER__IP__GEN__ICMP_HDR_H__

typedef struct icmp_id_seq
{
	u16_t	iis_id, iis_seq;
} icmp_id_seq_t;

typedef struct icmp_ip_id
{
	ip_hdr_t iii_hdr;
	/* ip_hdr_options and 64 bytes of data */
} icmp_ip_id_t;

typedef struct icmp_ram		/* RFC 1256 */
{
	u8_t	iram_na;
	u8_t	iram_aes;
	u16_t	iram_lt;
} icmp_ram_t;

typedef struct icmp_pp
{
	u8_t	ipp_ptr;
	u8_t	ipp_unused[3];
} icmp_pp_t;

typedef struct icmp_mtu		/* RFC 1191 */
{
	u16_t	im_unused;
	u16_t	im_mtu;
} icmp_mtu_t;

typedef struct icmp_hdr
{
	u8_t ih_type, ih_code;
	u16_t ih_chksum;
	union
	{
		u32_t ihh_unused;
		icmp_id_seq_t ihh_idseq;
		ipaddr_t ihh_gateway;
		icmp_ram_t ihh_ram;
		icmp_pp_t ihh_pp;
		icmp_mtu_t ihh_mtu;
	} ih_hun;
	union
	{
		icmp_ip_id_t ihd_ipid;
		u8_t uhd_data[1];
	} ih_dun;
} icmp_hdr_t;

#endif /* __SERVER__IP__GEN__ICMP_HDR_H__ */

/*
 * $PchId: icmp_hdr.h,v 1.5 2002/06/10 07:10:48 philip Exp $
 */
