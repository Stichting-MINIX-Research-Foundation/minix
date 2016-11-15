/*	$NetBSD: iscsi_pdu.h,v 1.2 2014/06/21 03:42:52 dholland Exp $	*/

/*-
 * Copyright (c) 2004,2005,2006,2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Wasabi Systems, Inc.
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
#ifndef _ISCSI_PDU_H
#define _ISCSI_PDU_H

#define BHS_SIZE           48	/* Basic Header Segment (without digest) */
#define PDU_HEADER_SIZE    52	/* PDU Header with Digest */

#define OP_IMMEDIATE       0x40	/* Bit 1 in Opcode field: immediate delivery */
#define OPCODE_MASK        0x3f	/* Mask for opcode */

/* PDU Flags field */

#define FLAG_FINAL         0x80	/* Bit 0: Final PDU in sequence */
#define FLAG_TRANSIT       0x80	/* Bit 0: Transit to next login phase */
#define FLAG_CONTINUE      0x40	/* Bit 1: Continue PDU */
#define FLAG_ACK           0x40	/* Bit 1: Acknowledge */
#define FLAG_READ          0x40	/* Bit 1: Read Data */
#define FLAG_WRITE         0x20	/* Bit 2: Write Data */
#define FLAG_BIDI_OFLO     0x10	/* Bit 3: Bidirectional Read Residual Oflow */
#define FLAG_BIDI_UFLOW    0x08	/* Bit 4: Bidirectional Read Residual Uflow */
#define FLAG_OVERFLOW      0x04	/* Bit 5: Residual Overflow */
#define FLAG_UNDERFLOW     0x02	/* Bit 6: Residual Underflow */
#define FLAG_STATUS        0x01	/* Bit 7: Command Status is valid */

/* CSG/NSG flag field codes */

#define SG_SECURITY_NEGOTIATION           0
#define SG_LOGIN_OPERATIONAL_NEGOTIATION  1
#define SG_FULL_FEATURE_PHASE             3

#define CSG_SHIFT          2	/* shift factor for CSG field */
#define SG_MASK            3	/* mask for CSG (after shift) and NSG */

#define NEXT_PHASE(ph)  ((!ph) ? 1 : 3)	/* no phase 2 */

/* Task Management Function Codes (in Flags byte) */

#define ABORT_TASK            1
#define ABORT_TASK_SET        2
#define CLEAR_ACA             3
#define CLEAR_TASK_SET        4
#define LOGICAL_UNIT_RESET    5
#define TARGET_WARM_RESET     6
#define TARGET_COLD_RESET     7
#define TASK_REASSIGN         8

/* ISID T-Field (first byte) */

#define T_FORMAT_OUI    0x00
#define T_FORMAT_EN     0x40
#define T_FORMAT_RANDOM 0x80


/* Task Attributes */

#define ATTR_UNTAGGED         0
#define ATTR_SIMPLE           1
#define ATTR_ORDERED          2
#define ATTR_HEAD_OF_QUEUE    3
#define ATTR_ACA              4

/* Initiator Opcodes */

#define IOP_NOP_Out              0x00
#define IOP_SCSI_Command         0x01
#define IOP_SCSI_Task_Management 0x02
#define IOP_Login_Request        0x03
#define IOP_Text_Request         0x04
#define IOP_SCSI_Data_out        0x05
#define IOP_Logout_Request       0x06
#define IOP_SNACK_Request        0x10

/* Target Opcodes */

#define TOP_NOP_In               0x20
#define TOP_SCSI_Response        0x21
#define TOP_SCSI_Task_Management 0x22
#define TOP_Login_Response       0x23
#define TOP_Text_Response        0x24
#define TOP_SCSI_Data_in         0x25
#define TOP_Logout_Response      0x26
#define TOP_R2T                  0x31
#define TOP_Asynchronous_Message 0x32
#define TOP_Reject               0x3f

/*
 * The Opcode-dependent fields of the BHS, defined per PDU
 */

/* Command + Response */

struct scsi_command_pdu_s
{
	uint32_t ExpectedDataTransferLength;
	uint32_t CmdSN;
	uint32_t ExpStatSN;
	uint8_t SCSI_CDB[16];
} __packed;

typedef struct scsi_command_pdu_s scsi_command_pdu_t;

struct scsi_response_pdu_s
{
	uint32_t SNACKTag;
	uint32_t StatSN;
	uint32_t ExpCmdSN;
	uint32_t MaxCmdSN;
	uint32_t ExpStatSN;
	uint32_t ReadResidualCount;
	uint32_t ResidualCount;
} __packed;

typedef struct scsi_response_pdu_s scsi_response_pdu_t;


/* Task Management */

struct task_management_req_pdu_s
{
	uint32_t ReferencedTaskTag;
	uint32_t CmdSN;
	uint32_t ExpStatSN;
	uint32_t RefCmdSN;
	uint32_t ExpDataSN;
	uint8_t reserved[8];
} __packed;

typedef struct task_management_req_pdu_s task_management_req_pdu_t;


struct task_management_rsp_pdu_s
{
	uint32_t reserved1;
	uint32_t StatSN;
	uint32_t ExpCmdSN;
	uint32_t MaxCmdSN;
	uint8_t reserved2[12];
} __packed;

typedef struct task_management_rsp_pdu_s task_management_rsp_pdu_t;


/* Data Out & In, R2T */

struct data_out_pdu_s
{
	uint32_t TargetTransferTag;
	uint32_t reserved1;
	uint32_t ExpStatSN;
	uint32_t reserved2;
	uint32_t DataSN;
	uint32_t BufferOffset;
	uint32_t reserved3;
} __packed;

typedef struct data_out_pdu_s data_out_pdu_t;


struct data_in_pdu_s
{
	uint32_t TargetTransferTag;
	uint32_t StatSN;
	uint32_t ExpCmdSN;
	uint32_t MaxCmdSN;
	uint32_t DataSN;
	uint32_t BufferOffset;
	uint32_t ResidualCount;
} __packed;

typedef struct data_in_pdu_s data_in_pdu_t;


struct r2t_pdu_s
{
	uint32_t TargetTransferTag;
	uint32_t StatSN;
	uint32_t ExpCmdSN;
	uint32_t MaxCmdSN;
	uint32_t R2TSN;
	uint32_t BufferOffset;
	uint32_t DesiredDataTransferLength;
} __packed;

typedef struct r2t_pdu_s r2t_pdu_t;


/* Asynch message */

struct asynch_pdu_s
{
	uint32_t reserved1;
	uint32_t StatSN;
	uint32_t ExpCmdSN;
	uint32_t MaxCmdSN;
	uint8_t AsyncEvent;
	uint8_t AsyncVCode;
	uint16_t Parameter1;
	uint16_t Parameter2;
	uint16_t Parameter3;
	uint32_t reserved2;
} __packed;

typedef struct asynch_pdu_s asynch_pdu_t;


/* Text request / response */

struct text_req_pdu_s
{
	uint32_t TargetTransferTag;
	uint32_t CmdSN;
	uint32_t ExpStatSN;
	uint8_t reserved[16];
} __packed;

typedef struct text_req_pdu_s text_req_pdu_t;


struct text_rsp_pdu_s
{
	uint32_t TargetTransferTag;
	uint32_t StatSN;
	uint32_t ExpCmdSN;
	uint32_t MaxCmdSN;
	uint8_t reserved[12];
} __packed;

typedef struct text_rsp_pdu_s text_rsp_pdu_t;


/* Login request / response */

struct login_req_pdu_s
{
	uint16_t CID;
	uint16_t reserved1;
	uint32_t CmdSN;
	uint32_t ExpStatSN;
	uint8_t reserved2[16];
} __packed;

typedef struct login_req_pdu_s login_req_pdu_t;

/* Overlays LUN field in login request and response */
struct login_isid_s
{
	uint8_t ISID_A;
	uint16_t ISID_B;
	uint8_t ISID_C;
	uint16_t ISID_D;
	uint16_t TSIH;
} __packed;

typedef struct login_isid_s login_isid_t;

struct login_rsp_pdu_s
{
	uint32_t reserved1;
	uint32_t StatSN;
	uint32_t ExpCmdSN;
	uint32_t MaxCmdSN;
	uint8_t StatusClass;
	uint8_t StatusDetail;
	uint8_t reserved2[10];
} __packed;

typedef struct login_rsp_pdu_s login_rsp_pdu_t;


/* Logout request / response */

struct logout_req_pdu_s
{
	uint16_t CID;
	uint16_t reserved2;
	uint32_t CmdSN;
	uint32_t ExpStatSN;
	uint8_t reserved3[16];
} __packed;

typedef struct logout_req_pdu_s logout_req_pdu_t;


struct logout_rsp_pdu_s
{
	uint32_t reserved2;
	uint32_t StatSN;
	uint32_t ExpCmdSN;
	uint32_t MaxCmdSN;
	uint32_t reserved3;
	uint16_t Time2Wait;
	uint16_t Time2Retain;
	uint32_t reserved4;
} __packed;

typedef struct logout_rsp_pdu_s logout_rsp_pdu_t;


/* SNACK request */

/* SNACK Types (in Flags field) */

#define SNACK_DATA_NAK     0
#define SNACK_STATUS_NAK   1
#define SNACK_DATA_ACK     2
#define SNACK_RDATA_NAK    3

struct snack_req_pdu_s
{
	uint32_t TargetTransferTag;
	uint32_t reserved1;
	uint32_t ExpStatSN;
	uint8_t reserved2[8];
	uint32_t BegRun;
	uint32_t RunLength;
} __packed;

typedef struct snack_req_pdu_s snack_req_pdu_t;


/* Reject */

#define REJECT_DIGEST_ERROR         2
#define REJECT_SNACK                3
#define REJECT_PROTOCOL_ERROR       4
#define REJECT_CMD_NOT_SUPPORTED    5
#define REJECT_IMMED_COMMAND        6
#define REJECT_TASK_IN_PROGRESS     7
#define REJECT_INVALID_DATA_ACK     8
#define REJECT_INVALID_PDU_FIELD    9
#define REJECT_LONG_OPERATION       10
#define REJECT_NEGOTIATION_RESET    11
#define REJECT_WAITING_FOR_LOGOUT   12


struct reject_pdu_s
{
	uint32_t reserved2;
	uint32_t StatSN;
	uint32_t ExpCmdSN;
	uint32_t MaxCmdSN;
	uint8_t DataSN;
	uint8_t reserved[8];
} __packed;

typedef struct reject_pdu_s reject_pdu_t;


/* NOP Out & In */

struct nop_out_pdu_s
{
	uint32_t TargetTransferTag;
	uint32_t CmdSN;
	uint32_t ExpStatSN;
	uint8_t reserved[16];
} __packed;

typedef struct nop_out_pdu_s nop_out_pdu_t;


struct nop_in_pdu_s
{
	uint32_t TargetTransferTag;
	uint32_t StatSN;
	uint32_t ExpCmdSN;
	uint32_t MaxCmdSN;
	uint8_t reserved3[12];
} __packed;

typedef struct nop_in_pdu_s nop_in_pdu_t;


/*
 * The complete PDU Header.
 */

struct pdu_header_s
{
	uint8_t Opcode;
	uint8_t Flags;
	uint8_t OpcodeSpecific[2];
	uint8_t TotalAHSLength;
	uint8_t DataSegmentLength[3];
	uint64_t LUN;
	uint32_t InitiatorTaskTag;
	union
	{
		scsi_command_pdu_t command;
		scsi_response_pdu_t response;
		task_management_req_pdu_t task_req;
		task_management_rsp_pdu_t task_rsp;
		data_out_pdu_t data_out;
		data_in_pdu_t data_in;
		r2t_pdu_t r2t;
		asynch_pdu_t asynch;
		text_req_pdu_t text_req;
		text_rsp_pdu_t text_rsp;
		login_req_pdu_t login_req;
		login_rsp_pdu_t login_rsp;
		logout_req_pdu_t logout_req;
		logout_rsp_pdu_t logout_rsp;
		snack_req_pdu_t snack;
		reject_pdu_t reject;
		nop_out_pdu_t nop_out;
		nop_in_pdu_t nop_in;
	} p;
	uint32_t HeaderDigest;
} __packed;

typedef struct pdu_header_s pdu_header_t;

#endif /* !_ISCSI_PDU_H */
