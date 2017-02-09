/*	$NetBSD: z8536reg.h,v 1.3 2012/01/31 22:13:19 hauke Exp $	*/

/*-
 * Copyright (c) 2008 Hauke Fath
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
 */

/*
 * Zilog Z8536 CIO (Counter/Timer and Parallel I/O Unit)
 * Register Definitions
 *
 * The CIO has four registers: One control register, and three data
 * registers for ports A/B/C. To set up the CIO through the control
 * register, first write the number of the internal register to it,
 * then access or set the selected register contents. Once selected,
 * an internal register can be polled continuously by reading out the
 * control port.
 *
 * Internal registers are read-writable, except where noted.
 */
#define Z8536_IOSIZE		0x04

#define Z8536_MICR		0x00	/* Master Interrupt Control Register */
#define       MICR_RESET	0x01	/* Chip Reset */
#define	      MICR_RJA		0x02	/* Only z8036 (ZBUS version) */
#define	      MICR_CTVIS	0x04	/* CT     vector includes status */
#define	      MICR_PBVIS	0x08	/* Port B vector includes status */
#define	      MICR_PAVIS	0x10	/* Port A vector includes status */
#define	      MICR_NV		0x20	/* No Vector (NV) */
#define	      MICR_DLC		0x40	/* Disable Lower Chain (DLC) */
#define	      MICR_MIE		0x80	/* Master Interrupt Enable (MIE) */

#define Z8536_MCCR         	0x01	/* Master Configuration Register */
#define       MCCR_CTINDPT   	0x00 	/* Counter/Timers Independent */
#define       MCCR_CT1GT2  	0x01 	/* CT 1 /OUTPUT gates CT 2 */
#define       MCCR_CT1TR2  	0x02 	/* CT 1 /OUTPUT triggers CT 2 */
#define       MCCR_CT1CT2  	0x03 	/* CT 1 /OUTPUT is CT 2's COUNT */
#define       MCCR_PAE     	0x04 	/* Port A Enable */
#define       MCCR_PLC     	0x08 	/* Port Link Control (A/B) */
#define       MCCR_PC_CT3E    	0x10 	/* Counter/Timer 3 + Port C Enable */
#define       MCCR_CT2E    	0x20 	/* Counter/Timer 2 Enable */
#define       MCCR_CT1E    	0x40	/* Counter/Timer 1 Enable */
#define       MCCR_PBE     	0x80	/* Port B Enable */

/* Interrupt Vector Registers */
#define Z8536_IVRA       	0x02	/* Port A Interrupt Vector */
#define Z8536_IVRB       	0x03	/* Port B Interrupt Vector */
#define Z8536_IVRCT		0x04	/* Counter/Timer Interrupt Vector */

/* Port C setup */
#define Z8536_DPPRC        	0x05	/* Port C Data Path Polarity */
#define Z8536_DDRC         	0x06	/* Port C Data Direction */
#define Z8536_SIOCRC       	0x07	/* Port C Special I/O Control */

#define Z8536_PCSRA        	0x08	/* Port A Command and Status */
#define Z8536_PCSRB        	0x09	/* Port B Command and Status */

/* Z8536_PCSRA + Z8536_PCSRB command and status bits */
#define      PCSR_IOE		0x01	/* Interrupt on error */
#define      PCSR_PMF		0x02	/* Pattern match flag (RO) */
#define      PCSR_IRF		0x04	/* Input register full (RO) */
#define      PCSR_ORE		0x08	/* Output register empty (RO) */
#define      PCSR_ERR		0x10	/* Interrupt error */
#define      PCSR_IP		0x20	/* Interrupt pending */
#define      PCSR_IE		0x40	/* Interrupt enable */
#define      PCSR_IUS		0x80	/* Interrupt under service */
/* PCSR{A,B} interrupt bits: IUS/IE/IP */
#define       PCSR_NULL		0x00  	/* Null Code */
#define       PCSR_CLR_IP_IUS	0x20  	/* Clear IP and IUS */
#define       PCSR_SET_IUS	0x40  	/* Set Interrupt Under Service */
#define       PCSR_CLR_IUS	0x60  	/* Clear Interrupt Under Service */
#define       PCSR_SET_IP	0x80  	/* Set Interrupt Pending */
#define       PCSR_CLR_IP	0xA0  	/* Clear Interrupt Pending */
#define       PCSR_SET_IE	0xC0  	/* Set Interrupt Enable */
#define       PCSR_CLR_IE	0xE0  	/* Clear Interrupt Enable */

/* Counter/Timer 1..3 Command and Status Registers */
#define Z8536_CTCSR1       	0x0A 	/* CT 1 Command and Status */
#define Z8536_CTCSR2       	0x0B 	/* CT 2 Command and Status */
#define Z8536_CTCSR3       	0x0C 	/* CT 3 Command and Status */

/* CTCSR setup bits */
#define       CTCS_CIP     	0x01 	/* Count in Progress (RO) */
#define       CTCS_TCB     	0x02 	/* Trigger Command Bit (WO) */
#define       CTCS_GCB     	0x04 	/* Gate Command Bit */
#define       CTCS_RCC     	0x08 	/* Read Counter Control */
#define       CTCS_ERR     	0x10 	/* Interrupt Error (RO) */
#define       CTCS_IP      	0x20 	/* Interrupt Pending */
#define       CTCS_IE      	0x40 	/* Interrupt Enable */
#define       CTCS_IUS     	0x80 	/* Interrupt Under Service */

/* CTCSR interrupt bits: IUS/IE/IP */
#define       CTCS_NULL		0x00  	/* Null Code */
#define       CTCS_CLR_IP_IUS	0x20  	/* Clear IP and IUS */
#define       CTCS_SET_IUS	0x40  	/* Set Interrupt Under Service */
#define       CTCS_CLR_IUS	0x60  	/* Clear Interrupt Under Service */
#define       CTCS_SET_IP	0x80  	/* Set Interrupt Pending */
#define       CTCS_CLR_IP	0xA0  	/* Clear Interrupt Pending */
#define       CTCS_SET_IE	0xC0  	/* Set Interrupt Enable */
#define       CTCS_CLR_IE	0xE0  	/* Clear Interrupt Enable */

/* Avoid changing intr bits unintendedly */
#define	CTCSR_MASK(FLAGS)	((FLAGS) & 0x3f)

/* The port data registers are directly accessible at their own IO address */
#define Z8536_PDRA         	0x0D	/* Port A Data Register */
#define Z8536_PDRB         	0x0E	/* Port B Data Register */
#define Z8536_PDRC         	0x0F	/* Port C Data Register */

/* Bytewise access to current count registers (read-only) */ 
#define Z8536_CTCCR1_MSB 	0x10	/* CT 1 Current Count MSB  */
#define Z8536_CTCCR1_LSB 	0x11	/* CT 1 Current Count LSB  */
#define Z8536_CTCCR2_MSB 	0x12	/* CT 2 Current Count MSB  */
#define Z8536_CTCCR2_LSB 	0x13	/* CT 2 Current Count LSB  */
#define Z8536_CTCCR3_MSB 	0x14	/* CT 3 Current Count MSB  */
#define Z8536_CTCCR3_LSB 	0x15	/* CT 3 Current Count LSB  */

/* Bytewise access to time constant registers */
#define Z8536_CTTCR1_MSB 	0x16	/* CT 1 Time Constant MSB  */
#define Z8536_CTTCR1_LSB 	0x17	/* CT 1 Time Constant LSB  */
#define Z8536_CTTCR2_MSB 	0x18	/* CT 2 Time Constant MSB  */
#define Z8536_CTTCR2_LSB 	0x19	/* CT 2 Time Constant LSB  */
#define Z8536_CTTCR3_MSB 	0x1A	/* CT 3 Time Constant MSB  */
#define Z8536_CTTCR3_LSB 	0x1B	/* CT 3 Time Constant LSB  */

/* Counter/Timer Mode specification */
#define Z8536_CTMSR1       	0x1C 	/* CT 1 Mode Specification */
#define Z8536_CTMSR2       	0x1D 	/* CT 2 Mode Specification */
#define Z8536_CTMSR3       	0x1E 	/* CT 3 Mode Specification */
#define       CTMS_DCS_PULSE	0x00  	/* Pulse Output */
#define       CTMS_DCS_ONESHOT	0x01  	/* One-Shot Output */
#define       CTMS_DCS_SQUARE	0x02  	/* Square Wave Output */
#define       CTMS_REB		0x04  	/* Retrigger Enable */
#define       CTMS_EGE		0x08  	/* External Gate Enable */
#define       CTMS_ETE		0x10  	/* External Trigger Enable  */
#define       CTMS_ECE		0x20  	/* External Count Enable */
#define       CTMS_EOE		0x40  	/* External Output Enable */
#define       CTMS_CSC		0x80  	/* Continuous / Single Cycle */

#define Z8536_CVR         	0x1F	/* Current Interrupt Vector (RO) */

/* Port A specification registers */
#define Z8536_PMSRA        	0x20	/* Port A Mode Specification */
#define Z8536_PHSRA        	0x21	/* Port A Handshake Specification */
#define Z8536_DPPRA        	0x22	/* Port A Data Path Polarity */
#define Z8536_DDRA         	0x23	/* Port A Data Direction */
#define Z8536_SIOCRA       	0x24	/* Port A Special I/O Control */
#define Z8536_PPRA         	0x25	/* Port A Pattern Polarity */
#define Z8536_PTRA         	0x26	/* Port A Pattern Transition */
#define Z8536_PMRA         	0x27	/* Port A Pattern Mask */

/* Port B specification registers */
#define Z8536_PMSRB        	0x28	/* Port B Mode Specification */
#define Z8536_PHSRB        	0x29	/* Port B Handshake Specification */
#define Z8536_DPPRB        	0x2A	/* Port B Data Path Polarity */
#define Z8536_DDRB         	0x2B	/* Port B Data Direction */
#define Z8536_SIOCRB       	0x2C	/* Port B Special I/O Control */
#define Z8536_PPRB         	0x2D	/* Port B Pattern Polarity */
#define Z8536_PTRB         	0x2E	/* Port B Pattern Transition */
#define Z8536_PMRB         	0x2F	/* Port B Pattern Mask */

/* Bit definitions, common to ports A and B */

/* Z8536_PMSRA + Z8536_PMSRB port mode specification bits */
#define       PMSR_LPM		0x01	/* Bit mode: latched */
#define       PMSR_DTE		0x01	/* Hsk mode: deskew timer enable */
/*
 *	PMS1	PMS0	Pattern mode specification
 *	0	0	disable pattern match
 *	0	1	"and" mode, transition-triggered interrupt
 *	1	0	"or" mode, transition-triggered interrupt
 *	1	1	"or-priority encoded vector" mode, level-
 *			triggered interrupt (only transparent LPM mode)
 */
#define       PMSR_PMS0		0x02
#define       PMSR_PMS1		0x04
#define       PMSR_PMS_OFF	0x00	/* Disable pattern match */
#define       PMSR_PMS_AND	0x02	/* "and" mode, transition-triggered */
#define       PMSR_PMS_OR	0x04	/* "or" mode, transition-triggered */
/*
 * "or-priority encoded vector" mode, level-triggered interrupt
 * (only in transparent LPM mode)
 */
#define       PMSR_PMS_OR_PEV	0x06	
#define       PMSR_IMO		0x08	/* Interrupt on match only */
#define       PMSR_SB		0x10	/* Single buffered mode */
#define       PMSR_ITB		0x20	/* Interrupt on two bytes */
/*
 *	PTS1	PTS0	Port type selects
 *	0	0	bit port
 *	0	1	input port
 *	1	0	output port
 *	1	1	bidirectional port
 */
#define       PMSR_PTS0		0x40
#define       PMSR_PTS1		0x80
#define       PMSR_PTS_BIT	0x00
#define       PMSR_PTS_IN	0x40
#define       PMSR_PTS_OUT	0x80
#define       PMSR_PTS_BIDI	0xC0
/*
 * Z8536_PHSRA + Z8536_PHSRB port handshake specification bits
 * Bits 0-2 set deskew timer for output ports
 *
 *	RWS2	RWS1	RWS0	Status signals on port C
 *	0	0	0	REQUEST/-WAIT disabled
 *	0	0	1	output -WAIT
 *	0	1	1	input -WAIT
 *	1	0	0	special REQUEST
 *	1	0	1	output REQUEST
 *	1	1	1	input REQUEST
 */
#define	      PHSR_RWS0		0x08
#define	      PHSR_RWS1		0x10
#define	      PHSR_RWS2		0x20
/*
 *	HTS1	HTS0	Handshake type specification
 *	0	0	interlocked handshake
 *	0	1	strobed handshake
 *	1	0	pulsed handshake
 *	1	1	three-wire-handshake
 */
#define       PHSR_HTS0		0x40
#define	      PHSR_HTS1		0x80
#define	      PHSR_HTS_INT	0x00
#define	      PHSR_HTS_STR	0x40
#define	      PHSR_HTS_PUL	0x80
#define	      PHSR_HTS_TWI	0xC0
