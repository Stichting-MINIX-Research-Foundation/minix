/*      $NetBSD: jmide_reg.h,v 1.4 2011/10/24 16:06:43 njoly Exp $    */

/*
 * Copyright (c) 2007 Manuel Bouyer.
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/* registers definitions for the JMicon JMB36x IDE/SATA controllers */

#define PCI_JM_CONTROL0	0x40 /* controller control register 0 */
#define JM_CONTROL0_ROM_EN	0x80000000 /* external ROM enable */
#define JM_CONTROL0_ID_WR	0x40000000 /* device ID write enable */
#define JM_CONTROL0_PCIIDE0_MS	0x00800000 /* sata M/S on chan0,  PATA0 on chan1 */
#define JM_CONTROL0_PCIIDE_CS	0x00400000 /* pciide channels swap */
#define JM_CONTROL0_SATA_PS	0x00200000 /* SATA channel M/S swap */
#define JM_CONTROL0_AHCI_PS	0x00100000 /* SATA AHCI ports swap */
#define JM_CONTROL0_SATA1_AHCI	0x00008000 /* SATA port 1 AHCI enable */
#define JM_CONTROL0_SATA1_IDE	0x00004000 /* SATA port 1 PCIIDE enable */
#define JM_CONTROL0_SATA0_AHCI	0x00002000 /* SATA port 0 AHCI enable */
#define JM_CONTROL0_SATA0_IDE	0x00001000 /* SATA port 0 PCIIDE enable */
#define JM_CONTROL0_AHCI_F1	0x00000200 /* AHCI on function 1 */
#define JM_CONTROL0_AHCI_EN	0x00000100 /* AHCI enable */
#define JM_CONTROL0_PATA0_RST	0x00000040 /* PATA port 0 reset */
#define JM_CONTROL0_PATA0_EN	0x00000020 /* PATA port 0 enable */
#define JM_CONTROL0_PATA0_SEC	0x00000010 /* PATA 0 enable on secondary chan */
#define JM_CONTROL0_PATA0_40P	0x00000008 /* PATA 0 40pin cable */
#define JM_CONTROL0_PCIIDE_F1	0x00000002 /* PCIIDE on function 1 */
#define JM_CONTROL0_PATA0_PRI	0x00000001 /* PATA 0 enable on primary chan */

#define PCI_JM_CONTROL1 0x80 /* controller control register 5 */
#define JM_CONTROL1_PATA1_PRI	0x01000000 /* force PATA 1 on chan0 */
#define JM_CONTROL1_PATA1_RST	0x00400000 /* PATA 1 reset */
#define JM_CONTROL1_PATA1_EN	0x00200000 /* PATA 1 enable */
#define JM_CONTROL1_PATA1_40P	0x00080000 /* PATA 1 40pin cable */
