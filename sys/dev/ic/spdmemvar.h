/* $NetBSD: spdmemvar.h,v 1.7 2015/05/15 08:44:24 msaitoh Exp $ */

/*
 * Copyright (c) 2007 Paul Goyette
 * Copyright (c) 2007 Tobias Nygren
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This information is extracted from JEDEC standard SPD4_01 (www.jedec.org)
 */

#if BYTE_ORDER == BIG_ENDIAN
#define SPD_BITFIELD(a, b, c, d) d; c; b; a
#else
#define SPD_BITFIELD(a, b, c, d) a; b; c; d
#endif

	/*
	 * NOTE
	 *
	 * Fields with "offsets" are field widths, measured in bits,
	 * with "offset" additional bits.  Thus, a field with value
	 * of 2 with an offset of 14 defines a field with total width
	 * of 16 bits.
	 */

struct spdmem_fpm {				/* FPM and EDO DIMMS */
	uint8_t	fpm_len;
	uint8_t fpm_size;
	uint8_t fpm_type;
	uint8_t fpm_rows;
	uint8_t fpm_cols;
	uint8_t fpm_banks;
	uint16_t fpm_datawidth;			/* endian-sensitive */
	uint8_t fpm_voltage;
	uint8_t	fpm_tRAC;
	uint8_t fpm_tCAC;
	uint8_t fpm_config;
	SPD_BITFIELD(				\
		uint8_t fpm_refresh:7,		\
		uint8_t fpm_selfrefresh:1, ,	\
	);
	uint8_t fpm_dram_dramwidth;
	uint8_t fpm_dram_eccwidth;
	uint8_t	fpm_unused2[17];
	uint8_t	fpm_superset;
	uint8_t fpm_unused3[30];
	uint8_t	fpm_cksum;
} __packed;

struct spdmem_sdram {				/* PC66/PC100/PC133 SDRAM */
	uint8_t	sdr_len;
	uint8_t sdr_size;
	uint8_t sdr_type;
	SPD_BITFIELD(				\
		uint8_t sdr_rows:4,		\
		uint8_t sdr_rows2:4, ,		\
	);
	SPD_BITFIELD(				\
		uint8_t sdr_cols:4,		\
		uint8_t sdr_cols2:4, ,		\
	);
	uint8_t sdr_banks;
	uint16_t sdr_datawidth;			/* endian-sensitive */
	uint8_t sdr_voltage;
	SPD_BITFIELD(				\
		uint8_t sdr_cycle_tenths:4,	\
		uint8_t sdr_cycle_whole:4, ,	\
	);
	SPD_BITFIELD(
		uint8_t sdr_tAC_tenths:4,	\
		uint8_t	sdr_tAC_whole:4, ,	\
	);
	uint8_t sdr_config;
	SPD_BITFIELD(				\
		uint8_t sdr_refresh:7,		\
		uint8_t sdr_selfrefresh:1, ,	\
	);
	SPD_BITFIELD(				\
		uint8_t sdr_dramwidth:7,	\
		uint8_t sdr_dram_asym_bank2:1, ,\
	);
	SPD_BITFIELD(				\
		uint8_t sdr_eccwidth:7,		\
		uint8_t sdr_ecc_asym_bank2:1, ,	\
	);
	uint8_t sdr_min_clk_delay;
	SPD_BITFIELD(				\
		uint8_t sdr_burstlengths:4,	\
		uint8_t sdr_unused1:4, ,	\
	);
	uint8_t sdr_banks_per_chip;
	uint8_t sdr_tCAS;
	uint8_t sdr_tCS;
	uint8_t sdr_tWE;
	uint8_t sdr_mod_attrs;
	uint8_t sdr_dev_attrs;
	uint8_t sdr_min_cc_1;
	uint8_t sdr_max_tAC_1;
	uint8_t sdr_min_cc_2;
	uint8_t sdr_max_tAC_2;
	uint8_t sdr_tRP;
	uint8_t sdr_tRRD;
	uint8_t sdr_tRCD;
	uint8_t sdr_tRAS;
	uint8_t sdr_module_rank_density;
	uint8_t sdr_tIS;
#define	sdr_superset sdr_tIS
	uint8_t sdr_tIH;
	uint8_t sdr_tDS;
	uint8_t sdr_tDH;
	uint8_t sdr_unused2[5];
	uint8_t sdr_tRC;
	uint8_t	sdr_unused3[18];
	uint8_t	sdr_esdram;
	uint8_t	sdr_super_tech;
	uint8_t	sdr_spdrev;
	uint8_t	sdr_cksum;
} __packed;

struct spdmem_rom {
	uint8_t	rom_len;
	uint8_t rom_size;
	uint8_t rom_type;
	uint8_t rom_rows;
	uint8_t rom_cols;
	uint8_t rom_banks;
	uint16_t rom_datawidth;			/* endian-sensitive */
	uint8_t rom_voltage;
	uint16_t rom_tAA;			/* endian-sensitive */
	uint8_t rom_config;
	uint8_t	rom_unused1;
	uint8_t	rom_tPA;
	uint8_t rom_tOE;
	uint16_t rom_tCE;			/* endian-sensitive */
	uint8_t	rom_burstlength;
	uint8_t rom_unused2[14];
	uint8_t	rom_superset[31];
	uint8_t	rom_cksum;
} __packed;


struct spdmem_ddr {				/* Dual Data Rate SDRAM */
	uint8_t	ddr_len;
	uint8_t ddr_size;
	uint8_t ddr_type;
	SPD_BITFIELD(				\
		uint8_t ddr_rows:4,		\
		uint8_t ddr_rows2:4, ,		\
	);
	SPD_BITFIELD(				\
		uint8_t ddr_cols:4,		\
		uint8_t ddr_cols2:4, ,		\
	);
	uint8_t ddr_ranks;
	uint16_t ddr_datawidth;			/* endian-sensitive */
	uint8_t ddr_voltage;
	SPD_BITFIELD(				\
		uint8_t ddr_cycle_tenths:4,	\
		uint8_t ddr_cycle_whole:4, ,	\
	);
	SPD_BITFIELD(				\
		uint8_t ddr_tAC_hundredths:4,	\
		uint8_t	ddr_tAC_tenths:4, ,	\
	);
	uint8_t ddr_config;
	SPD_BITFIELD(				\
		uint8_t ddr_refresh:7,		\
		uint8_t ddr_selfrefresh:1, ,	\
	);
	SPD_BITFIELD(				\
		uint8_t ddr_dramwidth:7,	\
		uint8_t ddr_dram_asym_bank2:1, ,\
	);
	SPD_BITFIELD(				\
		uint8_t ddr_eccwidth:7,		\
		uint8_t ddr_ecc_asym_bank2:1, ,	\
	);
	uint8_t ddr_min_clk_delay;
	SPD_BITFIELD(				\
		uint8_t ddr_burstlengths:4,	\
		uint8_t ddr_unused1:4, ,	\
	);
	uint8_t ddr_banks_per_chip;
	uint8_t ddr_tCAS;
	uint8_t ddr_tCS;
	uint8_t ddr_tWE;
	uint8_t ddr_mod_attrs;
	uint8_t ddr_dev_attrs;
	uint8_t ddr_min_cc_05;
	uint8_t ddr_max_tAC_05;
	uint8_t ddr_min_cc_1;
	uint8_t ddr_max_tAC_1;
	uint8_t ddr_tRP;
	uint8_t ddr_tRRD;
	uint8_t ddr_tRCD;
	uint8_t ddr_tRAS;
	uint8_t ddr_module_rank_density;
	uint8_t ddr_tIS;
#define	ddr_superset ddr_tIS
	uint8_t ddr_tIH;
	uint8_t ddr_tDS;
	uint8_t ddr_tDH;
	uint8_t	ddr_unused2[5];
	uint8_t ddr_tRC;
	uint8_t ddr_tRFC;
	uint8_t ddr_tCK;
	uint8_t	ddr_tDQSQ;
	uint8_t	ddr_tQHS;
	uint8_t	ddr_unused3;
	uint8_t	ddr_height;
	uint8_t ddr_unused4[15];
	uint8_t	ddr_cksum;
} __packed;

struct spdmem_ddr2 {				/* Dual Data Rate 2 SDRAM */
	uint8_t	ddr2_len;
	uint8_t ddr2_size;
	uint8_t ddr2_type;
	SPD_BITFIELD(				\
		uint8_t ddr2_rows:5,		\
		uint8_t ddr2_unused1:3,	,	\
	);
	SPD_BITFIELD(				\
		uint8_t ddr2_cols:4,		\
		uint8_t ddr2_unused2:4, ,	\
	);
	SPD_BITFIELD(				\
		uint8_t ddr2_ranks:3,
		uint8_t ddr2_cardoncard:1,	\
		uint8_t ddr2_package:1,		\
		uint8_t ddr2_height:3		\
	);
	uint8_t ddr2_datawidth;
	uint8_t	ddr2_unused3;
	uint8_t ddr2_voltage;
	SPD_BITFIELD(				\
		uint8_t ddr2_cycle_frac:4,	\
		uint8_t ddr2_cycle_whole:4, ,	\
	);
	SPD_BITFIELD(				\
		uint8_t ddr2_tAC_hundredths:4,	\
		uint8_t	ddr2_tAC_tenths:4, ,	\
	);
	uint8_t ddr2_config;
	SPD_BITFIELD(				\
		uint8_t ddr2_refresh:7,		\
		uint8_t ddr2_selfrefresh:1, ,	\
	);
	uint8_t	ddr2_dramwidth;
	uint8_t	ddr2_eccwidth;
	uint8_t	ddr2_unused4;
	SPD_BITFIELD(				\
		uint8_t ddr2_burstlengths:4,	\
		uint8_t ddr2_unused5:4, ,	\
	);
	uint8_t ddr2_banks_per_chip;
	uint8_t ddr2_tCAS;
	uint8_t ddr2_mechanical;
	uint8_t	ddr2_dimm_type;
	uint8_t ddr2_mod_attrs;
	uint8_t ddr2_dev_attrs;
	uint8_t ddr2_min_cc_1;
	uint8_t ddr2_max_tAC_1;
	uint8_t ddr2_min_cc_2;
	uint8_t ddr2_max_tAC_2;
	uint8_t ddr2_tRP;
	uint8_t ddr2_tRRD;
	uint8_t ddr2_tRCD;
	uint8_t ddr2_tRAS;
	uint8_t ddr2_module_rank_density;
	uint8_t ddr2_tIS;
	uint8_t ddr2_tIH;
	uint8_t ddr2_tDS;
	uint8_t ddr2_tDH;
	uint8_t ddr2_tWR;
	uint8_t ddr2_tWTR;
	uint8_t ddr2_tRTP;
	uint8_t ddr2_probe;
	uint8_t	ddr2_extensions;
	uint8_t	ddr2_tRC;
	uint8_t	ddr2_tRFC;
	uint8_t	ddr2_tCK;
	uint8_t	ddr2_tDQSQ;
	uint8_t	ddr2_tQHS;
	uint8_t	ddr2_pll_relock;
	uint8_t	ddr2_Tcasemax;
	uint8_t	ddr2_Psi_TA_DRAM;
	uint8_t	ddr2_dt0;
	uint8_t	ddr2_dt2NQ;
	uint8_t	ddr2_dr2P;
	uint8_t	ddr2_dt3N;
	uint8_t	ddr2_dt3Pfast;
	uint8_t	ddr2_dt3Pslow;
	uint8_t	ddr2_dt4R_4R4W_mode;
	uint8_t	ddr2_dt5B;
	uint8_t	ddr2_dt7;
	uint8_t	ddr2_Psi_TA_PLL;
	uint8_t	ddr2_Psi_TA_Reg;
	uint8_t	ddr2_dt_PLL_Active;
	uint8_t	ddr2_dt_Reg_Active;
	uint8_t ddr2_spdrev;
	uint8_t	ddr2_cksum;
} __packed;

struct spdmem_fbdimm {				/* Fully-buffered DIMM */
	uint8_t	fbdimm_len;
	uint8_t fbdimm_size;
	uint8_t fbdimm_type;
	SPD_BITFIELD(				\
		uint8_t	fbdimm_ps1_voltage:4,	\
		uint8_t	fbdimm_ps2_voltage:4, ,	\
	);
	SPD_BITFIELD(				\
		uint8_t	fbdimm_banks:2,		\
		uint8_t	fbdimm_cols:3,		\
		uint8_t	fbdimm_rows:3,		\
	);
	SPD_BITFIELD(				\
		uint8_t	fbdimm_thick:3,		\
		uint8_t	fbdimm_height:3,	\
		uint8_t	fbdimm_unused1:2,	\
	);
	uint8_t	fbdimm_mod_type;
	SPD_BITFIELD(				\
		uint8_t	fbdimm_dev_width:3,	\
		uint8_t	fbdimm_ranks:3,		\
		uint8_t fbdimm_unused2:2,	\
	);
	SPD_BITFIELD(				\
		uint8_t	fbdimm_ftb_divisor:4,	\
		uint8_t	fbdimm_ftp_dividend:4, ,\
	);
	uint8_t	fbdimm_mtb_dividend;
	uint8_t	fbdimm_mtb_divisor;
	uint8_t	fbdimm_tCKmin;
	uint8_t	fbdimm_tCKmax;
	uint8_t	fbdimm_tCAS;
	uint8_t	fbdimm_tAAmin;
	SPD_BITFIELD(				\
		uint8_t	fbdimm_tWR_min:4,	\
		uint8_t	fbdimm_WR_range:4, ,	\
	);
	uint8_t	fbdimm_tWR;
	SPD_BITFIELD(				\
		uint8_t	fbdimm_tWL_min:4,	\
		uint8_t	fbdimm_tWL_range:4, ,	\
	);
	SPD_BITFIELD(				\
		uint8_t	fbdimm_tAL_min:4,	\
		uint8_t	fbdimm_tAL_range:4, ,	\
	);
	uint8_t	fbdimm_tRCDmin;
	uint8_t	fbdimm_tRRDmin;
	uint8_t	fbdimm_tRPmin;
	SPD_BITFIELD(				\
		uint8_t	fbdimm_tRAS_msb:4,	\
		uint8_t	fbdimm_tRC_msb:4, ,	\
	);
	uint8_t	fbdimm_tRAS_lsb;
	uint8_t	fbdimm_tRC_lsb;
	uint16_t fbdimm_tRFC;			/* endian-sensitive */
	uint8_t	fbdimm_tWTR;
	uint8_t	fbdimm_tRTP;
	SPD_BITFIELD(				\
		uint8_t	fbdimm_burst_4:1,	\
		uint8_t	fbdimm_burst_8:1,	\
		uint8_t	fbdimm_unused3:6,	\
	);
	uint8_t	fbdimm_terms;
	uint8_t	fbdimm_drivers;
	uint8_t	fbdimm_tREFI;
	uint8_t	fbdimm_Tcasemax;
	uint8_t	fbdimm_Psi_TA_SDRAM;
	uint8_t	fbdimm_DT0;
	uint8_t	fbdimm_DT2N_DT2Q;
	uint8_t	fbdimm_DT2P;
	uint8_t	fbdimm_DT3N;
	uint8_t	fbdimm_DT4R_DT4R4W;
	uint8_t	fbdimm_DT5B;
	uint8_t	fbdimm_DT7;
	uint8_t	fbdimm_unused4[84];
	uint16_t fbdimm_crc;
} __packed;

struct spdmem_rambus {				/* Direct Rambus DRAM */
	uint8_t	rdr_len;
	uint8_t rdr_size;
	uint8_t rdr_type;
	SPD_BITFIELD(				\
		uint8_t	rdr_rows:4,		\
		uint8_t	rdr_cols:4, ,		\
	);
} __packed;

struct spdmem_ddr3 {				/* Dual Data Rate 3 SDRAM */
	uint8_t	ddr3_len;
	uint8_t ddr3_size;
	uint8_t ddr3_type;
	uint8_t	ddr3_mod_type;
	SPD_BITFIELD(				\
		/* chipsize is offset by 28: 0 = 256M, 1 = 512M, ... */ \
		uint8_t ddr3_chipsize:4,	\
		/* logbanks is offset by 3 */	\
		uint8_t ddr3_logbanks:3,	\
		uint8_t ddr3_unused1:1,		\
	);
	/* cols is offset by 9, rows offset by 12 */
	SPD_BITFIELD(				\
		uint8_t ddr3_cols:3,		\
		uint8_t ddr3_rows:5, ,		\
	);
	SPD_BITFIELD(				\
		uint8_t ddr3_NOT15V:1,		\
		uint8_t ddr3_135V:1,		\
		uint8_t ddr3_125V:1,		\
		uint8_t	ddr3_unused2:5		\
	);
	/* chipwidth in bits offset by 2: 0 = X4, 1 = X8, 2 = X16 */
	/* physbanks is offset by 1 */
	SPD_BITFIELD(				\
		uint8_t ddr3_chipwidth:3,	\
		uint8_t ddr3_physbanks:5, ,	\
	);
	/* datawidth in bits offset by 3: 1 = 16b, 2 = 32b, 3 = 64b */
	SPD_BITFIELD(				\
		uint8_t ddr3_datawidth:3,	\
		uint8_t ddr3_hasECC:2,		\
		uint8_t ddr3_unused2a:3 ,	\
	);
	/* Fine time base, in pico-seconds */
	SPD_BITFIELD(				\
		uint8_t ddr3_ftb_divisor:4,	\
		uint8_t ddr3_ftb_dividend:4, ,	\
	);
	uint8_t ddr3_mtb_dividend;	/* 0x0108 = 0.1250ns */
	uint8_t	ddr3_mtb_divisor;	/* 0x010f = 0.0625ns */
	uint8_t	ddr3_tCKmin;		/* in terms of mtb */
	uint8_t	ddr3_unused3;
	uint16_t ddr3_CAS_sup;		/* Bit 0 ==> CAS 4 cycles */
	uint8_t	ddr3_tAAmin;		/* in terms of mtb */
	uint8_t	ddr3_tWRmin;
	uint8_t	ddr3_tRCDmin;
	uint8_t	ddr3_tRRDmin;
	uint8_t	ddr3_tRPmin;
	SPD_BITFIELD(				\
		uint8_t	ddr3_tRAS_msb:4,	\
		uint8_t	ddr3_tRC_msb:4, ,	\
	);
	uint8_t	ddr3_tRAS_lsb;
	uint8_t	ddr3_tRC_lsb;
	uint8_t	ddr3_tRFCmin_lsb;
	uint8_t	ddr3_tRFCmin_msb;
	uint8_t	ddr3_tWTRmin;
	uint8_t	ddr3_tRTPmin;
	SPD_BITFIELD(				\
		uint8_t	ddr3_tFAW_msb:4, , ,	\
	);
	uint8_t	ddr3_tFAW_lsb;
	uint8_t	ddr3_output_drvrs;
	SPD_BITFIELD(				\
		uint8_t	ddr3_ext_temp_range:1,	\
		uint8_t	ddr3_ext_temp_2x_refresh:1, \
		uint8_t	ddr3_asr_refresh:1,	\
		/* Bit 4 indicates on-die thermal sensor */
		/* Bit 7 indicates Partial-Array Self-Refresh (PASR) */
		uint8_t	ddr3_unused7:5		\
	);
	SPD_BITFIELD(				\
		uint8_t ddr3_therm_sensor_acc:7,\
		uint8_t ddr3_has_therm_sensor:1, , \
	);
	SPD_BITFIELD(				\
		uint8_t ddr3_non_std_devtype:7,	\
		uint8_t ddr3_std_device:1, ,	\
	);
	uint8_t	ddr3_unused4[26];
	uint8_t	ddr3_mod_height;
	uint8_t	ddr3_mod_thickness;
	uint8_t	ddr3_ref_card;
	uint8_t	ddr3_mapping;
	uint8_t	ddr3_unused5[53];
	uint8_t	ddr3_mfgID_lsb;
	uint8_t	ddr3_mfgID_msb;
	uint8_t	ddr3_mfgloc;
	uint8_t	ddr3_mfg_year;
	uint8_t	ddr3_mfg_week;
	uint8_t	ddr3_serial[4];
	uint16_t ddr3_crc;
	uint8_t ddr3_part[18];
	uint8_t ddr3_rev[2];
	uint8_t	ddr3_dram_mfgID_lsb;
	uint8_t	ddr3_dram_mfgID_msb;
	uint8_t ddr3_vendor[26];
} __packed;

/* DDR4 info from JEDEC Standard No. 21-C, Annex L - 4.1.2.12 */

/* Module-type specific bytes - bytes 0x080 thru 0x0ff */

struct spdmem_ddr4_mod_unbuffered {
	SPD_BITFIELD(					\
		uint8_t	ddr4_unbuf_mod_height:4,	\
		uint8_t ddr4_unbuf_card_ext:4, ,	\
	);
	SPD_BITFIELD(					\
		uint8_t	ddr4_unbuf_max_thick_front:4,	\
		uint8_t	ddr4_unbuf_max_thick_back:4, ,	\
	);
	SPD_BITFIELD(					\
		uint8_t	ddr4_unbuf_refcard:5,		\
		uint8_t	ddr4_unbuf_refcard_rev:2,	\
		uint8_t	ddr4_unbuf_refcard_ext:1,	\
	);
	SPD_BITFIELD(					\
		uint8_t	ddr4_unbuf_mirror_mapping:1,	\
		uint8_t	ddr4_unbuf_unused1:7, ,		\
	);
	uint8_t	ddr4_unbuf_unused2[122];
	uint8_t	ddr4_unbuf_crc[2];
} __packed;

struct spdmem_ddr4_mod_registered {
	SPD_BITFIELD(					\
		uint8_t	ddr4_reg_mod_height:4,	\
		uint8_t ddr4_reg_card_ext:4, ,	\
	);
	SPD_BITFIELD(					\
		uint8_t	ddr4_reg_max_thick_front:4,	\
		uint8_t	ddr4_reg_max_thick_back:4, ,	\
	);
	SPD_BITFIELD(					\
		uint8_t	ddr4_reg_refcard:5,		\
		uint8_t	ddr4_reg_refcard_rev:2,	\
		uint8_t	ddr4_reg_refcard_ext:1,	\
	);
	SPD_BITFIELD(					\
		uint8_t	ddr4_reg_regcnt:2,		\
		uint8_t	ddr4_reg_dram_rows:2,		\
		uint8_t	ddr4_reg_unused1:4,		\
	);
	SPD_BITFIELD(					\
		uint8_t	ddr4_reg_heat_spread_char:7,	\
		uint8_t	ddr4_reg_heat_spread_exist:1, ,	\
	);
	uint8_t	ddr4_reg_mfg_id_lsb;
	uint8_t	ddr4_reg_mfg_id_msb;
	uint8_t	ddr4_reg_revision;
	SPD_BITFIELD(					\
		uint8_t	ddr4_reg_mirror_mapping:1,	\
		uint8_t	ddr4_reg_unused2:7, ,		\
	);
	SPD_BITFIELD(					\
		uint8_t	ddr4_reg_output_drive_CKE:2,	\
		uint8_t	ddr4_reg_output_drive_ODT:2,	\
		uint8_t	ddr4_reg_output_drive_CmdAddr:2,\
		uint8_t	ddr4_reg_output_drive_chipsel:2	\
	);
	SPD_BITFIELD(					\
		uint8_t	ddr4_reg_output_drive_CK_Y0Y2:2,\
		uint8_t	ddr4_reg_output_drive_CK_Y1Y3:2,\
		uint8_t	ddr4_reg_unused3:4,		\
	);
	uint8_t	ddr4_reg_unused4[115];
	uint8_t	ddr4_reg_crc[2];
} __packed;

struct spdmem_ddr4_mod_reduced_load {
	SPD_BITFIELD(					\
		uint8_t	ddr4_rload_mod_height:4,	\
		uint8_t ddr4_rload_card_ext:4, ,	\
	);
	SPD_BITFIELD(					\
		uint8_t	ddr4_rload_max_thick_front:4,	\
		uint8_t	ddr4_rload_max_thick_back:4, ,	\
	);
	SPD_BITFIELD(					\
		uint8_t	ddr4_rload_refcard:5,		\
		uint8_t	ddr4_rload_refcard_rev:2,	\
		uint8_t	ddr4_rload_refcard_ext:1,	\
	);
	SPD_BITFIELD(					\
		uint8_t	ddr4_rload_regcnt:2,		\
		uint8_t	ddr4_rload_dram_rows:2,		\
		uint8_t	ddr4_rload_unused1:4,		\
	);
	SPD_BITFIELD(					\
		uint8_t	ddr4_rload_unused2:7,		\
		uint8_t	ddr4_rload_heat_spread_exist:1, , \
	);
	uint8_t	ddr4_rload_reg_mfg_id_lsb;
	uint8_t	ddr4_rload_reg_mfg_id_msb;
	uint8_t	ddr4_rload_reg_revision;
	SPD_BITFIELD(					\
		uint8_t	ddr4_rload_reg_mirror_mapping:1,\
		uint8_t	ddr4_rload_unused3:7, ,		\
	);
	SPD_BITFIELD(					\
		uint8_t	ddr4_rload_output_drive_CKE:2,	\
		uint8_t	ddr4_rload_output_drive_ODT:2,	\
		uint8_t	ddr4_rload_output_drive_CmdAddr:2, \
		uint8_t	ddr4_rload_output_drive_chipsel:2  \
	);
	SPD_BITFIELD(					\
		uint8_t	ddr4_rload_output_drive_CK_Y0Y2:2, \
		uint8_t	ddr4_rload_output_drive_CK_Y1Y3:2, \
		uint8_t	ddr4_rload_unused4:4,		\
	);
	uint8_t	ddr4_rload_dbuff_revision;
	SPD_BITFIELD(					\
		uint8_t	ddr4_rload_VrefDQ_0:6,		\
		uint8_t	ddr4_rload_unused5:2, ,		\
	);
	SPD_BITFIELD(					\
		uint8_t	ddr4_rload_VrefDQ_1:6,		\
		uint8_t	ddr4_rload_unused6:2, ,		\
	);
	SPD_BITFIELD(					\
		uint8_t	ddr4_rload_VrefDQ_2:6,		\
		uint8_t	ddr4_rload_unused7:2, ,		\
	);
	SPD_BITFIELD(					\
		uint8_t	ddr4_rload_VrefDQ_3:6,		\
		uint8_t	ddr4_rload_unused8:2, ,		\
	);
	SPD_BITFIELD(					\
		uint8_t	ddr4_rload_VrefDQ_buffer:6,	\
		uint8_t	ddr4_rload_unused9:2, ,		\
	);
	SPD_BITFIELD(						\
		uint8_t	ddr4_rload_MDQ_Read_Term_Str_1866:3,	\
		uint8_t	ddr4_rload_unused10:1,			\
		uint8_t	ddr4_rload_MDQ_Drive_Str_1866:3,	\
		uint8_t	ddr4_rload_unused11:1			\
	);
	SPD_BITFIELD(						\
		uint8_t	ddr4_rload_MDQ_Read_Term_Str_2400:3,	\
		uint8_t	ddr4_rload_unused12:1,			\
		uint8_t	ddr4_rload_MDQ_Drive_Str_2400:3,	\
		uint8_t	ddr4_rload_unused13:1			\
	);
	SPD_BITFIELD(						\
		uint8_t	ddr4_rload_MDQ_Read_Term_Str_3200:3,	\
		uint8_t	ddr4_rload_unused14:1,			\
		uint8_t	ddr4_rload_MDQ_Drive_Str_3200:3,	\
		uint8_t	ddr4_rload_unused15:1			\
	);
	SPD_BITFIELD(						\
		uint8_t	ddr4_rload_DRAM_Drive_Str_1866:2,	\
		uint8_t	ddr4_rload_DRAM_Drive_Str_2400:2,	\
		uint8_t	ddr4_rload_DRAM_Drive_Str_3200:2,	\
		uint8_t	ddr4_rload_unused16:2			\
	);
	SPD_BITFIELD(						\
		uint8_t	ddr4_rload_DRAM_ODT_RTT_NOM_1866:3,	\
		uint8_t	ddr4_rload_DRAM_ODT_RTT_WR_1866:3,	\
		uint8_t	ddr4_rload_unused17:2,			\
	);
	SPD_BITFIELD(						\
		uint8_t	ddr4_rload_DRAM_ODT_RTT_NOM_2400:3,	\
		uint8_t	ddr4_rload_DRAM_ODT_RTT_WR_2400:3,	\
		uint8_t	ddr4_rload_unused18:2,			\
	);
	SPD_BITFIELD(						\
		uint8_t	ddr4_rload_DRAM_ODT_RTT_NOM_3200:3,	\
		uint8_t	ddr4_rload_DRAM_ODT_RTT_WR_3200:3,	\
		uint8_t	ddr4_rload_unused19:2,			\
	);
	SPD_BITFIELD(						\
		uint8_t	ddr4_rload_DRAM_ODT_RTT_PARK_01_1866:3,	\
		uint8_t	ddr4_rload_DRAM_ODT_RTT_PARK_23_1866:3,	\
		uint8_t	ddr4_rload_unused20:2,			\
	);
	SPD_BITFIELD(						\
		uint8_t	ddr4_rload_DRAM_ODT_RTT_PARK_01_2400:3,	\
		uint8_t	ddr4_rload_DRAM_ODT_RTT_PARK_23_2400:3,	\
		uint8_t	ddr4_rload_unused21:2,			\
	);
	SPD_BITFIELD(						\
		uint8_t	ddr4_rload_DRAM_ODT_RTT_PARK_01_3200:3,	\
		uint8_t	ddr4_rload_DRAM_ODT_RTT_PARK_23_3200:3,	\
		uint8_t	ddr4_rload_unused22:2,			\
	);
	uint8_t	ddr4_rload_unused23[99];
	uint8_t	ddr4_rload_crc[2];
} __packed;

struct spdmem_ddr4 {				/* Dual Data Rate 4 SDRAM */
	SPD_BITFIELD(				\
		uint8_t	ddr4_ROM_used:4,	\
		uint8_t	ddr4_ROM_size:3,	\
		uint8_t	ddr4_unused0:1,		\
	);
	uint8_t	ddr4_romrev;
	uint8_t	ddr4_type;
	SPD_BITFIELD(				\
		uint8_t	ddr4_mod_type:4,	\
		uint8_t	ddr4_unused1:4, ,	\
	);
	SPD_BITFIELD(				\
		/* capacity is offset by 28: 0 = 256M, 1 = 512M, ... */ \
		uint8_t	ddr4_capacity:4,	\
		/* logbanks is offset by 2 */	\
		uint8_t	ddr4_logbanks:2,	\
		/* bankgroups is offset by 0 */
		uint8_t	ddr4_bankgroups:2,	\
	);
	/* cols is offset by 9, rows offset by 12 */
	SPD_BITFIELD(				\
		uint8_t	ddr4_cols:3,		\
		uint8_t	ddr4_rows:3,		\
		uint8_t	ddr4_unused2:2,		\
	);
	SPD_BITFIELD(				\
		uint8_t	ddr4_signal_loading:2,	\
		uint8_t	ddr4_unused3:2,		\
		uint8_t	ddr4_diecount:3,	\
		uint8_t	ddr4_non_monolithic:1	\
	);
	SPD_BITFIELD(				\
		uint8_t ddr4_max_activate_count:4,	\
		uint8_t ddr4_max_activate_window:2,	\
		uint8_t ddr4_unused4:2,	\
	);
	uint8_t	ddr4_unused5;		/* SDRAM Thermal & Refresh Options */
	SPD_BITFIELD(				\
		uint8_t ddr4_unused6:6,		\
		uint8_t ddr4_ppr_support:2, ,	/* post package repair */ \
	);
	uint8_t ddr4_unused7;
	SPD_BITFIELD(				\
		uint8_t	ddr4_dram_vdd_12:2,	\
		uint8_t	ddr4_dram_vdd_tbd1:2,	\
		uint8_t	ddr4_dram_vdd_tbd2:2,	\
		uint8_t	ddr4_unused8:2		\
	);
	SPD_BITFIELD(				\
		/* device width is 0=4, 1=8, 2=16, or 4=32 bits */ \
		uint8_t	ddr4_device_width:3,	\
		/* number of package ranks is field value plus 1 */ \
		uint8_t	ddr4_package_ranks:3,	\
		uint8_t	ddr4_unused9:2,		\
	);
	SPD_BITFIELD(					\
		/* primary width is offset by 3, extension is offset by 2 */ \
		uint8_t	ddr4_primary_bus_width:3,	\
		uint8_t	ddr4_bus_width_extension:2,	\
		uint8_t	ddr4_unused10:3,		\
	);
	SPD_BITFIELD(				\
		uint8_t ddr4_unused11:7,	\
		uint8_t ddr4_has_therm_sensor:1, , \
	);
	SPD_BITFIELD(				\
		uint8_t ddr4_ext_mod_type:4,	\
		uint8_t ddr4_unused12:4, ,	\
	);
	uint8_t	ddr4_unused13;
	SPD_BITFIELD(				\
		/* units = 1ps (10**-12sec) */	\
		uint8_t	ddr4_fine_timebase:2,	\
		/* units = 125ps	    */	\
		uint8_t	ddr4_medium_timebase:2, ,	\
	);
	uint8_t	ddr4_tCKAVGmin_mtb;
	uint8_t	ddr4_tCKAVGmax_mtb;
	/* Bit 0 of CAS_supported[0 corresponds to CL=7 */
	uint8_t	ddr4_CAS_supported[4];
	uint8_t	ddr4_tAAmin_mtb;
	uint8_t	ddr4_tRCDmin_mtb;
	uint8_t	ddr4_tRPmin_mtb;
	SPD_BITFIELD(				\
		uint8_t	ddr4_tRASmin_msb:4,	\
		uint8_t	ddr4_tRCmin_mtb_msb:4, ,	\
	);
	uint8_t	ddr4_tRASmin_lsb;
	uint8_t	ddr4_tRCmin_mtb_lsb;
	uint8_t	ddr4_tRFC1min_lsb;
	uint8_t	ddr4_tRFC1min_msb;
	uint8_t	ddr4_tRFC2min_lsb;
	uint8_t	ddr4_tRFC2min_msb;
	uint8_t	ddr4_tRFC4min_lsb;
	uint8_t	ddr4_tRFC4min_msb;
	SPD_BITFIELD(				\
		uint8_t	ddr4_tFAW_mtb_msb,	\
		uint8_t	ddr4_unused14, ,	\
	);
	uint8_t	ddr4_tFAWmin_mtb_lsb;
	uint8_t	ddr4_tRRD_Smin_mtb;
	uint8_t	ddr4_tRRD_Lmin_mtb;
	uint8_t	ddr4_tCCD_Lmin_mtb;
	uint8_t	ddr4_unused15[19];
	uint8_t	ddr4_connector_map[18];
	uint8_t	ddr4_unused16[39];
	uint8_t	ddr4_tCCD_Lmin_ftb;
	uint8_t	ddr4_tRRD_Lmin_ftb;
	uint8_t	ddr4_tRRD_Smin_ftb;
	uint8_t	ddr4_tRCmin_ftb;
	uint8_t	ddr4_tRPmin_ftb;
	uint8_t	ddr4_tRCDmin_ftb;
	uint8_t	ddr4_tAAmin_ftb;
	uint8_t	ddr4_tCKAVGmax_ftb;
	uint8_t	ddr4_tCKAVGmin_ftb;
	uint16_t ddr4_crc;
	union {
		struct spdmem_ddr4_mod_unbuffered	u2_unbuf;
		struct spdmem_ddr4_mod_registered	u2_reg;
		struct spdmem_ddr4_mod_reduced_load	u2_red_load;
	} ddr4_u2;
	uint8_t	ddr4_unused17[64];
	uint8_t	ddr4_module_mfg_lsb;
	uint8_t	ddr4_module_mfg_msb;
	uint8_t	ddr4_module_mfg_loc;
	uint8_t	ddr4_module_mfg_year;
	uint8_t	ddr4_module_mfg_week;
	uint8_t	ddr4_serial_number[4];
	uint8_t	ddr4_part_number[20];
	uint8_t	ddr4_revision_code;
	uint8_t	ddr4_dram_mfgID_lsb;
	uint8_t	ddr4_dram_mfgID_msb;
	uint8_t	ddr4_dram_stepping;
	uint8_t ddr4_mfg_specific_data[29];
	uint8_t	ddr4_unused18[2];
	uint8_t	ddr4_user_data[128];
} __packed;

struct spdmem {
	union {
		struct spdmem_fbdimm	u1_fbd;
		struct spdmem_fpm	u1_fpm;
		struct spdmem_ddr 	u1_ddr;
		struct spdmem_ddr2	u1_ddr2;
		struct spdmem_sdram	u1_sdr;
		struct spdmem_rambus	u1_rdr;
		struct spdmem_rom	u1_rom;
		struct spdmem_ddr3	u1_ddr3;
		struct spdmem_ddr4	u1_ddr4;
	} sm_u1;
} __packed;
#define	sm_fbd		sm_u1.u1_fbd
#define	sm_fpm		sm_u1.u1_fpm
#define	sm_ddr		sm_u1.u1_ddr
#define	sm_ddr2		sm_u1.u1_ddr2
#define	sm_rdr		sm_u1.u1_rdr
#define	sm_rom		sm_u1.u1_rom
#define	sm_ddr3		sm_u1.u1_ddr3
#define	sm_sdr		sm_u1.u1_sdr
#define	sm_ddr4		sm_u1.u1_ddr4

/* some fields are in the same place for all memory types */

#define sm_len		sm_fpm.fpm_len
#define sm_size		sm_fpm.fpm_size
#define sm_type		sm_fpm.fpm_type
#define sm_cksum	sm_fpm.fpm_cksum
#define sm_config	sm_fpm.fpm_config
#define sm_voltage	sm_fpm.fpm_voltage
#define	sm_refresh	sm_fpm.fpm_refresh
#define	sm_selfrefresh	sm_fpm.fpm_selfrefresh

#define SPDMEM_TYPE_MAXLEN 24

struct spdmem_softc {
	uint8_t		(*sc_read)(struct spdmem_softc *, uint8_t);
	struct spdmem	sc_spd_data;
	struct sysctllog *sc_sysctl_log;
	char		sc_type[SPDMEM_TYPE_MAXLEN];
};

int  spdmem_common_probe(struct spdmem_softc *);
void spdmem_common_attach(struct spdmem_softc *, device_t);
int  spdmem_common_detach(struct spdmem_softc *, device_t);
