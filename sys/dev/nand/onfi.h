/*	$NetBSD: onfi.h,v 1.1 2011/02/26 18:07:31 ahoka Exp $	*/

/*-
 * Copyright (c) 2010 Department of Software Engineering,
 *		      University of Szeged, Hungary
 * Copyright (c) 2010 Adam Hoka <ahoka@NetBSD.org>
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by the Department of Software Engineering, University of Szeged, Hungary
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _ONFI_H_
#define _ONFI_H_

/**
 * ONFI 2.2, Section 5.1: Command Set
 *
 * Indented ones are 2nd or 3rd cycle commands.
 */

enum {
	ONFI_READ				= 0x00,	/* M */
	 ONFI_READ_START			= 0x30,	/* M */
	 ONFI_READ_INTERLEAVED			= 0x32,	/* O */
	 ONFI_READ_COPYBACK			= 0x35,	/* O */
	 ONFI_READ_CACHE_RANDOM			= 0x31,	/* O */

	ONFI_CHANGE_READ_COLUMN			= 0x05,	/* M */
	ONFI_CHANGE_READ_COLUMN_ENHANCED	= 0x06,	/* O */
	 ONFI_CHANGE_READ_COLUMN_START		= 0xe0,	/* M */

	ONFI_READ_CACHE_SEQUENTIAL		= 0x31,	/* O */
	ONFI_READ_CACHE_END			= 0x3f,	/* O */

	ONFI_BLOCK_ERASE			= 0x60,	/* M */
	 ONFI_BLOCK_ERASE_START			= 0xd0,	/* M */
	 ONFI_BLOCK_ERASE_INTERLEAVED		= 0xd1,	/* O */

	ONFI_READ_STATUS			= 0x70,	/* M */
	ONFI_READ_STATUS_ENHANCED		= 0x78,	/* O */

	ONFI_PAGE_PROGRAM			= 0x80,	/* M */
	 ONFI_PAGE_PROGRAM_START		= 0x10,	/* M */
	 ONFI_PAGE_PROGRAM_INTERLEAVED		= 0x11,	/* O */
	 ONFI_PAGE_CACHE_PROGRAM		= 0x15,	/* O */

	ONFI_COPYBACK_PROGRAM			= 0x85,	/* O */
	 ONFI_COPYBACK_PROGRAM_START		= 0x10,	/* O */ 
	 ONFI_COPYBACK_PROGRAM_INTERLEAVED	= 0x11,	/* O */
/*-
 * Small Data's first opcode may be 80h if the operation is a program only
 * with no data output. For the last second cycle of a Small Data Move,
 * it is a 10h command to confirm the Program or Copyback operation
 */
	ONFI_SMALL_DATA_MOVE			= 0x85,	/* O */
	ONFI_SMALL_DATA_MOVE_START		= 0x11,	/* O */

	ONFI_CHANGE_WRITE_COLUMN		= 0x85,	/* M */
	ONFI_CHANGE_ROW_ADDRESS			= 0x85,	/* O */

	ONFI_READ_ID				= 0x90,	/* M */
	ONFI_READ_PARAMETER_PAGE		= 0xec,	/* M */
	ONFI_READ_UNIQUE_ID			= 0xed,	/* O */
	ONFI_GET_FEATURES			= 0xee,	/* O */
	ONFI_SET_FEATURES			= 0xef,	/* O */
	ONFI_RESET_LUN				= 0xfa,	/* O */
	ONFI_SYNCHRONOUS_RESET			= 0xfc,	/* O */
	ONFI_RESET				= 0xff	/* M */
};

/**
 * status codes from ONFI_READ_STATUS
 */
enum {
	ONFI_STATUS_FAIL			= (1<<0),
	ONFI_STATUS_FAILC			= (1<<1),
	ONFI_STATUS_R				= (1<<2),
	ONFI_STATUS_CSP				= (1<<3),
	ONFI_STATUS_VSP				= (1<<4),
	ONFI_STATUS_ARDY			= (1<<5),
	ONFI_STATUS_RDY				= (1<<6),
	ONFI_STATUS_WP				= (1<<7)
};

enum {
	ONFI_FEATURE_16BIT			= (1<<0),
	ONFI_FEATURE_EXTENDED_PARAM		= (1<<7)
};
	
/* 5.7.1. Parameter Page Data Structure Definition */
struct onfi_parameter_page {
	/* Revision information and features block */
	uint32_t param_signature; /* M: onfi signature ({'O','N','F','I'}) */
	uint16_t param_revision;  /* M: revision number */
	uint16_t param_features;  /* M: features supported */
	uint16_t param_optional_cmds; /* M: optional commands */
	uint16_t param_reserved_1;    /* R: reserved */
	uint16_t param_extended_len;  /* O: extended parameter page lenght */
	uint8_t param_num_param_pg;  /* O: number of parameter pages */
	uint8_t param_reserved_2[17]; /* R: reserved */
	/* Manufacturer information block */
	uint8_t param_manufacturer[12]; /* M: device manufacturer (ASCII) */
	uint8_t param_model[20];	      /* M: device model (ASCII) */
	uint8_t param_manufacturer_id;  /* M: JEDEC ID of manufacturer */
	uint16_t param_date;	      /* O: date code (BCD) */
	uint8_t param_reserved_3[13];   /* R: reserved */
	/* Memory organization block */
	uint32_t param_pagesize; /* M: number of data bytes per page */
	uint16_t param_sparesize; /* M: number of spare bytes per page */
	uint32_t param_part_pagesize; /* O: obsolete */
	uint16_t param_part_sparesize; /* O: obsolete */
	uint32_t param_blocksize;      /* M: number of pages per block */
	uint32_t param_lunsize;       /* M: number of blocks per LUN */
	uint8_t param_numluns;	      /* M: number of LUNs */
	uint8_t param_addr_cycles;   /* M: address cycles:
					col: 4-7 (high), row: 0-3 (low) */
	uint8_t param_cellsize;   /* M: number of bits per cell */
	uint16_t param_lun_maxbad; /* M: maximum badblocks per LUN */
	uint16_t param_block_endurance; /* M: block endurance */
	uint8_t param_guaranteed_blocks; /* M: guaranteed valid blocks at
					  begginning of target */
	uint16_t param_guaranteed_endurance; /* M: block endurance of
					      guranteed blocks */
	uint8_t param_programs_per_page; /* M: number of programs per page */
	uint8_t param_partial_programming_attr; /* O: obsolete */
	uint8_t param_ecc_correctable_bits;     /* M: number of bits
						 ECC correctability */
	uint8_t param_interleaved_addr_bits; /* M: num of interleaved address
					      bits (only low half is valid) */
	uint8_t param_interleaved_op_attrs; /* O: obsolete */
	uint8_t param_reserved_4[13];	  /* R: reserved */
	/* Electrical parameters block */
	uint8_t param_io_c_max; /* M: I/O pin capacitance, maximum */
	uint16_t param_async_timing_mode; /* M: async timing mode support */
	uint16_t param_async_progcache_timing_mode; /* O: obsolete */
	uint16_t param_t_prog; /* M: maximum page program time (us) */
	uint16_t param_t_bers; /* M: maximum block erase time (us) */
	uint16_t param_t_r;    /* M: maximum page read time (us) */
	uint16_t param_ccs;    /* M: minimum change column setup time (ns) */
	uint16_t param_sync_timing_mode; /* source sync timing mode support */
	uint8_t param_sync_features;     /* M: source sync features */
	uint16_t param_clk_input_c;   /* O: CLK input pin cap., typical */
	uint16_t param_io_c;	    /* O: I/O pin capacitance, typical */
	uint16_t param_input_c;	    /* O: input pin capacitance, typical */
	uint8_t param_input_c_max;    /* M: input pin capacitance, maximum */
	uint8_t param_driver_strength; /* M: driver strength support */
	uint16_t param_t_r_interleaved; /* O: maximum interleaved
					 page read time (us) */
	uint16_t param_t_adl;	/* O: program page register clear enhancement
				 tADL value (ns) */
	uint8_t param_reserved_5[8]; /* R: reserved */
	/* Vendor block */
	uint16_t param_vendor_revision; /* M: vendor specific rev number */
	uint8_t param_vendor_specific[88]; /* vendor specific information */
	uint16_t param_integrity_crc;	 /* M: integrity CRC */
} __packed;

#endif	/* _ONFI_H_ */
