/*      $NetBSD: mcp23s17.h,v 1.1 2014/04/06 17:59:39 kardel Exp $ */

/*-
 * Copyright (c) 2014 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Frank Kardel.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
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
#ifndef MCP23S17_H
#define MCP23S17_H

/*
 * SPI connected 16-bit general purpose parallel I/O
 *
 * see http://ww1.microchip.com/downloads/en/DeviceDoc/21952b.pdf
 */

/* resources */
#define MCP23x17_GPIO_NPINS	16
#define MCP23x17_GPIO_NPINS_MASK (MCP23x17_GPIO_NPINS-1)

/* address layout */
#define MCP23x17_BANKADDR(BANK, ADDR, PORT) (((BANK) == 1) ? ((PORT) << 4 | (ADDR)) : ((ADDR)<<1 | (PORT)))

#define MCP23x17_IODIR(BANK, PORT) MCP23x17_BANKADDR(BANK, 0x0, PORT)
#define MCP23x17_IODIRA(BANK)   MCP23x17_IODIR(BANK, 0)
#define MCP23x17_IODIRB(BANK)   MCP23x17_IODIR(BANK, 1)
#define MCP23x17_IPOL(BANK, PORT) MCP23x17_BANKADDR(BANK, 0x1, PORT)
#define MCP23x17_IPOLA(BANK)    MCP23x17_IPOL(BANK, 0)
#define MCP23x17_IPOLB(BANK)    MCP23x17_IPOL(BANK, 1)
#define MCP23x17_GPINTEN(BANK, PORT) MCP23x17_BANKADDR(BANK, 0x2, PORT)
#define MCP23x17_GPINTENA(BANK) MCP23x17_GPINTEN(BANK, 0)
#define MCP23x17_GPINTENB(BANK) MCP23x17_GPINTEN(BANK, 1)
#define MCP23x17_DEFVAL(BANK, PORT)  MCP23x17_BANKADDR(BANK, 0x3, PORT)
#define MCP23x17_DEFVALA(BANK)  MCP23x17_DEFVAL(BANK, 0)
#define MCP23x17_DEFVALB(BANK)  MCP23x17_DEFVAL(BANK, 1)
#define MCP23x17_INTCON(BANK, PORT)  MCP23x17_BANKADDR(BANK, 0x4, PORT)
#define MCP23x17_INTCONA(BANK)  MCP23x17_INTCON(BANK, 0)
#define MCP23x17_INTCONB(BANK)  MCP23x17_INTCON(BANK, 0)
#define MCP23x17_IOCON(BANK, PORT)   MCP23x17_BANKADDR(BANK, 0x5, PORT)
#define MCP23x17_IOCONA(BANK)   MCP23x17_IOCON(BANK, 0)
#define MCP23x17_IOCONB(BANK)   MCP23x17_IOCON(BANK, 1)
#define MCP23x17_GPPU(BANK, PORT)    MCP23x17_BANKADDR(BANK, 0x6, PORT)
#define MCP23x17_GPPUA(BANK)     MCP23x17_GPPU(BANK, 0)
#define MCP23x17_GPPUB(BANK)     MCP23x17_GPPU(BANK, 1)
#define MCP23x17_INTF(BANK, PORT)    MCP23x17_BANKADDR(BANK, 0x7, PORT)
#define MCP23x17_INTFA(BANK)    MCP23x17_INTF(BANK, 0)
#define MCP23x17_INTFB(BANK)    MCP23x17_INTF(BANK, 1)
#define MCP23x17_INTCAP(BANK, PORT)  MCP23x17_BANKADDR(BANK, 0x8, PORT)
#define MCP23x17_INTCAPA(BANK)  MCP23x17_INTCAP(BANK, 0)
#define MCP23x17_INTCAPB(BANK)  MCP23x17_INTCAP(BANK, 1)
#define MCP23x17_GPIO(BANK, PORT)    MCP23x17_BANKADDR(BANK, 0x9, PORT)
#define MCP23x17_GPIOA(BANK)    MCP23x17_GPIO(BANK, 0)
#define MCP23x17_GPIOB(BANK)    MCP23x17_GPIO(BANK, 1)
#define MCP23x17_OLAT(BANK, PORT)    MCP23x17_BANKADDR(BANK, 0xA, PORT)
#define MCP23x17_OLATA(BANK)    MCP23x17_OLAT(BANK, 0)
#define MCP23x17_OLATB(BANK)    MCP23x17_OLAT(BANK, 1)

/* commands */
#define MCP23x17_OP_BASE_WR     0x40 /* write register */
#define MCP23x17_OP_BASE_RD     0x41 /* read register */
#define MCP23x17_OP_READ(HA)    (MCP23x17_OP_BASE_RD|(HA)<<1)
#define MCP23x17_OP_WRITE(HA)   (MCP23x17_OP_BASE_WR|(HA)<<1)

/* bits */
#define MCP23x17_IOCON_BANK	__BIT(7) /* select address layout */
#define MCP23x17_IOCON_MIRROR	__BIT(6) /* mirror INTA/INTB interrupt outputs */
#define MCP23x17_IOCON_SEQOP	__BIT(5) /* sequential address operation */
#define MCP23x17_IOCON_DISLW	__BIT(4) /* slew rate SDA output */
#define MCP23x17_IOCON_HAEN	__BIT(3) /* hardware address enable bit (only MCP23S17) */
#define MCP23x17_IOCON_ODR	__BIT(2) /* configure INT pin as open drain */
#define MCP23x17_IOCON_INTPOL	__BIT(1) /* INT pin polarity (unless ODR is set) */

#endif
