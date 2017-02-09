/*	$NetBSD: scsi_ctron_ether.h,v 1.9 2005/12/11 12:23:50 christos Exp $	*/

/*
 * SCSI interface description
 */


/*
 * SCSI command format
 */

#define CTRON_ETHERCMD2(OP, SUB) {OP, SUB}
#define CTRON_ETHER_SEND CTRON_ETHERCMD2(0xc, 0x1)
#define CTRON_ETHER_RECV 0xe1
#define CTRON_ETHER_GET_ADDR CTRON_ETHERCMD2(0xc, 0x4)
#define CTRON_ETHER_ADD_PROTO CTRON_ETHERCMD2(0xd, 0x1)
#define CTRON_ETHER_REM_PROTO CTRON_ETHERCMD2(0xd, 0x2)
#define CTRON_ETHER_SET_MODE CTRON_ETHERCMD2(0xd, 0x3)
#define CTRON_ETHER_SET_MULTI CTRON_ETHERCMD2(0xd, 0x4)
#define CTRON_ETHER_REMOVE_MULTI CTRON_ETHERCMD2(0xd, 0x5)
#define CTRON_ETHER_GET_STATS CTRON_ETHERCMD2(0xd, 0x6)
#define CTRON_ETHER_SET_MEDIA CTRON_ETHERCMD2(0xd, 0x07)
#define CTRON_ETHER_GET_MEDIA CTRON_ETHERCMD2(0xd, 0x08)
#define CTRON_ETHER_SET_ADDR CTRON_ETHERCMD2(0xd, 0xA)
#define CTRON_ETHER_LOAD_IMAGE CTRON_ETHERCMD2(0xd, 0x09)
#define IS_RECV(generic) ((generic)->opcode == 0xe1)
#define IS_SEND(generic) ((generic)->opcode == 0xc && (generic)->bytes[0] == 0x1)

struct scsi_ctron_ether_recv {
	u_int8_t opcode;	/* This really *is* all */
};

struct scsi_ctron_ether_generic {
	u_int8_t opcode[2];
	u_int8_t byte3;
	u_int8_t length[2];
	u_int8_t byte6;
};

struct scsi_ctron_ether_set_mode {
	u_int8_t opcode[2];
	u_int8_t mode;
	u_int8_t length[2];
	u_int8_t byte6;
};


struct ctron_stats {
	u_int32_t  frames_xmit;
	u_int32_t  bytes_xmit;
	u_int32_t  ucast_xmit;			/* never incremented? */
	u_int32_t  mcast_xmit;			/* gets ucasts and mcasts?? */
	u_int32_t  bcast_xmit;
	u_int32_t  defer_xmit;
	u_int32_t  sgl_coll;
	u_int32_t  multi_coll;
	u_int32_t  tot_xmit_err;
	u_int32_t  late_coll;
	u_int32_t  excess_coll;
	u_int32_t  int_err_xmit;
	u_int32_t  carr_err;
	u_int32_t  media_abort;
	u_int32_t  frames_rec;
	u_int32_t  bytes_rec;
	u_int32_t  ucast_rec;			/* never incremented? */
	u_int32_t  mcast_rec;			/* gets ucasts and mcasts?? */
	u_int32_t  bcast_rec;
	u_int32_t  tot_rec_err;
	u_int32_t  too_long;
	u_int32_t  too_short;
	u_int32_t  align_err;
	u_int32_t  crc_err;
	u_int32_t  len_err;
	u_int32_t  int_err_rec;
	u_int32_t  sqe_err;
};

struct scsi_ctron_ether_inquiry_data {
/* standard; */
	u_int8_t device;		/* 3 (T_CPU) */
	u_int8_t dev_qual2;		/* 0 (fixed) */
	u_int8_t version;		/* 0 */
	u_int8_t response_format;		/* 0 */
	u_int8_t additional_len;	/* 75!! */
	u_int8_t unused[2];		/* 0, 0 */
	u_int8_t flags; 		/* 0x18 (sync+linked!?) */
	char vendor[8]; 		/* ie; "Cabletrn" or "CABLETRN" */
	char product[16];		/* ie; "EA412/...." */
	char revision[4];		/* ie; "0100" or "1.00" */
	char extra[8];			/* ie; "00.00.19" or "01.00.00" */
/* non-standard; */
	u_int8_t hwaddr[6];		/* PROM ethernet addr */
	u_int8_t swaddr[6];		/* curr ethernet addr */
	char date[22];			/* firmware date string (asciz) */
	u_int8_t mtype; 		/* media type?? */
	u_int8_t hwport;		/* value of h/w read port?? */
};

enum scsi_ctron_ether_media {
	CMEDIA_PRIMARY=0,		/* twisted pair */
	CMEDIA_SECONDARY=1,		/* coax */
	CMEDIA_AUTOSENSE=2		/* set_media command only */
};
