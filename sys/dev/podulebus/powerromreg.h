/*	$NetBSD: powerromreg.h,v 1.1 2001/07/04 15:01:08 bjh21 Exp $	*/

/*
 * This file is in the public domain.
 */

/*
 * PowerROM card types
 */

/*
 * The Partis PowerROM is a replacement ROM (or rather series of ROMs)
 * for an assortment of SCSI cards for Acorn machines.  It's awkward
 * in that it replaces the podule ID with its own, which is the same
 * for all PowerROMs (and for Power-tec SCSI cards).  To find out the
 * actual hardware underneath it, we have to call the podule loader in
 * the ROM, and it will return one of the following constants.
 *
 * <URL:http://home.eunet.no/~kjetilbt/power-tec.mbox>
 */

#define PRID_POWERTEC		0x00 /* Power-tec card, ID in hardware */

#define PRID_MORLEY_SCSI1	0x11 /* Morley 16 bit SCSI-1 */
#define PRID_MORLEY_CACHED	0x12 /* Morley 16 bit Cached SCSI-1 */
#define PRID_MORLEY_TURBO	0x13 /* Morley Turbo 16 bit SCSI-1 */

#define PRID_OAK_SCSI1		0x21 /* Oak 16 bit SCSI-1 */

#define PRID_CUMANA_SCSI1_8	0x31 /* Cumana 8 bit SCSI-1 */
#define PRID_CUMANA_SCSI1_16	0x32 /* Cumana 16 bit SCSI-1 */
#define PRID_CUMANA_SCSI2	0x33 /* Cumana 16/32 bit SCSI-2 */
#define PRID_CUMANA_SCSI3	0x34 /* Cumana 32 bit Ultra SCSI-3 */

#define PRID_LINDIS_SCSI1	0x41 /* Lindis 8/16 bit SCSI-1 */

#define PRID_HCCS_SCSI1		0x51 /* HCCS 8 bit SCSI-1 */
#define PRID_HCCS_REDSHIFT	0x52 /* HCCS RedShift 16 bit SCSI-1 */

#define PRID_TECHNO_SCSI1	0x61 /* Techno 16 bit SCSI-1 */

#define PRID_ARXE_SCSI1		0x71 /* Arxe/TSP 16 bit SCSI-1 */
#define PRID_ARXE_SCSI1_2	0x72 /* Arxe/TSP 16 bit SCSI-1 */
#define PRID_ARXE_TURBO		0x73 /* Arxe/TSP 8 bit Turbo SCSI-1 */

#define PRID_MCS_CONNECT32	0x81 /* MCS Connect 32 bit SCSI-2 */

#define PRID_CASTLE_SCSI2	0x91 /* Castle 16 bit SCSI-2 */

#define PRID_ACORN_SCSI1	0xa1 /* Acorn 16 bit SCSI-1 */

#define PRID_SYMBIOS_UWIDE	0xb1 /* Symbios 32 bit UltraWide SCSI-2 */

/* I don't think the ones below will ever appear on a podule */

#define PRID_PARALLEL_ATAPI	0xe1 /* Misc Parallel ATAPI */
#define PRID_IOEB_ATAPI		0xe2 /* Misc ADFS IOEB ATAPI */
#define PRID_IOMD_ATAPI		0xe3 /* Misc ADFS IOMD ATAPI */

#define PRID_PARALLEL_SCSI1	0xf1 /* Misc Parallel SCSI-1 */
#define PRID_ECONET_SCSI1	0xf2 /* Misc Econet SCSI-1 */
#define PRID_UPODULE_SCSI1	0xf3 /* Misc uPodule SCSI-1 */
#define PRID_EPST_SCSI1		0xf4 /* Misc EPST SCSI-1 */

