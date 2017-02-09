/*	$NetBSD: cfi.h,v 1.6 2011/12/17 19:42:41 phx Exp $	*/

#ifndef _CFI_H_
#define _CFI_H_

#include <dev/nor/cfi_0002.h>
#include <sys/bus.h>

/*
 * minimum size to bus_space_map for probe/identify QRY:
 * larget offset needed is CFI_QUERY_MODE_ALT_ADDRESS
 * scaled by maximum attempted port width, so
 *	min >= (0x555 * sizeof(uint32_t))
 */
#define CFI_QRY_MIN_MAP_SIZE	0x2000


typedef enum {
	CFI_STATE_DATA_ARRAY = 0,
	CFI_STATE_QUERY,
	/* TBD */
} cfi_state_t;


struct cfi_erase_blk_info {
	uint16_t z;			/* Erase Blocks are z * 256 bytes */
	uint16_t y;			/* y+1 = #Erase Blocks in region */
};

/*
 * CFI Query structure
 */
struct cfi_query_data {
    /* Query info */
    uint8_t	qry[3];			/* { 'Q', 'R', 'Y' } */
    uint16_t	id_pri;			/* primary comand set ID */
    uint16_t	addr_pri;		/* primary table addr */
    uint16_t	id_alt;			/* alternate command set ID */
    uint16_t	addr_alt;		/* alternate table addr */
    /* System Interface info */
    uint8_t	vcc_min;		/* min Vcc */
    uint8_t	vcc_max;		/* max Vcc */
    uint8_t	vpp_min;		/* min Vpp */
    uint8_t	vpp_max;		/* max Vpp */
    uint8_t	write_word_time_typ;	/* typ 1-word timeout, 1<<N usec */
    uint8_t	write_nbyte_time_typ;	/* typ multi-byte timeout, 1<<N usec */
    uint8_t	erase_blk_time_typ;	/* typ 1-blk erase timeout, 1<<N msec */
    uint8_t	erase_chip_time_typ;	/* typ chip erase timeout, 1<<N msec */
    uint8_t	write_word_time_max;	/* max 1-word timeout, typ<<N */
    uint8_t	write_nbyte_time_max;	/* max multi-byte timeout, typ<<N */
    uint8_t	erase_blk_time_max;	/* max 1-blk erase timeout, typ<<N */
    uint8_t	erase_chip_time_max;	/* max chip erase timeout, typ<<N */
    /* Device Geometry Definition */
    uint8_t	device_size;		/* 1<<N bytes */
    uint16_t	interface_code_desc;	/* JEP137 interface code description */
    uint16_t	write_nbyte_size_max;	/* max size of multi-byte write, 1<<N */
    uint8_t	erase_blk_regions;	/* number of erase block regions */
    struct cfi_erase_blk_info
		erase_blk_info[4];	/* describe erase block regions */
    /* Vendor-specific Primary command set info */
    union {
	struct cmdset_0002_query_data cmd_0002;
    } pri;
#ifdef NOTYET
    /* Vendor-specific Alternate command set info */
    union {
	/* some command set structure here */
    } pri;
#endif
};

/*
 * decode interface_code_desc
 */
#define CFI_IFCODE_X8		0
#define CFI_IFCODE_X16		1
#define CFI_IFCODE_X8X16	2
static inline const char *
cfi_interface_desc_str(uint16_t icd)
{
	switch(icd) {
	case CFI_IFCODE_X8:
		return "x8";
	case CFI_IFCODE_X16:
		return "x16";
	case CFI_IFCODE_X8X16:
		return "x8/x16";
	default:
		return "";
	}
}

/*
 * id_pri: CFI Command set and control assignments
 */
#define CFI_ID_PRI_NONE		0x0000
#define CFI_ID_PRI_INTEL_EXT	0x0001
#define CFI_ID_PRI_AMD_STD	0x0002
#define CFI_ID_PRI_INTEL_STD	0x0003
#define CFI_ID_PRI_AMD_EXT	0x0004
#define CFI_ID_PRI_WINBOND	0x0005
#define CFI_ID_PRI_ST_ADV	0x0020
#define CFI_ID_PRI_MITSU_ADV	0x0100
#define CFI_ID_PRI_MITSU_EXT	0x0101
#define CFI_ID_PRI_SST_PAGE	0x0102
#define CFI_ID_PRI_SST_OLD	0x0701
#define CFI_ID_PRI_INTEL_PERF	0x0200
#define CFI_ID_PRI_INTEL_DATA	0x0210
#define CFI_ID_PRI_RESV		0xffff	/* not allowed, reserved */

/*
 * JEDEC ID (autoselect) data
 */
struct cfi_jedec_id_data {
	uint16_t	id_mid;		/* manufacturer ID */
	uint16_t	id_did[3];	/* device ID */
	uint16_t	id_prot_state;
	uint16_t	id_indicators;
	uint8_t		id_swb_lo;	/* lower software bits */
	uint8_t		id_swb_hi;	/* upper software bits */
};

struct cfi;	/* fwd ref */

struct cfi_ops {
	void	(*cfi_reset)(struct cfi *);
	int 	(*cfi_busy)(struct cfi *, flash_off_t);
	int	(*cfi_program_word)(struct cfi *, flash_off_t);
	int	(*cfi_erase_sector)(struct cfi *, flash_off_t);
};

/* NOTE:
 * CFI_0002_STATS are just meant temporarily for debugging
 * not for long-term use. Some event counters at the flash and nor
 * layers might be helpful eventually
 */
#ifdef CFI_0002_STATS
struct cfi_0002_stats {
	u_long read_page;
	u_long program_page;
	u_long erase_all;
	u_long erase_block;
	u_long busy;
	u_long busy_usec_min;
	u_long busy_usec_max;
	struct timeval busy_poll_tv;
	struct timeval busy_yield_tv;
	u_long busy_poll;
	u_long busy_yield;
	u_long busy_yield_hit;
	u_long busy_yield_miss;
	u_long busy_yield_timo;
};

extern void cfi_0002_stats_reset(struct cfi *);
extern void cfi_0002_stats_print(struct cfi *);
#define CFI_0002_STATS_INIT(dev, cfi)			\
    do {						\
	aprint_normal_dev(dev, "cfi=%p\n", cfi);	\
	cfi_0002_stats_reset(cfi);			\
    } while (0)
#define CFI_0002_STATS_INC(cfi, field)	(cfi)->cfi_0002_stats.field++

#else

#define CFI_0002_STATS_INIT(dev, cfi)
#define CFI_0002_STATS_INC(cfi, field)

#endif	/* CFI_0002_STATS */

struct cfi {
	bus_space_tag_t		cfi_bst;
	bus_space_handle_t	cfi_bsh;
	cfi_state_t		cfi_state;
	uint8_t			cfi_portwidth;	/* port width, 1<<N bytes */
	uint8_t			cfi_chipwidth;	/* chip width, 1<<N bytes */
	bool			cfi_emulated;	/* ary data are faked */
	bus_size_t		cfi_unlock_addr1;
	bus_size_t		cfi_unlock_addr2;
	struct cfi_query_data	cfi_qry_data;	/* CFI Query data */
	struct cfi_jedec_id_data
				cfi_id_data;	/* JEDEC ID data */
	const char	       *cfi_name;	/* optional chip name */
	struct cfi_ops		cfi_ops;	/* chip dependent functions */
	u_long			cfi_yield_time;	/* thresh. for yield in wait */
#ifdef CFI_0002_STATS
	struct cfi_0002_stats	cfi_0002_stats;
#endif
};

/*
 * struct cfi_jedec_tab is an amalgamation of
 * - info to identify a chip based on JEDEC ID data, and
 * - values needed to fill in struct cfi (i.e. fields we depend on)
 */
struct cfi_jedec_tab {
	/* ID */
	const char     *jt_name;
	uint32_t	jt_mid;
	uint32_t	jt_did;
	/* cmdset */
	uint16_t	jt_id_pri;
	uint16_t	jt_id_alt;
	/* geometry */
	uint8_t		jt_device_size;			/* 1<<N bytes */
	uint16_t	jt_interface_code_desc;
	uint8_t		jt_write_nbyte_size_max;	/* 1<<N bytes */
	uint8_t		jt_erase_blk_regions;
	struct cfi_erase_blk_info
			jt_erase_blk_info[4];
	/* timing */
	uint8_t		jt_write_word_time_typ;		/* 1<<N usec */
	uint8_t		jt_write_nbyte_time_typ;	/* 1<<N msec */
	uint8_t		jt_erase_blk_time_typ;		/* 1<<N msec */
	uint8_t		jt_erase_chip_time_typ;		/* 1<<N msec */
	uint8_t		jt_write_word_time_max;		/* typ<<N usec */
	uint8_t		jt_write_nbyte_time_max;	/* typ<<N msec */
	uint8_t		jt_erase_blk_time_max;		/* typ<<N msec */
	uint8_t		jt_erase_chip_time_max;		/* typ<<N msec */
};


enum {
	CFI_ADDR_ANY = 0x00*8,		    /* XXX "don't care" */
	CFI_RESET_DATA = 0xf0,
	CFI_ALT_RESET_DATA = 0xff,

	CFI_QUERY_MODE_ADDR = 0x55*8,	    /* some devices accept anything */
	CFI_QUERY_MODE_ALT_ADDR = 0x555*8,
	CFI_QUERY_DATA = 0x98,

	CFI_AMD_UNLOCK_ADDR1 = 0x555*8,
	CFI_AMD_UNLOCK_ADDR2 = 0x555*4,
};

static inline void
cfi_reset(struct cfi * const cfi)
{
	KASSERT(cfi->cfi_ops.cfi_reset != NULL);
	cfi->cfi_ops.cfi_reset(cfi);
}

static inline int
cfi_erase_sector(struct cfi * const cfi, flash_off_t offset)
{
	KASSERT(cfi->cfi_ops.cfi_erase_sector != NULL);
	return cfi->cfi_ops.cfi_erase_sector(cfi, offset);
}

static inline int
cfi_program_word(struct cfi * const cfi, flash_off_t offset)
{
	KASSERT(cfi->cfi_ops.cfi_program_word != NULL);
	return cfi->cfi_ops.cfi_program_word(cfi, offset);
}

extern const struct nor_interface nor_interface_cfi;

extern bool cfi_probe(struct cfi * const);
extern bool cfi_identify(struct cfi * const);
extern void cfi_print(device_t, struct cfi * const);
extern void cfi_reset_default(struct cfi * const);
extern void cfi_reset_std(struct cfi * const);
extern void cfi_reset_alt(struct cfi * const);
extern void cfi_cmd(struct cfi * const, bus_size_t, uint32_t);

#endif	/* _CFI_H_ */
