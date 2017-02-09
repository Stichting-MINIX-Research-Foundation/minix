/* $NetBSD: spdmem.c,v 1.14 2015/05/15 08:44:24 msaitoh Exp $ */

/*
 * Copyright (c) 2007 Nicolas Joly
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
 * Serial Presence Detect (SPD) memory identification
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: spdmem.c,v 1.14 2015/05/15 08:44:24 msaitoh Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/endian.h>
#include <sys/sysctl.h>
#include <machine/bswap.h>

#include <dev/i2c/i2cvar.h>
#include <dev/ic/spdmemreg.h>
#include <dev/ic/spdmemvar.h>

/* Routines for decoding spd data */
static void decode_edofpm(const struct sysctlnode *, device_t, struct spdmem *);
static void decode_rom(const struct sysctlnode *, device_t, struct spdmem *);
static void decode_sdram(const struct sysctlnode *, device_t, struct spdmem *,
	int);
static void decode_ddr(const struct sysctlnode *, device_t, struct spdmem *);
static void decode_ddr2(const struct sysctlnode *, device_t, struct spdmem *);
static void decode_ddr3(const struct sysctlnode *, device_t, struct spdmem *);
static void decode_ddr4(const struct sysctlnode *, device_t, struct spdmem *);
static void decode_fbdimm(const struct sysctlnode *, device_t, struct spdmem *);

static void decode_size_speed(device_t, const struct sysctlnode *,
			      int, int, int, int, bool, const char *, int);
static void decode_voltage_refresh(device_t, struct spdmem *);

#define IS_RAMBUS_TYPE (s->sm_len < 4)

static const char* const spdmem_basic_types[] = {
	"unknown",
	"FPM",
	"EDO",
	"Pipelined Nibble",
	"SDRAM",
	"ROM",
	"DDR SGRAM",
	"DDR SDRAM",
	"DDR2 SDRAM",
	"DDR2 SDRAM FB",
	"DDR2 SDRAM FB Probe",
	"DDR3 SDRAM",
	"DDR4 SDRAM"
};

static const char* const spdmem_ddr4_module_types[] = {
	"DDR4 Extended",
	"DDR4 RDIMM",
	"DDR4 UDIMM",
	"DDR4 SO-DIMM",
	"DDR4 Load-Reduced DIMM",
	"DDR4 Mini-RDIMM",
	"DDR4 Mini-UDIMM",
	"DDR4 Reserved",
	"DDR4 72Bit SO-RDIMM",
	"DDR4 72Bit SO-UDIMM",
	"DDR4 Undefined",
	"DDR4 Reserved",
	"DDR4 16Bit SO-DIMM",
	"DDR4 32Bit SO-DIMM",
	"DDR4 Reserved",
	"DDR4 Undefined"
};

static const char* const spdmem_superset_types[] = {
	"unknown",
	"ESDRAM",
	"DDR ESDRAM",
	"PEM EDO",
	"PEM SDRAM"
};

static const char* const spdmem_voltage_types[] = {
	"TTL (5V tolerant)",
	"LvTTL (not 5V tolerant)",
	"HSTL 1.5V",
	"SSTL 3.3V",
	"SSTL 2.5V",
	"SSTL 1.8V"
};

static const char* const spdmem_refresh_types[] = {
	"15.625us",
	"3.9us",
	"7.8us",
	"31.3us",
	"62.5us",
	"125us"
};

static const char* const spdmem_parity_types[] = {
	"no parity or ECC",
	"data parity",
	"data ECC",
	"data parity and ECC",
	"cmd/addr parity",
	"cmd/addr/data parity",
	"cmd/addr parity, data ECC",
	"cmd/addr/data parity, data ECC"
};

int spd_rom_sizes[] = { 0, 128, 256, 384, 512 };


/* Cycle time fractional values (units of .001 ns) for DDR2 SDRAM */
static const uint16_t spdmem_cycle_frac[] = {
	0, 100, 200, 300, 400, 500, 600, 700, 800, 900,
	250, 333, 667, 750, 999, 999
};

/* Format string for timing info */
#define	LATENCY	"tAA-tRCD-tRP-tRAS: %d-%d-%d-%d\n"

/* CRC functions used for certain memory types */

static uint16_t spdcrc16 (struct spdmem_softc *sc, int count)
{
	uint16_t crc;
	int i, j;
	uint8_t val;
	crc = 0;
	for (j = 0; j <= count; j++) {
		val = (sc->sc_read)(sc, j);
		crc = crc ^ val << 8;
		for (i = 0; i < 8; ++i)
			if (crc & 0x8000)
				crc = crc << 1 ^ 0x1021;
			else
				crc = crc << 1;
	}
	return (crc & 0xFFFF);
}

int
spdmem_common_probe(struct spdmem_softc *sc)
{
	int cksum = 0;
	uint8_t i, val, spd_type;
	int spd_len, spd_crc_cover;
	uint16_t crc_calc, crc_spd;

	spd_type = (sc->sc_read)(sc, 2);

	/* For older memory types, validate the checksum over 1st 63 bytes */
	if (spd_type <= SPDMEM_MEMTYPE_DDR2SDRAM) {
		for (i = 0; i < 63; i++)
			cksum += (sc->sc_read)(sc, i);

		val = (sc->sc_read)(sc, 63);

		if (cksum == 0 || (cksum & 0xff) != val) {
			aprint_debug("spd checksum failed, calc = 0x%02x, "
				     "spd = 0x%02x\n", cksum, val);
			return 0;
		} else
			return 1;
	}

	/* For DDR3 and FBDIMM, verify the CRC */
	else if (spd_type <= SPDMEM_MEMTYPE_DDR3SDRAM) {
		spd_len = (sc->sc_read)(sc, 0);
		if (spd_len & SPDMEM_SPDCRC_116)
			spd_crc_cover = 116;
		else
			spd_crc_cover = 125;
		switch (spd_len & SPDMEM_SPDLEN_MASK) {
		case SPDMEM_SPDLEN_128:
			spd_len = 128;
			break;
		case SPDMEM_SPDLEN_176:
			spd_len = 176;
			break;
		case SPDMEM_SPDLEN_256:
			spd_len = 256;
			break;
		default:
			return 0;
		}
		if (spd_crc_cover > spd_len)
			return 0;
		crc_calc = spdcrc16(sc, spd_crc_cover);
		crc_spd = (sc->sc_read)(sc, 127) << 8;
		crc_spd |= (sc->sc_read)(sc, 126);
		if (crc_calc != crc_spd) {
			aprint_debug("crc16 failed, covers %d bytes, "
				     "calc = 0x%04x, spd = 0x%04x\n",
				     spd_crc_cover, crc_calc, crc_spd);
			return 0;
		}
		return 1;
	} else if (spd_type == SPDMEM_MEMTYPE_DDR4SDRAM) {
		spd_len = (sc->sc_read)(sc, 0) & 0x0f;
		if ((unsigned int)spd_len > __arraycount(spd_rom_sizes))
			return 0;
		spd_len = spd_rom_sizes[spd_len];
		spd_crc_cover=128;
		if (spd_crc_cover > spd_len)
			return 0;
		crc_calc = spdcrc16(sc, spd_crc_cover);
		crc_spd = (sc->sc_read)(sc, 127) << 8;
		crc_spd |= (sc->sc_read)(sc, 126);
		if (crc_calc != crc_spd) {
			aprint_debug("crc16 failed, covers %d bytes, "
				     "calc = 0x%04x, spd = 0x%04x\n",
				     spd_crc_cover, crc_calc, crc_spd);
			return 0;
		}
		/*
		 * We probably could also verify the CRC for the other
		 * "pages" of SPD data in blocks 1 and 2, but we'll do
		 * it some other time.
		 */
		return 1;
	} else
		return 0;

	/* For unrecognized memory types, don't match at all */
	return 0;
}

void
spdmem_common_attach(struct spdmem_softc *sc, device_t self)
{
	struct spdmem *s = &(sc->sc_spd_data);
	const char *type;
	const char *rambus_rev = "Reserved";
	int dimm_size;
	unsigned int i, spd_len, spd_size;
	const struct sysctlnode *node = NULL;

	s->sm_len = (sc->sc_read)(sc, 0);
	s->sm_size = (sc->sc_read)(sc, 1);
	s->sm_type = (sc->sc_read)(sc, 2);

	if (s->sm_type == SPDMEM_MEMTYPE_DDR4SDRAM) {
		/*
		 * An even newer encoding with one byte holding both
		 * the used-size and capacity values
		 */
		spd_len = s->sm_len & 0x0f;
		spd_size = (s->sm_len >> 4) & 0x07;

		spd_len = spd_rom_sizes[spd_len];
		spd_size *= 512;

	} else if (s->sm_type >= SPDMEM_MEMTYPE_FBDIMM) {
		/*
		 * FBDIMM and DDR3 (and probably all newer) have a different
		 * encoding of the SPD EEPROM used/total sizes
		 */
		spd_size = 64 << (s->sm_len & SPDMEM_SPDSIZE_MASK);
		switch (s->sm_len & SPDMEM_SPDLEN_MASK) {
		case SPDMEM_SPDLEN_128:
			spd_len = 128;
			break;
		case SPDMEM_SPDLEN_176:
			spd_len = 176;
			break;
		case SPDMEM_SPDLEN_256:
			spd_len = 256;
			break;
		default:
			spd_len = 64;
			break;
		}
	} else {
		spd_size = 1 << s->sm_size;
		spd_len = s->sm_len;
		if (spd_len < 64)
			spd_len = 64;
	}
	if (spd_len > spd_size)
		spd_len = spd_size;
	if (spd_len > sizeof(struct spdmem))
		spd_len = sizeof(struct spdmem);
	for (i = 3; i < spd_len; i++)
		((uint8_t *)s)[i] = (sc->sc_read)(sc, i);

	/*
	 * Setup our sysctl subtree, hw.spdmemN
	 */
	sc->sc_sysctl_log = NULL;
	sysctl_createv(&sc->sc_sysctl_log, 0, NULL, &node,
	    0, CTLTYPE_NODE,
	    device_xname(self), NULL, NULL, 0, NULL, 0,
	    CTL_HW, CTL_CREATE, CTL_EOL);
	if (node != NULL && spd_len != 0)
                sysctl_createv(&sc->sc_sysctl_log, 0, NULL, NULL,
                    0,
                    CTLTYPE_STRUCT, "spd_data",
		    SYSCTL_DESCR("raw spd data"), NULL,
                    0, s, spd_len,
                    CTL_HW, node->sysctl_num, CTL_CREATE, CTL_EOL);

	/*
	 * Decode and print key SPD contents
	 */
	if (IS_RAMBUS_TYPE) {
		if (s->sm_type == SPDMEM_MEMTYPE_RAMBUS)
			type = "Rambus";
		else if (s->sm_type == SPDMEM_MEMTYPE_DIRECTRAMBUS)
			type = "Direct Rambus";
		else
			type = "Rambus (unknown)";

		switch (s->sm_len) {
		case 0:
			rambus_rev = "Invalid";
			break;
		case 1:
			rambus_rev = "0.7";
			break;
		case 2:
			rambus_rev = "1.0";
			break;
		default:
			rambus_rev = "Reserved";
			break;
		}
	} else {
		if (s->sm_type < __arraycount(spdmem_basic_types))
			type = spdmem_basic_types[s->sm_type];
		else
			type = "unknown memory type";

		if (s->sm_type == SPDMEM_MEMTYPE_EDO &&
		    s->sm_fpm.fpm_superset == SPDMEM_SUPERSET_EDO_PEM)
			type = spdmem_superset_types[SPDMEM_SUPERSET_EDO_PEM];
		if (s->sm_type == SPDMEM_MEMTYPE_SDRAM &&
		    s->sm_sdr.sdr_superset == SPDMEM_SUPERSET_SDRAM_PEM)
			type = spdmem_superset_types[SPDMEM_SUPERSET_SDRAM_PEM];
		if (s->sm_type == SPDMEM_MEMTYPE_DDRSDRAM &&
		    s->sm_ddr.ddr_superset == SPDMEM_SUPERSET_DDR_ESDRAM)
			type =
			    spdmem_superset_types[SPDMEM_SUPERSET_DDR_ESDRAM];
		if (s->sm_type == SPDMEM_MEMTYPE_SDRAM &&
		    s->sm_sdr.sdr_superset == SPDMEM_SUPERSET_ESDRAM) {
			type = spdmem_superset_types[SPDMEM_SUPERSET_ESDRAM];
		}
		if (s->sm_type == SPDMEM_MEMTYPE_DDR4SDRAM &&
		    s->sm_ddr4.ddr4_mod_type <
				__arraycount(spdmem_ddr4_module_types)) {
			type = spdmem_ddr4_module_types[s->sm_ddr4.ddr4_mod_type];
		}
	}

	strlcpy(sc->sc_type, type, SPDMEM_TYPE_MAXLEN);
	if (node != NULL)
		sysctl_createv(&sc->sc_sysctl_log, 0, NULL, NULL,
		    0,
		    CTLTYPE_STRING, "mem_type",
		    SYSCTL_DESCR("memory module type"), NULL,
		    0, sc->sc_type, 0,
		    CTL_HW, node->sysctl_num, CTL_CREATE, CTL_EOL);

	if (IS_RAMBUS_TYPE) {
		aprint_naive("\n");
		aprint_normal("\n");
		aprint_normal_dev(self, "%s, SPD Revision %s", type, rambus_rev);
		dimm_size = 1 << (s->sm_rdr.rdr_rows + s->sm_rdr.rdr_cols - 13);
		if (dimm_size >= 1024)
			aprint_normal(", %dGB\n", dimm_size / 1024);
		else
			aprint_normal(", %dMB\n", dimm_size);

		/* No further decode for RAMBUS memory */
		return;
	}
	switch (s->sm_type) {
	case SPDMEM_MEMTYPE_EDO:
	case SPDMEM_MEMTYPE_FPM:
		decode_edofpm(node, self, s);
		break;
	case SPDMEM_MEMTYPE_ROM:
		decode_rom(node, self, s);
		break;
	case SPDMEM_MEMTYPE_SDRAM:
		decode_sdram(node, self, s, spd_len);
		break;
	case SPDMEM_MEMTYPE_DDRSDRAM:
		decode_ddr(node, self, s);
		break;
	case SPDMEM_MEMTYPE_DDR2SDRAM:
		decode_ddr2(node, self, s);
		break;
	case SPDMEM_MEMTYPE_DDR3SDRAM:
		decode_ddr3(node, self, s);
		break;
	case SPDMEM_MEMTYPE_FBDIMM:
	case SPDMEM_MEMTYPE_FBDIMM_PROBE:
		decode_fbdimm(node, self, s);
		break;
	case SPDMEM_MEMTYPE_DDR4SDRAM:
		decode_ddr4(node, self, s);
		break;
	}

	/* Dump SPD */
	for (i = 0; i < spd_len;  i += 16) {
		unsigned int j, k;
		aprint_debug_dev(self, "0x%02x:", i);
		k = (spd_len > (i + 16)) ? i + 16 : spd_len;
		for (j = i; j < k; j++)
			aprint_debug(" %02x", ((uint8_t *)s)[j]);
		aprint_debug("\n");
	}
}

int
spdmem_common_detach(struct spdmem_softc *sc, device_t self)
{
	sysctl_teardown(&sc->sc_sysctl_log);

	return 0;
}

static void
decode_size_speed(device_t self, const struct sysctlnode *node,
		  int dimm_size, int cycle_time, int d_clk, int bits,
		  bool round, const char *ddr_type_string, int speed)
{
	int p_clk;
	struct spdmem_softc *sc = device_private(self);

	if (dimm_size < 1024)
		aprint_normal("%dMB", dimm_size);
	else
		aprint_normal("%dGB", dimm_size / 1024);
	if (node != NULL)
		sysctl_createv(&sc->sc_sysctl_log, 0, NULL, NULL,
		    CTLFLAG_IMMEDIATE,
		    CTLTYPE_INT, "size",
		    SYSCTL_DESCR("module size in MB"), NULL,
		    dimm_size, NULL, 0,
		    CTL_HW, node->sysctl_num, CTL_CREATE, CTL_EOL);

	if (cycle_time == 0) {
		aprint_normal("\n");
		return;
	}

	/*
	 * Calculate p_clk first, since for DDR3 we need maximum significance.
	 * DDR3 rating is not rounded to a multiple of 100.  This results in
	 * cycle_time of 1.5ns displayed as PC3-10666.
	 *
	 * For SDRAM, the speed is provided by the caller so we use it.
	 */
	d_clk *= 1000 * 1000;
	if (speed)
		p_clk = speed;
	else
		p_clk = (d_clk * bits) / 8 / cycle_time;
	d_clk = ((d_clk + cycle_time / 2) ) / cycle_time;
	if (round) {
		if ((p_clk % 100) >= 50)
			p_clk += 50;
		p_clk -= p_clk % 100;
	}
	aprint_normal(", %dMHz (%s-%d)\n",
		      d_clk, ddr_type_string, p_clk);
	if (node != NULL)
		sysctl_createv(&sc->sc_sysctl_log, 0, NULL, NULL,
			       CTLFLAG_IMMEDIATE,
			       CTLTYPE_INT, "speed",
			       SYSCTL_DESCR("memory speed in MHz"),
			       NULL, d_clk, NULL, 0,
			       CTL_HW, node->sysctl_num, CTL_CREATE, CTL_EOL);
}

static void
decode_voltage_refresh(device_t self, struct spdmem *s)
{
	const char *voltage, *refresh;

	if (s->sm_voltage < __arraycount(spdmem_voltage_types))
		voltage = spdmem_voltage_types[s->sm_voltage];
	else
		voltage = "unknown";

	if (s->sm_refresh < __arraycount(spdmem_refresh_types))
		refresh = spdmem_refresh_types[s->sm_refresh];
	else
		refresh = "unknown";

	aprint_verbose_dev(self, "voltage %s, refresh time %s%s\n",
			voltage, refresh,
			s->sm_selfrefresh?" (self-refreshing)":"");
}

static void
decode_edofpm(const struct sysctlnode *node, device_t self, struct spdmem *s) {
	aprint_naive("\n");
	aprint_normal("\n");
	aprint_normal_dev(self, "%s", spdmem_basic_types[s->sm_type]);

	aprint_normal("\n");
	aprint_verbose_dev(self,
	    "%d rows, %d cols, %d banks, %dns tRAC, %dns tCAC\n",
	    s->sm_fpm.fpm_rows, s->sm_fpm.fpm_cols, s->sm_fpm.fpm_banks,
	    s->sm_fpm.fpm_tRAC, s->sm_fpm.fpm_tCAC);
}

static void
decode_rom(const struct sysctlnode *node, device_t self, struct spdmem *s) {
	aprint_naive("\n");
	aprint_normal("\n");
	aprint_normal_dev(self, "%s", spdmem_basic_types[s->sm_type]);

	aprint_normal("\n");
	aprint_verbose_dev(self, "%d rows, %d cols, %d banks\n",
	    s->sm_rom.rom_rows, s->sm_rom.rom_cols, s->sm_rom.rom_banks);
}

static void
decode_sdram(const struct sysctlnode *node, device_t self, struct spdmem *s,
	     int spd_len) {
	int dimm_size, cycle_time, bits, tAA, i, speed, freq;

	aprint_naive("\n");
	aprint_normal("\n");
	aprint_normal_dev(self, "%s", spdmem_basic_types[s->sm_type]);

	aprint_normal("%s, %s, ",
		(s->sm_sdr.sdr_mod_attrs & SPDMEM_SDR_MASK_REG)?
			" (registered)":"",
		(s->sm_config < __arraycount(spdmem_parity_types))?
			spdmem_parity_types[s->sm_config]:"invalid parity");

	dimm_size = 1 << (s->sm_sdr.sdr_rows + s->sm_sdr.sdr_cols - 17);
	dimm_size *= s->sm_sdr.sdr_banks * s->sm_sdr.sdr_banks_per_chip;

	cycle_time = s->sm_sdr.sdr_cycle_whole * 1000 +
		     s->sm_sdr.sdr_cycle_tenths * 100;
	bits = le16toh(s->sm_sdr.sdr_datawidth);
	if (s->sm_config == 1 || s->sm_config == 2)
		bits -= 8;

	/* Calculate speed here - from OpenBSD */
	if (spd_len >= 128)
		freq = ((uint8_t *)s)[126];
	else
		freq = 0;
	switch (freq) {
		/*
		 * Must check cycle time since some PC-133 DIMMs 
		 * actually report PC-100
		 */
	    case 100:
	    case 133:
		if (cycle_time < 8000)
			speed = 133;
		else
			speed = 100;
		break;
	    case 0x66:		/* Legacy DIMMs use _hex_ 66! */
	    default:
		speed = 66;
	}
	decode_size_speed(self, node, dimm_size, cycle_time, 1, bits, FALSE,
			  "PC", speed);

	aprint_verbose_dev(self,
	    "%d rows, %d cols, %d banks, %d banks/chip, %d.%dns cycle time\n",
	    s->sm_sdr.sdr_rows, s->sm_sdr.sdr_cols, s->sm_sdr.sdr_banks,
	    s->sm_sdr.sdr_banks_per_chip, cycle_time/1000,
	    (cycle_time % 1000) / 100);

	tAA  = 0;
	for (i = 0; i < 8; i++)
		if (s->sm_sdr.sdr_tCAS & (1 << i))
			tAA = i;
	tAA++;
	aprint_verbose_dev(self, LATENCY, tAA, s->sm_sdr.sdr_tRCD,
	    s->sm_sdr.sdr_tRP, s->sm_sdr.sdr_tRAS);

	decode_voltage_refresh(self, s);
}

static void
decode_ddr(const struct sysctlnode *node, device_t self, struct spdmem *s) {
	int dimm_size, cycle_time, bits, tAA, i;

	aprint_naive("\n");
	aprint_normal("\n");
	aprint_normal_dev(self, "%s", spdmem_basic_types[s->sm_type]);

	aprint_normal("%s, %s, ",
		(s->sm_ddr.ddr_mod_attrs & SPDMEM_DDR_MASK_REG)?
			" (registered)":"",
		(s->sm_config < __arraycount(spdmem_parity_types))?
			spdmem_parity_types[s->sm_config]:"invalid parity");

	dimm_size = 1 << (s->sm_ddr.ddr_rows + s->sm_ddr.ddr_cols - 17);
	dimm_size *= s->sm_ddr.ddr_ranks * s->sm_ddr.ddr_banks_per_chip;

	cycle_time = s->sm_ddr.ddr_cycle_whole * 1000 +
		  spdmem_cycle_frac[s->sm_ddr.ddr_cycle_tenths];
	bits = le16toh(s->sm_ddr.ddr_datawidth);
	if (s->sm_config == 1 || s->sm_config == 2)
		bits -= 8;
	decode_size_speed(self, node, dimm_size, cycle_time, 2, bits, TRUE,
			  "PC", 0);

	aprint_verbose_dev(self,
	    "%d rows, %d cols, %d ranks, %d banks/chip, %d.%dns cycle time\n",
	    s->sm_ddr.ddr_rows, s->sm_ddr.ddr_cols, s->sm_ddr.ddr_ranks,
	    s->sm_ddr.ddr_banks_per_chip, cycle_time/1000,
	    (cycle_time % 1000 + 50) / 100);

	tAA  = 0;
	for (i = 2; i < 8; i++)
		if (s->sm_ddr.ddr_tCAS & (1 << i))
			tAA = i;
	tAA /= 2;

#define __DDR_ROUND(scale, field)	\
		((scale * s->sm_ddr.field + cycle_time - 1) / cycle_time)

	aprint_verbose_dev(self, LATENCY, tAA, __DDR_ROUND(250, ddr_tRCD),
		__DDR_ROUND(250, ddr_tRP), __DDR_ROUND(1000, ddr_tRAS));

#undef	__DDR_ROUND

	decode_voltage_refresh(self, s);
}

static void
decode_ddr2(const struct sysctlnode *node, device_t self, struct spdmem *s) {
	int dimm_size, cycle_time, bits, tAA, i;

	aprint_naive("\n");
	aprint_normal("\n");
	aprint_normal_dev(self, "%s", spdmem_basic_types[s->sm_type]);

	aprint_normal("%s, %s, ",
		(s->sm_ddr2.ddr2_mod_attrs & SPDMEM_DDR2_MASK_REG)?
			" (registered)":"",
		(s->sm_config < __arraycount(spdmem_parity_types))?
			spdmem_parity_types[s->sm_config]:"invalid parity");

	dimm_size = 1 << (s->sm_ddr2.ddr2_rows + s->sm_ddr2.ddr2_cols - 17);
	dimm_size *= (s->sm_ddr2.ddr2_ranks + 1) *
		     s->sm_ddr2.ddr2_banks_per_chip;

	cycle_time = s->sm_ddr2.ddr2_cycle_whole * 1000 +
		 spdmem_cycle_frac[s->sm_ddr2.ddr2_cycle_frac];
	bits = s->sm_ddr2.ddr2_datawidth;
	if ((s->sm_config & 0x03) != 0)
		bits -= 8;
	decode_size_speed(self, node, dimm_size, cycle_time, 2, bits, TRUE,
			  "PC2", 0);

	aprint_verbose_dev(self,
	    "%d rows, %d cols, %d ranks, %d banks/chip, %d.%02dns cycle time\n",
	    s->sm_ddr2.ddr2_rows, s->sm_ddr2.ddr2_cols,
	    s->sm_ddr2.ddr2_ranks + 1, s->sm_ddr2.ddr2_banks_per_chip,
	    cycle_time / 1000, (cycle_time % 1000 + 5) /10 );

	tAA  = 0;
	for (i = 2; i < 8; i++)
		if (s->sm_ddr2.ddr2_tCAS & (1 << i))
			tAA = i;

#define __DDR2_ROUND(scale, field)	\
		((scale * s->sm_ddr2.field + cycle_time - 1) / cycle_time)

	aprint_verbose_dev(self, LATENCY, tAA, __DDR2_ROUND(250, ddr2_tRCD),
		__DDR2_ROUND(250, ddr2_tRP), __DDR2_ROUND(1000, ddr2_tRAS));

#undef	__DDR_ROUND

	decode_voltage_refresh(self, s);
}

static void
decode_ddr3(const struct sysctlnode *node, device_t self, struct spdmem *s) {
	int dimm_size, cycle_time, bits;

	aprint_naive("\n");
	aprint_normal(": %18s\n", s->sm_ddr3.ddr3_part);
	aprint_normal_dev(self, "%s", spdmem_basic_types[s->sm_type]);

	if (s->sm_ddr3.ddr3_mod_type ==
		SPDMEM_DDR3_TYPE_MINI_RDIMM ||
	    s->sm_ddr3.ddr3_mod_type == SPDMEM_DDR3_TYPE_RDIMM)
		aprint_normal(" (registered)");
	aprint_normal(", %sECC, %stemp-sensor, ",
		(s->sm_ddr3.ddr3_hasECC)?"":"no ",
		(s->sm_ddr3.ddr3_has_therm_sensor)?"":"no ");

	/*
	 * DDR3 size specification is quite different from others
	 *
	 * Module capacity is defined as
	 *	Chip_Capacity_in_bits / 8bits-per-byte *
	 *	external_bus_width / internal_bus_width
	 * We further divide by 2**20 to get our answer in MB
	 */
	dimm_size = (s->sm_ddr3.ddr3_chipsize + 28 - 20) - 3 +
		    (s->sm_ddr3.ddr3_datawidth + 3) -
		    (s->sm_ddr3.ddr3_chipwidth + 2);
	dimm_size = (1 << dimm_size) * (s->sm_ddr3.ddr3_physbanks + 1);

	cycle_time = (1000 * s->sm_ddr3.ddr3_mtb_dividend + 
			    (s->sm_ddr3.ddr3_mtb_divisor / 2)) /
		     s->sm_ddr3.ddr3_mtb_divisor;
	cycle_time *= s->sm_ddr3.ddr3_tCKmin;
	bits = 1 << (s->sm_ddr3.ddr3_datawidth + 3);
	decode_size_speed(self, node, dimm_size, cycle_time, 2, bits, FALSE,
			  "PC3", 0);

	aprint_verbose_dev(self,
	    "%d rows, %d cols, %d log. banks, %d phys. banks, "
	    "%d.%03dns cycle time\n",
	    s->sm_ddr3.ddr3_rows + 9, s->sm_ddr3.ddr3_cols + 12,
	    1 << (s->sm_ddr3.ddr3_logbanks + 3),
	    s->sm_ddr3.ddr3_physbanks + 1,
	    cycle_time/1000, cycle_time % 1000);

#define	__DDR3_CYCLES(field) (s->sm_ddr3.field / s->sm_ddr3.ddr3_tCKmin)

	aprint_verbose_dev(self, LATENCY, __DDR3_CYCLES(ddr3_tAAmin),
		__DDR3_CYCLES(ddr3_tRCDmin), __DDR3_CYCLES(ddr3_tRPmin), 
		(s->sm_ddr3.ddr3_tRAS_msb * 256 + s->sm_ddr3.ddr3_tRAS_lsb) /
		    s->sm_ddr3.ddr3_tCKmin);

#undef	__DDR3_CYCLES

	/* For DDR3, Voltage is written in another area */
	if (!s->sm_ddr3.ddr3_NOT15V || s->sm_ddr3.ddr3_135V
	    || s->sm_ddr3.ddr3_125V) {
		aprint_verbose("%s:", device_xname(self));
		if (!s->sm_ddr3.ddr3_NOT15V)
			aprint_verbose(" 1.5V");
		if (s->sm_ddr3.ddr3_135V)
			aprint_verbose(" 1.35V");
		if (s->sm_ddr3.ddr3_125V)
			aprint_verbose(" 1.25V");
		aprint_verbose(" operable\n");
	}
}

static void
decode_fbdimm(const struct sysctlnode *node, device_t self, struct spdmem *s) {
	int dimm_size, cycle_time, bits;

	aprint_naive("\n");
	aprint_normal("\n");
	aprint_normal_dev(self, "%s", spdmem_basic_types[s->sm_type]);

	/*
	 * FB-DIMM module size calculation is very much like DDR3
	 */
	dimm_size = s->sm_fbd.fbdimm_rows + 12 +
		    s->sm_fbd.fbdimm_cols +  9 - 20 - 3;
	dimm_size = (1 << dimm_size) * (1 << (s->sm_fbd.fbdimm_banks + 2));

	cycle_time = (1000 * s->sm_fbd.fbdimm_mtb_dividend +
			    (s->sm_fbd.fbdimm_mtb_divisor / 2)) /
		     s->sm_fbd.fbdimm_mtb_divisor;
	bits = 1 << (s->sm_fbd.fbdimm_dev_width + 2);
	decode_size_speed(self, node, dimm_size, cycle_time, 2, bits, TRUE,
			  "PC2", 0);

	aprint_verbose_dev(self,
	    "%d rows, %d cols, %d banks, %d.%02dns cycle time\n",
	    s->sm_fbd.fbdimm_rows, s->sm_fbd.fbdimm_cols,
	    1 << (s->sm_fbd.fbdimm_banks + 2),
	    cycle_time / 1000, (cycle_time % 1000 + 5) /10 );

#define	__FBDIMM_CYCLES(field) (s->sm_fbd.field / s->sm_fbd.fbdimm_tCKmin)

	aprint_verbose_dev(self, LATENCY, __FBDIMM_CYCLES(fbdimm_tAAmin),
		__FBDIMM_CYCLES(fbdimm_tRCDmin), __FBDIMM_CYCLES(fbdimm_tRPmin), 
		(s->sm_fbd.fbdimm_tRAS_msb * 256 +
			s->sm_fbd.fbdimm_tRAS_lsb) /
		    s->sm_fbd.fbdimm_tCKmin);

#undef	__FBDIMM_CYCLES

	decode_voltage_refresh(self, s);
}

static void
decode_ddr4(const struct sysctlnode *node, device_t self, struct spdmem *s) {
	int dimm_size, cycle_time;
	int tAA_clocks, tRCD_clocks,tRP_clocks, tRAS_clocks;

	aprint_naive("\n");
	aprint_normal(": %20s\n", s->sm_ddr4.ddr4_part_number);
	aprint_normal_dev(self, "%s", spdmem_basic_types[s->sm_type]);
	if (s->sm_ddr4.ddr4_mod_type < __arraycount(spdmem_ddr4_module_types))
		aprint_normal(" (%s)", 
		    spdmem_ddr4_module_types[s->sm_ddr4.ddr4_mod_type]);
	aprint_normal(", %stemp-sensor, ",
		(s->sm_ddr4.ddr4_has_therm_sensor)?"":"no ");

	/*
	 * DDR4 size calculation from JEDEC spec
	 *
	 * Module capacity in bytes is defined as
	 *	Chip_Capacity_in_bits / 8bits-per-byte *
	 *	primary_bus_width / DRAM_width *
	 *	logical_ranks_per_DIMM
	 *
	 * logical_ranks_per DIMM equals package_ranks, but multiply
	 * by diecount for 3DS packages
	 *
	 * We further divide by 2**20 to get our answer in MB
	 */
	dimm_size = (s->sm_ddr4.ddr4_capacity + 28)	/* chip_capacity */
		     - 20				/* convert to MB */
		     - 3				/* bits --> bytes */
		     + (s->sm_ddr4.ddr4_primary_bus_width + 3); /* bus width */
	switch (s->sm_ddr4.ddr4_device_width) {		/* DRAM width */
	case 0:	dimm_size -= 2;
		break;
	case 1: dimm_size -= 3;
		break;
	case 2:	dimm_size -= 4;
		break;
	case 4: dimm_size -= 5;
		break;
	default:
		dimm_size = -1;		/* flag invalid value */
	}
	if (dimm_size >=0) {				
		dimm_size = (1 << dimm_size) *
		    (s->sm_ddr4.ddr4_package_ranks + 1); /* log.ranks/DIMM */
		if (s->sm_ddr4.ddr4_signal_loading == 2) {
			dimm_size *= s->sm_ddr4.ddr4_diecount;
		}
	}

	/*
	 * For now, the only value for mtb is 1 = 125ps, and ftp = 1ps 
	 * so we don't need to figure out the time-base units - just
	 * hard-code them for now.
	 */
	cycle_time = 125 * s->sm_ddr4.ddr4_tCKAVGmin_mtb + 
			   s->sm_ddr4.ddr4_tCKAVGmin_ftb;
	aprint_normal("%d MB, %d.%03dns cycle time (%dMHz)\n", dimm_size,
	    cycle_time/1000, cycle_time % 1000, 1000000 / cycle_time);

	decode_size_speed(self, node, dimm_size, cycle_time, 2,
			  1 << (s->sm_ddr4.ddr4_device_width + 3),
			  TRUE, "PC4", 0);

	aprint_verbose_dev(self,
	    "%d rows, %d cols, %d banks, %d bank groups\n",
	    s->sm_ddr3.ddr3_rows + 9, s->sm_ddr3.ddr3_cols + 12,
	    1 << (2 + s->sm_ddr4.ddr4_logbanks),
	    1 << s->sm_ddr4.ddr4_bankgroups);

/*
 * Note that the ddr4_xxx_ftb fields are actually signed offsets from
 * the corresponding mtb value, so we might have to subtract 256!
 */
#define	__DDR4_VALUE(field) (s->sm_ddr4.ddr4_##field##_mtb * 256 +	\
			     s->sm_ddr4.ddr4_##field##_ftb) - 		\
			     ((s->sm_ddr4.ddr4_##field##_ftb > 127)?256:0)

	tAA_clocks =  (__DDR4_VALUE(tAAmin)  * 1000 ) / cycle_time;
	tRP_clocks =  (__DDR4_VALUE(tRPmin)  * 1000 ) / cycle_time;
	tRCD_clocks = (__DDR4_VALUE(tRCDmin) * 1000 ) / cycle_time;
	tRAS_clocks = (s->sm_ddr4.ddr4_tRASmin_msb * 256 +
		       s->sm_ddr4.ddr4_tRASmin_lsb) * 125 * 1000 / cycle_time;

/*
 * Per JEDEC spec, rounding is done by taking the time value, dividing
 * by the cycle time, subtracting .010 from the result, and then
 * rounded up to the nearest integer.  Unfortunately, none of their
 * examples say what to do when the result of the subtraction is already
 * an integer.  For now, assume that we still round up (so an interval
 * of exactly 12.010 clock cycles will be printed as 13).
 */
#define	__DDR4_ROUND(value) ((value - 10) / 1000 + 1)

	aprint_verbose_dev(self, LATENCY, __DDR4_ROUND(tAA_clocks),
			   __DDR4_ROUND(tRP_clocks),
			   __DDR4_ROUND(tRCD_clocks),
			   __DDR4_ROUND(tRAS_clocks));

#undef	__DDR4_VALUE
#undef	__DDR4_ROUND
}
