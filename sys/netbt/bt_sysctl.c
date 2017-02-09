/*	$NetBSD: bt_sysctl.c,v 1.3 2014/02/25 18:30:12 pooka Exp $	*/

/*-
 * Copyright (c) 2005 Iain Hibbert.
 * Copyright (c) 2006 Itronix Inc.
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
 * 3. The name of Itronix Inc. may not be used to endorse
 *    or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ITRONIX INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL ITRONIX INC. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: bt_sysctl.c,v 1.3 2014/02/25 18:30:12 pooka Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <netbt/bluetooth.h>
#include <netbt/hci.h>
#include <netbt/l2cap.h>
#include <netbt/rfcomm.h>
#include <netbt/sco.h>

SYSCTL_SETUP(sysctl_net_bluetooth_setup, "sysctl net.bluetooth subtree setup")
{

	sysctl_createv(clog, 0, NULL, NULL,
		CTLFLAG_PERMANENT,
		CTLTYPE_NODE, "bluetooth",
		SYSCTL_DESCR("Bluetooth Protocol Family"),
		NULL, 0,
		NULL, 0,
		CTL_NET, PF_BLUETOOTH, CTL_EOL);

#ifdef BLUETOOTH_DEBUG
	sysctl_createv(clog, 0, NULL, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "debug",
		SYSCTL_DESCR("debug level"),
		NULL, 0,
		&bluetooth_debug, sizeof(bluetooth_debug),
		CTL_NET, PF_BLUETOOTH,
		CTL_CREATE, CTL_EOL);
#endif

	sysctl_createv(clog, 0, NULL, NULL,
		CTLFLAG_PERMANENT,
		CTLTYPE_NODE, "hci",
		SYSCTL_DESCR("Host Controller Interface"),
		NULL, 0,
		NULL, 0,
		CTL_NET, PF_BLUETOOTH, BTPROTO_HCI, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "sendspace",
		SYSCTL_DESCR("Socket Send Buffer Size"),
		NULL, 0,
		&hci_sendspace, sizeof(hci_sendspace),
		CTL_NET, PF_BLUETOOTH, BTPROTO_HCI,
		CTL_CREATE, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "recvspace",
		SYSCTL_DESCR("Socket Receive Buffer Size"),
		NULL, 0,
		&hci_recvspace, sizeof(hci_recvspace),
		CTL_NET, PF_BLUETOOTH, BTPROTO_HCI,
		CTL_CREATE, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "acl_expiry",
		SYSCTL_DESCR("ACL Connection Expiry Time"),
		NULL, 0,
		&hci_acl_expiry, sizeof(hci_acl_expiry),
		CTL_NET, PF_BLUETOOTH, BTPROTO_HCI,
		CTL_CREATE, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "memo_expiry",
		SYSCTL_DESCR("Memo Expiry Time"),
		NULL, 0,
		&hci_memo_expiry, sizeof(hci_memo_expiry),
		CTL_NET, PF_BLUETOOTH, BTPROTO_HCI,
		CTL_CREATE, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "eventq_max",
		SYSCTL_DESCR("Max Event queue length"),
		NULL, 0,
		&hci_eventq_max, sizeof(hci_eventq_max),
		CTL_NET, PF_BLUETOOTH, BTPROTO_HCI,
		CTL_CREATE, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "aclrxq_max",
		SYSCTL_DESCR("Max ACL rx queue length"),
		NULL, 0,
		&hci_aclrxq_max, sizeof(hci_aclrxq_max),
		CTL_NET, PF_BLUETOOTH, BTPROTO_HCI,
		CTL_CREATE, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "scorxq_max",
		SYSCTL_DESCR("Max SCO rx queue length"),
		NULL, 0,
		&hci_scorxq_max, sizeof(hci_scorxq_max),
		CTL_NET, PF_BLUETOOTH, BTPROTO_HCI,
		CTL_CREATE, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
		CTLFLAG_PERMANENT,
		CTLTYPE_NODE, "l2cap",
		SYSCTL_DESCR("Logical Link Control & Adapataion Protocol"),
		NULL, 0, NULL, 0,
		CTL_NET, PF_BLUETOOTH, BTPROTO_L2CAP, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "sendspace",
		SYSCTL_DESCR("Socket Send Buffer Size"),
		NULL, 0,
		&l2cap_sendspace, sizeof(l2cap_sendspace),
		CTL_NET, PF_BLUETOOTH, BTPROTO_L2CAP,
		CTL_CREATE, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "recvspace",
		SYSCTL_DESCR("Socket Receive Buffer Size"),
		NULL, 0,
		&l2cap_recvspace, sizeof(l2cap_recvspace),
		CTL_NET, PF_BLUETOOTH, BTPROTO_L2CAP,
		CTL_CREATE, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "rtx",
		SYSCTL_DESCR("Response Timeout"),
		NULL, 0,
		&l2cap_response_timeout, sizeof(l2cap_response_timeout),
		CTL_NET, PF_BLUETOOTH, BTPROTO_L2CAP,
		CTL_CREATE, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "ertx",
		SYSCTL_DESCR("Extended Response Timeout"),
		NULL, 0,
		&l2cap_response_extended_timeout,
		sizeof(l2cap_response_extended_timeout),
		CTL_NET, PF_BLUETOOTH, BTPROTO_L2CAP,
		CTL_CREATE, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
		CTLFLAG_PERMANENT,
		CTLTYPE_NODE, "rfcomm",
		SYSCTL_DESCR("Serial Cable Emulation"),
		NULL, 0, NULL, 0,
		CTL_NET, PF_BLUETOOTH, BTPROTO_RFCOMM, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "sendspace",
		SYSCTL_DESCR("Socket Send Buffer Size"),
		NULL, 0,
		&rfcomm_sendspace, sizeof(rfcomm_sendspace),
		CTL_NET, PF_BLUETOOTH, BTPROTO_RFCOMM,
		CTL_CREATE, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "recvspace",
		SYSCTL_DESCR("Socket Receive Buffer Size"),
		NULL, 0,
		&rfcomm_recvspace, sizeof(rfcomm_recvspace),
		CTL_NET, PF_BLUETOOTH, BTPROTO_RFCOMM,
		CTL_CREATE, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "mtu_default",
		SYSCTL_DESCR("Default MTU"),
		NULL, 0,
		&rfcomm_mtu_default, sizeof(rfcomm_mtu_default),
		CTL_NET, PF_BLUETOOTH, BTPROTO_RFCOMM,
		CTL_CREATE, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "ack_timeout",
		SYSCTL_DESCR("Acknowledgement Timer"),
		NULL, 0,
		&rfcomm_ack_timeout, sizeof(rfcomm_ack_timeout),
		CTL_NET, PF_BLUETOOTH, BTPROTO_RFCOMM,
		CTL_CREATE, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "mcc_timeout",
		SYSCTL_DESCR("Response Timeout for Multiplexer Control Channel"),
		NULL, 0,
		&rfcomm_mcc_timeout, sizeof(rfcomm_mcc_timeout),
		CTL_NET, PF_BLUETOOTH, BTPROTO_RFCOMM,
		CTL_CREATE, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
		CTLFLAG_PERMANENT,
		CTLTYPE_NODE, "sco",
		SYSCTL_DESCR("SCO data"),
		NULL, 0, NULL, 0,
		CTL_NET, PF_BLUETOOTH, BTPROTO_SCO, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "sendspace",
		SYSCTL_DESCR("Socket Send Buffer Size"),
		NULL, 0,
		&sco_sendspace, sizeof(sco_sendspace),
		CTL_NET, PF_BLUETOOTH, BTPROTO_SCO,
		CTL_CREATE, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "recvspace",
		SYSCTL_DESCR("Socket Receive Buffer Size"),
		NULL, 0,
		&sco_recvspace, sizeof(sco_recvspace),
		CTL_NET, PF_BLUETOOTH, BTPROTO_SCO,
		CTL_CREATE, CTL_EOL);
}
