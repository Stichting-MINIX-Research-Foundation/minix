/*******************************************************************************
Copyright (C) Marvell International Ltd. and its affiliates

Developed by Semihalf

********************************************************************************
Marvell BSD License

If you received this File from Marvell, you may opt to use, redistribute and/or
modify this File under the following licensing terms.
Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    *   Redistributions of source code must retain the above copyright notice,
            this list of conditions and the following disclaimer.

    *   Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.

    *   Neither the name of Marvell nor the names of its contributors may be
        used to endorse or promote products derived from this software without
        specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

#ifndef _MVSPIREG_H_
#define _MVSPIREG_H_

#define		MVSPI_SIZE			0x50		/* Size of MVSPI */

/* Definition of registers */
#define		MVSPI_CTRL_REG			0x00		/* MVSPI Control Register */
#define		MVSPI_INTCONF_REG		0x04		/* MVSPI Interface Configuration Register */
#define		MVSPI_DATAOUT_REG		0x08		/* MVSPI Data Out Register */
#define		MVSPI_DATAIN_REG		0x0C		/* MVSPI Data In Register */
#define		MVSPI_IRQCAUSE_REG		0x10		/* MVSPI Interrupt Cause Register */
#define		MVSPI_IRQMASK_REG		0x14		/* MVSPI Interrupt Mask Register */
#define		MVSPI_TIMEPAR1_REG		0x18		/* MVSPI Timing Parameters 1 Register*/
#define		MVSPI_TIMEPAR2_REG		0x1C		/* MVSPI Timing Parameters 2 Register */
#define		MVSPI_DIRWRITE_REG		0x20		/* MVSPI Direct Write Configuration Register*/ 
#define		MVSPI_DIRWRITEHD_REG		0x24		/* MVSPI Direct Write Header Register */
#define		MVSPI_DIRREADHD_REG		0x28		/* MVSPI Direct Read Header Register */
#define		MVSPI_CSADRDEC_REG		0x2C		/* MVSPI CS Address Decode Register */
#define		MVSPI_CSnTIMPAR_REG		0x30		/* MVSPI CSn Timing Parameters Register */
#define		MVSPI_CNTVER_REG		0x50		/* MVSPI Controller Version Register */

/* Masks */
#define		MVSPI_CPOL_MASK			0x0800		/* CPOL bit = 1 */
#define		MVSPI_CPHA_MASK			0x1000		/* CPHA bit = 1 */
#define		MVSPI_DIRHS_MASK		0xFBFF		/* SPI Direct Read High Speed Transaction Mask */
#define		MVSPI_1BYTE_MASK		0xFFDF		/* Number of bits in each I/O transfer Mask */
#define		MVSPI_SPR_MASK			0x0007		/* SPR field mask */
#define		MVSPI_SPPR_MASK			0x00D0		/* SPPR field mask */
#define		MVSPI_SPPRHI_MASK		0x00C0		/* SPPR_HI field mask */
#define		MVSPI_SPPR0_MASK		0x0010		/* SPPR0 field mask */
#define		MVSPI_CSNACT_MASK		0x0001		/* CSn transfer acknowledge bit */

#define		MVSPI_CR_SMEMRDY		0x0002		/* MVSPI Control Register Serial Memory Data Transfer Ready */

#define		MVSPI_DUMMY_BYTE		0xFF		/* Dummy byte */

#define		MVSPI_WAIT_RDY_MAX_LOOP		100000		/* Transfer timeout threshold */
#define		MVSPI_SPR_MAXVALUE		15		/* Maximum value for SPR coeficient */
#define		MVSPI_SPPR_MAXVALUE		7		/* Maximum value for SPPR coeficient */

#endif		/* _MVSPIREG_H_ */
