/*	$NetBSD: i128reg.h,v 1.3 2008/04/29 06:53:02 martin Exp $ */

/*-
 * Copyright (c) 2007 Michael Lorenz
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: i128reg.h,v 1.3 2008/04/29 06:53:02 martin Exp $");

/* 
 * register definition for Number Nine Imagine 128 graphics controllers
 *
 * adapted from XFree86's i128 driver source
 */

#ifndef I128REG_H
#define I128REG_H

#define INTP     0x4000
#define  INTP_DD_INT 0x01	/* drawing op completed  */
#define  INTP_CL_INT 0x02
#define INTM     0x4004
#define  INTM_DD_MSK 0x01
#define  INTM_CL_MSK 0x02
#define FLOW     0x4008
#define  FLOW_DEB    0x01	/* drawing engine busy   */
#define  FLOW_MCB    0x02	/* mem controller busy   */
#define  FLOW_CLP    0x04
#define  FLOW_PRV    0x08	/* prev cmd still running or cache ready */
#define BUSY     0x400C
#define  BUSY_BUSY   0x01	/* command pipeline busy */
#define XYW_AD   0x4010
#define Z_CTRL   0x4018
#define BUF_CTRL 0x4020
#define  BC_AMV      0x02
#define  BC_MP       0x04
#define  BC_AMD      0x08
#define  BC_SEN_MSK  0x0300
#define  BC_SEN_DB   0x0000
#define  BC_SEN_VB   0x0100
#define  BC_SEN_MB   0x0200
#define  BC_SEN_CB   0x0300
#define  BC_DEN_MSK  0x0C00
#define  BC_DEN_DB   0x0000
#define  BC_DEN_VB   0x0400
#define  BC_DEN_MB   0x0800
#define  BC_DEN_CB   0x0C00
#define  BC_DSE      0x1000
#define  BC_VSE      0x2000
#define  BC_MSE      0x4000
#define  BC_PS_MSK   0x001F0000
#define  BC_MDM_MSK  0x00600000
#define  BC_MDM_KEY  0x00200000
#define  BC_MDM_PLN  0x00400000
#define  BC_BLK_ENA  0x00800000
#define  BC_PSIZ_MSK 0x03000000
#define  BC_PSIZ_8B  0x00000000
#define  BC_PSIZ_16B 0x01000000
#define  BC_PSIZ_32B 0x02000000
#define  BC_PSIZ_NOB 0x03000000
#define  BC_CO       0x40000000
#define  BC_CR       0x80000000
#define DE_PGE   0x4024
#define  DP_DVP_MSK  0x0000001F
#define  DP_MP_MSK   0x000F0000
#define DE_SORG   0x4028
#define DE_DORG   0x402C
#define DE_MSRC   0x4030
#define DE_WKEY   0x4038
#define DE_KYDAT  0x403C
#define DE_ZPTCH  0x403C
#define DE_SPTCH  0x4040
#define DE_DPTCH  0x4044
#define CMD       0x4048
#define  CMD_OPC_MSK 0x000000FF
#define  CMD_ROP_MSK 0x0000FF00
#define  CMD_STL_MSK 0x001F0000
#define  CMD_CLP_MSK 0x00E00000
#define  CMD_PAT_MSK 0x0F000000
#define  CMD_HDF_MSK 0x70000000
#define CMD_OPC   0x4050
#define  CO_NOOP     0x00
#define  CO_BITBLT   0x01
#define  CO_LINE     0x02
#define  CO_ELINE    0x03
#define  CO_TRIAN    0x04
#define  CO_RXFER    0x06
#define  CO_WXFER    0x07
#define CMD_ROP   0x4054
#define  CR_CLEAR    0x00
#define  CR_NOR      0x01
#define  CR_AND_INV  0x02
#define  CR_COPY_INV 0x03
#define  CR_AND_REV  0x04
#define  CR_INVERT   0x05
#define  CR_XOR      0x06
#define  CR_NAND     0x07
#define  CR_AND      0x08
#define  CR_EQUIV    0x09
#define  CR_NOOP     0x0A
#define  CR_OR_INV   0x0B
#define  CR_COPY     0x0C
#define  CR_OR_REV   0x0D
#define  CR_OR       0x0E
#define  CR_SET      0x0F
#define CMD_STYLE 0x4058
#define  CS_SOLID    0x01
#define  CS_TRNSP    0x02
#define  CS_STP_NO   0x00
#define  CS_STP_PL   0x04
#define  CS_STP_PA32 0x08
#define  CS_STP_PA8  0x0C
#define  CS_EDI      0x10
#define CMD_PATRN 0x405C
#define  CP_APAT_NO  0x00
#define  CP_APAT_8X  0x01
#define  CP_APAT_32X 0x02
#define  CP_NLST     0x04
#define  CP_PRST     0x08
#define CMD_CLP   0x4060
#define  CC_NOCLP    0x00
#define  CC_CLPRECI  0x02
#define  CC_CLPRECO  0x03
#define  CC_CLPSTOP  0x04
#define CMD_HDF   0x4064
#define  CH_BIT_SWP  0x01
#define  CH_BYT_SWP  0x02
#define  CH_WRD_SWP  0x04
#define FORE      0x4068
#define BACK      0x406C
#define MASK      0x4070
#define RMSK      0x4074
#define LPAT      0x4078
#define PCTRL     0x407C
#define  PC_PLEN_MSK  0x0000001F
#define  PC_PSCL_MSK  0x000000E0
#define  PC_SPTR_MSK  0x00001F00
#define  PC_SSCL_MSK  0x0000E000
#define  PC_STATE_MSK 0xFFFF0000
#define CLPTL     0x4080		/* clipping top/left */
#define  CLPTLY_MSK   0x0000FFFF
#define  CLPTLX_MSK   0xFFFF0000
#define CLPBR     0x4084		/* clipping bottom/right */
#define  CLPBRY_MSK   0x0000FFFF
#define  CLPBRX_MSK   0xFFFF0000
#define XY0_SRC   0x4088
#define XY1_DST   0x408C      /* trigger */
#define XY2_WH    0x4090
#define XY3_DIR   0x4094
#define  DIR_LR_TB    0x00000000
#define  DIR_LR_BT    0x00000001
#define  DIR_RL_TB    0x00000002
#define  DIR_RL_BT    0x00000003
#define  DIR_BT		0x00000001
#define  DIR_RL		0x00000002
#define XY4_ZM    0x4098
#define  ZOOM_NONE    0x00000000
#define  XY_Y_DATA    0x0000FFFF
#define  XY_X_DATA    0xFFFF0000
#define  XY_I_DATA1   0x0000FFFF
#define  XY_I_DATA2   0xFFFF0000
#define DL_ADR    0x40F8
#define DL_CNTRL  0x40FC
#define ACNTRL    0x416C

/* wait until the blitter can accept another command */
#define I128_READY(tag, regh) \
	do {} while ((bus_space_read_4(tag, regh, BUSY) & BUSY_BUSY) != 0);

/* wait until it's safe to access video memory */
#define I128_DONE(tag, regh) \
	do {} while ((bus_space_read_4(tag, regh, FLOW) & 0x0f) != 0);

#endif /* I128REG_H */
