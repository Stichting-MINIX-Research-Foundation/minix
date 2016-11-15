/* $NetBSD: if_lmc.c,v 1.56 2014/11/28 08:03:46 ozaki-r Exp $ */

/*-
 * Copyright (c) 2002-2006 David Boggs. <boggs@boggs.palo-alto.ca.us>
 * All rights reserved.
 *
 * BSD LICENSE:
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * GNU GENERAL PUBLIC LICENSE:
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * DESCRIPTION:
 *
 * This is an open-source Unix device driver for PCI-bus WAN interface cards.
 * It sends and receives packets in HDLC frames over synchronous links.
 * A generic PC plus Unix plus some LMC cards makes an OPEN router.
 * This driver works with FreeBSD, NetBSD, OpenBSD, BSD/OS and Linux.
 * It has been tested on i386 (32-bit little-end), PowerPC (32-bit
 * big-end), Sparc (64-bit big-end), and Alpha (64-bit little-end)
 * architectures.
 *
 * HISTORY AND AUTHORS:
 *
 * Ron Crane had the neat idea to use a Fast Ethernet chip as a PCI
 * interface and add an Ethernet-to-HDLC gate array to make a WAN card.
 * David Boggs designed the Ethernet-to-HDLC gate arrays and PC cards.
 * We did this at our company, LAN Media Corporation (LMC).
 * SBE Corp aquired LMC and continues to make the cards.
 *
 * Since the cards use Tulip Ethernet chips, we started with Matt Thomas'
 * ubiquitous "de" driver.  Michael Graff stripped out the Ethernet stuff
 * and added HSSI stuff.  Basil Gunn ported it to Solaris (lost) and
 * Rob Braun ported it to Linux.  Andrew Stanley-Jones added support
 * for three more cards and wrote the first version of lmcconfig.
 * During 2002-5 David Boggs rewrote it and now feels responsible for it.
 *
 * RESPONSIBLE INDIVIDUAL:
 *
 * Send bug reports and improvements to <boggs@boggs.palo-alto.ca.us>.
 */

# include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_lmc.c,v 1.56 2014/11/28 08:03:46 ozaki-r Exp $");
# include <sys/param.h>	/* OS version */
# include "opt_inet.h"	/* INET6, INET */
# include "opt_altq_enabled.h" /* ALTQ */
# define IOREF_CSR 1	/* 1=IO refs; 0=MEM refs */
# define IFNET 1
# define NETDEV 0
# define NAPI 0
# define SPPP 1
# define P2P 0
# define GEN_HDLC 0
# define SYNC_PPP 0
# define NETGRAPH 0
# define DEVICE_POLLING 0
#
# include <sys/systm.h>
# include <sys/kernel.h>
# include <sys/module.h>
# include <sys/mbuf.h>
# include <sys/socket.h>
# include <sys/sockio.h>
# include <sys/device.h>
# include <sys/reboot.h>
# include <sys/kauth.h>
# include <sys/proc.h>
# include <net/if.h>
# include <net/if_types.h>
# include <net/if_media.h>
# include <net/netisr.h>
# include <sys/bus.h>
# include <sys/intr.h>
# include <machine/lock.h>
# include <machine/types.h>
# include <dev/pci/pcivar.h>
# if INET || INET6
#  include <netinet/in.h>
#  include <netinet/in_var.h>
# endif
# if SPPP
#  include <net/if_spppvar.h>
# endif
#  include <net/bpf.h>
# if !defined(ALTQ)
#  define ALTQ 0
# endif
/* and finally... */
# include "if_lmc.h"




/* The SROM is a generic 93C46 serial EEPROM (64 words by 16 bits). */
/* Data is set up before the RISING edge of CLK; CLK is parked low. */
static void  /* context: process */
srom_shift_bits(softc_t *sc, u_int32_t data, u_int32_t len)
  {
  u_int32_t csr = READ_CSR(sc, TLP_SROM_MII);
  for (; len>0; len--)
    {  /* MSB first */
    if (data & (1<<(len-1)))
      csr |=  TLP_SROM_DIN;	/* DIN setup */
    else
      csr &= ~TLP_SROM_DIN;	/* DIN setup */
    WRITE_CSR(sc, TLP_SROM_MII, csr);
    csr |=  TLP_SROM_CLK;	/* CLK rising edge */
    WRITE_CSR(sc, TLP_SROM_MII, csr);
    csr &= ~TLP_SROM_CLK;	/* CLK falling edge */
    WRITE_CSR(sc, TLP_SROM_MII, csr);
    }
  }

/* Data is sampled on the RISING edge of CLK; CLK is parked low. */
static u_int16_t  /* context: process */
srom_read(softc_t *sc, u_int8_t addr)
  {
  int i;
  u_int32_t csr;
  u_int16_t data;

  /* Enable SROM access. */
  csr = (TLP_SROM_SEL | TLP_SROM_RD | TLP_MII_MDOE);
  WRITE_CSR(sc, TLP_SROM_MII, csr);
  /* CS rising edge prepares SROM for a new cycle. */
  csr |= TLP_SROM_CS;
  WRITE_CSR(sc, TLP_SROM_MII, csr);	/* assert CS */
  srom_shift_bits(sc,  6,   4);		/* issue read cmd */
  srom_shift_bits(sc, addr, 6);		/* issue address */
  for (data=0, i=16; i>=0; i--)		/* read ->17<- bits of data */
    {  /* MSB first */
    csr = READ_CSR(sc, TLP_SROM_MII);	/* DOUT sampled */
    data = (data<<1) | ((csr & TLP_SROM_DOUT) ? 1:0);
    csr |=  TLP_SROM_CLK;		/* CLK rising edge */
    WRITE_CSR(sc, TLP_SROM_MII, csr);
    csr &= ~TLP_SROM_CLK;		/* CLK falling edge */
    WRITE_CSR(sc, TLP_SROM_MII, csr);
    }
  /* Disable SROM access. */
  WRITE_CSR(sc, TLP_SROM_MII, TLP_MII_MDOE);

  return data;
  }

/* The SROM is formatted by the mfgr and should NOT be written! */
/* But lmcconfig can rewrite it in case it gets overwritten somehow. */
static void  /* context: process */
srom_write(softc_t *sc, u_int8_t addr, u_int16_t data)
  {
  u_int32_t csr;
  int i;

  /* Enable SROM access. */
  csr = (TLP_SROM_SEL | TLP_SROM_RD | TLP_MII_MDOE);
  WRITE_CSR(sc, TLP_SROM_MII, csr);

  /* Issue write-enable command. */
  csr |= TLP_SROM_CS;
  WRITE_CSR(sc, TLP_SROM_MII, csr);	/* assert CS */
  srom_shift_bits(sc,  4, 4);		/* issue write enable cmd */
  srom_shift_bits(sc, 63, 6);		/* issue address */
  csr &= ~TLP_SROM_CS;
  WRITE_CSR(sc, TLP_SROM_MII, csr);	/* deassert CS */

  /* Issue erase command. */
  csr |= TLP_SROM_CS;
  WRITE_CSR(sc, TLP_SROM_MII, csr);	/* assert CS */
  srom_shift_bits(sc, 7, 4);		/* issue erase cmd */
  srom_shift_bits(sc, addr, 6);		/* issue address */
  csr &= ~TLP_SROM_CS;
  WRITE_CSR(sc, TLP_SROM_MII, csr);	/* deassert CS */

  /* Issue write command. */
  csr |= TLP_SROM_CS;
  WRITE_CSR(sc, TLP_SROM_MII, csr);	/* assert CS */
  for (i=0; i<10; i++)  /* 100 ms max wait */
    if ((READ_CSR(sc, TLP_SROM_MII) & TLP_SROM_DOUT)==0) SLEEP(10000);
  srom_shift_bits(sc, 5, 4);		/* issue write cmd */
  srom_shift_bits(sc, addr, 6);		/* issue address */
  srom_shift_bits(sc, data, 16);	/* issue data */
  csr &= ~TLP_SROM_CS;
  WRITE_CSR(sc, TLP_SROM_MII, csr);	/* deassert CS */

  /* Issue write-disable command. */
  csr |= TLP_SROM_CS;
  WRITE_CSR(sc, TLP_SROM_MII, csr);	/* assert CS */
  for (i=0; i<10; i++)  /* 100 ms max wait */
    if ((READ_CSR(sc, TLP_SROM_MII) & TLP_SROM_DOUT)==0) SLEEP(10000);
  srom_shift_bits(sc, 4, 4);		/* issue write disable cmd */
  srom_shift_bits(sc, 0, 6);		/* issue address */
  csr &= ~TLP_SROM_CS;
  WRITE_CSR(sc, TLP_SROM_MII, csr);	/* deassert CS */

  /* Disable SROM access. */
  WRITE_CSR(sc, TLP_SROM_MII, TLP_MII_MDOE);
  }

/* Not all cards have BIOS roms. */
/* The BIOS ROM is an AMD 29F010 1Mbit (128K by 8) EEPROM. */
static u_int8_t  /* context: process */
bios_read(softc_t *sc, u_int32_t addr)
  {
  u_int32_t srom_mii;

  /* Load the BIOS rom address register. */
  WRITE_CSR(sc, TLP_BIOS_ROM, addr);

  /* Enable the BIOS rom. */
  srom_mii = TLP_BIOS_SEL | TLP_BIOS_RD | TLP_MII_MDOE;
  WRITE_CSR(sc, TLP_SROM_MII, srom_mii);

  /* Wait at least 20 PCI cycles. */
  DELAY(20);

  /* Read the BIOS rom data. */
  srom_mii = READ_CSR(sc, TLP_SROM_MII);

  /* Disable the BIOS rom. */
  WRITE_CSR(sc, TLP_SROM_MII, TLP_MII_MDOE);

  return (u_int8_t)srom_mii & 0xFF;
  }

static void  /* context: process */
bios_write_phys(softc_t *sc, u_int32_t addr, u_int8_t data)
  {
  u_int32_t srom_mii;

  /* Load the BIOS rom address register. */
  WRITE_CSR(sc, TLP_BIOS_ROM, addr);

  /* Enable the BIOS rom. */
  srom_mii = TLP_BIOS_SEL | TLP_BIOS_WR | TLP_MII_MDOE;

  /* Load the data into the data register. */
  srom_mii = (srom_mii & 0xFFFFFF00) | (data & 0xFF);
  WRITE_CSR(sc, TLP_SROM_MII, srom_mii);

  /* Wait at least 20 PCI cycles. */
  DELAY(20);

  /* Disable the BIOS rom. */
  WRITE_CSR(sc, TLP_SROM_MII, TLP_MII_MDOE);
  }

static void  /* context: process */
bios_write(softc_t *sc, u_int32_t addr, u_int8_t data)
  {
  u_int8_t read_data;

  /* this sequence enables writing */
  bios_write_phys(sc, 0x5555, 0xAA);
  bios_write_phys(sc, 0x2AAA, 0x55);
  bios_write_phys(sc, 0x5555, 0xA0);
  bios_write_phys(sc, addr,   data);

  /* Wait for the write operation to complete. */
  for (;;)  /* interruptible syscall */
    {
    for (;;)
      {
      read_data = bios_read(sc, addr);
      if ((read_data & 0x80) == (data & 0x80)) break;
      if  (read_data & 0x20)
        {  /* Data sheet says read it again. */
        read_data = bios_read(sc, addr);
        if ((read_data & 0x80) == (data & 0x80)) break;
        if (sc->config.debug)
          printf("%s: bios_write() failed; rom addr=0x%x\n",
           NAME_UNIT, addr);
        return;
        }
      }
    read_data = bios_read(sc, addr);
    if (read_data == data) break;
    }
  }

static void  /* context: process */
bios_erase(softc_t *sc)
  {
  unsigned char read_data;

  /* This sequence enables erasing: */
  bios_write_phys(sc, 0x5555, 0xAA);
  bios_write_phys(sc, 0x2AAA, 0x55);
  bios_write_phys(sc, 0x5555, 0x80);
  bios_write_phys(sc, 0x5555, 0xAA);
  bios_write_phys(sc, 0x2AAA, 0x55);
  bios_write_phys(sc, 0x5555, 0x10);

  /* Wait for the erase operation to complete. */
  for (;;) /* interruptible syscall */
    {
    for (;;)
      {
      read_data = bios_read(sc, 0);
      if (read_data & 0x80) break;
      if (read_data & 0x20)
        {  /* Data sheet says read it again. */
        read_data = bios_read(sc, 0);
        if (read_data & 0x80) break;
        if (sc->config.debug)
          printf("%s: bios_erase() failed\n", NAME_UNIT);
        return;
        }
      }
    read_data = bios_read(sc, 0);
    if (read_data == 0xFF) break;
    }
  }

/* MDIO is 3-stated between tranactions. */
/* MDIO is set up before the RISING edge of MDC; MDC is parked low. */
static void  /* context: process */
mii_shift_bits(softc_t *sc, u_int32_t data, u_int32_t len)
  {
  u_int32_t csr = READ_CSR(sc, TLP_SROM_MII);
  for (; len>0; len--)
    {  /* MSB first */
    if (data & (1<<(len-1)))
      csr |=  TLP_MII_MDOUT; /* MDOUT setup */
    else
      csr &= ~TLP_MII_MDOUT; /* MDOUT setup */
    WRITE_CSR(sc, TLP_SROM_MII, csr);
    csr |=  TLP_MII_MDC;     /* MDC rising edge */
    WRITE_CSR(sc, TLP_SROM_MII, csr);
    csr &= ~TLP_MII_MDC;     /* MDC falling edge */
    WRITE_CSR(sc, TLP_SROM_MII, csr);
    }
  }

/* The specification for the MII is IEEE Std 802.3 clause 22. */
/* MDIO is sampled on the RISING edge of MDC; MDC is parked low. */
static u_int16_t  /* context: process */
mii_read(softc_t *sc, u_int8_t regad)
  {
  int i;
  u_int32_t csr;
  u_int16_t data = 0;

  WRITE_CSR(sc, TLP_SROM_MII, TLP_MII_MDOUT);

  mii_shift_bits(sc, 0xFFFFF, 20);	/* preamble */
  mii_shift_bits(sc, 0xFFFFF, 20);	/* preamble */
  mii_shift_bits(sc, 1, 2);		/* start symbol */
  mii_shift_bits(sc, 2, 2);		/* read op */
  mii_shift_bits(sc, 0, 5);		/* phyad=0 */
  mii_shift_bits(sc, regad, 5);		/* regad */
  csr = READ_CSR(sc, TLP_SROM_MII);
  csr |= TLP_MII_MDOE;
  WRITE_CSR(sc, TLP_SROM_MII, csr);
  mii_shift_bits(sc, 0, 2);		/* turn-around */
  for (i=15; i>=0; i--)			/* data */
    {  /* MSB first */
    csr = READ_CSR(sc, TLP_SROM_MII);	/* MDIN sampled */
    data = (data<<1) | ((csr & TLP_MII_MDIN) ? 1:0);
    csr |=  TLP_MII_MDC;		/* MDC rising edge */
    WRITE_CSR(sc, TLP_SROM_MII, csr);
    csr &= ~TLP_MII_MDC;		/* MDC falling edge */
    WRITE_CSR(sc, TLP_SROM_MII, csr);
    }
  return data;
  }

static void  /* context: process */
mii_write(softc_t *sc, u_int8_t regad, u_int16_t data)
  {
  WRITE_CSR(sc, TLP_SROM_MII, TLP_MII_MDOUT);
  mii_shift_bits(sc, 0xFFFFF, 20);	/* preamble */
  mii_shift_bits(sc, 0xFFFFF, 20);	/* preamble */
  mii_shift_bits(sc, 1, 2);		/* start symbol */
  mii_shift_bits(sc, 1, 2);		/* write op */
  mii_shift_bits(sc, 0, 5);		/* phyad=0 */
  mii_shift_bits(sc, regad, 5);		/* regad */
  mii_shift_bits(sc, 2, 2);		/* turn-around */
  mii_shift_bits(sc, data, 16);		/* data */
  WRITE_CSR(sc, TLP_SROM_MII, TLP_MII_MDOE);
  if (regad == 16) sc->led_state = data; /* a small optimization */
  }

static void
mii16_set_bits(softc_t *sc, u_int16_t bits)
  {
  u_int16_t mii16 = mii_read(sc, 16);
  mii16 |= bits;
  mii_write(sc, 16, mii16);
  }

static void
mii16_clr_bits(softc_t *sc, u_int16_t bits)
  {
  u_int16_t mii16 = mii_read(sc, 16);
  mii16 &= ~bits;
  mii_write(sc, 16, mii16);
  }

static void
mii17_set_bits(softc_t *sc, u_int16_t bits)
  {
  u_int16_t mii17 = mii_read(sc, 17);
  mii17 |= bits;
  mii_write(sc, 17, mii17);
  }

static void
mii17_clr_bits(softc_t *sc, u_int16_t bits)
  {
  u_int16_t mii17 = mii_read(sc, 17);
  mii17 &= ~bits;
  mii_write(sc, 17, mii17);
  }

/*
 * Watchdog code is more readable if it refreshes LEDs
 *  once a second whether they need it or not.
 * But MII refs take 150 uSecs each, so remember the last value
 *  written to MII16 and avoid LED writes that do nothing.
 */

static void
led_off(softc_t *sc, u_int16_t led)
  {
  if ((led & sc->led_state) == led) return;
  mii16_set_bits(sc, led);
  }

static void
led_on(softc_t *sc, u_int16_t led)
  {
  if ((led & sc->led_state) == 0) return;
  mii16_clr_bits(sc, led);
  }

static void
led_inv(softc_t *sc, u_int16_t led)
  {
  u_int16_t mii16 = mii_read(sc, 16);
  mii16 ^= led;
  mii_write(sc, 16, mii16);
  }

/*
 * T1 & T3 framer registers are accessed through MII regs 17 & 18.
 * Write the address to MII reg 17 then R/W data through MII reg 18.
 * The hardware interface is an Intel-style 8-bit muxed A/D bus.
 */
static void
framer_write(softc_t *sc, u_int16_t addr, u_int8_t data)
  {
  mii_write(sc, 17, addr);
  mii_write(sc, 18, data);
  }

static u_int8_t
framer_read(softc_t *sc, u_int16_t addr)
  {
  mii_write(sc, 17, addr);
  return (u_int8_t)mii_read(sc, 18);
  }

/* Tulip's hardware implementation of General Purpose IO
 *   (GPIO) pins makes life difficult for software.
 * Bits 7-0 in the Tulip GPIO CSR are used for two purposes
 *   depending on the state of bit 8.
 * If bit 8 is 0 then bits 7-0 are "data" bits.
 * If bit 8 is 1 then bits 7-0 are "direction" bits.
 * If a direction bit is one, the data bit is an output.
 * The problem is that the direction bits are WRITE-ONLY.
 * Software must remember the direction bits in a shadow copy.
 * (sc->gpio_dir) in order to change some but not all of the bits.
 * All accesses to the Tulip GPIO register use these five procedures.
 */

static void
gpio_make_input(softc_t *sc, u_int32_t bits)
  {
  sc->gpio_dir &= ~bits;
  WRITE_CSR(sc, TLP_GPIO, TLP_GPIO_DIR | (sc->gpio_dir));
  }

static void
gpio_make_output(softc_t *sc, u_int32_t bits)
  {
  sc->gpio_dir |= bits;
  WRITE_CSR(sc, TLP_GPIO, TLP_GPIO_DIR | (sc->gpio_dir));
  }

static u_int32_t
gpio_read(softc_t *sc)
  {
  return READ_CSR(sc, TLP_GPIO);
  }

static void
gpio_set_bits(softc_t *sc, u_int32_t bits)
  {
  WRITE_CSR(sc, TLP_GPIO, (gpio_read(sc) |  bits) & 0xFF);
  }

static void
gpio_clr_bits(softc_t *sc, u_int32_t bits)
  {
  WRITE_CSR(sc, TLP_GPIO, (gpio_read(sc) & ~bits) & 0xFF);
  }

/* Reset ALL of the flip-flops in the gate array to zero. */
/* This does NOT change the gate array programming. */
/* Called during initialization so it must not sleep. */
static void  /* context: kernel (boot) or process (syscall) */
xilinx_reset(softc_t *sc)
  {
  /* Drive RESET low to force initialization. */
  gpio_clr_bits(sc, GPIO_RESET);
  gpio_make_output(sc, GPIO_RESET);

  /* Hold RESET low for more than 10 uSec. */
  DELAY(50);

  /* Done with RESET; make it an input. */
  gpio_make_input(sc,  GPIO_RESET);
  }

/* Load Xilinx gate array program from on-board rom. */
/* This changes the gate array programming. */
static void  /* context: process */
xilinx_load_from_rom(softc_t *sc)
  {
  int i;

  /* Drive MODE low to load from ROM rather than GPIO. */
  gpio_clr_bits(sc, GPIO_MODE);
  gpio_make_output(sc, GPIO_MODE);

  /* Drive DP & RESET low to force configuration. */
  gpio_clr_bits(sc, GPIO_RESET | GPIO_DP);
  gpio_make_output(sc, GPIO_RESET | GPIO_DP);

  /* Hold RESET & DP low for more than 10 uSec. */
  DELAY(50);

  /* Done with RESET & DP; make them inputs. */
  gpio_make_input(sc, GPIO_DP | GPIO_RESET);

  /* BUSY-WAIT for Xilinx chip to configure itself from ROM bits. */
  for (i=0; i<100; i++) /* 1 sec max delay */
    if ((gpio_read(sc) & GPIO_DP)==0) SLEEP(10000);

  /* Done with MODE; make it an input. */
  gpio_make_input(sc, GPIO_MODE);
  }

/* Load the Xilinx gate array program from userland bits. */
/* This changes the gate array programming. */
static int  /* context: process */
xilinx_load_from_file(softc_t *sc, char *addr, u_int32_t len)
  {
  char *data;
  int i, j, error;

  /* Get some pages to hold the Xilinx bits; biggest file is < 6 KB. */
  if (len > 8192) return EFBIG;  /* too big */
  data = malloc(len, M_TEMP, M_WAITOK);
  if (data == NULL) return ENOMEM;

  /* Copy the Xilinx bits from userland. */
  if ((error = copyin(addr, data, len)))
    {
    free(data, M_TEMP);
    return error;
    }

  /* Drive MODE high to load from GPIO rather than ROM. */
  gpio_set_bits(sc, GPIO_MODE);
  gpio_make_output(sc, GPIO_MODE);

  /* Drive DP & RESET low to force configuration. */
  gpio_clr_bits(sc, GPIO_RESET | GPIO_DP);
  gpio_make_output(sc, GPIO_RESET | GPIO_DP);

  /* Hold RESET & DP low for more than 10 uSec. */
  DELAY(50);
 
  /* Done with RESET & DP; make them inputs. */
  gpio_make_input(sc, GPIO_RESET | GPIO_DP);

  /* BUSY-WAIT for Xilinx chip to clear its config memory. */
  gpio_make_input(sc, GPIO_INIT);
  for (i=0; i<10000; i++) /* 1 sec max delay */
    if ((gpio_read(sc) & GPIO_INIT)==0) SLEEP(10000);

  /* Configure CLK and DATA as outputs. */
  gpio_set_bits(sc, GPIO_CLK);  /* park CLK high */
  gpio_make_output(sc, GPIO_CLK | GPIO_DATA);

  /* Write bits to Xilinx; CLK is parked HIGH. */
  /* DATA is set up before the RISING edge of CLK. */
  for (i=0; i<len; i++)
    for (j=0; j<8; j++)
      {  /* LSB first */
      if (data[i] & (1<<j))
        gpio_set_bits(sc, GPIO_DATA); /* DATA setup */
      else
        gpio_clr_bits(sc, GPIO_DATA); /* DATA setup */
      gpio_clr_bits(sc, GPIO_CLK); /* CLK falling edge */
      gpio_set_bits(sc, GPIO_CLK); /* CLK rising edge */
      }

  /* Stop driving all Xilinx-related signals. */
  /* Pullup and pulldown resistors take over. */
  gpio_make_input(sc, GPIO_CLK | GPIO_DATA | GPIO_MODE);

  free(data, M_TEMP);

  return 0;
  }

/* Write fragments of a command into the synthesized oscillator. */
/* DATA is set up before the RISING edge of CLK.  CLK is parked low. */
static void
synth_shift_bits(softc_t *sc, u_int32_t data, u_int32_t len)
  {
  int i;

  for (i=0; i<len; i++)
    { /* LSB first */
    if (data & (1<<i))
      gpio_set_bits(sc, GPIO_DATA); /* DATA setup */
    else
      gpio_clr_bits(sc, GPIO_DATA); /* DATA setup */
    gpio_set_bits(sc, GPIO_CLK);    /* CLK rising edge */
    gpio_clr_bits(sc, GPIO_CLK);    /* CLK falling edge */
    }
  }

/* Write a command to the synthesized oscillator on SSI and HSSIc. */
static void  /* context: process */
synth_write(softc_t *sc, struct synth *synth)
  {
  /* SSI cards have a programmable prescaler */
  if (sc->status.card_type == CSID_LMC_SSI)
    {
    if (synth->prescale == 9) /* divide by 512 */
      mii17_set_bits(sc, MII17_SSI_PRESCALE);
    else                      /* divide by  32 */
      mii17_clr_bits(sc, MII17_SSI_PRESCALE);
    }

  gpio_clr_bits(sc,    GPIO_DATA | GPIO_CLK);
  gpio_make_output(sc, GPIO_DATA | GPIO_CLK);

  /* SYNTH is a low-true chip enable for the AV9110 chip. */
  gpio_set_bits(sc,    GPIO_SSI_SYNTH);
  gpio_make_output(sc, GPIO_SSI_SYNTH);
  gpio_clr_bits(sc,    GPIO_SSI_SYNTH);

  /* Serially shift the command into the AV9110 chip. */
  synth_shift_bits(sc, synth->n, 7);
  synth_shift_bits(sc, synth->m, 7);
  synth_shift_bits(sc, synth->v, 1);
  synth_shift_bits(sc, synth->x, 2);
  synth_shift_bits(sc, synth->r, 2);
  synth_shift_bits(sc, 0x16, 5); /* enable clk/x output */

  /* SYNTH (chip enable) going high ends the command. */
  gpio_set_bits(sc,   GPIO_SSI_SYNTH);
  gpio_make_input(sc, GPIO_SSI_SYNTH);

  /* Stop driving serial-related signals; pullups/pulldowns take over. */
  gpio_make_input(sc, GPIO_DATA | GPIO_CLK);

  /* remember the new synthesizer parameters */
  if (&sc->config.synth != synth) sc->config.synth = *synth;
  }

/* Write a command to the DAC controlling the VCXO on some T3 adapters. */
/* The DAC is a TI-TLV5636: 12-bit resolution and a serial interface. */
/* DATA is set up before the FALLING edge of CLK.  CLK is parked HIGH. */
static void  /* context: process */
dac_write(softc_t *sc, u_int16_t data)
  {
  int i;

  /* Prepare to use DATA and CLK. */
  gpio_set_bits(sc,    GPIO_DATA | GPIO_CLK);
  gpio_make_output(sc, GPIO_DATA | GPIO_CLK);

  /* High-to-low transition prepares DAC for new value. */
  gpio_set_bits(sc,    GPIO_T3_DAC);
  gpio_make_output(sc, GPIO_T3_DAC);
  gpio_clr_bits(sc,    GPIO_T3_DAC);

  /* Serially shift command bits into DAC. */
  for (i=0; i<16; i++)
    { /* MSB first */
    if (data & (1<<(15-i)))
      gpio_set_bits(sc, GPIO_DATA); /* DATA setup */
    else
      gpio_clr_bits(sc, GPIO_DATA); /* DATA setup */
    gpio_clr_bits(sc, GPIO_CLK);    /* CLK falling edge */
    gpio_set_bits(sc, GPIO_CLK);    /* CLK rising edge */
    }

  /* Done with DAC; make it an input; loads new value into DAC. */
  gpio_set_bits(sc,   GPIO_T3_DAC);
  gpio_make_input(sc, GPIO_T3_DAC);

  /* Stop driving serial-related signals; pullups/pulldowns take over. */
  gpio_make_input(sc, GPIO_DATA | GPIO_CLK);
  }

/* Begin HSSI card code */

static struct card hssi_card =
  {
  .ident    = hssi_ident,
  .watchdog = hssi_watchdog,
  .ioctl    = hssi_ioctl,
  .attach   = hssi_attach,
  .detach   = hssi_detach,
  };

static void
hssi_ident(softc_t *sc)
  {
  printf(", EIA-613");
  }

static void  /* context: softirq */
hssi_watchdog(softc_t *sc)
  {
  u_int16_t mii16 = mii_read(sc, 16) & MII16_HSSI_MODEM;

  sc->status.link_state = STATE_UP;

  led_inv(sc, MII16_HSSI_LED_UL);  /* Software is alive. */
  led_on(sc, MII16_HSSI_LED_LL);  /* always on (SSI cable) */

  /* Check the transmit clock. */
  if (sc->status.tx_speed == 0)
    {
    led_on(sc, MII16_HSSI_LED_UR);
    sc->status.link_state = STATE_DOWN;
    }
  else
    led_off(sc, MII16_HSSI_LED_UR);

  /* Is the modem ready? */
  if ((mii16 & MII16_HSSI_CA)==0)
    {
    led_off(sc, MII16_HSSI_LED_LR);
    sc->status.link_state = STATE_DOWN;
    }
  else
    led_on(sc, MII16_HSSI_LED_LR);

  /* Print the modem control signals if they changed. */
  if ((sc->config.debug) && (mii16 != sc->last_mii16))
    {
    const char *on = "ON ", *off = "OFF";
    printf("%s: TA=%s CA=%s LA=%s LB=%s LC=%s TM=%s\n", NAME_UNIT,
     (mii16 & MII16_HSSI_TA) ? on : off,
     (mii16 & MII16_HSSI_CA) ? on : off,
     (mii16 & MII16_HSSI_LA) ? on : off,
     (mii16 & MII16_HSSI_LB) ? on : off,
     (mii16 & MII16_HSSI_LC) ? on : off,
     (mii16 & MII16_HSSI_TM) ? on : off);
    }

  /* SNMP one-second-report */
  sc->status.snmp.hssi.sigs = mii16 & MII16_HSSI_MODEM;

  /* Remember this state until next time. */
  sc->last_mii16 = mii16;

  /* If a loop back is in effect, link status is UP */
  if (sc->config.loop_back != CFG_LOOP_NONE)
    sc->status.link_state = STATE_UP;
  }

static int  /* context: process */
hssi_ioctl(softc_t *sc, struct ioctl *ioctl)
  {
  int error = 0;

  if (ioctl->cmd == IOCTL_SNMP_SIGS)
    {
    u_int16_t mii16 = mii_read(sc, 16);
    mii16 &= ~MII16_HSSI_MODEM;
    mii16 |= (MII16_HSSI_MODEM & ioctl->data);
    mii_write(sc, 16, mii16);
    }
  else if (ioctl->cmd == IOCTL_SET_STATUS)
    {
    if (ioctl->data)
      mii16_set_bits(sc, MII16_HSSI_TA);
    else
      mii16_clr_bits(sc, MII16_HSSI_TA);
    }
  else
    error = EINVAL;

  return error;
  }

/* Must not sleep. */
static void
hssi_attach(softc_t *sc, struct config *config)
  {
  if (config == NULL) /* startup config */
    {
    sc->status.card_type  = READ_PCI_CFG(sc, TLP_CSID);
    sc->config.crc_len    = CFG_CRC_16;
    sc->config.loop_back  = CFG_LOOP_NONE;
    sc->config.tx_clk_src = CFG_CLKMUX_ST;
    sc->config.dte_dce    = CFG_DTE;
    sc->config.synth.n    = 52; /* 52.000 Mbs */
    sc->config.synth.m    = 5;
    sc->config.synth.v    = 0;
    sc->config.synth.x    = 0;
    sc->config.synth.r    = 0;
    sc->config.synth.prescale = 2;
    }
  else if (config != &sc->config) /* change config */
    {
    u_int32_t *old_synth = (u_int32_t *)&sc->config.synth;
    u_int32_t *new_synth = (u_int32_t *)&config->synth;
    if ((sc->config.crc_len    == config->crc_len)    &&
        (sc->config.loop_back  == config->loop_back)  &&
        (sc->config.tx_clk_src == config->tx_clk_src) &&
        (sc->config.dte_dce    == config->dte_dce)    &&
        (*old_synth            == *new_synth))
      return; /* nothing changed */
    sc->config.crc_len    = config->crc_len;
    sc->config.loop_back  = config->loop_back;
    sc->config.tx_clk_src = config->tx_clk_src;
    sc->config.dte_dce    = config->dte_dce;
    *old_synth            = *new_synth;
    }
  /* If (config == &sc->config) then the FPGA microcode
   * was just initialized and the current config should
   * be reloaded into the card.
   */

  /* set CRC length */
  if (sc->config.crc_len == CFG_CRC_32)
    mii16_set_bits(sc, MII16_HSSI_CRC32);
  else
    mii16_clr_bits(sc, MII16_HSSI_CRC32);

  /* Assert pin LA in HSSI conn: ask modem for local loop. */
  if (sc->config.loop_back == CFG_LOOP_LL)
    mii16_set_bits(sc, MII16_HSSI_LA);
  else
    mii16_clr_bits(sc, MII16_HSSI_LA);

  /* Assert pin LB in HSSI conn: ask modem for remote loop. */
  if (sc->config.loop_back == CFG_LOOP_RL)
    mii16_set_bits(sc, MII16_HSSI_LB);
  else
    mii16_clr_bits(sc, MII16_HSSI_LB);

  if (sc->status.card_type == CSID_LMC_HSSI)
    {
    /* set TXCLK src */
    if (sc->config.tx_clk_src == CFG_CLKMUX_ST)
      gpio_set_bits(sc, GPIO_HSSI_TXCLK);
    else
      gpio_clr_bits(sc, GPIO_HSSI_TXCLK);
    gpio_make_output(sc, GPIO_HSSI_TXCLK);
    }
  else if (sc->status.card_type == CSID_LMC_HSSIc)
    {  /* cPCI HSSI rev C has extra features */
    /* Set TXCLK source. */
    u_int16_t mii16 = mii_read(sc, 16);
    mii16 &= ~MII16_HSSI_CLKMUX;
    mii16 |= (sc->config.tx_clk_src&3)<<13;
    mii_write(sc, 16, mii16);

    /* cPCI HSSI implements loopback towards the net. */
    if (sc->config.loop_back == CFG_LOOP_LINE)
      mii16_set_bits(sc, MII16_HSSI_LOOP);
    else
      mii16_clr_bits(sc, MII16_HSSI_LOOP);

    /* Set DTE/DCE mode. */
    if (sc->config.dte_dce == CFG_DCE)
      gpio_set_bits(sc, GPIO_HSSI_DCE);
    else
      gpio_clr_bits(sc, GPIO_HSSI_DCE);
    gpio_make_output(sc, GPIO_HSSI_DCE);

    /* Program the synthesized oscillator. */
    synth_write(sc, &sc->config.synth);
    }
  }

static void
hssi_detach(softc_t *sc)
  {
  mii16_clr_bits(sc, MII16_HSSI_TA);
  led_on(sc, MII16_LED_ALL);
  }

/* End HSSI card code */

/* Begin DS3 card code */

static struct card t3_card =
  {
  .ident    = t3_ident,
  .watchdog = t3_watchdog,
  .ioctl    = t3_ioctl,
  .attach   = t3_attach,
  .detach   = t3_detach,
  };

static void
t3_ident(softc_t *sc)
  {
  printf(", TXC03401 rev B");
  }

static void  /* context: softirq */
t3_watchdog(softc_t *sc)
  {
  u_int16_t CV;
  u_int8_t CERR, PERR, MERR, FERR, FEBE;
  u_int8_t ctl1, stat16, feac;
  u_int16_t mii16;

  sc->status.link_state = STATE_UP;

  /* Read the alarm registers. */
  ctl1   = framer_read(sc, T3CSR_CTL1);
  stat16 = framer_read(sc, T3CSR_STAT16);
  mii16  = mii_read(sc, 16);

  /* Always ignore the RTLOC alarm bit. */
  stat16 &= ~STAT16_RTLOC;

  /* Software is alive. */
  led_inv(sc, MII16_DS3_LED_GRN);

  /* Receiving Alarm Indication Signal (AIS). */
  if (stat16 & STAT16_RAIS) /* receiving ais */
    led_on(sc, MII16_DS3_LED_BLU);
  else if (ctl1 & CTL1_TXAIS) /* sending ais */
    led_inv(sc, MII16_DS3_LED_BLU);
  else
    led_off(sc, MII16_DS3_LED_BLU);

  /* Receiving Remote Alarm Indication (RAI). */
  if (stat16 & STAT16_XERR) /* receiving rai */
    led_on(sc, MII16_DS3_LED_YEL);
  else if ((ctl1 & CTL1_XTX) == 0) /* sending rai */
    led_inv(sc, MII16_DS3_LED_YEL);
  else
    led_off(sc, MII16_DS3_LED_YEL);

  /* If certain status bits are set then the link is 'down'. */
  /* The bad bits are: rxlos rxoof rxais rxidl xerr. */
  if (stat16 & ~(STAT16_FEAC | STAT16_SEF))
    sc->status.link_state = STATE_DOWN;

  /* Declare local Red Alarm if the link is down. */
  if (sc->status.link_state == STATE_DOWN)
    led_on(sc, MII16_DS3_LED_RED);
  else if (sc->loop_timer) /* loopback is active */
    led_inv(sc, MII16_DS3_LED_RED);
  else
    led_off(sc, MII16_DS3_LED_RED);

  /* Print latched error bits if they changed. */
  if ((sc->config.debug) && ((stat16 & ~STAT16_FEAC) != sc->last_stat16))
    {
    const char *on = "ON ", *off = "OFF";
    printf("%s: RLOS=%s ROOF=%s RAIS=%s RIDL=%s SEF=%s XERR=%s\n",
     NAME_UNIT,
     (stat16 & STAT16_RLOS) ? on : off,
     (stat16 & STAT16_ROOF) ? on : off,
     (stat16 & STAT16_RAIS) ? on : off,
     (stat16 & STAT16_RIDL) ? on : off,
     (stat16 & STAT16_SEF)  ? on : off,
     (stat16 & STAT16_XERR) ? on : off);
    }

  /* Check and print error counters if non-zero. */
  CV   = framer_read(sc, T3CSR_CVHI)<<8;
  CV  += framer_read(sc, T3CSR_CVLO);
  PERR = framer_read(sc, T3CSR_PERR);
  CERR = framer_read(sc, T3CSR_CERR);
  FERR = framer_read(sc, T3CSR_FERR);
  MERR = framer_read(sc, T3CSR_MERR);
  FEBE = framer_read(sc, T3CSR_FEBE);

  /* CV is invalid during LOS. */
  if (stat16 & STAT16_RLOS) CV = 0;
  /* CERR & FEBE are invalid in M13 mode */
  if (sc->config.format == CFG_FORMAT_T3M13) CERR = FEBE = 0;
  /* FEBE is invalid during AIS. */
  if (stat16 & STAT16_RAIS) FEBE = 0;
  if (sc->config.debug && (CV || PERR || CERR || FERR || MERR || FEBE))
    printf("%s: CV=%u PERR=%u CERR=%u FERR=%u MERR=%u FEBE=%u\n",
     NAME_UNIT, CV,   PERR,   CERR,   FERR,   MERR,   FEBE);

  /* Driver keeps crude link-level error counters (SNMP is better). */
  sc->status.cntrs.lcv_errs  += CV;
  sc->status.cntrs.par_errs  += PERR;
  sc->status.cntrs.cpar_errs += CERR;
  sc->status.cntrs.frm_errs  += FERR;
  sc->status.cntrs.mfrm_errs += MERR;
  sc->status.cntrs.febe_errs += FEBE;

  /* Check for FEAC messages (FEAC not defined in M13 mode). */
  if (FORMAT_T3CPAR && (stat16 & STAT16_FEAC)) do
    {
    feac = framer_read(sc, T3CSR_FEAC_STK);
    if ((feac & FEAC_STK_VALID)==0) break;
    /* Ignore RxFEACs while a far end loopback has been requested. */
    if (sc->status.snmp.t3.line & TLOOP_FAR_LINE) continue;
    switch (feac & FEAC_STK_FEAC)
      {
      case T3BOP_LINE_UP:   break;
      case T3BOP_LINE_DOWN: break;
      case T3BOP_LOOP_DS3:
        {
        if (sc->last_FEAC == T3BOP_LINE_DOWN)
          {
          if (sc->config.debug)
            printf("%s: Received a 'line loopback deactivate' FEAC msg\n", NAME_UNIT);
          mii16_clr_bits(sc, MII16_DS3_LNLBK);
          sc->loop_timer = 0;
	  }
        if (sc->last_FEAC == T3BOP_LINE_UP)
          {
          if (sc->config.debug)
            printf("%s: Received a 'line loopback activate' FEAC msg\n", NAME_UNIT);
          mii16_set_bits(sc, MII16_DS3_LNLBK);
          sc->loop_timer = 300;
	  }
        break;
        }
      case T3BOP_OOF:
        {
        if (sc->config.debug)
          printf("%s: Received a 'far end LOF' FEAC msg\n", NAME_UNIT);
        break;
	}
      case T3BOP_IDLE:
        {
        if (sc->config.debug)
          printf("%s: Received a 'far end IDL' FEAC msg\n", NAME_UNIT);
        break;
	}
      case T3BOP_AIS:
        {
        if (sc->config.debug)
          printf("%s: Received a 'far end AIS' FEAC msg\n", NAME_UNIT);
        break;
	}
      case T3BOP_LOS:
        {
        if (sc->config.debug)
          printf("%s: Received a 'far end LOS' FEAC msg\n", NAME_UNIT);
        break;
	}
      default:
        {
        if (sc->config.debug)
          printf("%s: Received a 'type 0x%02X' FEAC msg\n", NAME_UNIT, feac & FEAC_STK_FEAC);
        break;
	}
      }
    sc->last_FEAC = feac & FEAC_STK_FEAC;
    } while (feac & FEAC_STK_MORE);
  stat16 &= ~STAT16_FEAC;

  /* Send Service-Affecting priority FEAC messages */
  if (((sc->last_stat16 ^ stat16) & 0xF0) && (FORMAT_T3CPAR))
    {
    /* Transmit continuous FEACs */
    framer_write(sc, T3CSR_CTL14,
     framer_read(sc, T3CSR_CTL14) & ~CTL14_FEAC10);
    if      (stat16 & STAT16_RLOS)
      framer_write(sc, T3CSR_TX_FEAC, 0xC0 + T3BOP_LOS);
    else if (stat16 & STAT16_ROOF)
      framer_write(sc, T3CSR_TX_FEAC, 0xC0 + T3BOP_OOF);
    else if (stat16 & STAT16_RAIS)
      framer_write(sc, T3CSR_TX_FEAC, 0xC0 + T3BOP_AIS);
    else if (stat16 & STAT16_RIDL)
      framer_write(sc, T3CSR_TX_FEAC, 0xC0 + T3BOP_IDLE);
    else
      framer_write(sc, T3CSR_TX_FEAC, CTL5_EMODE);
    }

  /* Start sending RAI, Remote Alarm Indication. */
  if ((stat16 & STAT16_ROOF) && !(stat16 & STAT16_RLOS) &&
   !(sc->last_stat16 & STAT16_ROOF))
    framer_write(sc, T3CSR_CTL1, ctl1 &= ~CTL1_XTX);
  /* Stop sending RAI, Remote Alarm Indication. */
  else if (!(stat16 & STAT16_ROOF) && (sc->last_stat16 & STAT16_ROOF))
    framer_write(sc, T3CSR_CTL1, ctl1 |=  CTL1_XTX);

  /* Start sending AIS, Alarm Indication Signal */
  if ((stat16 & STAT16_RLOS) && !(sc->last_stat16 & STAT16_RLOS))
    {
    mii16_set_bits(sc, MII16_DS3_FRAME);
    framer_write(sc, T3CSR_CTL1, ctl1 |  CTL1_TXAIS);
    }
  /* Stop sending AIS, Alarm Indication Signal */
  else if (!(stat16 & STAT16_RLOS) && (sc->last_stat16 & STAT16_RLOS))
    {
    mii16_clr_bits(sc, MII16_DS3_FRAME);
    framer_write(sc, T3CSR_CTL1, ctl1 & ~CTL1_TXAIS);
    }

  /* Time out loopback requests. */
  if (sc->loop_timer)
    if (--sc->loop_timer == 0)
      if (mii16 & MII16_DS3_LNLBK)
        {
        if (sc->config.debug)
          printf("%s: Timeout: Loop Down after 300 seconds\n", NAME_UNIT);
        mii16_clr_bits(sc, MII16_DS3_LNLBK); /* line loopback off */
        }

  /* SNMP error counters */
  sc->status.snmp.t3.lcv  = CV;
  sc->status.snmp.t3.pcv  = PERR;
  sc->status.snmp.t3.ccv  = CERR;
  sc->status.snmp.t3.febe = FEBE;

  /* SNMP Line Status */
  sc->status.snmp.t3.line = 0;
  if (!(ctl1 & CTL1_XTX))      sc->status.snmp.t3.line |= TLINE_TX_RAI;
  if (stat16 & STAT16_XERR)    sc->status.snmp.t3.line |= TLINE_RX_RAI;
  if (ctl1   & CTL1_TXAIS)     sc->status.snmp.t3.line |= TLINE_TX_AIS;
  if (stat16 & STAT16_RAIS)    sc->status.snmp.t3.line |= TLINE_RX_AIS;
  if (stat16 & STAT16_ROOF)    sc->status.snmp.t3.line |= TLINE_LOF;
  if (stat16 & STAT16_RLOS)    sc->status.snmp.t3.line |= TLINE_LOS;
  if (stat16 & STAT16_SEF)     sc->status.snmp.t3.line |= T3LINE_SEF;

  /* SNMP Loopback Status */
  sc->status.snmp.t3.loop &= ~TLOOP_FAR_LINE;
  if (sc->config.loop_back == CFG_LOOP_TULIP)
                               sc->status.snmp.t3.loop |= TLOOP_NEAR_OTHER;
  if (ctl1  & CTL1_3LOOP)      sc->status.snmp.t3.loop |= TLOOP_NEAR_INWARD;
  if (mii16 & MII16_DS3_TRLBK) sc->status.snmp.t3.loop |= TLOOP_NEAR_OTHER;
  if (mii16 & MII16_DS3_LNLBK) sc->status.snmp.t3.loop |= TLOOP_NEAR_LINE;
/*if (ctl12 & CTL12_RTPLOOP)   sc->status.snmp.t3.loop |= TLOOP_NEAR_PAYLOAD; */

  /* Remember this state until next time. */
  sc->last_stat16 = stat16;

  /* If an INWARD loopback is in effect, link status is UP */
  if (sc->config.loop_back != CFG_LOOP_NONE) /* XXX INWARD ONLY */
    sc->status.link_state = STATE_UP;
  }

static void  /* context: process */
t3_send_dbl_feac(softc_t *sc, int feac1, int feac2)
  {
  u_int8_t tx_feac;
  int i;

  /* The FEAC transmitter could be sending a continuous */
  /*  FEAC msg when told to send a double FEAC message. */
  /* So save the current state of the FEAC transmitter. */
  tx_feac = framer_read(sc, T3CSR_TX_FEAC);
  /* Load second FEAC code and stop FEAC transmitter. */
  framer_write(sc, T3CSR_TX_FEAC,  CTL5_EMODE + feac2);
  /* FEAC transmitter sends 10 more FEACs and then stops. */
  SLEEP(20000); /* sending one FEAC takes 1700 uSecs */
  /* Load first FEAC code and start FEAC transmitter. */
  framer_write(sc, T3CSR_DBL_FEAC, CTL13_DFEXEC + feac1);
  /* Wait for double FEAC sequence to complete -- about 70 ms. */
  for (i=0; i<10; i++) /* max delay 100 ms */
    if (framer_read(sc, T3CSR_DBL_FEAC) & CTL13_DFEXEC) SLEEP(10000);
  /* Flush received FEACS; do not respond to our own loop cmd! */
  while (framer_read(sc, T3CSR_FEAC_STK) & FEAC_STK_VALID) DELAY(1);
  /* Restore previous state of the FEAC transmitter. */
  /* If it was sending a continous FEAC, it will resume. */
  framer_write(sc, T3CSR_TX_FEAC, tx_feac);
  }

static int  /* context: process */
t3_ioctl(softc_t *sc, struct ioctl *ioctl)
  {
  int error = 0;

  switch (ioctl->cmd)
    {
    case IOCTL_SNMP_SEND:  /* set opstatus? */
      {
      if (sc->config.format != CFG_FORMAT_T3CPAR)
        error = EINVAL;
      else if (ioctl->data == TSEND_LINE)
        {
        sc->status.snmp.t3.loop |= TLOOP_FAR_LINE;
        t3_send_dbl_feac(sc, T3BOP_LINE_UP, T3BOP_LOOP_DS3);
        }
      else if (ioctl->data == TSEND_RESET)
        {
        t3_send_dbl_feac(sc, T3BOP_LINE_DOWN, T3BOP_LOOP_DS3);
        sc->status.snmp.t3.loop &= ~TLOOP_FAR_LINE;
        }
      else
        error = EINVAL;
      break;
      }
    case IOCTL_SNMP_LOOP:  /* set opstatus = test? */
      {
      if (ioctl->data == CFG_LOOP_NONE)
        {
        mii16_clr_bits(sc, MII16_DS3_FRAME);
        mii16_clr_bits(sc, MII16_DS3_TRLBK);
        mii16_clr_bits(sc, MII16_DS3_LNLBK);
        framer_write(sc, T3CSR_CTL1,
         framer_read(sc, T3CSR_CTL1) & ~CTL1_3LOOP);
        framer_write(sc, T3CSR_CTL12,
         framer_read(sc, T3CSR_CTL12) & ~(CTL12_RTPLOOP | CTL12_RTPLLEN));
	}
      else if (ioctl->data == CFG_LOOP_LINE)
        mii16_set_bits(sc, MII16_DS3_LNLBK);
      else if (ioctl->data == CFG_LOOP_OTHER)
        mii16_set_bits(sc, MII16_DS3_TRLBK);
      else if (ioctl->data == CFG_LOOP_INWARD)
        framer_write(sc, T3CSR_CTL1,
         framer_read(sc, T3CSR_CTL1) | CTL1_3LOOP);
      else if (ioctl->data == CFG_LOOP_DUAL)
        {
        mii16_set_bits(sc, MII16_DS3_LNLBK);
        framer_write(sc, T3CSR_CTL1,
         framer_read(sc, T3CSR_CTL1) | CTL1_3LOOP);
	}
      else if (ioctl->data == CFG_LOOP_PAYLOAD)
        {
        mii16_set_bits(sc, MII16_DS3_FRAME);
        framer_write(sc, T3CSR_CTL12,
         framer_read(sc, T3CSR_CTL12) |  CTL12_RTPLOOP);
        framer_write(sc, T3CSR_CTL12,
         framer_read(sc, T3CSR_CTL12) |  CTL12_RTPLLEN);
        DELAY(25); /* at least two frames (22 uS) */
        framer_write(sc, T3CSR_CTL12,
         framer_read(sc, T3CSR_CTL12) & ~CTL12_RTPLLEN);
	}
      else
        error = EINVAL;
      break;
      }
    case IOCTL_SET_STATUS:
      {
#if 0
      if (ioctl->data)
        framer_write(sc, T3CSR_CTL1,
         framer_read(sc, T3CSR_CTL1) & ~CTL1_TXIDL);
      else /* off */
        framer_write(sc, T3CSR_CTL1,
         framer_read(sc, T3CSR_CTL1) |  CTL1_TXIDL);
#endif
      break;
      }
    default:
      error = EINVAL;
      break;
    }

  return error;
  }

/* Must not sleep. */
static void
t3_attach(softc_t *sc, struct config *config)
  {
  int i;
  u_int8_t ctl1;

  if (config == NULL) /* startup config */
    {
    sc->status.card_type  = CSID_LMC_T3;
    sc->config.crc_len    = CFG_CRC_16;
    sc->config.loop_back  = CFG_LOOP_NONE;
    sc->config.format     = CFG_FORMAT_T3CPAR;
    sc->config.cable_len  = 10; /* meters */
    sc->config.scrambler  = CFG_SCRAM_DL_KEN;
    sc->config.tx_clk_src = CFG_CLKMUX_INT;

    /* Center the VCXO -- get within 20 PPM of 44736000. */
    dac_write(sc, 0x9002); /* set Vref = 2.048 volts */
    dac_write(sc, 2048); /* range is 0..4095 */
    }
  else if (config != &sc->config) /* change config */
    {
    if ((sc->config.crc_len    == config->crc_len)   &&
        (sc->config.loop_back  == config->loop_back) &&
        (sc->config.format     == config->format)    &&
        (sc->config.cable_len  == config->cable_len) &&
        (sc->config.scrambler  == config->scrambler) &&
        (sc->config.tx_clk_src == config->tx_clk_src))
      return; /* nothing changed */
    sc->config.crc_len    = config->crc_len;
    sc->config.loop_back  = config->loop_back;
    sc->config.format     = config->format;
    sc->config.cable_len  = config->cable_len;
    sc->config.scrambler  = config->scrambler;
    sc->config.tx_clk_src = config->tx_clk_src;
    }

  /* Set cable length. */
  if (sc->config.cable_len > 30)
    mii16_clr_bits(sc, MII16_DS3_ZERO);
  else
    mii16_set_bits(sc, MII16_DS3_ZERO);

  /* Set payload scrambler polynomial. */
  if (sc->config.scrambler == CFG_SCRAM_LARS)
    mii16_set_bits(sc, MII16_DS3_POLY);
  else
    mii16_clr_bits(sc, MII16_DS3_POLY);

  /* Set payload scrambler on/off. */
  if (sc->config.scrambler == CFG_SCRAM_OFF)
    mii16_clr_bits(sc, MII16_DS3_SCRAM);
  else
    mii16_set_bits(sc, MII16_DS3_SCRAM);

  /* Set CRC length. */
  if (sc->config.crc_len == CFG_CRC_32)
    mii16_set_bits(sc, MII16_DS3_CRC32);
  else
    mii16_clr_bits(sc, MII16_DS3_CRC32);

  /* Loopback towards host thru the line interface. */
  if (sc->config.loop_back == CFG_LOOP_OTHER)
    mii16_set_bits(sc, MII16_DS3_TRLBK);
  else
    mii16_clr_bits(sc, MII16_DS3_TRLBK);

  /* Loopback towards network thru the line interface. */
  if (sc->config.loop_back == CFG_LOOP_LINE)
    mii16_set_bits(sc, MII16_DS3_LNLBK);
  else if (sc->config.loop_back == CFG_LOOP_DUAL)
    mii16_set_bits(sc, MII16_DS3_LNLBK);
  else
    mii16_clr_bits(sc, MII16_DS3_LNLBK);

  /* Configure T3 framer chip; write EVERY writeable register. */
  ctl1 = CTL1_SER | CTL1_XTX;
  if (sc->config.format    == CFG_FORMAT_T3M13) ctl1 |= CTL1_M13MODE;
  if (sc->config.loop_back == CFG_LOOP_INWARD)  ctl1 |= CTL1_3LOOP;
  if (sc->config.loop_back == CFG_LOOP_DUAL)    ctl1 |= CTL1_3LOOP;
  framer_write(sc, T3CSR_CTL1,     ctl1);
  framer_write(sc, T3CSR_TX_FEAC,  CTL5_EMODE);
  framer_write(sc, T3CSR_CTL8,     CTL8_FBEC);
  framer_write(sc, T3CSR_CTL12,    CTL12_DLCB1 | CTL12_C21 | CTL12_MCB1);
  framer_write(sc, T3CSR_DBL_FEAC, 0);
  framer_write(sc, T3CSR_CTL14,    CTL14_RGCEN | CTL14_TGCEN);
  framer_write(sc, T3CSR_INTEN,    0);
  framer_write(sc, T3CSR_CTL20,    CTL20_CVEN);

  /* Clear error counters and latched error bits */
  /*  that may have happened while initializing. */
  for (i=0; i<21; i++) framer_read(sc, i);
  }

static void
t3_detach(softc_t *sc)
  {
  framer_write(sc, T3CSR_CTL1,
   framer_read(sc, T3CSR_CTL1) |  CTL1_TXIDL);
  led_on(sc, MII16_LED_ALL);
  }

/* End DS3 card code */

/* Begin SSI card code */

static struct card ssi_card =
  {
  .ident    = ssi_ident,
  .watchdog = ssi_watchdog,
  .ioctl    = ssi_ioctl,
  .attach   = ssi_attach,
  .detach   = ssi_detach,
  };

static void
ssi_ident(softc_t *sc)
  {
  printf(", LTC1343/44");
  }

static void  /* context: softirq */
ssi_watchdog(softc_t *sc)
  {
  u_int16_t cable;
  u_int16_t mii16 = mii_read(sc, 16) & MII16_SSI_MODEM;

  sc->status.link_state = STATE_UP;

  /* Software is alive. */
  led_inv(sc, MII16_SSI_LED_UL);

  /* Check the transmit clock. */
  if (sc->status.tx_speed == 0)
    {
    led_on(sc, MII16_SSI_LED_UR);
    sc->status.link_state = STATE_DOWN;
    }
  else
    led_off(sc, MII16_SSI_LED_UR);

  /* Check the external cable. */
  cable = mii_read(sc, 17);
  cable = cable &  MII17_SSI_CABLE_MASK;
  cable = cable >> MII17_SSI_CABLE_SHIFT;
  if (cable == 7)
    {
    led_off(sc, MII16_SSI_LED_LL); /* no cable */
    sc->status.link_state = STATE_DOWN;
    }
  else
    led_on(sc, MII16_SSI_LED_LL);

  /* The unit at the other end of the cable is ready if: */
  /*  DTE mode and DCD pin is asserted */
  /*  DCE mode and DSR pin is asserted */
  if (((sc->config.dte_dce == CFG_DTE) && !(mii16 & MII16_SSI_DCD)) ||
      ((sc->config.dte_dce == CFG_DCE) && !(mii16 & MII16_SSI_DSR)))
    {
    led_off(sc, MII16_SSI_LED_LR);
    sc->status.link_state = STATE_DOWN;
    }
  else
    led_on(sc, MII16_SSI_LED_LR);

  if (sc->config.debug && (cable != sc->status.cable_type))
    printf("%s: SSI cable type changed to '%s'\n",
     NAME_UNIT, ssi_cables[cable]);
  sc->status.cable_type = cable;

  /* Print the modem control signals if they changed. */
  if ((sc->config.debug) && (mii16 != sc->last_mii16))
    {
    const char *on = "ON ", *off = "OFF";
    printf("%s: DTR=%s DSR=%s RTS=%s CTS=%s DCD=%s RI=%s LL=%s RL=%s TM=%s\n",
     NAME_UNIT,
     (mii16 & MII16_SSI_DTR) ? on : off,
     (mii16 & MII16_SSI_DSR) ? on : off,
     (mii16 & MII16_SSI_RTS) ? on : off,
     (mii16 & MII16_SSI_CTS) ? on : off,
     (mii16 & MII16_SSI_DCD) ? on : off,
     (mii16 & MII16_SSI_RI)  ? on : off,
     (mii16 & MII16_SSI_LL)  ? on : off,
     (mii16 & MII16_SSI_RL)  ? on : off,
     (mii16 & MII16_SSI_TM)  ? on : off);
    }

  /* SNMP one-second report */
  sc->status.snmp.ssi.sigs = mii16 & MII16_SSI_MODEM;

  /* Remember this state until next time. */
  sc->last_mii16 = mii16;

  /* If a loop back is in effect, link status is UP */
  if (sc->config.loop_back != CFG_LOOP_NONE)
    sc->status.link_state = STATE_UP;
  }

static int  /* context: process */
ssi_ioctl(softc_t *sc, struct ioctl *ioctl)
  {
  int error = 0;

  if (ioctl->cmd == IOCTL_SNMP_SIGS)
    {
    u_int16_t mii16 = mii_read(sc, 16);
    mii16 &= ~MII16_SSI_MODEM;
    mii16 |= (MII16_SSI_MODEM & ioctl->data);
    mii_write(sc, 16, mii16);
    }
  else if (ioctl->cmd == IOCTL_SET_STATUS)
    {
    if (ioctl->data)
      mii16_set_bits(sc, (MII16_SSI_DTR | MII16_SSI_RTS | MII16_SSI_DCD));
    else
      mii16_clr_bits(sc, (MII16_SSI_DTR | MII16_SSI_RTS | MII16_SSI_DCD));
    }
  else
    error = EINVAL;

  return error;
  }

/* Must not sleep. */
static void
ssi_attach(softc_t *sc, struct config *config)
  {
  if (config == NULL) /* startup config */
    {
    sc->status.card_type  = CSID_LMC_SSI;
    sc->config.crc_len    = CFG_CRC_16;
    sc->config.loop_back  = CFG_LOOP_NONE;
    sc->config.tx_clk_src = CFG_CLKMUX_ST;
    sc->config.dte_dce    = CFG_DTE;
    sc->config.synth.n    = 51; /* 1.536 MHz */
    sc->config.synth.m    = 83;
    sc->config.synth.v    =  1;
    sc->config.synth.x    =  1;
    sc->config.synth.r    =  1;
    sc->config.synth.prescale = 4;
    }
  else if (config != &sc->config) /* change config */
    {
    u_int32_t *old_synth = (u_int32_t *)&sc->config.synth;
    u_int32_t *new_synth = (u_int32_t *)&config->synth;
    if ((sc->config.crc_len    == config->crc_len)    &&
        (sc->config.loop_back  == config->loop_back)  &&
        (sc->config.tx_clk_src == config->tx_clk_src) &&
        (sc->config.dte_dce    == config->dte_dce)    &&
        (*old_synth            == *new_synth))
      return; /* nothing changed */
    sc->config.crc_len    = config->crc_len;
    sc->config.loop_back  = config->loop_back;
    sc->config.tx_clk_src = config->tx_clk_src;
    sc->config.dte_dce    = config->dte_dce;
    *old_synth            = *new_synth;
    }

  /* Disable the TX clock driver while programming the oscillator. */
  gpio_clr_bits(sc, GPIO_SSI_DCE);
  gpio_make_output(sc, GPIO_SSI_DCE);

  /* Program the synthesized oscillator. */
  synth_write(sc, &sc->config.synth);

  /* Set DTE/DCE mode. */
  /* If DTE mode then DCD & TXC are received. */
  /* If DCE mode then DCD & TXC are driven. */
  /* Boards with MII rev=4.0 do not drive DCD. */
  if (sc->config.dte_dce == CFG_DCE)
    gpio_set_bits(sc, GPIO_SSI_DCE);
  else
    gpio_clr_bits(sc, GPIO_SSI_DCE);
  gpio_make_output(sc, GPIO_SSI_DCE);

  /* Set CRC length. */
  if (sc->config.crc_len == CFG_CRC_32)
    mii16_set_bits(sc, MII16_SSI_CRC32);
  else
    mii16_clr_bits(sc, MII16_SSI_CRC32);

  /* Loop towards host thru cable drivers and receivers. */
  /* Asserts DCD at the far end of a null modem cable. */
  if (sc->config.loop_back == CFG_LOOP_PINS)
    mii16_set_bits(sc, MII16_SSI_LOOP);
  else
    mii16_clr_bits(sc, MII16_SSI_LOOP);

  /* Assert pin LL in modem conn: ask modem for local loop. */
  /* Asserts TM at the far end of a null modem cable. */
  if (sc->config.loop_back == CFG_LOOP_LL)
    mii16_set_bits(sc, MII16_SSI_LL);
  else
    mii16_clr_bits(sc, MII16_SSI_LL);

  /* Assert pin RL in modem conn: ask modem for remote loop. */
  if (sc->config.loop_back == CFG_LOOP_RL)
    mii16_set_bits(sc, MII16_SSI_RL);
  else
    mii16_clr_bits(sc, MII16_SSI_RL);
  }

static void
ssi_detach(softc_t *sc)
  {
  mii16_clr_bits(sc, (MII16_SSI_DTR | MII16_SSI_RTS | MII16_SSI_DCD));
  led_on(sc, MII16_LED_ALL);
  }

/* End SSI card code */

/* Begin T1E1 card code */

static struct card t1_card =
  {
  .ident    = t1_ident,
  .watchdog = t1_watchdog,
  .ioctl    = t1_ioctl,
  .attach   = t1_attach,
  .detach   = t1_detach,
  };

static void
t1_ident(softc_t *sc)
  {
  printf(", Bt837%x rev %x",
   framer_read(sc, Bt8370_DID)>>4,
   framer_read(sc, Bt8370_DID)&0x0F);
  }

static void  /* context: softirq */
t1_watchdog(softc_t *sc)
  {
  u_int16_t LCV = 0, FERR = 0, CRC = 0, FEBE = 0;
  u_int8_t alm1, alm3, loop, isr0;
  int i;

  sc->status.link_state = STATE_UP;

  /* Read the alarm registers */
  alm1 = framer_read(sc, Bt8370_ALM1);
  alm3 = framer_read(sc, Bt8370_ALM3);
  loop = framer_read(sc, Bt8370_LOOP);
  isr0 = framer_read(sc, Bt8370_ISR0);

  /* Always ignore the SIGFRZ alarm bit, */
  alm1 &= ~ALM1_SIGFRZ;
  if (FORMAT_T1ANY)  /* ignore RYEL in T1 modes */
    alm1 &= ~ALM1_RYEL;
  else if (FORMAT_E1NONE) /* ignore all alarms except LOS */
    alm1 &= ALM1_RLOS;

  /* Software is alive. */
  led_inv(sc, MII16_T1_LED_GRN);

  /* Receiving Alarm Indication Signal (AIS). */
  if      (alm1 & ALM1_RAIS) /* receiving ais */
    led_on(sc, MII16_T1_LED_BLU);
  else if (alm1 & ALM1_RLOS) /* sending ais */
    led_inv(sc, MII16_T1_LED_BLU);
  else
    led_off(sc, MII16_T1_LED_BLU);

  /* Receiving Remote Alarm Indication (RAI). */
  if (alm1 & (ALM1_RMYEL | ALM1_RYEL)) /* receiving rai */
    led_on(sc, MII16_T1_LED_YEL);
  else if (alm1 & ALM1_RLOF) /* sending rai */
    led_inv(sc, MII16_T1_LED_YEL);
  else
    led_off(sc, MII16_T1_LED_YEL);

  /* If any alarm bits are set then the link is 'down'. */
  /* The bad bits are: rmyel ryel rais ralos rlos rlof. */
  /* Some alarm bits have been masked by this point. */
  if (alm1) sc->status.link_state = STATE_DOWN;

  /* Declare local Red Alarm if the link is down. */
  if (sc->status.link_state == STATE_DOWN)
    led_on(sc, MII16_T1_LED_RED);
  else if (sc->loop_timer) /* loopback is active */
    led_inv(sc, MII16_T1_LED_RED);
  else
    led_off(sc, MII16_T1_LED_RED);

  /* Print latched error bits if they changed. */
  if ((sc->config.debug) && (alm1 != sc->last_alm1))
    {
    const char *on = "ON ", *off = "OFF";
    printf("%s: RLOF=%s RLOS=%s RALOS=%s RAIS=%s RYEL=%s RMYEL=%s\n",
     NAME_UNIT,
     (alm1 & ALM1_RLOF)  ? on : off,
     (alm1 & ALM1_RLOS)  ? on : off,
     (alm1 & ALM1_RALOS) ? on : off,
     (alm1 & ALM1_RAIS)  ? on : off,
     (alm1 & ALM1_RYEL)  ? on : off,
     (alm1 & ALM1_RMYEL) ? on : off);
    }

  /* Check and print error counters if non-zero. */
  LCV = framer_read(sc, Bt8370_LCV_LO)  +
        (framer_read(sc, Bt8370_LCV_HI)<<8);
  if (!FORMAT_E1NONE)
    FERR = framer_read(sc, Bt8370_FERR_LO) +
          (framer_read(sc, Bt8370_FERR_HI)<<8);
  if (FORMAT_E1CRC || FORMAT_T1ESF)
    CRC  = framer_read(sc, Bt8370_CRC_LO)  +
          (framer_read(sc, Bt8370_CRC_HI)<<8);
  if (FORMAT_E1CRC)
    FEBE = framer_read(sc, Bt8370_FEBE_LO) +
          (framer_read(sc, Bt8370_FEBE_HI)<<8);
  /* Only LCV is valid if Out-Of-Frame */
  if (FORMAT_E1NONE) FERR = CRC = FEBE = 0;
  if ((sc->config.debug) && (LCV || FERR || CRC || FEBE))
    printf("%s: LCV=%u FERR=%u CRC=%u FEBE=%u\n",
     NAME_UNIT, LCV,   FERR,   CRC,   FEBE);

  /* Driver keeps crude link-level error counters (SNMP is better). */
  sc->status.cntrs.lcv_errs  += LCV;
  sc->status.cntrs.frm_errs  += FERR;
  sc->status.cntrs.crc_errs  += CRC;
  sc->status.cntrs.febe_errs += FEBE;

  /* Check for BOP messages in the ESF Facility Data Link. */
  if ((FORMAT_T1ESF) && (framer_read(sc, Bt8370_ISR1) & 0x80))
    {
    u_int8_t bop_code = framer_read(sc, Bt8370_RBOP) & 0x3F;

    switch (bop_code)
      {
      case T1BOP_OOF:
        {
        if ((sc->config.debug) && !(sc->last_alm1 & ALM1_RMYEL))
          printf("%s: Receiving a 'yellow alarm' BOP msg\n", NAME_UNIT);
        break;
        }
      case T1BOP_LINE_UP:
        {
        if (sc->config.debug)
          printf("%s: Received a 'line loopback activate' BOP msg\n", NAME_UNIT);
        framer_write(sc, Bt8370_LOOP, LOOP_LINE);
        sc->loop_timer = 305;
        break;
        }
      case T1BOP_LINE_DOWN:
        {
        if (sc->config.debug)
          printf("%s: Received a 'line loopback deactivate' BOP msg\n", NAME_UNIT);
        framer_write(sc, Bt8370_LOOP,
         framer_read(sc, Bt8370_LOOP) & ~LOOP_LINE);
        sc->loop_timer = 0;
        break;
        }
      case T1BOP_PAY_UP:
        {
        if (sc->config.debug)
          printf("%s: Received a 'payload loopback activate' BOP msg\n", NAME_UNIT);
        framer_write(sc, Bt8370_LOOP, LOOP_PAYLOAD);
        sc->loop_timer = 305;
        break;
        }
      case T1BOP_PAY_DOWN:
        {
        if (sc->config.debug)
          printf("%s: Received a 'payload loopback deactivate' BOP msg\n", NAME_UNIT);
        framer_write(sc, Bt8370_LOOP,
         framer_read(sc, Bt8370_LOOP) & ~LOOP_PAYLOAD);
        sc->loop_timer = 0;
        break;
        }
      default:
        {
        if (sc->config.debug)
          printf("%s: Received a type 0x%02X BOP msg\n", NAME_UNIT, bop_code);
        break;
        }
      }
    }

  /* Check for HDLC pkts in the ESF Facility Data Link. */
  if ((FORMAT_T1ESF) && (framer_read(sc, Bt8370_ISR2) & 0x70))
    {
    /* while (not fifo-empty && not start-of-msg) flush fifo */
    while ((framer_read(sc, Bt8370_RDL1_STAT) & 0x0C)==0)
      framer_read(sc, Bt8370_RDL1);
    /* If (not fifo-empty), then begin processing fifo contents. */
    if ((framer_read(sc, Bt8370_RDL1_STAT) & 0x0C) == 0x08)
      {
      u_int8_t msg[64];
      u_int8_t stat = framer_read(sc, Bt8370_RDL1);
      sc->status.cntrs.fdl_pkts++;
      for (i=0; i<(stat & 0x3F); i++)
        msg[i] = framer_read(sc, Bt8370_RDL1);
      /* Is this FDL message a T1.403 performance report? */
      if (((stat & 0x3F)==11) &&
          ((msg[0]==0x38) || (msg[0]==0x3A)) &&
           (msg[1]==1)   &&  (msg[2]==3))
        /* Copy 4 PRs from FDL pkt to SNMP struct. */
        memcpy(sc->status.snmp.t1.prm, msg+3, 8);
      }
    }

  /* Check for inband loop up/down commands. */
  if (FORMAT_T1ANY)
    {
    u_int8_t isr6   = framer_read(sc, Bt8370_ISR6);
    u_int8_t alarm2 = framer_read(sc, Bt8370_ALM2);
    u_int8_t tlb    = framer_read(sc, Bt8370_TLB);

    /* Inband Code == Loop Up && On Transition && Inband Tx Inactive */
    if ((isr6 & 0x40) && (alarm2 & 0x40) && !(tlb & 1))
      { /* CSU loop up is 10000 10000 ... */
      if (sc->config.debug)
        printf("%s: Received a 'CSU Loop Up' inband msg\n", NAME_UNIT);
      framer_write(sc, Bt8370_LOOP, LOOP_LINE); /* Loop up */
      sc->loop_timer = 305;
      }
    /* Inband Code == Loop Down && On Transition && Inband Tx Inactive */
    if ((isr6 & 0x80) && (alarm2 & 0x80) && !(tlb & 1))
      { /* CSU loop down is 100 100 100 ... */
      if (sc->config.debug)
        printf("%s: Received a 'CSU Loop Down' inband msg\n", NAME_UNIT);
      framer_write(sc, Bt8370_LOOP,
       framer_read(sc, Bt8370_LOOP) & ~LOOP_LINE); /* loop down */
      sc->loop_timer = 0;
      }
    }

  /* Manually send Yellow Alarm BOP msgs. */
  if (FORMAT_T1ESF)
    {
    u_int8_t isr7 = framer_read(sc, Bt8370_ISR7);

    if ((isr7 & 0x02) && (alm1 & 0x02)) /* RLOF on-transition */
      { /* Start sending continuous Yellow Alarm BOP messages. */
      framer_write(sc, Bt8370_BOP,  RBOP_25 | TBOP_CONT);
      framer_write(sc, Bt8370_TBOP, 0x00); /* send BOP; order matters */
      }
    else if ((isr7 & 0x02) && !(alm1 & 0x02)) /* RLOF off-transition */
      { /* Stop sending continuous Yellow Alarm BOP messages. */
      framer_write(sc, Bt8370_BOP,  RBOP_25 | TBOP_OFF);
      }
    }

  /* Time out loopback requests. */
  if (sc->loop_timer)
    if (--sc->loop_timer == 0)
      if (loop)
        {
        if (sc->config.debug)
          printf("%s: Timeout: Loop Down after 300 seconds\n", NAME_UNIT);
        framer_write(sc, Bt8370_LOOP, loop & ~(LOOP_PAYLOAD | LOOP_LINE));
        }

  /* RX Test Pattern status */
  if ((sc->config.debug) && (isr0 & 0x10))
    printf("%s: RX Test Pattern Sync\n", NAME_UNIT);

  /* SNMP Error Counters */
  sc->status.snmp.t1.lcv  = LCV;
  sc->status.snmp.t1.fe   = FERR;
  sc->status.snmp.t1.crc  = CRC;
  sc->status.snmp.t1.febe = FEBE;

  /* SNMP Line Status */
  sc->status.snmp.t1.line = 0;
  if  (alm1 & ALM1_RMYEL)  sc->status.snmp.t1.line |= TLINE_RX_RAI;
  if  (alm1 & ALM1_RYEL)   sc->status.snmp.t1.line |= TLINE_RX_RAI;
  if  (alm1 & ALM1_RLOF)   sc->status.snmp.t1.line |= TLINE_TX_RAI;
  if  (alm1 & ALM1_RAIS)   sc->status.snmp.t1.line |= TLINE_RX_AIS;
  if  (alm1 & ALM1_RLOS)   sc->status.snmp.t1.line |= TLINE_TX_AIS;
  if  (alm1 & ALM1_RLOF)   sc->status.snmp.t1.line |= TLINE_LOF;
  if  (alm1 & ALM1_RLOS)   sc->status.snmp.t1.line |= TLINE_LOS;
  if  (alm3 & ALM3_RMAIS)  sc->status.snmp.t1.line |= T1LINE_RX_TS16_AIS;
  if  (alm3 & ALM3_SRED)   sc->status.snmp.t1.line |= T1LINE_TX_TS16_LOMF;
  if  (alm3 & ALM3_SEF)    sc->status.snmp.t1.line |= T1LINE_SEF;
  if  (isr0 & 0x10)        sc->status.snmp.t1.line |= T1LINE_RX_TEST;
  if ((alm1 & ALM1_RMYEL) && (FORMAT_E1CAS))
                           sc->status.snmp.t1.line |= T1LINE_RX_TS16_LOMF;

  /* SNMP Loopback Status */
  sc->status.snmp.t1.loop &= ~(TLOOP_FAR_LINE | TLOOP_FAR_PAYLOAD);
  if (sc->config.loop_back == CFG_LOOP_TULIP)
                           sc->status.snmp.t1.loop |= TLOOP_NEAR_OTHER;
  if (loop & LOOP_PAYLOAD) sc->status.snmp.t1.loop |= TLOOP_NEAR_PAYLOAD;
  if (loop & LOOP_LINE)    sc->status.snmp.t1.loop |= TLOOP_NEAR_LINE;
  if (loop & LOOP_ANALOG)  sc->status.snmp.t1.loop |= TLOOP_NEAR_OTHER;
  if (loop & LOOP_FRAMER)  sc->status.snmp.t1.loop |= TLOOP_NEAR_INWARD;

  /* Remember this state until next time. */
  sc->last_alm1 = alm1;

  /* If an INWARD loopback is in effect, link status is UP */
  if (sc->config.loop_back != CFG_LOOP_NONE) /* XXX INWARD ONLY */
    sc->status.link_state = STATE_UP;
  }

static void  /* context: process */
t1_send_bop(softc_t *sc, int bop_code)
  {
  u_int8_t bop;
  int i;

  /* The BOP transmitter could be sending a continuous */
  /*  BOP msg when told to send this BOP_25 message. */
  /* So save and restore the state of the BOP machine. */
  bop = framer_read(sc, Bt8370_BOP);
  framer_write(sc, Bt8370_BOP, RBOP_OFF | TBOP_OFF);
  for (i=0; i<40; i++) /* max delay 400 ms. */
    if (framer_read(sc, Bt8370_BOP_STAT) & 0x80) SLEEP(10000);
  /* send 25 repetitions of bop_code */
  framer_write(sc, Bt8370_BOP, RBOP_OFF | TBOP_25);
  framer_write(sc, Bt8370_TBOP, bop_code); /* order matters */
  /* wait for tx to stop */
  for (i=0; i<40; i++) /* max delay 400 ms. */
    if (framer_read(sc, Bt8370_BOP_STAT) & 0x80) SLEEP(10000);
  /* Restore previous state of the BOP machine. */
  framer_write(sc, Bt8370_BOP, bop);
  }

static int  /* context: process */
t1_ioctl(softc_t *sc, struct ioctl *ioctl)
  {
  int error = 0;

  switch (ioctl->cmd)
    {
    case IOCTL_SNMP_SEND:  /* set opstatus? */
      {
      switch (ioctl->data)
        {
        case TSEND_NORMAL:
          {
          framer_write(sc, Bt8370_TPATT, 0x00); /* tx pattern generator off */
          framer_write(sc, Bt8370_RPATT, 0x00); /* rx pattern detector off */
          framer_write(sc, Bt8370_TLB,   0x00); /* tx inband generator off */
          break;
	  }
        case TSEND_LINE:
          {
          if (FORMAT_T1ESF)
            t1_send_bop(sc, T1BOP_LINE_UP);
          else if (FORMAT_T1SF)
            {
            framer_write(sc, Bt8370_LBP, 0x08); /* 10000 10000 ... */
            framer_write(sc, Bt8370_TLB, 0x05); /* 5 bits, framed, start */
	    }
          sc->status.snmp.t1.loop |= TLOOP_FAR_LINE;
          break;
	  }
        case TSEND_PAYLOAD:
          {
          t1_send_bop(sc, T1BOP_PAY_UP);
          sc->status.snmp.t1.loop |= TLOOP_FAR_PAYLOAD;
          break;
	  }
        case TSEND_RESET:
          {
          if (sc->status.snmp.t1.loop == TLOOP_FAR_LINE)
            {
            if (FORMAT_T1ESF)
              t1_send_bop(sc, T1BOP_LINE_DOWN);
            else if (FORMAT_T1SF)
              {
              framer_write(sc, Bt8370_LBP, 0x24); /* 100100 100100 ... */
              framer_write(sc, Bt8370_TLB, 0x09); /* 6 bits, framed, start */
	      }
            sc->status.snmp.t1.loop &= ~TLOOP_FAR_LINE;
	    }
          if (sc->status.snmp.t1.loop == TLOOP_FAR_PAYLOAD)
            {
            t1_send_bop(sc, T1BOP_PAY_DOWN);
            sc->status.snmp.t1.loop &= ~TLOOP_FAR_PAYLOAD;
	    }
          break;
	  }
        case TSEND_QRS:
          {
          framer_write(sc, Bt8370_TPATT, 0x1E); /* framed QRSS */
          break;
	  }
        default:
          {
          error = EINVAL;
          break;
	  }
	}
      break;
      }
    case IOCTL_SNMP_LOOP:  /* set opstatus = test? */
      {
      u_int8_t new_loop = 0;

      if      (ioctl->data == CFG_LOOP_NONE)
        new_loop = 0;
      else if (ioctl->data == CFG_LOOP_PAYLOAD)
        new_loop = LOOP_PAYLOAD;
      else if (ioctl->data == CFG_LOOP_LINE)
        new_loop = LOOP_LINE;
      else if (ioctl->data == CFG_LOOP_OTHER)
        new_loop = LOOP_ANALOG;
      else if (ioctl->data == CFG_LOOP_INWARD)
        new_loop = LOOP_FRAMER;
      else if (ioctl->data == CFG_LOOP_DUAL)
        new_loop = LOOP_DUAL;
      else
        error = EINVAL;
      if (!error)
        {
        framer_write(sc, Bt8370_LOOP, new_loop);
        sc->config.loop_back = ioctl->data;
	}
      break;
      }
    case IOCTL_SET_STATUS:
      {
#if 0
      if (ioctl->data)
        mii16_set_bits(sc, MII16_T1_XOE);
      else
        mii16_clr_bits(sc, MII16_T1_XOE);
#endif
      break;
      }
    default:
      error = EINVAL;
      break;
    }

  return error;
  }

/* Must not sleep. */
static void
t1_attach(softc_t *sc, struct config *config)
  {
  int i;
  u_int8_t pulse, lbo, gain;

  if (config == NULL) /* startup config */
    {
    /* Disable transmitter output drivers. */
    mii16_clr_bits(sc, MII16_T1_XOE);
    /* Bt8370 occasionally powers up in a loopback mode. */
    /* Data sheet says zero LOOP reg and do a sw-reset. */
    framer_write(sc, Bt8370_LOOP, 0x00); /* no loopback */
    framer_write(sc, Bt8370_CR0,  0x80); /* sw-reset */
    for (i=0; i<10; i++) /* wait for sw-reset to clear; max 10 ms */
      if (framer_read(sc, Bt8370_CR0) & 0x80) DELAY(1000);

    sc->status.card_type   = CSID_LMC_T1E1;
    sc->config.crc_len     = CFG_CRC_16;
    sc->config.loop_back   = CFG_LOOP_NONE;
    sc->config.tx_clk_src  = CFG_CLKMUX_RT; /* loop timed */
#if 1 /* USA */ /* decide using time zone? */
    sc->config.format      = CFG_FORMAT_T1ESF;
#else /* REST OF PLANET */
    sc->config.format      = CFG_FORMAT_E1FASCRC;
#endif
    sc->config.time_slots  = 0xFFFFFFFF;
    sc->config.cable_len   = 10;
    sc->config.tx_pulse    = CFG_PULSE_AUTO;
    sc->config.rx_gain_max = CFG_GAIN_AUTO;
    sc->config.tx_lbo      = CFG_LBO_AUTO;
    }
  else if (config != &sc->config) /* change config */
    {
    if ((sc->config.crc_len     == config->crc_len)     &&
        (sc->config.loop_back   == config->loop_back)   &&
        (sc->config.tx_clk_src  == config->tx_clk_src)  &&
        (sc->config.format      == config->format)      &&
        (sc->config.time_slots  == config->time_slots)  &&
        (sc->config.cable_len   == config->cable_len)   &&
        (sc->config.tx_pulse    == config->tx_pulse)    &&
        (sc->config.rx_gain_max == config->rx_gain_max) &&
        (sc->config.tx_lbo      == config->tx_lbo))
      return; /* nothing changed */
    sc->config.crc_len     = config->crc_len;
    sc->config.loop_back   = config->loop_back;
    sc->config.tx_clk_src  = config->tx_clk_src;
    sc->config.format      = config->format;
    sc->config.cable_len   = config->cable_len;
    sc->config.time_slots  = config->time_slots;
    sc->config.tx_pulse    = config->tx_pulse;
    sc->config.rx_gain_max = config->rx_gain_max;
    sc->config.tx_lbo      = config->tx_lbo;
    }

  /* Set CRC length. */
  if (sc->config.crc_len == CFG_CRC_32)
    mii16_set_bits(sc, MII16_T1_CRC32);
  else
    mii16_clr_bits(sc, MII16_T1_CRC32);

  /* Invert HDLC payload data in SF/AMI mode. */
  /* HDLC stuff bits satisfy T1 pulse density. */
  if (FORMAT_T1SF)
    mii16_set_bits(sc, MII16_T1_INVERT);
  else
    mii16_clr_bits(sc, MII16_T1_INVERT);

  /* Set the transmitter output impedance. */
  if (FORMAT_E1ANY) mii16_set_bits(sc, MII16_T1_Z);

  /* 001:CR0 -- Control Register 0 - T1/E1 and frame format */
  framer_write(sc, Bt8370_CR0, sc->config.format);

  /* 002:JAT_CR -- Jitter Attenuator Control Register */
  if (sc->config.tx_clk_src == CFG_CLKMUX_RT) /* loop timing */
    framer_write(sc, Bt8370_JAT_CR, 0xA3); /* JAT in RX path */
  else
    { /* 64-bit elastic store; free-running JCLK and CLADO */
    framer_write(sc, Bt8370_JAT_CR, 0x4B); /* assert jcenter */
    framer_write(sc, Bt8370_JAT_CR, 0x43); /* release jcenter */
    }

  /* 00C-013:IERn -- Interrupt Enable Registers */
  for (i=Bt8370_IER7; i<=Bt8370_IER0; i++)
    framer_write(sc, i, 0); /* no interrupts; polled */

  /* 014:LOOP -- loopbacks */
  if      (sc->config.loop_back == CFG_LOOP_PAYLOAD)
    framer_write(sc, Bt8370_LOOP, LOOP_PAYLOAD);
  else if (sc->config.loop_back == CFG_LOOP_LINE)
    framer_write(sc, Bt8370_LOOP, LOOP_LINE);
  else if (sc->config.loop_back == CFG_LOOP_OTHER)
    framer_write(sc, Bt8370_LOOP, LOOP_ANALOG);
  else if (sc->config.loop_back == CFG_LOOP_INWARD)
    framer_write(sc, Bt8370_LOOP, LOOP_FRAMER);
  else if (sc->config.loop_back == CFG_LOOP_DUAL)
    framer_write(sc, Bt8370_LOOP, LOOP_DUAL);
  else
    framer_write(sc, Bt8370_LOOP, 0x00); /* no loopback */

  /* 015:DL3_TS -- Data Link 3 */
  framer_write(sc, Bt8370_DL3_TS, 0x00); /* disabled */

  /* 018:PIO -- Programmable I/O */
  framer_write(sc, Bt8370_PIO, 0xFF); /* all pins are outputs */

  /* 019:POE -- Programmable Output Enable */
  framer_write(sc, Bt8370_POE, 0x00); /* all outputs are enabled */

  /* 01A;CMUX -- Clock Input Mux */
  if (sc->config.tx_clk_src == CFG_CLKMUX_EXT)
    framer_write(sc, Bt8370_CMUX, 0x0C); /* external timing */
  else
    framer_write(sc, Bt8370_CMUX, 0x0F); /* internal timing */

  /* 020:LIU_CR -- Line Interface Unit Config Register */
  framer_write(sc, Bt8370_LIU_CR, 0xC1); /* reset LIU, squelch */

  /* 022:RLIU_CR -- RX Line Interface Unit Config Reg */
  /* Errata sheet says do not use freeze-short, but we do anyway! */
  framer_write(sc, Bt8370_RLIU_CR, 0xB1); /* AGC=2048, Long Eye */

  /* Select Rx sensitivity based on cable length. */
  if ((gain = sc->config.rx_gain_max) == CFG_GAIN_AUTO)
    {
    if      (sc->config.cable_len > 2000)
      gain = CFG_GAIN_EXTEND;
    else if (sc->config.cable_len > 1000)
      gain = CFG_GAIN_LONG;
    else if (sc->config.cable_len > 100)
      gain = CFG_GAIN_MEDIUM;
    else
      gain = CFG_GAIN_SHORT;
    }

  /* 024:VGA_MAX -- Variable Gain Amplifier Max gain */
  framer_write(sc, Bt8370_VGA_MAX, gain);

  /* 028:PRE_EQ -- Pre Equalizer */
  if (gain == CFG_GAIN_EXTEND)
    framer_write(sc, Bt8370_PRE_EQ, 0xE6);  /* ON; thresh 6 */
  else
    framer_write(sc, Bt8370_PRE_EQ, 0xA6);  /* OFF; thresh 6 */

  /* 038-03C:GAINn -- RX Equalizer gain thresholds */
  framer_write(sc, Bt8370_GAIN0, 0x24);
  framer_write(sc, Bt8370_GAIN1, 0x28);
  framer_write(sc, Bt8370_GAIN2, 0x2C);
  framer_write(sc, Bt8370_GAIN3, 0x30);
  framer_write(sc, Bt8370_GAIN4, 0x34);

  /* 040:RCR0 -- Receiver Control Register 0 */
  if      (FORMAT_T1ESF)
    framer_write(sc, Bt8370_RCR0, 0x05); /* B8ZS, 2/5 FErrs */
  else if (FORMAT_T1SF)
    framer_write(sc, Bt8370_RCR0, 0x84); /* AMI,  2/5 FErrs */
  else if (FORMAT_E1NONE)
    framer_write(sc, Bt8370_RCR0, 0x41); /* HDB3, rabort */
  else if (FORMAT_E1CRC)
    framer_write(sc, Bt8370_RCR0, 0x09); /* HDB3, 3 FErrs or 915 CErrs */
  else  /* E1 no CRC */
    framer_write(sc, Bt8370_RCR0, 0x19); /* HDB3, 3 FErrs */

  /* 041:RPATT -- Receive Test Pattern configuration */
  framer_write(sc, Bt8370_RPATT, 0x3E); /* looking for framed QRSS */

  /* 042:RLB -- Receive Loop Back code detector config */
  framer_write(sc, Bt8370_RLB, 0x09); /* 6 bits down; 5 bits up */

  /* 043:LBA -- Loop Back Activate code */
  framer_write(sc, Bt8370_LBA, 0x08); /* 10000 10000 10000 ... */

  /* 044:LBD -- Loop Back Deactivate code */
  framer_write(sc, Bt8370_LBD, 0x24); /* 100100 100100 100100 ... */

  /* 045:RALM -- Receive Alarm signal configuration */
  framer_write(sc, Bt8370_RALM, 0x0C); /* yel_intg rlof_intg */

  /* 046:LATCH -- Alarm/Error/Counter Latch register */
  framer_write(sc, Bt8370_LATCH, 0x1F); /* stop_cnt latch_{cnt,err,alm} */

  /* Select Pulse Shape based on cable length (T1 only). */
  if ((pulse = sc->config.tx_pulse) == CFG_PULSE_AUTO)
    {
    if (FORMAT_T1ANY)
      {
      if      (sc->config.cable_len > 200)
        pulse = CFG_PULSE_T1CSU;
      else if (sc->config.cable_len > 160)
        pulse = CFG_PULSE_T1DSX4;
      else if (sc->config.cable_len > 120)
        pulse = CFG_PULSE_T1DSX3;
      else if (sc->config.cable_len > 80)
        pulse = CFG_PULSE_T1DSX2;
      else if (sc->config.cable_len > 40)
        pulse = CFG_PULSE_T1DSX1;
      else
        pulse = CFG_PULSE_T1DSX0;
      }
    else
      pulse = CFG_PULSE_E1TWIST;
    }

  /* Select Line Build Out based on cable length (T1CSU only). */
  if ((lbo = sc->config.tx_lbo) == CFG_LBO_AUTO)
    {
    if (pulse == CFG_PULSE_T1CSU)
      {
      if      (sc->config.cable_len > 1500)
        lbo = CFG_LBO_0DB;
      else if (sc->config.cable_len > 1000)
        lbo = CFG_LBO_7DB;
      else if (sc->config.cable_len >  500)
        lbo = CFG_LBO_15DB;
      else
        lbo = CFG_LBO_22DB;
      }
    else
      lbo = 0;
    }

  /* 068:TLIU_CR -- Transmit LIU Control Register */
  framer_write(sc, Bt8370_TLIU_CR, (0x40 | (lbo & 0x30) | (pulse & 0x0E)));

  /* 070:TCR0 -- Transmit Framer Configuration */
  framer_write(sc, Bt8370_TCR0, sc->config.format>>1);

  /* 071:TCR1 -- Transmitter Configuration */
  if (FORMAT_T1SF)
    framer_write(sc, Bt8370_TCR1, 0x43); /* tabort, AMI PDV enforced */
  else
    framer_write(sc, Bt8370_TCR1, 0x41); /* tabort, B8ZS or HDB3 */

  /* 072:TFRM -- Transmit Frame format       MYEL YEL MF FE CRC FBIT */
  if      (sc->config.format == CFG_FORMAT_T1ESF)
    framer_write(sc, Bt8370_TFRM, 0x0B); /*  -   YEL MF -  CRC FBIT */
  else if (sc->config.format == CFG_FORMAT_T1SF)
    framer_write(sc, Bt8370_TFRM, 0x19); /*  -   YEL MF -   -  FBIT */
  else if (sc->config.format == CFG_FORMAT_E1FAS)
    framer_write(sc, Bt8370_TFRM, 0x11); /*  -   YEL -  -   -  FBIT */
  else if (sc->config.format == CFG_FORMAT_E1FASCRC)
    framer_write(sc, Bt8370_TFRM, 0x1F); /*  -   YEL MF FE CRC FBIT */
  else if (sc->config.format == CFG_FORMAT_E1FASCAS)
    framer_write(sc, Bt8370_TFRM, 0x31); /* MYEL YEL -  -   -  FBIT */
  else if (sc->config.format == CFG_FORMAT_E1FASCRCCAS)
    framer_write(sc, Bt8370_TFRM, 0x3F); /* MYEL YEL MF FE CRC FBIT */
  else if (sc->config.format == CFG_FORMAT_E1NONE)
    framer_write(sc, Bt8370_TFRM, 0x00); /* NO FRAMING BITS AT ALL! */

  /* 073:TERROR -- Transmit Error Insert */
  framer_write(sc, Bt8370_TERROR, 0x00); /* no errors, please! */

  /* 074:TMAN -- Transmit Manual Sa-byte/FEBE configuration */
  framer_write(sc, Bt8370_TMAN, 0x00); /* none */

  /* 075:TALM -- Transmit Alarm Signal Configuration */
  if (FORMAT_E1ANY)
    framer_write(sc, Bt8370_TALM, 0x38); /* auto_myel auto_yel auto_ais */
  else if (FORMAT_T1ANY)
    framer_write(sc, Bt8370_TALM, 0x18); /* auto_yel auto_ais */

  /* 076:TPATT -- Transmit Test Pattern Configuration */
  framer_write(sc, Bt8370_TPATT, 0x00); /* disabled */

  /* 077:TLB -- Transmit Inband Loopback Code Configuration */
  framer_write(sc, Bt8370_TLB, 0x00); /* disabled */

  /* 090:CLAD_CR -- Clack Rate Adapter Configuration */
  if (FORMAT_T1ANY)
    framer_write(sc, Bt8370_CLAD_CR, 0x06); /* loop filter gain 1/2^6 */
  else
    framer_write(sc, Bt8370_CLAD_CR, 0x08); /* loop filter gain 1/2^8 */

  /* 091:CSEL -- CLAD frequency Select */
  if (FORMAT_T1ANY)
    framer_write(sc, Bt8370_CSEL, 0x55); /* 1544 kHz */
  else
    framer_write(sc, Bt8370_CSEL, 0x11); /* 2048 kHz */

  /* 092:CPHASE -- CLAD Phase detector */
  if (FORMAT_T1ANY)
    framer_write(sc, Bt8370_CPHASE, 0x22); /* phase compare @  386 kHz */
  else
    framer_write(sc, Bt8370_CPHASE, 0x00); /* phase compare @ 2048 kHz */

  if (FORMAT_T1ESF) /* BOP & PRM are enabled in T1ESF mode only. */
    {
    /* 0A0:BOP -- Bit Oriented Protocol messages */
    framer_write(sc, Bt8370_BOP, RBOP_25 | TBOP_OFF);
    /* 0A4:DL1_TS -- Data Link 1 Time Slot Enable */
    framer_write(sc, Bt8370_DL1_TS, 0x40); /* FDL bits in odd frames */
    /* 0A6:DL1_CTL -- Data Link 1 Control */
    framer_write(sc, Bt8370_DL1_CTL, 0x03); /* FCS mode, TX on, RX on */
    /* 0A7:RDL1_FFC -- Rx Data Link 1 Fifo Fill Control */
    framer_write(sc, Bt8370_RDL1_FFC, 0x30); /* assert "near full" at 48 */
    /* 0AA:PRM -- Performance Report Messages */
    framer_write(sc, Bt8370_PRM, 0x80);
    }

  /* 0D0:SBI_CR -- System Bus Interface Configuration Register */
  if (FORMAT_T1ANY)
    framer_write(sc, Bt8370_SBI_CR, 0x47); /* 1.544 with 24 TS +Fbits */
  else
    framer_write(sc, Bt8370_SBI_CR, 0x46); /* 2.048 with 32 TS */

  /* 0D1:RSB_CR -- Receive System Bus Configuration Register */
  /* Change RINDO & RFSYNC on falling edge of RSBCLKI. */
  framer_write(sc, Bt8370_RSB_CR, 0x70);

  /* 0D2,0D3:RSYNC_{TS,BIT} -- Receive frame Sync offset */
  framer_write(sc, Bt8370_RSYNC_BIT, 0x00);
  framer_write(sc, Bt8370_RSYNC_TS,  0x00);

  /* 0D4:TSB_CR -- Transmit System Bus Configuration Register */
  /* Change TINDO & TFSYNC on falling edge of TSBCLKI. */
  framer_write(sc, Bt8370_TSB_CR, 0x30);

  /* 0D5,0D6:TSYNC_{TS,BIT} -- Transmit frame Sync offset */
  framer_write(sc, Bt8370_TSYNC_BIT, 0x00);
  framer_write(sc, Bt8370_TSYNC_TS,  0x00);

  /* 0D7:RSIG_CR -- Receive SIGnalling Configuration Register */
  framer_write(sc, Bt8370_RSIG_CR, 0x00);

  /* Assign and configure 64Kb TIME SLOTS. */
  /* TS24..TS1 must be assigned for T1, TS31..TS0 for E1. */
  /* Timeslots with no user data have RINDO and TINDO off. */
  for (sc->status.time_slots = 0, i=0; i<32; i++)
    {
    /* 0E0-0FF:SBCn -- System Bus Per-Channel Control */
    if      (FORMAT_T1ANY && (i==0 || i>24))
      framer_write(sc, Bt8370_SBCn +i, 0x00); /* not assigned in T1 mode */
    else if (FORMAT_E1ANY && (i==0)  && !FORMAT_E1NONE)
      framer_write(sc, Bt8370_SBCn +i, 0x01); /* assigned, TS0  o/h bits */
    else if (FORMAT_E1CAS && (i==16) && !FORMAT_E1NONE)
      framer_write(sc, Bt8370_SBCn +i, 0x01); /* assigned, TS16 o/h bits */
    else if ((sc->status.time_slots |= (sc->config.time_slots & (1<<i))))
      framer_write(sc, Bt8370_SBCn +i, 0x0D); /* assigned, RINDO, TINDO */
    else
      framer_write(sc, Bt8370_SBCn +i, 0x01); /* assigned, idle */

    /* 100-11F:TPCn -- Transmit Per-Channel Control */
    if      (FORMAT_E1CAS && (i==0))
      framer_write(sc, Bt8370_TPCn +i, 0x30); /* tidle, sig=0000 (MAS) */
    else if (FORMAT_E1CAS && (i==16))
      framer_write(sc, Bt8370_TPCn +i, 0x3B); /* tidle, sig=1011 (XYXX) */
    else if ((sc->config.time_slots & (1<<i)) == 0)
      framer_write(sc, Bt8370_TPCn +i, 0x20); /* tidle: use TSLIP_LOn */
    else
      framer_write(sc, Bt8370_TPCn +i, 0x00); /* nothing special */

    /* 140-15F:TSLIP_LOn -- Transmit PCM Slip Buffer */
    framer_write(sc, Bt8370_TSLIP_LOn +i, 0x7F); /* idle chan data */
    /* 180-19F:RPCn -- Receive Per-Channel Control */
    framer_write(sc, Bt8370_RPCn +i, 0x00);   /* nothing special */
    }

  /* Enable transmitter output drivers. */
  mii16_set_bits(sc, MII16_T1_XOE);
  }

static void
t1_detach(softc_t *sc)
  {
  led_on(sc, MII16_LED_ALL);
  }

/* End T1E1 card code */


#if SYNC_PPP  /* Linux */

static struct stack sync_ppp_stack =
  {
  .ioctl    = sync_ppp_ioctl,
  .type     = sync_ppp_type,
  .mtu      = sync_ppp_mtu,
  .watchdog = sync_ppp_watchdog,
  .open     = sync_ppp_open,
  .attach   = sync_ppp_attach,
  .detach   = sync_ppp_detach,
  };

static int  /* context: process */
sync_ppp_ioctl(softc_t *sc, struct ifreq *ifr, int cmd)
  {
  return sppp_do_ioctl(sc->netdev, ifr, cmd);
  }

static int  /* context: interrupt */
sync_ppp_type(softc_t *sc, struct sk_buff *skb)
  {
  return htons(ETH_P_WAN_PPP);
  }

static int  /* context: process */
sync_ppp_mtu(softc_t *sc, int mtu)
  {
  return ((mtu < 128) || (mtu > PPP_MTU)) ? -EINVAL : 0;
  }

static void  /* context: softirq */
sync_ppp_watchdog(softc_t *sc)
  {
  /* Notice when the link comes up. */
  if ((sc->last_link_state != STATE_UP) &&
    (sc->status.link_state == STATE_UP))
    sppp_reopen(sc->netdev);

  /* Notice when the link goes down. */
  if ((sc->last_link_state == STATE_UP) &&
    (sc->status.link_state != STATE_UP))
    sppp_close(sc->netdev);

  /* Report current line protocol. */
  sc->status.stack = STACK_SYNC_PPP;
  if (sc->sppp->pp_flags & PP_CISCO)
    sc->status.proto = PROTO_C_HDLC;
  else
    sc->status.proto = PROTO_PPP;

  /* Report keep-alive status. */
  sc->status.keep_alive = sc->sppp->pp_flags & PP_KEEPALIVE;
  }

static int  /* never fails */
sync_ppp_open(softc_t *sc, struct config *config)
  {
  /* Refresh the keep_alive flag. */
  if (config->keep_alive)
    sc->sppp->pp_flags |=  PP_KEEPALIVE;
  else
    sc->sppp->pp_flags &= ~PP_KEEPALIVE;
  sc->config.keep_alive = config->keep_alive;

  /* Done if proto is not changing. */
  if (config->proto == sc->config.proto)
    return 0;

  /* Close */
  sppp_close(sc->netdev);

  /* Change line protocol. */
  switch (config->proto)
    {
    case PROTO_PPP:
      sc->sppp->pp_flags  &= ~PP_CISCO;
      sc->netdev->type = ARPHRD_PPP;
      sc->config.proto = PROTO_PPP;
      break;
    default:
    case PROTO_C_HDLC:
      sc->sppp->pp_flags  |=  PP_CISCO;
      sc->netdev->type = ARPHRD_CISCO;
      sc->config.proto = PROTO_C_HDLC;
      break;
    }

  /* Open */
  sppp_open(sc->netdev);

  return 0;
  }

static int  /* never fails */
sync_ppp_attach(softc_t *sc, struct config *config)
  {
  sc->ppd = &sc->ppp_dev;      /* struct ppp_device*  */
  sc->netdev->priv = &sc->ppd; /* struct ppp_device** */
  sc->ppp_dev.dev = sc->netdev;
  sc->sppp = &sc->ppp_dev.sppp;

  sppp_attach(&sc->ppp_dev);
  sc->netdev->do_ioctl = netdev_ioctl;
  config->keep_alive = 1;

  sc->config.stack = STACK_SYNC_PPP;
  sc->stack = &sync_ppp_stack;

  return 0;
  }

static int  /* never fails */
sync_ppp_detach(softc_t *sc)
  {
  sppp_close(sc->netdev);
  sppp_detach(sc->netdev);

  netdev_setup(sc->netdev);
  sc->config.stack = STACK_NONE;
  sc->config.proto = PROTO_NONE;
  sc->stack = NULL;

  return 0;
  }

#endif /* SYNC_PPP */

#if GEN_HDLC  /* Linux only */

static struct stack gen_hdlc_stack =
  {
  .ioctl    = gen_hdlc_ioctl,
  .type     = gen_hdlc_type,
  .mtu      = gen_hdlc_mtu,
  .watchdog = gen_hdlc_watchdog,
  .open     = gen_hdlc_open,
  .attach   = gen_hdlc_attach,
  .detach   = gen_hdlc_detach,
  };

static int  /* context: process */
gen_hdlc_ioctl(softc_t *sc, struct ifreq *ifr, int cmd)
  {
  te1_settings settings;
  int error = 0;

  if (cmd == SIOCWANDEV)
    switch (ifr->ifr_settings.type)
      {
      case IF_GET_IFACE: /* get interface config */
        {
        unsigned int size;

        /* NOTE: This assumes struct sync_serial_settings has the */
        /* same layout as the first part of struct te1_settings. */
        if (sc->status.card_type == CSID_LMC_T1E1)
          {
          if (FORMAT_T1ANY) ifr->ifr_settings.type = IF_IFACE_T1;
          if (FORMAT_E1ANY) ifr->ifr_settings.type = IF_IFACE_E1;
          size = sizeof(te1_settings);
	  }
        else
          {
          ifr->ifr_settings.type = IF_IFACE_SYNC_SERIAL;
          size = sizeof(sync_serial_settings);
	  }
        if (ifr->ifr_settings.size < size)
          {
          ifr->ifr_settings.size = size;
          return -ENOBUFS;
	  }
        ifr->ifr_settings.size = size;

        if (sc->config.tx_clk_src == CFG_CLKMUX_ST)
          settings.clock_type = CLOCK_EXT;
        if (sc->config.tx_clk_src == CFG_CLKMUX_INT)
          settings.clock_type = CLOCK_TXINT;
        if (sc->config.tx_clk_src == CFG_CLKMUX_RT)
          settings.clock_type = CLOCK_TXFROMRX;
        settings.loopback = (sc->config.loop_back != CFG_LOOP_NONE) ? 1:0;
        settings.clock_rate = sc->status.tx_speed;
        if (sc->status.card_type == CSID_LMC_T1E1)
          settings.slot_map = sc->status.time_slots;

        error = copy_to_user(ifr->ifr_settings.ifs_ifsu.te1,
         &settings, size);
        break;
        }
      case IF_IFACE_SYNC_SERIAL: /* set interface config */
      case IF_IFACE_T1:
      case IF_IFACE_E1:
        {
        struct config config = sc->config;

        if (!capable(CAP_NET_ADMIN)) return -EPERM;
        if (ifr->ifr_settings.size > sizeof(te1_settings))
          return -ENOBUFS;
        error = copy_from_user(&settings,
         ifr->ifr_settings.ifs_ifsu.te1, sizeof(te1_settings));

        if      (settings.clock_type == CLOCK_EXT)
          config.tx_clk_src = CFG_CLKMUX_ST;
        else if (settings.clock_type == CLOCK_TXINT)
          config.tx_clk_src = CFG_CLKMUX_INT;
        else if (settings.clock_type == CLOCK_TXFROMRX)
          config.tx_clk_src = CFG_CLKMUX_RT;

        if (settings.loopback)
          config.loop_back = CFG_LOOP_TULIP;
        else
          config.loop_back = CFG_LOOP_NONE;

        tulip_loop(sc, &config);
        sc->card->attach(sc, &config);
        break;
        }
      default:  /* Pass the rest to the line pkg. */
        {
        error = hdlc_ioctl(sc->netdev, ifr, cmd);
        break;
        }
      }
    else
      error = -EINVAL;

  return error;
  }

static int  /* context: interrupt */
gen_hdlc_type(softc_t *sc, struct sk_buff *skb)
  {
  return hdlc_type_trans(skb, sc->netdev);
  }

static int  /* context: process */
gen_hdlc_mtu(softc_t *sc, int mtu)
  {
  return ((mtu < 68) || (mtu > HDLC_MAX_MTU)) ? -EINVAL : 0;
  }

static void  /* context: softirq */
gen_hdlc_watchdog(softc_t *sc)
  {
  /* Notice when the link comes up. */
  if ((sc->last_link_state != STATE_UP) &&
    (sc->status.link_state == STATE_UP))
    hdlc_set_carrier(1, sc->netdev);

  /* Notice when the link goes down. */
  if ((sc->last_link_state == STATE_UP) &&
    (sc->status.link_state != STATE_UP))
    hdlc_set_carrier(0, sc->netdev);

  /* Report current line protocol. */
  sc->status.stack = STACK_GEN_HDLC;
  switch (sc->hdlcdev->proto.id)
    {
    case IF_PROTO_PPP:
      {
      struct sppp* sppp = &sc->hdlcdev->state.ppp.pppdev.sppp;
      sc->status.keep_alive = sppp->pp_flags & PP_KEEPALIVE;
      sc->status.proto = PROTO_PPP;
      break;
      }
    case IF_PROTO_CISCO:
      sc->status.proto = PROTO_C_HDLC;
      break;
    case IF_PROTO_FR:
      sc->status.proto = PROTO_FRM_RLY;
      break;
    case IF_PROTO_HDLC:
      sc->status.proto = PROTO_IP_HDLC;
      break;
    case IF_PROTO_X25:
      sc->status.proto = PROTO_X25;
      break;
    case IF_PROTO_HDLC_ETH:
      sc->status.proto = PROTO_ETH_HDLC;
      break;
    default:
      sc->status.proto = PROTO_NONE;
      break;
    }
  }

static int
gen_hdlc_open(softc_t *sc, struct config *config)
  {
  int error = 0;

  /* Refresh the keep_alive flag. */
  if (sc->hdlcdev->proto.id == IF_PROTO_PPP)
    {
    struct sppp* sppp = &sc->hdlcdev->state.ppp.pppdev.sppp;
    if (config->keep_alive)
      sppp->pp_flags |=  PP_KEEPALIVE;
    else
      sppp->pp_flags &= ~PP_KEEPALIVE;
    sc->config.keep_alive = config->keep_alive;
    }

  /* Done if proto is not changing. */
  if (config->proto == sc->config.proto)
    return 0;

  /* Close */
  hdlc_close(sc->netdev);

  /* Generic-HDLC gets protocol params using copy_from_user().
   * This is a problem for a kernel-resident device driver.
   * Luckily, PPP does not need any params so no copy_from_user().
   */

  /* Change line protocol. */
  if (config->proto == PROTO_PPP)
    {
    struct ifreq ifr;
    ifr.ifr_settings.size = 0;
    ifr.ifr_settings.type = IF_PROTO_PPP;
    hdlc_ioctl(sc->netdev, &ifr, SIOCWANDEV);
    }
  /* Changing to any protocol other than PPP */
  /*  requires using the 'sethdlc' program. */

  /* Open */
  if ((error = hdlc_open(sc->netdev)))
    {
    if (sc->config.debug)
      printk("%s: hdlc_open(): error %d\n", NAME_UNIT, error);
    if (error == -ENOSYS)
      printk("%s: Try 'sethdlc %s hdlc|ppp|cisco|fr'\n",
       NAME_UNIT, NAME_UNIT);
    sc->config.proto = PROTO_NONE;
    }
  else
    sc->config.proto = config->proto;

  return error;
  }

static int  /* never fails */
gen_hdlc_attach(softc_t *sc, struct config *config)
  {
  sc->netdev->priv = sc->hdlcdev;

  /* hdlc_attach(sc->netdev); */
  sc->netdev->mtu = HDLC_MAX_MTU;
  sc->hdlcdev->attach = gen_hdlc_card_params;
  sc->hdlcdev->xmit = netdev_start;
  config->keep_alive = 1;

  sc->config.stack = STACK_GEN_HDLC;
  sc->stack = &gen_hdlc_stack;

  return 0;
  }

static int  /* never fails */
gen_hdlc_detach(softc_t *sc)
  {
  hdlc_close(sc->netdev);
  /* hdlc_detach(sc->netdev); */
  hdlc_proto_detach(sc->hdlcdev);
  memset(&sc->hdlcdev->proto, 0, sizeof sc->hdlcdev->proto);

  netdev_setup(sc->netdev);
  sc->config.stack = STACK_NONE;
  sc->config.proto = PROTO_NONE;
  sc->stack = NULL;

  return 0;
  }

static int
gen_hdlc_card_params(struct net_device *netdev,
 unsigned short encoding, unsigned short parity)
  {
  softc_t *sc = NETDEV2SC(netdev);
  struct config config = sc->config;

  /* Encoding does not seem to apply to synchronous interfaces, */
  /* but Parity seems to be generic-HDLC's name for CRC. */
  if (parity == PARITY_CRC32_PR1_CCITT)
    config.crc_len = CFG_CRC_32;
  if (parity == PARITY_CRC16_PR1_CCITT)
    config.crc_len = CFG_CRC_16;
  sc->card->attach(sc, &config);

  return 0;
  }

#endif /* GEN_HDLC */

#if P2P  /* BSD/OS */

static struct stack p2p_stack =
  {
  .ioctl    = p2p_stack_ioctl,
  .input    = p2p_stack_input,
  .output   = p2p_stack_output,
  .watchdog = p2p_stack_watchdog,
  .open     = p2p_stack_open,
  .attach   = p2p_stack_attach,
  .detach   = p2p_stack_detach,
  };

static int  /* context: process */
p2p_stack_ioctl(softc_t *sc, u_long cmd, void *data)
  {
  return p2p_ioctl(sc->ifp, cmd, data);
  }

static void  /* context: interrupt */
p2p_stack_input(softc_t *sc, struct mbuf *mbuf)
  {
  struct mbuf *new_mbuf = mbuf;

  while (new_mbuf)
    {
    sc->p2p->p2p_hdrinput(sc->p2p, new_mbuf->m_data, new_mbuf->m_len);
    new_mbuf = new_mbuf->m_next;
    }
  sc->p2p->p2p_input(sc->p2p, NULL);
  m_freem(mbuf);
  }

static void  /* context: interrupt */
p2p_stack_output(softc_t *sc)
  {
  if (!IFQ_IS_EMPTY(&sc->p2p->p2p_isnd))
    IFQ_DEQUEUE(&sc->p2p->p2p_isnd, sc->tx_mbuf);
  else
    IFQ_DEQUEUE(&sc->ifp->if_snd,   sc->tx_mbuf);
  }

static void  /* context: softirq */
p2p_stack_watchdog(softc_t *sc)
  {
  /* Notice change in link status. */
  if ((sc->last_link_state != sc->status.link_state) &&
   /* if_slowtimo() can run before raw_init() has inited rawcb. */
   (sc->p2p->p2p_modem != NULL) && (rawcb.rcb_next != NULL))
    (*sc->p2p->p2p_modem)(sc->p2p, sc->status.link_state==STATE_UP);

  /* Report current line protocol. */
  sc->status.stack = STACK_P2P;
  switch (sc->ifp->if_type)
    {
    case IFT_PPP:
      sc->status.proto = PROTO_PPP;
      break;
    case IFT_PTPSERIAL:
      sc->status.proto = PROTO_C_HDLC;
      break;
    case IFT_FRELAY:
      sc->status.proto = PROTO_FRM_RLY;
      break;
    default:
      sc->status.proto = PROTO_NONE;
      break;
    }
  }

static int
p2p_stack_open(softc_t *sc, struct config *config)
  {
  int error = 0;

  /* Done if proto is not changing. */
  if (config->proto == sc->config.proto)
    return 0;

  if (error = p2p_stack_detach(sc))
    return error;

  /* Change line protocol. */
  switch (config->proto)
    {
    case PROTO_PPP:
      sc->ifp->if_type = IFT_PPP;
      sc->config.proto = PROTO_PPP;
      break;
    case PROTO_C_HDLC:
      sc->ifp->if_type = IFT_PTPSERIAL;
      sc->config.proto = PROTO_C_HDLC;
      break;
    case PROTO_FRM_RLY:
      sc->ifp->if_type = IFT_FRELAY;
      sc->config.proto = PROTO_FRM_RLY;
      break;
    default:
    case PROTO_NONE:
      sc->ifp->if_type = IFT_NONE;
      sc->config.proto = PROTO_NONE;
      return 0;
    }

  error = p2p_stack_attach(sc, config);

  return error;
  }

static int
p2p_stack_attach(softc_t *sc, struct config *config)
  {
  int error;

  sc->p2p = &sc->p2pcom;
  sc->p2p->p2p_proto = 0; /* force p2p_attach to re-init */

  if ((error = p2p_attach(sc->p2p))) /* calls bpfattach() */
    {
    if (sc->config.debug)
      printf("%s: p2p_attach(): error %d\n", NAME_UNIT, error);
    if (error == EPFNOSUPPORT)
      printf("%s: Try 'ifconfig %s linktype ppp|frelay|chdlc'\n",
       NAME_UNIT, NAME_UNIT);
    sc->config.stack = STACK_NONE; /* not attached to P2P */
    return error;
    }
  sc->p2p->p2p_mdmctl = p2p_mdmctl;
  sc->p2p->p2p_getmdm = p2p_getmdm;

  sc->config.stack = STACK_P2P;
  sc->stack = &p2p_stack;

  return 0;
  }

static int
p2p_stack_detach(softc_t *sc)
  {
  int error = 0;

  if ((error = p2p_detach(sc->p2p))) /* calls bfpdetach() */
    {
    if (sc->config.debug)
      printf("%s: p2p_detach(): error %d\n",  NAME_UNIT, error);
    if (error == EBUSY)
      printf("%s: Try 'ifconfig %s down -remove'\n",
       NAME_UNIT, NAME_UNIT);
    sc->config.stack = STACK_P2P; /* still attached to P2P */
    return error;
    }

  ifnet_setup(sc->ifp);
  sc->config.stack = STACK_NONE;
  sc->config.proto = PROTO_NONE;
  sc->stack = NULL;

  return error;
  }

/* Callout from P2P: */
/* Get the state of DCD (Data Carrier Detect). */
static int  /* never fails */
p2p_getmdm(struct p2pcom *p2p, void *result)
  {
  softc_t *sc = IFP2SC(&p2p->p2p_if);

  /* Non-zero is not good enough; TIOCM_CAR is 0x40. */
  *(int *)result = (sc->status.link_state==STATE_UP) ? TIOCM_CAR : 0;

  return 0;
  }

/* Callout from P2P: */
/* Set the state of DTR (Data Terminal Ready). */
static int  /* never fails */
p2p_mdmctl(struct p2pcom *p2p, int flag)
  {
  softc_t *sc = IFP2SC(&p2p->p2p_if);

  set_ready(sc, flag);

  return 0;
  }

#endif /* P2P */

#if SPPP  /* FreeBSD, NetBSD, OpenBSD */

static struct stack sppp_stack =
  {
  .ioctl    = sppp_stack_ioctl,
  .input    = sppp_stack_input,
  .output   = sppp_stack_output,
  .watchdog = sppp_stack_watchdog,
  .open     = sppp_stack_open,
  .attach   = sppp_stack_attach,
  .detach   = sppp_stack_detach,
  };

# if !defined(PP_FR)
#  define PP_FR 0
# endif
# if !defined(DLT_C_HDLC)
#  define DLT_C_HDLC DLT_PPP
# endif
# if !defined(DLT_FRELAY)
#  define DLT_FRELAY DLT_PPP
# endif

static int  /* context: process */
sppp_stack_ioctl(softc_t *sc, u_long cmd, void *data)
  {
  return sppp_ioctl(sc->ifp, cmd, data);
  }

static void  /* context: interrupt */
sppp_stack_input(softc_t *sc, struct mbuf *mbuf)
  {
  sppp_input(sc->ifp, mbuf);
  }

static void  /* context: interrupt */
sppp_stack_output(softc_t *sc)
  {
  sc->tx_mbuf = sppp_dequeue(sc->ifp);
  }

static void  /* context: softirq */
sppp_stack_watchdog(softc_t *sc)
  {
  /* Notice when the link comes up. */
  if ((sc->last_link_state != STATE_UP) &&
    (sc->status.link_state == STATE_UP))
    sppp_tls(sc->sppp);

  /* Notice when the link goes down. */
  if ((sc->last_link_state == STATE_UP) &&
    (sc->status.link_state != STATE_UP))
    sppp_tlf(sc->sppp);

  /* Report current line protocol. */
  sc->status.stack = STACK_SPPP;
  if (sc->sppp->pp_flags & PP_CISCO)
    sc->status.proto = PROTO_C_HDLC;
  else
    sc->status.proto = PROTO_PPP;

  /* Report keep-alive status. */
  sc->status.keep_alive = sc->sppp->pp_flags & PP_KEEPALIVE;
  }

static int  /* never fails */
sppp_stack_open(softc_t *sc, struct config *config)
  {
  /* Refresh the keep_alive flag. */
  if (config->keep_alive)
    sc->sppp->pp_flags |=  PP_KEEPALIVE;
  else
    sc->sppp->pp_flags &= ~PP_KEEPALIVE;
  sc->config.keep_alive = config->keep_alive;

  /* Done if proto is not changing. */
  if (config->proto == sc->config.proto)
    return 0;

  /* Close */
  sc->ifp->if_flags &= ~IFF_UP; /* down */
  sppp_ioctl(sc->ifp, SIOCSIFFLAGS, NULL);

  /* Change line protocol. */
  LMC_BPF_DETACH(sc);
  switch (config->proto)
    {
    case PROTO_PPP:
      {
      sc->sppp->pp_flags &= ~PP_CISCO;
      LMC_BPF_ATTACH(sc, DLT_PPP, 4);
      sc->config.proto = PROTO_PPP;
      break;
      }

    default:
    case PROTO_C_HDLC:
      {
      sc->sppp->pp_flags |=  PP_CISCO;
      LMC_BPF_ATTACH(sc, DLT_C_HDLC, 4);
      sc->config.proto = PROTO_C_HDLC;
      break;
      }

    } /* switch(config->proto) */

  /* Open */
  sc->ifp->if_flags |= IFF_UP; /* up and not running */
  sppp_ioctl(sc->ifp, SIOCSIFFLAGS, NULL);

  return 0;
  }

static int  /* never fails */
sppp_stack_attach(softc_t *sc, struct config *config)
  {
  sc->sppp = &sc->spppcom;

  LMC_BPF_ATTACH(sc, DLT_RAW, 0);
  sppp_attach(sc->ifp);
  sc->sppp->pp_tls = sppp_tls;
  sc->sppp->pp_tlf = sppp_tlf;
  config->keep_alive = 1;

  sc->config.stack = STACK_SPPP;
  sc->stack = &sppp_stack;

  return 0;
  }

static int  /* never fails */
sppp_stack_detach(softc_t *sc)
  {
  sc->ifp->if_flags &= ~IFF_UP; /* down */
  sppp_ioctl(sc->ifp, SIOCSIFFLAGS, NULL); /* close() */
  sppp_detach(sc->ifp);
  LMC_BPF_DETACH(sc);

  ifnet_setup(sc->ifp);
  sc->config.stack = STACK_NONE;
  sc->config.proto = PROTO_NONE;
  sc->stack = NULL;

  return 0;
  }

/* Callout from SPPP: */
static void
sppp_tls(struct sppp *sppp)
  {
  /* Calling pp_up/down() required by PPP mode in OpenBSD. */
  /* Calling pp_up/down() panics      PPP mode in NetBSD. */
  /* Calling pp_up/down() breaks    Cisco mode in FreeBSD. */
  if (!(sppp->pp_flags & PP_CISCO))    /* not Cisco */
    sppp->pp_up(sppp);
  }

/* Callout from SPPP: */
static void
sppp_tlf(struct sppp *sppp)
  {
  /* Calling pp_up/down() required by PPP mode in OpenBSD. */
  /* Calling pp_up/down() panics      PPP mode in NetBSD. */
  /* Calling pp_up/down() breaks    Cisco mode in FreeBSD. */
  if (!(sppp->pp_flags & PP_CISCO))    /* not Cisco */
    sppp->pp_down(sppp);
  }

#endif /* SPPP */

/* RawIP is built into the driver. */

static struct stack rawip_stack =
  {
#if IFNET
  .ioctl    = rawip_ioctl,
  .input    = rawip_input,
  .output   = rawip_output,
#elif NETDEV
  .ioctl    = rawip_ioctl,
  .type     = rawip_type,
  .mtu      = rawip_mtu,
#endif
  .watchdog = rawip_watchdog,
  .open     = rawip_open,
  .attach   = rawip_attach,
  .detach   = rawip_detach,
  };

#if IFNET

static int  /* context: process */
rawip_ioctl(softc_t *sc, u_long cmd, void *data)
  {
  struct ifreq *ifr = (struct ifreq *) data;
  int error = 0;

  switch (cmd)
    {
    case SIOCADDMULTI:
    case SIOCDELMULTI:
      if (sc->config.debug)
        printf("%s: rawip_ioctl: SIOCADD/DELMULTI\n", NAME_UNIT);
    case SIOCSIFFLAGS:
      error = ifioctl_common(sc->ifp, cmd, data);
      break;
    case SIOCAIFADDR:
    case SIOCSIFDSTADDR:
      break;
    case SIOCINITIFADDR:
      sc->ifp->if_flags |= IFF_UP; /* a Unix tradition */
      break;
    case SIOCSIFMTU:
      if ((ifr->ifr_mtu < 72) || (ifr->ifr_mtu > 65535))
        error = EINVAL;
      else if ((error = ifioctl_common(sc->ifp, cmd, data)) == ENETRESET)
        error = 0;
      break;
    default:
      error = EINVAL;
      break;
    }

  return error;
  }

static void  /* context: interrupt */
rawip_input(softc_t *sc, struct mbuf *mbuf)
  {
  ifnet_input(sc->ifp, mbuf);
  }

static void  /* context: interrupt */
rawip_output(softc_t *sc)
  {
  IFQ_DEQUEUE(&sc->ifp->if_snd, sc->tx_mbuf);
  }

#elif NETDEV

static int  /* context: process */
rawip_ioctl(softc_t *sc, struct ifreq *ifr, int cmd)
  {
  if (sc->config.debug)
    printk("%s: rawip_ioctl; cmd=0x%08x\n", NAME_UNIT, cmd);
  return -EINVAL;
  }

static int  /* context: interrupt */
rawip_type(softc_t *sc, struct sk_buff *skb)
  {
  if (skb->data[0]>>4 == 4)
    return htons(ETH_P_IP);
  else if (skb->data[0]>>4 == 6)
    return htons(ETH_P_IPV6);
  else
    return htons(ETH_P_HDLC);
  }

static int  /* Process Context */
rawip_mtu(softc_t *sc, int mtu)
  {
  return ((mtu < 72) || (mtu > 65535)) ? -EINVAL : 0;
  }

#endif /* IFNET */

static void  /* context: softirq */
rawip_watchdog(softc_t *sc)
  {
#if IFNET
  if ((sc->status.link_state == STATE_UP) &&
      (sc->ifp->if_flags & IFF_UP))
    sc->ifp->if_flags |= IFF_RUNNING;
  if ((sc->status.link_state != STATE_UP) ||
     !(sc->ifp->if_flags & IFF_UP))
    sc->ifp->if_flags &= ~IFF_RUNNING;
#endif /* IFNET */

  /* Report current line protocol. */
  sc->status.stack = STACK_RAWIP;
  sc->status.proto = PROTO_IP_HDLC;
  }

static int
rawip_open(softc_t *sc, struct config *config)
  {
  sc->config.proto = PROTO_IP_HDLC;

  return 0;
  }

static int
rawip_attach(softc_t *sc, struct config *config)
  {
#if IFNET
  LMC_BPF_ATTACH(sc, DLT_RAW, 0);
#endif

  sc->config.stack = STACK_RAWIP;
  sc->stack = &rawip_stack;

  return 0;
  }

static int
rawip_detach(softc_t *sc)
  {
#if IFNET
  LMC_BPF_DETACH(sc);
  ifnet_setup(sc->ifp);
#elif NETDEV
  netdev_setup(sc->netdev);
#endif

  sc->config.stack = STACK_NONE;
  sc->config.proto = PROTO_NONE;
  sc->stack = NULL;

  return 0;
  }

#if IFNET

/* Called to give a newly arrived pkt to higher levels. */
/* Called from rxintr_cleanup() with bottom_lock held. */
/* This is only used with rawip_stack on a BSD. */
static void
ifnet_input(struct ifnet *ifp, struct mbuf *mbuf)
  {
  softc_t *sc = IFP2SC(ifp);
  pktqueue_t *pktq = NULL;

# if INET
  if (mbuf->m_data[0]>>4 == 4)
    pktq = ip_pktq;
# endif /* INET */

# if INET6
  if (mbuf->m_data[0]>>4 == 6)
    pktq = ip6_pktq;
# endif /* INET6 */

  if (!pktq)
    {
    m_freem(mbuf);
    sc->status.cntrs.idrops++;
    if (sc->config.debug)
      printf("%s: ifnet_input: rx pkt dropped: not IPv4 or IPv6\n", NAME_UNIT);
    return;
    }

  DISABLE_INTR;
  if (__predict_false(!pktq_enqueue(pktq, mbuf, 0)))
    {
    sc->status.cntrs.idrops++;
    m_freem(mbuf);
    }
  ENABLE_INTR;
  }

/* sppp and p2p replace this with their own proc.
 * This is only used with rawip_stack on a BSD.
 * This procedure is very similar to ng_rcvdata().
 */
static int  /* context: process */
ifnet_output(struct ifnet *ifp, struct mbuf *m,
 const struct sockaddr *dst, struct rtentry *rt)
  {
  softc_t *sc = IFP2SC(ifp);
  int error = 0;

  /* Fail if the link is down. */
  if (sc->status.link_state != STATE_UP)
    {
    m_freem(m);
    sc->status.cntrs.odrops++;
    if (sc->config.debug)
      printf("%s: ifnet_output: tx pkt dropped: link down\n", NAME_UNIT);
    return ENETDOWN;
    }

  /* ifnet_output() ENQUEUEs in a syscall or softirq. */
  /* txintr_setup() DEQUEUEs in a hard interrupt. */
  /* Some BSD QUEUE routines are not interrupt-safe. */
  {
  DISABLE_INTR; /* noop in FreeBSD */
  IFQ_ENQUEUE(&ifp->if_snd, m, NULL, error);
  ENABLE_INTR; /* noop in FreeBSD */
  }

  if (error)
    {
    sc->status.cntrs.odrops++;
    if (sc->config.debug)
      printf("%s: ifnet_output: tx pkt dropped: IFQ_ENQUEUE(): error %d\n",
       NAME_UNIT, error);
    }
  else
    /* Process tx pkts; do not process rx pkts. */
    lmc_interrupt(sc, 0, 0);

  return error;
  }

static int  /* context: process */
ifnet_ioctl(struct ifnet *ifp, u_long cmd, void *data)
  {
  softc_t *sc = IFP2SC(ifp);
  struct ifreq *ifr = (struct ifreq *) data;
  int error = 0;

  /* Aquire ioctl/watchdog interlock. */
  if ((error = TOP_LOCK(sc))) return error;

  switch (cmd)
    {
    /* Catch the IOCTLs used by lmcconfig. */
    case LMCIOCGSTAT:
    case LMCIOCGCFG:
    case LMCIOCSCFG:
    case LMCIOCREAD:
    case LMCIOCWRITE:
    case LMCIOCTL:
      error = lmc_ioctl(sc, cmd, data);
      break;

    case SIOCSIFCAP:
#  if DEVICE_POLLING
      if ((ifr->ifr_reqcap & IFCAP_POLLING) &&
       !(ifp->if_capenable & IFCAP_POLLING) &&
       !(error = ether_poll_register(bsd_poll, ifp)))
        { /* enable polling */
        WRITE_CSR(sc, TLP_INT_ENBL, TLP_INT_DISABLE);
        ifp->if_capenable |= IFCAP_POLLING;
        }
      else if (!(ifr->ifr_reqcap & IFCAP_POLLING) &&
              (ifp->if_capenable & IFCAP_POLLING) &&
             !(error = ether_poll_deregister(ifp)))
        { /* disable polling */
        ifp->if_capenable &= ~IFCAP_POLLING;
        WRITE_CSR(sc, TLP_INT_ENBL, TLP_INT_TXRX);
        }
      else
        error = EINVAL;
#  endif /* DEVICE_POLLING */
      break;

    case SIOCSIFMEDIA: /* calls lmc_ifmedia_change() */
    case SIOCGIFMEDIA: /* calls ifmedia_status() */
      error = ifmedia_ioctl(ifp, ifr, &sc->ifm, cmd);
      break;


    /* Pass the rest to the line protocol. */
    default:
      if (sc->stack)
        error = sc->stack->ioctl(sc, cmd, data);
      else
        error = ENOSYS;
      break;
    }

  /* release ioctl/watchdog interlock */
  TOP_UNLOCK(sc);

  if (error && sc->config.debug)
    printf("%s: ifnet_ioctl: op=IO%s%s len=%3lu grp='%c' num=%3lu err=%d\n",
     NAME_UNIT, cmd&IOC_IN ? "W":"", cmd&IOC_OUT ? "R":"",
     IOCPARM_LEN(cmd), (char)IOCGROUP(cmd), cmd&0xFF, error);

  return error;
  }

static void  /* context: process */
ifnet_start(struct ifnet *ifp)
  {
  softc_t *sc = IFP2SC(ifp);

  /* Process tx pkts; do not process rx pkts. */
  lmc_interrupt(sc, 0, 0);
  }

static void  /* context: softirq */
ifnet_watchdog(struct ifnet *ifp)
  {
  softc_t *sc = IFP2SC(ifp);
  struct cntrs *cntrs = &sc->status.cntrs;

  lmc_watchdog(sc); /* updates link_state */

  if (sc->status.link_state == STATE_UP)
    ifp->if_link_state = LINK_STATE_UP;
  else
    ifp->if_link_state = LINK_STATE_DOWN;

  /* Copy statistics from sc to ifp. */
  ifp->if_baudrate = sc->status.tx_speed;
  ifp->if_ibytes   = cntrs->ibytes;
  ifp->if_obytes   = cntrs->obytes;
  ifp->if_ipackets = cntrs->ipackets;
  ifp->if_opackets = cntrs->opackets;
  ifp->if_ierrors  = cntrs->ierrors;
  ifp->if_oerrors  = cntrs->oerrors;
  ifp->if_iqdrops  = cntrs->idrops;

  /* If the interface debug flag is set, set the driver debug flag. */
  if (sc->ifp->if_flags & IFF_DEBUG)
    sc->config.debug = 1;

  /* Call this procedure again after one second. */
  ifp->if_timer = 1;
  }

/* This setup is for RawIP; SPPP and P2P change many items. */
/* Note the similarity to linux's netdev_setup(). */
static void
ifnet_setup(struct ifnet *ifp)
  {
  ifp->if_flags    = IFF_POINTOPOINT;
  ifp->if_flags   |= IFF_SIMPLEX;
  ifp->if_flags   |= IFF_NOARP;
  ifp->if_input    = ifnet_input;
  ifp->if_output   = ifnet_output;
  ifp->if_start    = ifnet_start;
  ifp->if_ioctl    = ifnet_ioctl;
  ifp->if_watchdog = ifnet_watchdog;
  ifp->if_timer    = 1;
  ifp->if_type     = IFT_PTPSERIAL;
  ifp->if_addrlen  = 0;
  ifp->if_hdrlen   = 0;
  ifp->if_mtu      = MAX_DESC_LEN;
  }

/* Attach the ifnet kernel interface. */
/* context: kernel (boot) or process (syscall) */
static int
ifnet_attach(softc_t *sc)
  {
# if   SPPP
  sc->ifp = &sc->spppcom.pp_if;
# elif P2P
  sc->ifp = &sc->p2pcom.p2p_if;
# else
  sc->ifp = &sc->ifnet;
# endif

  sc->ifp->if_softc = sc;
  ifnet_setup(sc->ifp);

# if DEVICE_POLLING
  sc->ifp->if_capabilities |= IFCAP_POLLING;
# endif

  /* Every OS does it differently! */
  strlcpy(sc->ifp->if_xname, device_xname(sc->sc_dev), IFNAMSIZ);

  IFQ_SET_MAXLEN(&sc->ifp->if_snd, SNDQ_MAXLEN);
  IFQ_SET_READY(&sc->ifp->if_snd);

  if_attach(sc->ifp);

  if_alloc_sadl(sc->ifp);

  ifmedia_setup(sc);

  return 0;
  }

/* Detach the ifnet kernel interface. */
/* context: kernel (boot) or process (syscall). */
static void
ifnet_detach(softc_t *sc)
  {
  ifmedia_delete_instance(&sc->ifm, IFM_INST_ANY);

# if DEVICE_POLLING
  if (sc->ifp->if_capenable & IFCAP_POLLING)
    ether_poll_deregister(sc->ifp);
# endif

  IFQ_PURGE(&sc->ifp->if_snd);

  if_detach(sc->ifp);

  }

static void
ifmedia_setup(softc_t *sc)
  {
  /* Initialize ifmedia mechanism. */
  ifmedia_init(&sc->ifm, IFM_OMASK | IFM_GMASK | IFM_IMASK,
   lmc_ifmedia_change, ifmedia_status);

  ifmedia_add(&sc->ifm, IFM_ETHER | IFM_NONE, 0, NULL);
  ifmedia_set(&sc->ifm, IFM_ETHER | IFM_NONE);
  }

/* SIOCSIFMEDIA: context: process. */
static int
lmc_ifmedia_change(struct ifnet *ifp)
  {
  softc_t *sc = IFP2SC(ifp);
  struct config config = sc->config;
  int media = sc->ifm.ifm_media;
  int error;


  /* ifconfig lmc0 mediaopt loopback */
  if (media & IFM_LOOP)
    config.loop_back = CFG_LOOP_TULIP;
  else
    config.loop_back = CFG_LOOP_NONE;

  error = open_proto(sc, &config);
  tulip_loop(sc, &config);
  sc->card->attach(sc, &config);

  return error;
  }

/* SIOCGIFMEDIA: context: process. */
static void
ifmedia_status(struct ifnet *ifp, struct ifmediareq *ifmr)
  {
  softc_t *sc = IFP2SC(ifp);

  ifmr->ifm_status = IFM_AVALID;
  if (sc->status.link_state == STATE_UP)
    ifmr->ifm_status |= IFM_ACTIVE;

  if (sc->config.loop_back != CFG_LOOP_NONE)
    ifmr->ifm_active |= IFM_LOOP;

  }

#endif  /* IFNET */

#if NETDEV

/* This net_device method is called when IFF_UP goes true. */
static int  /* context: process */
netdev_open(struct net_device *netdev)
  {
  softc_t *sc = NETDEV2SC(netdev);

  WRITE_CSR(sc, TLP_INT_ENBL, TLP_INT_TXRX);
  netif_start_queue(sc->netdev);
  set_ready(sc, 1);

  return 0;
  }

/* This net_device method is called when IFF_UP goes false. */
static int  /* context: process */
netdev_stop(struct net_device *netdev)
  {
  softc_t *sc = NETDEV2SC(netdev);

  set_ready(sc, 0);
  netif_stop_queue(sc->netdev);
  WRITE_CSR(sc, TLP_INT_ENBL, TLP_INT_DISABLE);

  return 0;
  }

/* This net_device method hands outgoing packets to the transmitter. */
/* With txintr_setup(), it implements output flow control. */
static int  /* context: netdev->xmit_lock held; BHs disabled */
netdev_start(struct sk_buff *skb, struct net_device *netdev)
  {
  softc_t *sc = NETDEV2SC(netdev);

  if (sc->tx_skb == NULL)
    {
    /* Put this skb where the transmitter will see it. */
    sc->tx_skb = skb;

    /* Process tx pkts; do not process rx pkts. */
    lmc_interrupt(sc, 0, 0);

    return NETDEV_TX_OK;
    }
  else
    {
    /* txintr_setup() calls netif_wake_queue(). */
    netif_stop_queue(netdev);
    return NETDEV_TX_BUSY;
    }
  }

# if NAPI

/* This net_device method services the card without interrupts. */
/* With rxintr_cleanup(), it implements input flow control. */
static int  /* context: softirq */
netdev_poll(struct net_device *netdev, int *budget)
  {
  softc_t *sc = NETDEV2SC(netdev);
  int received;

  /* Handle the card interrupt with kernel ints enabled. */
  /* Allow processing up to netdev->quota incoming packets. */
  /* This is the ONLY time lmc_interrupt() may process rx pkts. */
  /* Otherwise (sc->quota == 0) and rxintr_cleanup() is a NOOP. */
  lmc_interrupt(sc, min(netdev->quota, *budget), 0);

  /* Report number of rx packets processed. */
  received = netdev->quota - sc->quota;
  netdev->quota -= received;
  *budget       -= received;

  /* If quota prevented processing all rx pkts, leave rx ints disabled. */
  if (sc->quota == 0)  /* this is off by one...but harmless */
    {
    WRITE_CSR(sc, TLP_INT_ENBL, TLP_INT_TX);
    return 1; /* more pkts to handle -- reschedule */
    }

  /* Remove self from poll list. */
  netif_rx_complete(netdev);

  /* Enable card interrupts. */
  WRITE_CSR(sc, TLP_INT_ENBL, TLP_INT_TXRX);

  return 0; /* all pkts handled -- success */
  }

# endif /* NAPI */

/* This net_device method handles IOCTL syscalls. */
static int  /* context: process */
netdev_ioctl(struct net_device *netdev, struct ifreq *ifr, int cmd)
  {
  softc_t *sc = NETDEV2SC(netdev);
  int error = 0;

  /* Aquire ioctl/watchdog interlock. */
  if ((error = TOP_LOCK(sc))) return error;

  if ((cmd >= SIOCDEVPRIVATE) && (cmd <= SIOCDEVPRIVATE+15))
    {
    struct iohdr *iohdr = (struct iohdr *)ifr;
    u_int16_t direction = iohdr->direction;
    u_int16_t length = iohdr->length;
    char *user_addr = (char *)iohdr->iohdr;
    char *kern_addr = NULL;

    if (iohdr->cookie != NGM_LMC_COOKIE)
      error = -EINVAL;

    /* Emulate a BSD-style IOCTL syscall. */
    if (!error)
      error = (kern_addr = kmalloc(length, GFP_KERNEL)) ? 0: -ENOMEM;
    if (!error && (direction & DIR_IOW))
      error = copy_from_user(kern_addr, user_addr, length);
    if (!error)
      error = -lmc_ioctl(sc, (unsigned long)cmd, kern_addr);
    if (!error && (direction & DIR_IOR))
      error = copy_to_user(user_addr, kern_addr, length);
    kfree(kern_addr);
    }
  else if (sc->stack)
    error = sc->stack->ioctl(sc, ifr, cmd);
  else
    error = -ENOSYS;
# if GEN_HDLC
  /* If generic-HDLC is present but not the currently-attached
   * stack, call hdlc_ioctl() anyway because proto must
   * be set using SIOCWANDEV or hdlc_open() will fail.
   */
  if (sc->stack != &gen_hdlc_stack)
    hdlc_ioctl(sc->netdev, ifr, cmd); /* ignore error result */
# endif

  /* Release ioctl/watchdog interlock. */
  TOP_UNLOCK(sc);

  if (error && sc->config.debug)
    printk("%s: netdev_ioctl; cmd=0x%08x error=%d\n",
     NAME_UNIT, cmd, error);

  return error;
  }

/* This net_device method sets the Maximum Tranmit Unit. */
/* This driver does not limit MTU; stacks and protos do. */
static int
netdev_mtu(struct net_device *netdev, int mtu)
  {
  softc_t *sc = NETDEV2SC(netdev);
  int error = 0;

  if (sc->stack)
    error = sc->stack->mtu(sc, mtu);
  else
    error = -ENOSYS;

  if (!error)
     netdev->mtu = mtu;

  return error;
  }

/* This net_device method restarts the transmitter if it hangs. */
static void  /* BHs disabled */
netdev_timeout(struct net_device *netdev)
  {
  softc_t *sc = NETDEV2SC(netdev);

  /* Process tx pkts; do not process rx pkts. */
  lmc_interrupt(sc, 0, 0);
  }

/* This net_device method returns a pointer to device statistics. */
static struct net_device_stats *  /* context: process */
netdev_stats(struct net_device *netdev)
  {
  softc_t *sc = NETDEV2SC(netdev);

# if GEN_HDLC
  return &sc->hdlcdev->stats;
# else
  return &sc->netdev_stats;
# endif
  }

static void  /* context: softirq */
netdev_watchdog(unsigned long softc)
  {
  softc_t *sc = (softc_t *)softc;
  struct cntrs *cntrs = &sc->status.cntrs;
  struct net_device_stats *stats = netdev_stats(sc->netdev);

  lmc_watchdog(sc); /* updates link_state */

  /* Notice when the link comes up. */
  if ((sc->last_link_state != STATE_UP) &&
    (sc->status.link_state == STATE_UP))
    {
    netif_wake_queue(sc->netdev);
    netif_carrier_on(sc->netdev);
    }

  /* Notice when the link goes down. */
  if ((sc->last_link_state == STATE_UP) &&
    (sc->status.link_state != STATE_UP))
    {
    netif_tx_disable(sc->netdev);
    netif_carrier_off(sc->netdev);
    }

  /* Copy statistics from sc to netdev. */
  stats->rx_bytes         = cntrs->ibytes;
  stats->tx_bytes         = cntrs->obytes;
  stats->rx_packets       = cntrs->ipackets;
  stats->tx_packets       = cntrs->opackets;
  stats->rx_errors        = cntrs->ierrors;
  stats->tx_errors        = cntrs->oerrors;
  stats->rx_dropped       = cntrs->idrops;
  stats->rx_missed_errors = cntrs->missed;
  stats->tx_dropped       = cntrs->odrops;
  stats->rx_fifo_errors   = cntrs->fifo_over;
  stats->rx_over_errors   = cntrs->overruns;
  stats->tx_fifo_errors   = cntrs->fifo_under;
/*stats->tx_under_errors  = cntrs=>underruns; */

  /* If the interface debug flag is set, set the driver debug flag. */
  if (sc->netdev->flags & IFF_DEBUG)
    sc->config.debug = 1;

  /* Call this procedure again after one second. */
  sc->wd_timer.expires = jiffies + HZ -8; /* -8 is a FUDGE factor */
  add_timer(&sc->wd_timer);
  }

/* This setup is for RawIP; Generic-HDLC changes many items. */
/* Note the similarity to BSD's ifnet_setup(). */
static void
netdev_setup(struct net_device *netdev)
  {
  netdev->flags           = IFF_POINTOPOINT;
  netdev->flags          |= IFF_NOARP;
  netdev->open            = netdev_open;
  netdev->stop            = netdev_stop;
  netdev->hard_start_xmit = netdev_start;
# if NAPI
  netdev->poll            = netdev_poll;
  netdev->weight          = 32; /* sc->rxring.num_descs; */
# endif
  netdev->rebuild_header  = NULL; /* no arp */
  netdev->hard_header     = NULL; /* no arp */
  netdev->do_ioctl        = netdev_ioctl;
  netdev->change_mtu      = netdev_mtu;
  netdev->tx_timeout      = netdev_timeout;
  netdev->get_stats       = netdev_stats;
  netdev->watchdog_timeo  = 1 * HZ;
  netdev->mtu             = MAX_DESC_LEN;
  netdev->type            = ARPHRD_HDLC;
  netdev->hard_header_len = 16;
  netdev->addr_len        = 0;
  netdev->tx_queue_len    = SNDQ_MAXLEN;
/* The receiver generates frag-lists for packets >4032 bytes.   */
/* The transmitter accepts scatter/gather lists and frag-lists. */
/* However Linux linearizes outgoing packets since our hardware */
/*  does not compute soft checksums.  All that work for nothing! */
/*netdev->features       |= NETIF_F_SG; */
/*netdev->features       |= NETIF_F_FRAGLIST; */
  }

/* Attach the netdevice kernel interface. */
/* context: kernel (boot) or process (syscall). */
static int
netdev_attach(softc_t *sc)
  {
  int error;

# if GEN_HDLC /* generic-hdlc line protocol pkg configured */

  /* Allocate space for the HDLC network device struct. */
  /* Allocating a netdev and attaching to generic-HDLC should be separate. */
  if ((sc->netdev = alloc_hdlcdev(sc)) == NULL)
    {
    printk("%s: netdev_attach: alloc_hdlcdev() failed\n", DEVICE_NAME);
    return -ENOMEM;
    }

  /* Initialize the basic network device struct. */
  /* This clobbers some netdev stuff set by alloc_hdlcdev(). */
  /* Our get_stats() and change_mtu() do the right thing. */
  netdev_setup(sc->netdev);

  /* HACK: make the private eco-net pointer -> struct softc. */
  sc->netdev->ec_ptr = sc;

  /* Cross-link pcidev and netdev. */
  SET_NETDEV_DEV(sc->netdev, &sc->pcidev->dev);
  sc->netdev->mem_end   = pci_resource_end(sc->pcidev, 1);
  sc->netdev->mem_start = pci_resource_start(sc->pcidev, 1);
  sc->netdev->base_addr = pci_resource_start(sc->pcidev, 0);
  sc->netdev->irq       = sc->pcidev->irq;

  /* Initialize the HDLC extension to the network device. */
  sc->hdlcdev         = sc->netdev->priv;
  sc->hdlcdev->attach = gen_hdlc_card_params;
  sc->hdlcdev->xmit   = netdev_start; /* the REAL hard_start_xmit() */

  if ((error = register_hdlc_device(sc->netdev)))
    {
    printk("%s: register_hdlc_device(): error %d\n", DEVICE_NAME, error);
    free_netdev(sc->netdev);
    return error;
    }

# else

  /* Allocate and initialize the basic network device struct. */
  if ((sc->netdev = alloc_netdev(0, DEVICE_NAME"%d", netdev_setup)) == NULL)
    {
    printk("%s: netdev_attach: alloc_netdev() failed\n", DEVICE_NAME);
    return -ENOMEM;
    }

  /* HACK: make the private eco-net pointer -> struct softc. */
  sc->netdev->ec_ptr = sc;

  /* Cross-link pcidev and netdev. */
  SET_NETDEV_DEV(sc->netdev, &sc->pcidev->dev);
  sc->netdev->mem_end   = pci_resource_end(sc->pcidev, 1);
  sc->netdev->mem_start = pci_resource_start(sc->pcidev, 1);
  sc->netdev->base_addr = pci_resource_start(sc->pcidev, 0);
  sc->netdev->irq       = sc->pcidev->irq;

  if ((error = register_netdev(sc->netdev)))
    {
    printk("%s: register_netdev(): error %d\n", DEVICE_NAME, error);
    free_netdev(sc->netdev);
    return error;
    }

# endif /* GEN_HDLC */

  /* Arrange to call netdev_watchdog() once a second. */
  init_timer(&sc->wd_timer);
  sc->wd_timer.expires  = jiffies + HZ; /* now plus one second */
  sc->wd_timer.function = &netdev_watchdog;
  sc->wd_timer.data     = (unsigned long) sc;
  add_timer(&sc->wd_timer);

  return 0; /* success */
  }

/* Detach the netdevice kernel interface. */
/* context: kernel (boot) or process (syscall). */
static void
netdev_detach(softc_t *sc)
  {
  if (sc->pcidev == NULL) return;
  if (sc->netdev == NULL) return;

  netdev_stop(sc->netdev); /* check for not inited */
  del_timer(&sc->wd_timer);

# if GEN_HDLC
  unregister_hdlc_device(sc->netdev);
# else
  unregister_netdev(sc->netdev);
# endif

  free_netdev(sc->netdev);
  }

#endif /* NETDEV */


#if BSD

/* There are TWO VERSIONS of interrupt/DMA code: Linux & BSD.
 * Handling Linux and the BSDs with CPP directives would
 *  make the code unreadable, so there are two versions.
 * Conceptually, the two versions do the same thing and
 *  lmc_interrupt() does not know they are different.
 *
 * We are "standing on the head of a pin" in these routines.
 * Tulip CSRs can be accessed, but nothing else is interrupt-safe!
 * Do NOT access: MII, GPIO, SROM, BIOSROM, XILINX, SYNTH, or DAC.
 */

/* Initialize a DMA descriptor ring. */
/* context: kernel (boot) or process (syscall) */
static int  /* BSD version */
create_ring(softc_t *sc, struct desc_ring *ring, int num_descs)
  {
  struct dma_desc *descs;
  int size_descs = sizeof(struct dma_desc)*num_descs;
  int i, error = 0;

  /* The DMA descriptor array must not cross a page boundary. */
  if (size_descs > PAGE_SIZE)
    {
    printf("%s: DMA descriptor array > PAGE_SIZE (%d)\n", NAME_UNIT,
     (u_int)PAGE_SIZE);
    return EINVAL;
    }


  /* Use the DMA tag passed to attach() for descriptors and buffers. */
  ring->tag = sc->pa_dmat;

  /* Allocate wired physical memory for DMA descriptor array. */
  if ((error = bus_dmamem_alloc(ring->tag, size_descs, PAGE_SIZE, 0,
   ring->segs, 1, &ring->nsegs, BUS_DMA_NOWAIT)))
    {
    printf("%s: bus_dmamem_alloc(): error %d\n", NAME_UNIT, error);
    return error;
    }

  /* Map physical address to kernel virtual address. */
  if ((error = bus_dmamem_map(ring->tag, ring->segs, ring->nsegs,
   size_descs, (void **)&ring->first, BUS_DMA_NOWAIT | BUS_DMA_COHERENT)))
    {
    printf("%s: bus_dmamem_map(): error %d\n", NAME_UNIT, error);
    return error;
    }
  descs = ring->first; /* suppress compiler warning about aliasing */
  memset(descs, 0, size_descs);

  /* Allocate dmamap for PCI access to DMA descriptor array. */
  if ((error = bus_dmamap_create(ring->tag, size_descs, 1,
   size_descs, 0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &ring->map)))
    {
    printf("%s: bus_dmamap_create(): error %d\n", NAME_UNIT, error);
    return error;
    }

  /* Map kernel virt addr to PCI bus addr for DMA descriptor array. */
  if ((error = bus_dmamap_load(ring->tag, ring->map, descs, size_descs,
   0, BUS_DMA_NOWAIT)))
    {
    printf("%s: bus_dmamap_load(): error %d\n", NAME_UNIT, error);
    return error;
    }
  ring->dma_addr = ring->map->dm_segs[0].ds_addr;

  /* Allocate dmamaps for each DMA descriptor. */
  for (i=0; i<num_descs; i++)
    if ((error = bus_dmamap_create(ring->tag, MAX_DESC_LEN, 2,
     MAX_CHUNK_LEN, 0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &descs[i].map)))
      {
      printf("%s: bus_dmamap_create(): error %d\n", NAME_UNIT, error);
      return error;
      }


  ring->read  = descs;
  ring->write = descs;
  ring->first = descs;
  ring->last  = descs + num_descs -1;
  ring->last->control = TLP_DCTL_END_RING;
  ring->num_descs = num_descs;
  ring->size_descs = size_descs;
  ring->head = NULL;
  ring->tail = NULL;

  return 0;
  }

/* Destroy a DMA descriptor ring */
/* context: kernel (boot) or process (syscall) */
static void  /* BSD version */
destroy_ring(softc_t *sc, struct desc_ring *ring)
  {
  struct dma_desc *desc;
  struct mbuf *m;

  /* Free queued mbufs. */
  while ((m = mbuf_dequeue(ring)))
    m_freem(m);

  /* TX may have one pkt that is not on any queue. */
  if (sc->tx_mbuf)
    {
    m_freem(sc->tx_mbuf);
    sc->tx_mbuf = NULL;
    }

  /* Unmap active DMA descriptors. */
  while (ring->read != ring->write)
    {
    bus_dmamap_unload(ring->tag, ring->read->map);
    if (ring->read++ == ring->last) ring->read = ring->first;
    }


  /* Free the dmamaps of all DMA descriptors. */
  for (desc=ring->first; desc!=ring->last+1; desc++)
    if (desc->map)
      bus_dmamap_destroy(ring->tag, desc->map);
  /* Unmap PCI address for DMA descriptor array. */
  if (ring->dma_addr)
    bus_dmamap_unload(ring->tag, ring->map);
  /* Free dmamap for DMA descriptor array. */
  if (ring->map)
    bus_dmamap_destroy(ring->tag, ring->map);
  /* Unmap kernel address for DMA descriptor array. */
  if (ring->first)
    bus_dmamem_unmap(ring->tag, (void *)ring->first, ring->size_descs);
  /* Free kernel memory for DMA descriptor array. */
  if (ring->segs[0].ds_addr)
    bus_dmamem_free(ring->tag, ring->segs, ring->nsegs);

  }

/* Singly-linked tail-queues hold mbufs with active DMA.
 * For RX, single mbuf clusters; for TX, mbuf chains are queued.
 * NB: mbufs are linked through their m_nextpkt field.
 * Callers must hold sc->bottom_lock; not otherwise locked.
 */

/* Put an mbuf (chain) on the tail of the descriptor ring queue. */
static void  /* BSD version */
mbuf_enqueue(struct desc_ring *ring, struct mbuf *m)
  {
  m->m_nextpkt = NULL;
  if (ring->tail == NULL)
    ring->head = m;
  else
    ring->tail->m_nextpkt = m;
  ring->tail = m;
  }

/* Get an mbuf (chain) from the head of the descriptor ring queue. */
static struct mbuf*  /* BSD version */
mbuf_dequeue(struct desc_ring *ring)
  {
  struct mbuf *m = ring->head;
  if (m)
    if ((ring->head = m->m_nextpkt) == NULL)
      ring->tail = NULL;
  return m;
  }

/* Clean up after a packet has been received. */
static int  /* BSD version */
rxintr_cleanup(softc_t *sc)
  {
  struct desc_ring *ring = &sc->rxring;
  struct dma_desc *first_desc, *last_desc;
  struct mbuf *first_mbuf=NULL, *last_mbuf=NULL;
  struct mbuf *new_mbuf;
  int pkt_len, desc_len;

  /* Input packet flow control (livelock prevention): */
  /* Give pkts to higher levels only if quota is > 0. */
  if (sc->quota <= 0) return 0;

  /* This looks complicated, but remember: typically packets up */
  /*  to 2048 bytes long fit in one mbuf and use one descriptor. */

  first_desc = last_desc = ring->read;

  /* ASSERTION: If there is a descriptor in the ring and the hardware has */
  /*  finished with it, then that descriptor will have RX_FIRST_DESC set. */
  if ((ring->read != ring->write) && /* descriptor ring not empty */
     !(ring->read->status & TLP_DSTS_OWNER) && /* hardware done */
     !(ring->read->status & TLP_DSTS_RX_FIRST_DESC)) /* should be set */
    panic("%s: rxintr_cleanup: rx-first-descriptor not set.\n", NAME_UNIT);

  /* First decide if a complete packet has arrived. */
  /* Run down DMA descriptors looking for one marked "last". */
  /* Bail out if an active descriptor is encountered. */
  /* Accumulate most significant bits of packet length. */
  pkt_len = 0;
  for (;;)
    {
    if (last_desc == ring->write) return 0;  /* no more descs */
    if (last_desc->status & TLP_DSTS_OWNER) return 0; /* still active */
    if (last_desc->status & TLP_DSTS_RX_LAST_DESC) break; /* end of packet */
    pkt_len += last_desc->length1 + last_desc->length2; /* entire desc filled */
    if (last_desc++->control & TLP_DCTL_END_RING) last_desc = ring->first; /* ring wrap */
    }

  /* A complete packet has arrived; how long is it? */
  /* H/w ref man shows RX pkt length as a 14-bit field. */
  /* An experiment found that only the 12 LSBs work. */
  if (((last_desc->status>>16)&0xFFF) == 0) pkt_len += 4096; /* carry-bit */
  pkt_len = (pkt_len & 0xF000) + ((last_desc->status>>16) & 0x0FFF);
  /* Subtract the CRC length unless doing so would underflow. */
  if (pkt_len >= sc->config.crc_len) pkt_len -= sc->config.crc_len;

  /* Run down DMA descriptors again doing the following:
   *  1) put pkt info in pkthdr of first mbuf,
   *  2) link mbufs,
   *  3) set mbuf lengths.
   */
  first_desc = ring->read;
  do
    {
    /* Read a DMA descriptor from the ring. */
    last_desc = ring->read;
    /* Advance the ring read pointer. */
    if (ring->read++ == ring->last) ring->read = ring->first;

    /* Dequeue the corresponding cluster mbuf. */
    new_mbuf = mbuf_dequeue(ring);
    if (new_mbuf == NULL)
      panic("%s: rxintr_cleanup: expected an mbuf\n", NAME_UNIT);

    desc_len = last_desc->length1 + last_desc->length2;
    /* If bouncing, copy bounce buf to mbuf. */
    DMA_SYNC(last_desc->map, desc_len, BUS_DMASYNC_POSTREAD);
    /* Unmap kernel virtual address to PCI bus address. */
    bus_dmamap_unload(ring->tag, last_desc->map);

    /* 1) Put pkt info in pkthdr of first mbuf. */
    if (last_desc == first_desc)
      {
      first_mbuf = new_mbuf;
      first_mbuf->m_pkthdr.len   = pkt_len; /* total pkt length */
# if IFNET
      first_mbuf->m_pkthdr.rcvif = sc->ifp; /* how it got here */
# else
      first_mbuf->m_pkthdr.rcvif = NULL;
# endif
      }
    else /* 2) link mbufs. */
      {
      KASSERT(last_mbuf != NULL);
      last_mbuf->m_next = new_mbuf;
      /* M_PKTHDR should be set in the first mbuf only. */
      new_mbuf->m_flags &= ~M_PKTHDR;
      }
    last_mbuf = new_mbuf;

    /* 3) Set mbuf lengths. */
    new_mbuf->m_len = (pkt_len >= desc_len) ? desc_len : pkt_len;
    pkt_len -= new_mbuf->m_len;
    } while ((last_desc->status & TLP_DSTS_RX_LAST_DESC)==0);

  /* Decide whether to accept or to drop this packet. */
  /* RxHDLC sets MIIERR for bad CRC, abort and partial byte at pkt end. */
  if (!(last_desc->status & TLP_DSTS_RX_BAD) &&
   (sc->status.link_state == STATE_UP) &&
   (first_mbuf->m_pkthdr.len > 0))
    {
    /* Optimization: copy a small pkt into a small mbuf. */
    if (first_mbuf->m_pkthdr.len <= COPY_BREAK)
      {
      MGETHDR(new_mbuf, M_DONTWAIT, MT_DATA);
      if (new_mbuf)
        {
        new_mbuf->m_pkthdr.rcvif = first_mbuf->m_pkthdr.rcvif;
        new_mbuf->m_pkthdr.len   = first_mbuf->m_pkthdr.len;
        new_mbuf->m_len          = first_mbuf->m_len;
        memcpy(new_mbuf->m_data,   first_mbuf->m_data,
         first_mbuf->m_pkthdr.len);
        m_freem(first_mbuf);
        first_mbuf = new_mbuf;
        }
      }

    /* Include CRC and one flag byte in input byte count. */
    sc->status.cntrs.ibytes += first_mbuf->m_pkthdr.len + sc->config.crc_len +1;
    sc->status.cntrs.ipackets++;

    /* Berkeley Packet Filter */
    LMC_BPF_MTAP(sc, first_mbuf);

    /* Give this good packet to the network stacks. */
    sc->quota--;
    if (sc->stack)
      sc->stack->input(sc, first_mbuf);
    else
      {
      m_freem(first_mbuf);
      sc->status.cntrs.idrops++;
      }
    }
  else if (sc->status.link_state != STATE_UP)
    {
    /* If the link is down, this packet is probably noise. */
    m_freem(first_mbuf);
    sc->status.cntrs.idrops++;
    if (sc->config.debug)
      printf("%s: rxintr_cleanup: rx pkt dropped: link down\n", NAME_UNIT);
    }
  else /* Log and drop this bad packet. */
    {
    if (sc->config.debug)
      printf("%s: RX bad pkt; len=%d %s%s%s%s\n",
       NAME_UNIT, first_mbuf->m_pkthdr.len,
       (last_desc->status & TLP_DSTS_RX_MII_ERR)  ? " miierr"  : "",
       (last_desc->status & TLP_DSTS_RX_DRIBBLE)  ? " dribble" : "",
       (last_desc->status & TLP_DSTS_RX_DESC_ERR) ? " descerr" : "",
       (last_desc->status & TLP_DSTS_RX_OVERRUN)  ? " overrun" : "");
    if (last_desc->status & TLP_DSTS_RX_OVERRUN)
      sc->status.cntrs.fifo_over++;
    else
      sc->status.cntrs.ierrors++;
    m_freem(first_mbuf);
    }

  return 1; /* did something */
  }

/* Setup (prepare) to receive a packet. */
/* Try to keep the RX descriptor ring full of empty buffers. */
static int  /* BSD version */
rxintr_setup(softc_t *sc)
  {
  struct desc_ring *ring = &sc->rxring;
  struct dma_desc *desc;
  struct mbuf *m;
  int desc_len;
  int error;

  /* Ring is full if (wrap(write+1)==read) */
  if (((ring->write == ring->last) ? ring->first : ring->write+1) == ring->read)
    return 0;  /* ring is full; nothing to do */

  /* Allocate a small mbuf and attach an mbuf cluster. */
  MGETHDR(m, M_DONTWAIT, MT_DATA);
  if (m == NULL)
    {
    sc->status.cntrs.rxbuf++;
    if (sc->config.debug)
      printf("%s: rxintr_setup: MGETHDR() failed\n", NAME_UNIT);
    return 0;
    }
  MCLGET(m, M_DONTWAIT);
  if ((m->m_flags & M_EXT)==0)
    {
    m_freem(m);
    sc->status.cntrs.rxbuf++;
    if (sc->config.debug)
      printf("%s: rxintr_setup: MCLGET() failed\n", NAME_UNIT);
    return 0;
    }

  /* Queue the mbuf for later processing by rxintr_cleanup. */
  mbuf_enqueue(ring, m);

  /* Write a DMA descriptor into the ring. */
  /* Hardware will not see it until the OWNER bit is set. */
  desc = ring->write;
  /* Advance the ring write pointer. */
  if (ring->write++ == ring->last) ring->write = ring->first;

  desc_len = (MCLBYTES < MAX_DESC_LEN) ? MCLBYTES : MAX_DESC_LEN;
  /* Map kernel virt addr to PCI bus addr. */
  if ((error = DMA_LOAD(desc->map, m->m_data, desc_len)))
    printf("%s: bus_dmamap_load(rx): error %d\n", NAME_UNIT, error);
  /* Invalidate the cache for this mbuf. */
  DMA_SYNC(desc->map, desc_len, BUS_DMASYNC_PREREAD);

  /* Set up the DMA descriptor. */
  desc->address1 = desc->map->dm_segs[0].ds_addr;
  desc->length1  = desc_len>>1;
  desc->address2 = desc->address1 + desc->length1;
  desc->length2  = desc_len>>1;

  /* Before setting the OWNER bit, flush cache backing DMA descriptors. */
  DMA_SYNC(ring->map, ring->size_descs, BUS_DMASYNC_PREWRITE);

  /* Commit the DMA descriptor to the hardware. */
  desc->status = TLP_DSTS_OWNER;

  /* Notify the receiver that there is another buffer available. */
  WRITE_CSR(sc, TLP_RX_POLL, 1);

  return 1; /* did something */
  }

/* Clean up after a packet has been transmitted. */
/* Free the mbuf chain and update the DMA descriptor ring. */
static int  /* BSD version */
txintr_cleanup(softc_t *sc)
  {
  struct desc_ring *ring = &sc->txring;
  struct dma_desc *desc;

  while ((ring->read != ring->write) && /* while ring is not empty */
        !(ring->read->status & TLP_DSTS_OWNER))
    {
    /* Read a DMA descriptor from the ring. */
    desc = ring->read;
    /* Advance the ring read pointer. */
    if (ring->read++ == ring->last) ring->read = ring->first;

    /* This is a no-op on most architectures. */
    DMA_SYNC(desc->map, desc->length1 + desc->length2, BUS_DMASYNC_POSTWRITE);
    /* Unmap kernel virtual address to PCI bus address. */
    bus_dmamap_unload(ring->tag, desc->map);

    /* If this descriptor is the last segment of a packet, */
    /*  then dequeue and free the corresponding mbuf chain. */
    if (desc->control & TLP_DCTL_TX_LAST_SEG)
      {
      struct mbuf *m;

      if ((m = mbuf_dequeue(ring)) == NULL)
        panic("%s: txintr_cleanup: expected an mbuf\n", NAME_UNIT);

      /* The only bad TX status is fifo underrun. */
      if (desc->status & TLP_DSTS_TX_UNDERRUN)
        {
        if (sc->config.debug)
          printf("%s: txintr_cleanup: tx fifo underrun\n", NAME_UNIT);
        sc->status.cntrs.fifo_under++;
        sc->status.cntrs.oerrors++;
	}
      else
        {
        /* Include CRC and one flag byte in output byte count. */
        sc->status.cntrs.obytes += m->m_pkthdr.len + sc->config.crc_len +1;
        sc->status.cntrs.opackets++;

        /* Berkeley Packet Filter */
        LMC_BPF_MTAP(sc, m);
	}

      m_freem(m);
      return 1;  /* did something */
      }
    }

  return 0;
  }

/* Build DMA descriptors for a transmit packet mbuf chain. */
static int  /* 0=success; 1=error */ /* BSD version */
txintr_setup_mbuf(softc_t *sc, struct mbuf *m)
  {
  struct desc_ring *ring = &sc->txring;
  struct dma_desc *desc;
  unsigned int desc_len;

  /* build DMA descriptors for a chain of mbufs. */
  while (m)
    {
    char *data = m->m_data;
    int length = m->m_len; /* zero length mbufs happen! */

    /* Build DMA descriptors for one mbuf. */
    while (length > 0)
      {
      int error;

      /* Ring is full if (wrap(write+1)==read) */
      if (((ring->temp==ring->last) ? ring->first : ring->temp+1) == ring->read)
        { /* Not enough DMA descriptors; try later. */
        for (; ring->temp!=ring->write;
         ring->temp = (ring->temp==ring->first)? ring->last : ring->temp-1)
          bus_dmamap_unload(ring->tag, ring->temp->map);
        sc->status.cntrs.txdma++; /* IFF_OACTIVE? */
        return 1;
	}

      /* Provisionally, write a descriptor into the ring. */
      /* But do not change the REAL ring write pointer. */
      /* Hardware will not see it until the OWNER bit is set. */
      desc = ring->temp;
      /* Advance the temporary ring write pointer. */
      if (ring->temp++ == ring->last) ring->temp = ring->first;

      /* Clear all control bits except the END_RING bit. */
      desc->control &= TLP_DCTL_END_RING;
      /* Do not pad short packets up to 64 bytes. */
      desc->control |= TLP_DCTL_TX_NO_PAD;
      /* Use Tulip's CRC-32 generator, if appropriate. */
      if (sc->config.crc_len != CFG_CRC_32)
        desc->control |= TLP_DCTL_TX_NO_CRC;
      /* Set the OWNER bit, except in the first descriptor. */
      if (desc != ring->write)
        desc->status = TLP_DSTS_OWNER;

      desc_len = (length > MAX_CHUNK_LEN) ? MAX_CHUNK_LEN : length;
      /* Map kernel virt addr to PCI bus addr. */
      if ((error = DMA_LOAD(desc->map, data, desc_len)))
        printf("%s: bus_dmamap_load(tx): error %d\n", NAME_UNIT, error);
      /* Flush the cache and if bouncing, copy mbuf to bounce buf. */
      DMA_SYNC(desc->map, desc_len, BUS_DMASYNC_PREWRITE);

      /* Prevent wild fetches if mapping fails (nsegs==0). */
      desc->length1  = desc->length2  = 0;
      desc->address1 = desc->address2 = 0;
        {
        bus_dma_segment_t *segs = desc->map->dm_segs;
        int nsegs = desc->map->dm_nsegs;
        if (nsegs >= 1)
          {
          desc->address1 = segs[0].ds_addr;
          desc->length1  = segs[0].ds_len;
          }
        if (nsegs == 2)
          {
          desc->address2 = segs[1].ds_addr;
          desc->length2  = segs[1].ds_len;
          }
        }

      data   += desc_len;
      length -= desc_len;
      } /* while (length > 0) */

    m = m->m_next;
    } /* while (m) */

  return 0; /* success */
  }

/* Setup (prepare) to transmit a packet. */
/* Select a packet, build DMA descriptors and give packet to hardware. */
/* If DMA descriptors run out, abandon the attempt and return 0. */
static int  /* BSD version */
txintr_setup(softc_t *sc)
  {
  struct desc_ring *ring = &sc->txring;
  struct dma_desc *first_desc, *last_desc;

  /* Protect against half-up links: Do not transmit */
  /*  if the receiver can not hear the far end. */
  if (sc->status.link_state != STATE_UP) return 0;

  /* Pick a packet to transmit. */
  if ((sc->tx_mbuf == NULL) && sc->stack)
    sc->stack->output(sc);
  if  (sc->tx_mbuf == NULL) return 0;  /* no pkt to transmit */

  /* Build DMA descriptors for an outgoing mbuf chain. */
  ring->temp = ring->write; /* temporary ring write pointer */
  if (txintr_setup_mbuf(sc, sc->tx_mbuf)) return 0;

  /* Enqueue the mbuf; txintr_cleanup will free it. */
  mbuf_enqueue(ring, sc->tx_mbuf);

  /* The transmitter has room for another packet. */
  sc->tx_mbuf = NULL;

  /* Set first & last segment bits. */
  /* last_desc is the desc BEFORE the one pointed to by ring->temp. */
  first_desc = ring->write;
  first_desc->control |= TLP_DCTL_TX_FIRST_SEG;
  last_desc = (ring->temp==ring->first)? ring->last : ring->temp-1;
   last_desc->control |= TLP_DCTL_TX_LAST_SEG;
  /* Interrupt at end-of-transmission?  Why bother the poor computer! */
/* last_desc->control |= TLP_DCTL_TX_INTERRUPT; */

  /* Make sure the OWNER bit is not set in the next descriptor. */
  /* The OWNER bit may have been set if a previous call aborted. */
  ring->temp->status = 0;

  /* Commit the DMA descriptors to the software. */
  ring->write = ring->temp;

  /* Before setting the OWNER bit, flush cache backing DMA descriptors. */
  DMA_SYNC(ring->map, ring->size_descs, BUS_DMASYNC_PREWRITE);

  /* Commit the DMA descriptors to the hardware. */
  first_desc->status = TLP_DSTS_OWNER;

  /* Notify the transmitter that there is another packet to send. */
  WRITE_CSR(sc, TLP_TX_POLL, 1);

  return 1; /* did something */
  }

/* BSD kernels call this when a hardware interrupt happens. */
static intr_return_t  /* context: interrupt */
bsd_interrupt(void *arg)
  {
  softc_t *sc = arg;

# if DEVICE_POLLING
  if (sc->ifp->if_capenable & IFCAP_POLLING)
    return IRQ_NONE;
# endif

  /* Cut losses early if this is not our interrupt. */
  if ((READ_CSR(sc, TLP_STATUS) & TLP_INT_TXRX)==0)
    return IRQ_NONE;

  /* Process tx and rx pkts. */
  lmc_interrupt(sc, sc->rxring.num_descs, 0);

  return IRQ_HANDLED;
  }

#endif /* BSD */

# if DEVICE_POLLING

/* This procedure services the card without interrupts. */
/* With rxintr_cleanup(), it implements input flow control. */
static void  /* context: softirq */
bsd_poll(struct ifnet *ifp, enum poll_cmd cmd, int quota)
  {
  softc_t *sc = IFP2SC(ifp);

  /* Cut losses early if this is not our interrupt. */
  if ((READ_CSR(sc, TLP_STATUS) & TLP_INT_TXRX)==0)
    return;

  /* Process all tx pkts and up to quota rx pkts. */
  lmc_interrupt(sc, quota, (cmd==POLL_AND_CHECK_STATUS));
  }

# endif /* DEVICE_POLLING */


/* Open a line protocol. */
/* context: kernel (boot) or process (syscall) */
static int
open_proto(softc_t *sc, struct config *config)
  {
  int error = 0;

  if (sc->stack)
    error = sc->stack->open(sc, config);
  else
    error = BSD ? ENOSYS : -ENOSYS;

  return error;
  }

/* Attach a line protocol stack. */
/* context: kernel (boot) or process (syscall) */
static int
attach_stack(softc_t *sc, struct config *config)
  {
  int error = 0;
  struct stack *stack = NULL;

  /* Done if stack is not changing. */
  if (sc->config.stack == config->stack)
    return 0;

  /* Detach the current stack. */
  if (sc->stack && ((error = sc->stack->detach(sc))))
    return error;

  switch (config->stack)
    {
    case STACK_RAWIP: /* built-in */
      stack = &rawip_stack;
      break;

#if SPPP
    case STACK_SPPP:
      stack = &sppp_stack;
      break;
#endif

#if P2P
    case STACK_P2P:
      stack = &p2p_stack;
      break;
#endif

#if GEN_HDLC
    case STACK_GEN_HDLC:
      stack = &gen_hdlc_stack;
      break;
#endif

#if SYNC_PPP
    case STACK_SYNC_PPP:
      stack = &sync_ppp_stack;
      break;
#endif


    default:
      stack = NULL;
      break;
    }

  if (stack)
    error = stack->attach(sc, config);
  else
    error = BSD ? ENOSYS : -ENOSYS;

  return error;
  }


/*
 * This handles IOCTLs from lmcconfig(8).
 * Must not run when card watchdogs run.
 * Always called with top_lock held.
 */
static int  /* context: process */
lmc_ioctl(softc_t *sc, u_long cmd, void *data)
  {
  struct iohdr  *iohdr  = (struct iohdr  *) data;
  struct ioctl  *ioctl  = (struct ioctl  *) data;
  struct status *status = (struct status *) data;
  struct config *config = (struct config *) data;
  int error = 0;

  /* All structs start with a string and a cookie. */
  if (iohdr->cookie != NGM_LMC_COOKIE)
    return EINVAL;

  switch (cmd)
    {
    case LMCIOCGSTAT:
      {
      *status = sc->status;
      iohdr->cookie = NGM_LMC_COOKIE;
      break;
      }
    case LMCIOCGCFG:
      {
      *config = sc->config;
      iohdr->cookie = NGM_LMC_COOKIE;
      break;
      }
    case LMCIOCSCFG:
      {
      if ((error = CHECK_CAP)) break;
      if ((error = attach_stack(sc, config)));
      else error = open_proto(sc, config);
      tulip_loop(sc, config);
      sc->card->attach(sc, config);
      sc->config.debug = config->debug;
      break;
      }
    case LMCIOCREAD:
      {
      if (ioctl->cmd == IOCTL_RW_PCI)
        {
        if (ioctl->address > 252) { error = EFAULT; break; }
        ioctl->data = READ_PCI_CFG(sc, ioctl->address);
	}
      else if (ioctl->cmd == IOCTL_RW_CSR)
        {
        if (ioctl->address >  15) { error = EFAULT; break; }
        ioctl->data = READ_CSR(sc, ioctl->address*TLP_CSR_STRIDE);
	}
      else if (ioctl->cmd == IOCTL_RW_SROM)
        {
        if (ioctl->address >  63) { error = EFAULT; break; }
        ioctl->data = srom_read(sc, ioctl->address);
	}
      else if (ioctl->cmd == IOCTL_RW_BIOS)
        ioctl->data = bios_read(sc, ioctl->address);
      else if (ioctl->cmd == IOCTL_RW_MII)
        ioctl->data = mii_read(sc, ioctl->address);
      else if (ioctl->cmd == IOCTL_RW_FRAME)
        ioctl->data = framer_read(sc, ioctl->address);
      else
        error = EINVAL;
      break;
      }
    case LMCIOCWRITE:
      {
      if ((error = CHECK_CAP)) break;
      if (ioctl->cmd == IOCTL_RW_PCI)
        {
        if (ioctl->address > 252) { error = EFAULT; break; }
        WRITE_PCI_CFG(sc, ioctl->address, ioctl->data);
	}
      else if (ioctl->cmd == IOCTL_RW_CSR)
        {
        if (ioctl->address >  15) { error = EFAULT; break; }
        WRITE_CSR(sc, ioctl->address*TLP_CSR_STRIDE, ioctl->data);
	}
      else if (ioctl->cmd == IOCTL_RW_SROM)
        {
        if (ioctl->address >  63) { error = EFAULT; break; }
        srom_write(sc, ioctl->address, ioctl->data); /* can sleep */
	}
      else if (ioctl->cmd == IOCTL_RW_BIOS)
        {
        if (ioctl->address == 0) bios_erase(sc);
        bios_write(sc, ioctl->address, ioctl->data); /* can sleep */
	}
      else if (ioctl->cmd == IOCTL_RW_MII)
        mii_write(sc, ioctl->address, ioctl->data);
      else if (ioctl->cmd == IOCTL_RW_FRAME)
        framer_write(sc, ioctl->address, ioctl->data);
      else if (ioctl->cmd == IOCTL_WO_SYNTH)
        synth_write(sc, (struct synth *)&ioctl->data);
      else if (ioctl->cmd == IOCTL_WO_DAC)
        {
        dac_write(sc, 0x9002); /* set Vref = 2.048 volts */
        dac_write(sc, ioctl->data & 0xFFF);
	}
      else
        error = EINVAL;
      break;
      }
    case LMCIOCTL:
      {
      if ((error = CHECK_CAP)) break;
      if (ioctl->cmd == IOCTL_XILINX_RESET)
        {
        xilinx_reset(sc);
        sc->card->attach(sc, &sc->config);
	}
      else if (ioctl->cmd == IOCTL_XILINX_ROM)
        {
        xilinx_load_from_rom(sc);
        sc->card->attach(sc, &sc->config);
	}
      else if (ioctl->cmd == IOCTL_XILINX_FILE)
        {
        error = xilinx_load_from_file(sc, ioctl->ucode, ioctl->data);
        if (error) xilinx_load_from_rom(sc); /* try the rom */
        sc->card->attach(sc, &sc->config);
	}
      else if (ioctl->cmd == IOCTL_RESET_CNTRS)
        reset_cntrs(sc);
      else
        error = sc->card->ioctl(sc, ioctl);
      break;
      }
    default:
      error = EINVAL;
      break;
    }

  return error;
  }

/* This is the core watchdog procedure.
 * ioctl syscalls and card watchdog routines must be interlocked.
 * Called by ng_watchdog(), ifnet_watchdog() and netdev_watchdog().
 */
static void  /* context: softirq */
lmc_watchdog(softc_t *sc)
  {
  /* Read and restart the Tulip timer. */
  u_int32_t tx_speed = READ_CSR(sc, TLP_TIMER);
  WRITE_CSR(sc, TLP_TIMER, 0xFFFF);

  /* Measure MII clock using a timer in the Tulip chip.
   * This timer counts transmitter bits divided by 4096.
   * Since this is called once a second the math is easy.
   * This is only correct when the link is NOT sending pkts.
   * On a fully-loaded link, answer will be HALF actual rate.
   * Clock rate during pkt is HALF clk rate between pkts.
   * Measuring clock rate really measures link utilization!
   */
  sc->status.tx_speed = (0xFFFF - (tx_speed & 0xFFFF)) << 12;

  /* Call the card-specific watchdog routine. */
  if (TOP_TRYLOCK(sc))
    {
    /* Remember link_state before updating it. */
    sc->last_link_state = sc->status.link_state;
    /* Update status.link_state. */
    sc->card->watchdog(sc);

    /* Increment a counter which tells user-land */
    /*  observers that SNMP state has been updated. */
    sc->status.ticks++;

    TOP_UNLOCK(sc);
    }
  else
    sc->status.cntrs.lck_watch++;

  /* Kernel date/time can take up to 5 seconds to start running. */
  if ((sc->status.ticks > 3) && /* h/w should be stable by now */
      (sc->status.cntrs.reset_time.tv_sec < 1000))
    {
    microtime(&sc->status.cntrs.reset_time);
    if (sc->status.cntrs.reset_time.tv_sec > 1000)
      reset_cntrs(sc);
    }

  /* Call the stack-specific watchdog routine. */
  if (sc->stack)
    sc->stack->watchdog(sc);

  /* In case an interrupt gets lost, process tx and rx pkts */
  lmc_interrupt(sc, sc->rxring.num_descs, 1);
  }

/* Administrative status of the driver (UP or DOWN) has changed.
 * A card-specific action is taken:
 *  HSSI: drop TA.
 * (T3:   send T3 idle ckt signal. )
 *  SSI:  drop RTS, DTR and DCD
 * (T1:   disable line interface tx; )
 */
static void
set_ready(softc_t *sc, int status)
  {
  struct ioctl ioctl;

  ioctl.cmd = IOCTL_SET_STATUS;
  ioctl.data = status;

  sc->card->ioctl(sc, &ioctl);
  }

static void
reset_cntrs(softc_t *sc)
  {
  memset(&sc->status.cntrs, 0, sizeof(struct cntrs));
  microtime(&sc->status.cntrs.reset_time);
  }

static void  /* context: process, softirq, interrupt! */
lmc_interrupt(void *arg, int quota, int check_status)
  {
  softc_t *sc = arg;
  int activity;

  /* Do this FIRST!  Otherwise UPs deadlock and MPs spin. */
  WRITE_CSR(sc, TLP_STATUS, READ_CSR(sc, TLP_STATUS));

  /* If any CPU is inside this critical section, then */
  /*  other CPUs should go away without doing anything. */
  if (BOTTOM_TRYLOCK(sc) == 0)
    {
    sc->status.cntrs.lck_intr++;
    return;
    }

  /* In Linux, pci_alloc_consistent() means DMA */
  /*  descriptors do not need explicit syncing? */
#if BSD
  {
  struct desc_ring *ring = &sc->txring;
  DMA_SYNC(sc->txring.map, sc->txring.size_descs,
   BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
  ring = &sc->rxring;
  DMA_SYNC(sc->rxring.map, sc->rxring.size_descs,
   BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
  }
#endif

  /* This is the main loop for interrupt processing. */
  sc->quota = quota;
  do
    {
    activity  = txintr_cleanup(sc);
    activity += txintr_setup(sc);
    activity += rxintr_cleanup(sc);
    activity += rxintr_setup(sc);
    } while (activity);

#if BSD
  {
  struct desc_ring *ring = &sc->txring;
  DMA_SYNC(sc->txring.map, sc->txring.size_descs,
   BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
  ring = &sc->rxring;
  DMA_SYNC(sc->rxring.map, sc->rxring.size_descs,
   BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
  }
#endif

  /* As the interrupt is dismissed, check for four unusual events. */
  if (check_status) check_intr_status(sc);

  BOTTOM_UNLOCK(sc);
  }

/* Check for four unusual events:
 *  1) fatal PCI bus errors       - some are recoverable
 *  2) transmitter FIFO underruns - increase fifo threshold
 *  3) receiver FIFO overruns     - clear potential hangup
 *  4) no receive descs or bufs   - count missed packets
 */
static void
check_intr_status(softc_t *sc)
  {
  u_int32_t status, cfcs, op_mode;
  u_int32_t missed, overruns;

  /* 1) A fatal bus error causes a Tulip to stop initiating bus cycles.
   * Module unload/load or boot are the only fixes for Parity Errors.
   * Master and Target Aborts can be cleared and life may continue.
   */
  status = READ_CSR(sc, TLP_STATUS);
  if (status & TLP_STAT_FATAL_ERROR)
    {
    u_int32_t fatal = (status & TLP_STAT_FATAL_BITS)>>TLP_STAT_FATAL_SHIFT;
    printf("%s: FATAL PCI BUS ERROR: %s%s%s%s\n", NAME_UNIT,
     (fatal == 0) ? "PARITY ERROR" : "",
     (fatal == 1) ? "MASTER ABORT" : "",
     (fatal == 2) ? "TARGET ABORT" : "",
     (fatal >= 3) ? "RESERVED (?)" : "");
    cfcs = READ_PCI_CFG(sc, TLP_CFCS);  /* try to clear it */
    cfcs &= ~(TLP_CFCS_MSTR_ABORT | TLP_CFCS_TARG_ABORT);
    WRITE_PCI_CFG(sc, TLP_CFCS, cfcs);
    }

  /* 2) If the transmitter fifo underruns, increase the transmit fifo
   *  threshold: the number of bytes required to be in the fifo
   *  before starting the transmitter (cost: increased tx delay).
   * The TX_FSM must be stopped to change this parameter.
   */
  if (status & TLP_STAT_TX_UNDERRUN)
    {
    op_mode = READ_CSR(sc, TLP_OP_MODE);
    /* enable store-and-forward mode if tx_threshold tops out? */
    if ((op_mode & TLP_OP_TX_THRESH) < TLP_OP_TX_THRESH)
      {
      op_mode += 0x4000;  /* increment TX_THRESH field; can not overflow */
      WRITE_CSR(sc, TLP_OP_MODE, op_mode & ~TLP_OP_TX_RUN);
      /* Wait for the TX FSM to stop; it might be processing a pkt. */
      while (READ_CSR(sc, TLP_STATUS) & TLP_STAT_TX_FSM); /* XXX HANG */
      WRITE_CSR(sc, TLP_OP_MODE, op_mode); /* restart tx */

      if (sc->config.debug)
        printf("%s: tx fifo underrun; threshold now %d bytes\n",
         NAME_UNIT, 128<<((op_mode>>TLP_OP_TR_SHIFT)&3));
      sc->status.cntrs.underruns++;
      }
    }

  /* 3) Errata memo from Digital Equipment Corp warns that 21140A
   *  receivers through rev 2.2 can hang if the fifo overruns.
   * Recommended fix: stop and start the RX FSM after an overrun.
   */
  missed = READ_CSR(sc, TLP_MISSED);
  if ((overruns = ((missed & TLP_MISS_OVERRUN)>>TLP_OVERRUN_SHIFT)))
    {
    if ((READ_PCI_CFG(sc, TLP_CFRV) & 0xFF) <= 0x22)
      {
      op_mode = READ_CSR(sc, TLP_OP_MODE);
      WRITE_CSR(sc, TLP_OP_MODE, op_mode & ~TLP_OP_RX_RUN);
      /* Wait for the RX FSM to stop; it might be processing a pkt. */
      while (READ_CSR(sc, TLP_STATUS) & TLP_STAT_RX_FSM); /* XXX HANG */
      WRITE_CSR(sc, TLP_OP_MODE, op_mode);  /* restart rx */
      }
    if (sc->config.debug)
      printf("%s: rx fifo overruns=%d\n", NAME_UNIT, overruns);
    sc->status.cntrs.overruns += overruns;
    }

  /* 4) When the receiver is enabled and a packet arrives, but no DMA
   *  descriptor is available, the packet is counted as 'missed'.
   * The receiver should never miss packets; warn if it happens.
   */
  if ((missed = (missed & TLP_MISS_MISSED)))
    {
    if (sc->config.debug)
      printf("%s: rx missed %d pkts\n", NAME_UNIT, missed);
    sc->status.cntrs.missed += missed;
    }
  }

/* Initialize the driver. */
/* context: kernel (boot) or process (syscall) */
static int
lmc_attach(softc_t *sc)
  {
  int error = 0;
  struct config config;

  /* Attach the Tulip PCI bus interface. */
  if ((error = tulip_attach(sc))) return error;

  /* Reset the Xilinx Field Programmable Gate Array. */
  xilinx_reset(sc); /* side effect: turns on all four LEDs */

  /* Attach card-specific stuff. */
  sc->card->attach(sc, NULL); /* changes sc->config */

  /* Reset the FIFOs between Gate array and Tulip chip. */
  mii16_set_bits(sc, MII16_FIFO);
  mii16_clr_bits(sc, MII16_FIFO);

#if IFNET
  /* Attach the ifnet kernel interface. */
  if ((error = ifnet_attach(sc))) return error;
#endif

#if NETDEV
  /* Attach the netdevice kernel interface. */
  if ((error = netdev_attach(sc))) return error;
#endif

  /* Attach a protocol stack and open a line protocol. */
  config = sc->config;
  config.stack = STACK_RAWIP;
  attach_stack(sc, &config);
  config.proto = PROTO_IP_HDLC;
  open_proto(sc, &config);

  /* Print obscure card information. */
  if (BOOT_VERBOSE)
    {
    u_int32_t cfrv = READ_PCI_CFG(sc, TLP_CFRV);
    u_int16_t mii3 = mii_read(sc, 3);
    u_int16_t srom[3];
    u_int8_t  *ieee = (u_int8_t *)srom;
    int i;

    printf("%s", NAME_UNIT);
    printf(": PCI rev %d.%d", (cfrv>>4) & 0xF, cfrv & 0xF);
    printf(", MII rev %d.%d", (mii3>>4) & 0xF, mii3 & 0xF);
    for (i=0; i<3; i++) srom[i] = srom_read(sc, 10+i);
    printf(", IEEE addr %02x:%02x:%02x:%02x:%02x:%02x",
     ieee[0], ieee[1], ieee[2], ieee[3], ieee[4], ieee[5]);
    sc->card->ident(sc);
    }

/* BSDs enable card interrupts and appear "ready" here. */
/* Linux does this in netdev_open(). */
#if BSD
  set_ready(sc, 1);
  WRITE_CSR(sc, TLP_INT_ENBL, TLP_INT_TXRX);
#endif

  return 0;
  }

/* context: kernel (boot) or process (syscall) */
static void
lmc_detach(softc_t *sc)
  {
  /* Disable card interrupts and appear "not ready". */
  set_ready(sc, 0);
  WRITE_CSR(sc, TLP_INT_ENBL, TLP_INT_DISABLE);

  /* Detach the line protocol package. */
  if (sc->stack)
    sc->stack->detach(sc);

#if IFNET
  /* Detach the ifnet kernel interface. */
  ifnet_detach(sc);
#endif

#if NETDEV
  /* Detach the netdevice kernel interface. */
  netdev_detach(sc);
#endif

  /* Detach framers, line interfaces, etc. on the card. */
  sc->card->detach(sc);

  /* Detach the Tulip PCI bus interface. */
  tulip_detach(sc);
  }

/* Loop back through the TULIP Ethernet chip; (no CRC).
 * Data sheet says stop DMA before changing OPMODE register.
 * But that's not as simple as it sounds; works anyway.
 */
static void
tulip_loop(softc_t *sc, struct config *config)
  {
  /* Check for enabling loopback thru Tulip chip. */
  if ((sc->config.loop_back != CFG_LOOP_TULIP) &&
         (config->loop_back == CFG_LOOP_TULIP))
    {
    u_int32_t op_mode = READ_CSR(sc, TLP_OP_MODE);
    op_mode |= TLP_OP_INT_LOOP;
    WRITE_CSR(sc, TLP_OP_MODE, op_mode);
    config->crc_len = CFG_CRC_0;
    }

  /* Check for disabling loopback thru Tulip chip. */
  if ((sc->config.loop_back == CFG_LOOP_TULIP) &&
         (config->loop_back != CFG_LOOP_TULIP))
    {
    u_int32_t op_mode = READ_CSR(sc, TLP_OP_MODE);
    op_mode &= ~TLP_OP_LOOP_MODE;
    WRITE_CSR(sc, TLP_OP_MODE, op_mode);
    config->crc_len = CFG_CRC_16;
    }

  sc->config.loop_back = config->loop_back;
  }

/* Attach the Tulip PCI bus interface.
 * Allocate DMA descriptors and enable DMA.
 * Returns 0 on success; error code on failure.
 * context: kernel (boot) or process (syscall)
 */
static int
tulip_attach(softc_t *sc)
  {
  int num_rx_descs, error = 0;
  u_int32_t bus_pbl, bus_cal, op_tr;
  u_int32_t cfdd, cfcs, cflt, csid, cfit;

  /* Make sure the COMMAND bits are reasonable. */
  cfcs = READ_PCI_CFG(sc, TLP_CFCS);
  cfcs &= ~TLP_CFCS_MWI_ENABLE;
  cfcs |=  TLP_CFCS_BUS_MASTER;
  cfcs |=  TLP_CFCS_MEM_ENABLE;
  cfcs |=  TLP_CFCS_IO_ENABLE;
  cfcs |=  TLP_CFCS_PAR_ERROR;
  cfcs |=  TLP_CFCS_SYS_ERROR;
  WRITE_PCI_CFG(sc, TLP_CFCS, cfcs);

  /* Set the LATENCY TIMER to the recommended value, */
  /*  and make sure the CACHE LINE SIZE is reasonable. */
  cfit = READ_PCI_CFG(sc, TLP_CFIT);
  cflt = READ_PCI_CFG(sc, TLP_CFLT);
  cflt &= ~TLP_CFLT_LATENCY;
  cflt |= (cfit & TLP_CFIT_MAX_LAT)>>16;
  /* "prgmbl burst length" and "cache alignment" used below. */
  switch(cflt & TLP_CFLT_CACHE)
    {
    case 8: /* 8 bytes per cache line */
      { bus_pbl = 32; bus_cal = 1; break; }
    case 16:
      { bus_pbl = 32; bus_cal = 2; break; }
    case 32:
      { bus_pbl = 32; bus_cal = 3; break; }
    default:
      {
      bus_pbl = 32; bus_cal = 1;
      cflt &= ~TLP_CFLT_CACHE;
      cflt |= 8;
      break;
      }
    }
  WRITE_PCI_CFG(sc, TLP_CFLT, cflt);

  /* Make sure SNOOZE and SLEEP modes are disabled. */
  cfdd = READ_PCI_CFG(sc, TLP_CFDD);
  cfdd &= ~TLP_CFDD_SLEEP;
  cfdd &= ~TLP_CFDD_SNOOZE;
  WRITE_PCI_CFG(sc, TLP_CFDD, cfdd);
  DELAY(11*1000); /* Tulip wakes up in 10 ms max */

  /* Software Reset the Tulip chip; stops DMA and Interrupts. */
  /* This does not change the PCI config regs just set above. */
  WRITE_CSR(sc, TLP_BUS_MODE, TLP_BUS_RESET); /* self-clearing */
  DELAY(5);  /* Tulip is dead for 50 PCI cycles after reset. */

  /* Initialize the PCI busmode register. */
  /* The PCI bus cycle type "Memory Write and Invalidate" does NOT */
  /*  work cleanly in any version of the 21140A, so do not enable it! */
  WRITE_CSR(sc, TLP_BUS_MODE,
        (bus_cal ? TLP_BUS_READ_LINE : 0) |
        (bus_cal ? TLP_BUS_READ_MULT : 0) |
        (bus_pbl<<TLP_BUS_PBL_SHIFT) |
        (bus_cal<<TLP_BUS_CAL_SHIFT) |
   ((BYTE_ORDER == BIG_ENDIAN) ? TLP_BUS_DESC_BIGEND : 0) |
   ((BYTE_ORDER == BIG_ENDIAN) ? TLP_BUS_DATA_BIGEND : 0) |
                TLP_BUS_DSL_VAL |
                TLP_BUS_ARB);

  /* Pick number of RX descriptors and TX fifo threshold. */
  /* tx_threshold in bytes: 0=128, 1=256, 2=512, 3=1024 */
  csid = READ_PCI_CFG(sc, TLP_CSID);
  switch(csid)
    {
    case CSID_LMC_HSSI:		/* 52 Mb/s */
    case CSID_LMC_HSSIc:	/* 52 Mb/s */
    case CSID_LMC_T3:		/* 45 Mb/s */
      { num_rx_descs = 48; op_tr = 2; break; }
    case CSID_LMC_SSI:		/* 10 Mb/s */
      { num_rx_descs = 32; op_tr = 1; break; }
    case CSID_LMC_T1E1:		/*  2 Mb/s */
      { num_rx_descs = 16; op_tr = 0; break; }
    default:
      { num_rx_descs = 16; op_tr = 0; break; }
    }

  /* Create DMA descriptors and initialize list head registers. */
  if ((error = create_ring(sc, &sc->txring, NUM_TX_DESCS))) return error;
  WRITE_CSR(sc, TLP_TX_LIST, sc->txring.dma_addr);
  if ((error = create_ring(sc, &sc->rxring, num_rx_descs))) return error;
  WRITE_CSR(sc, TLP_RX_LIST, sc->rxring.dma_addr);

  /* Initialize the operating mode register. */
  WRITE_CSR(sc, TLP_OP_MODE, TLP_OP_INIT | (op_tr<<TLP_OP_TR_SHIFT));

  /* Read the missed frame register (result ignored) to zero it. */
  error = READ_CSR(sc, TLP_MISSED); /* error is used as a bit-dump */

  /* Disable rx watchdog and tx jabber features. */
  WRITE_CSR(sc, TLP_WDOG, TLP_WDOG_INIT);

  return 0;
  }

/* Detach the Tulip PCI bus interface. */
/* Disable DMA and free DMA descriptors. */
/* context: kernel (boot) or process (syscall) */
static void
tulip_detach(void *arg)
  {
  softc_t *sc = arg;

  /* Software reset the Tulip chip; stops DMA and Interrupts. */
  WRITE_CSR(sc, TLP_BUS_MODE, TLP_BUS_RESET); /* self-clearing */
  DELAY(5);  /* Tulip is dead for 50 PCI cycles after reset. */

  /* Disconnect from the PCI bus except for config cycles. */
  /* Hmmm; Linux syslogs a warning that IO and MEM are disabled. */
  WRITE_PCI_CFG(sc, TLP_CFCS, TLP_CFCS_MEM_ENABLE | TLP_CFCS_IO_ENABLE);

  /* Free the DMA descriptor rings. */
  destroy_ring(sc, &sc->txring);
  destroy_ring(sc, &sc->rxring);
  }

/* Called during config probing -- softc does not yet exist. */
static void
print_driver_info(void)
  {
  /* Print driver information once only. */
  if (driver_announced++ == 0)
    {
    printf("LMC driver version %d/%d/%d; options",
     VER_YEAR, VER_MONTH, VER_DAY);
    if (ALTQ)           printf(" ALTQ");
                        printf(" BPF"); /* always defined */
    if (NAPI)           printf(" NAPI");
    if (DEVICE_POLLING) printf(" POLL");
    if (P2P)            printf(" P2P");
    if (SPPP)           printf(" SPPP");
    if (GEN_HDLC)       printf(" GEN_HDLC");
    if (SYNC_PPP)       printf(" SYNC_PPP");
    if (NETGRAPH)       printf(" NETGRAPH");
    printf(".\n");
    }
  }



/* This is the I/O configuration interface for NetBSD. */

/* Looking for a DEC 21140A chip on any Lan Media Corp card. */
/* context: kernel (boot) or process (syscall) */
static int
nbsd_match(device_t parent, cfdata_t match, void *aux)
  {
  struct pci_attach_args *pa = aux;
  u_int32_t cfid = pci_conf_read(pa->pa_pc, pa->pa_tag, TLP_CFID);
  u_int32_t csid = pci_conf_read(pa->pa_pc, pa->pa_tag, TLP_CSID);

  if (cfid != TLP_CFID_TULIP) return 0;
  switch (csid)
    {
    case CSID_LMC_HSSI:
    case CSID_LMC_HSSIc:
    case CSID_LMC_T3:
    case CSID_LMC_SSI:
    case CSID_LMC_T1E1:
      print_driver_info();
      return 100;
    default:
      return 0;
    }
  }

/* NetBSD bottom-half initialization. */
/* context: kernel (boot) or process (syscall) */
static void
nbsd_attach(device_t parent, device_t self, void *aux)
  {
  softc_t *sc = device_private(self);
  struct pci_attach_args *pa = aux;
  const char *intrstr;
  bus_addr_t csr_addr;
  int error;
  char intrbuf[PCI_INTRSTR_LEN];

  /* for READ/WRITE_PCI_CFG() */
  sc->sc_dev = self;
  sc->pa_pc   = pa->pa_pc;
  sc->pa_tag  = pa->pa_tag;
  sc->pa_dmat = pa->pa_dmat;

  /* What kind of card are we driving? */
  switch (READ_PCI_CFG(sc, TLP_CSID))
    {
    case CSID_LMC_HSSI:
    case CSID_LMC_HSSIc:
      sc->dev_desc =  HSSI_DESC;
      sc->card     = &hssi_card;
      break;
    case CSID_LMC_T3:
      sc->dev_desc =    T3_DESC;
      sc->card     =   &t3_card;
      break;
    case CSID_LMC_SSI:
      sc->dev_desc =   SSI_DESC;
      sc->card     =  &ssi_card;
      break;
    case CSID_LMC_T1E1:
      sc->dev_desc =  T1E1_DESC;
      sc->card     =   &t1_card;
      break;
    default:
      return;
    }

  /* Allocate PCI resources to access the Tulip chip CSRs. */
# if IOREF_CSR
  csr_addr = (bus_addr_t)READ_PCI_CFG(sc, TLP_CBIO) & -2;
  sc->csr_tag = pa->pa_iot;	/* bus_space tag for IO refs */
# else
  csr_addr = (bus_addr_t)READ_PCI_CFG(sc, TLP_CBMA);
  sc->csr_tag = pa->pa_memt;	/* bus_space tag for MEM refs */
# endif
  if ((error = bus_space_map(sc->csr_tag, csr_addr,
   TLP_CSR_SIZE, 0, &sc->csr_handle)))
    {
    aprint_error("%s: bus_space_map(): error %d\n", NAME_UNIT, error);
    return;
    }

  /* Allocate PCI interrupt resources. */
  if (pci_intr_map(pa, &sc->intr_handle))
    {
    aprint_error("%s: pci_intr_map() failed\n", NAME_UNIT);
    nbsd_detach(self, 0);
    return;
    }
  if ((sc->irq_cookie = pci_intr_establish(pa->pa_pc, sc->intr_handle,
   IPL_NET, bsd_interrupt, sc)) == NULL)
    {
    aprint_error("%s: pci_intr_establish() failed\n", NAME_UNIT);
    nbsd_detach(self, 0);
    return;
    }
  intrstr = pci_intr_string(pa->pa_pc, sc->intr_handle, intrbuf, sizeof(intrbuf));
  aprint_normal(" %s: %s\n", intrstr, sc->dev_desc);
  aprint_naive(": %s\n", sc->dev_desc);

  /* Install a shutdown hook. */
  if ((sc->sdh_cookie = shutdownhook_establish(tulip_detach, sc)) == NULL)
    {
    aprint_error("%s: shutdown_hook_establish() failed\n", NAME_UNIT);
    nbsd_detach(self, 0);
    return;
    }

  /* Initialize the top-half and bottom-half locks. */
  mutex_init(&sc->top_lock, MUTEX_DEFAULT, IPL_VM);
  __cpu_simple_lock_init(&sc->bottom_lock);

  /* Initialize the driver. */
  if ((error = lmc_attach(sc))) nbsd_detach(self, 0);
  }

/* context: kernel (boot) or process (syscall) */
static int
nbsd_detach(device_t self, int flags)
  {
  softc_t *sc = device_private(self);

  /* Detach from the bus and the kernel. */
  lmc_detach(sc);

  /* Release resources. */
  if (sc->sdh_cookie)
    shutdownhook_disestablish(sc->sdh_cookie);
  if (sc->irq_cookie)
    pci_intr_disestablish(sc->pa_pc, sc->irq_cookie);
  if (sc->csr_handle)
    bus_space_unmap(sc->csr_tag, sc->csr_handle, TLP_CSR_SIZE);

  /* Destroy locks. */
  mutex_destroy(&sc->top_lock);

  return 0;
  }

CFATTACH_DECL_NEW(lmc, sizeof(softc_t),		/* lmc_ca */
 nbsd_match, nbsd_attach, nbsd_detach, NULL);
