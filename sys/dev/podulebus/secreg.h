/* $NetBSD: secreg.h,v 1.1 2006/10/01 12:39:35 bjh21 Exp $ */

/*
 * Ben Harris 2006
 *
 * This file is in the public domain.
 */

/*
 * Register definitions for Acorn SCSI expansion cards (AKA30, AKA31, AKA32)
 */

/*
 * Offsets are in bus_space units (words)
 */

/* Podule "fast" space */
#define SEC_ROM		0x000
#define SEC_ISR		0x800 /* Interrupt status (read-only) */
#define SEC_ISR_SBIC	0x08  /* Interrupt from WD33C93A SBIC */
#define SEC_ISR_DMAC	0x02  /* TC uPD71071 DMAC */
#define SEC_ISR_IRQ	0x01  /* OR of the above */
#define SEC_CLRINT	0x800 /* Clear TC interrupt (write-only) */
#define SEC_MPR		0xc00 /* Memory page register */
#define SEC_MPR_UR	0x80  /* User reset */
#define SEC_MPR_IE	0x40  /* Interrupts enabled */
#define SEC_MPR_PAGE	0x3f  /* EPROM/SRAM page address */

/* Module space */
#define SEC_SRAM	0x000
#define SEC_SBIC	0x800
#define SEC_DMAC	0xc00

/* The address lines of the DMAC are permuted. */
#define DMAC(addr) ((addr) >> 1 | ((addr) & 1) << 7)

#define SEC_CLKFREQ	80	/* Clock speed in 100 kHz */
#define SEC_NPAGES	16
#define SEC_PAGESIZE	4096
#define SEC_MEMSIZE	(SEC_PAGESIZE * SEC_NPAGES)
