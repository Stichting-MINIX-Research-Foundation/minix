/* $NetBSD: xinput_rdesc.h,v 1.1 2011/07/30 12:15:44 jmcneill Exp $ */

/*-
 * Copyright (c) 2011 Jared D. McNeill <jmcneill@invisible.ca>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
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

/*
 * Descriptor from http://euc.jp/periphs/xbox-pad-report-desc.txt
 */

#define	USBIF_IS_XINPUT(uaa)			\
	((uaa)->class == UICLASS_VENDOR && 	\
	 (uaa)->subclass == 0x5d &&		\
	 (uaa)->proto == 0x01)

static const uByte uhid_xinput_report_descr[] = {
    0x05, 0x01,		/* Usage Page (Generic Desktop) */
    0x09, 0x05,		/* Usage (Game Pad) */
    0xa1, 0x01,		/* Collection (Application) */
    0x05, 0x01,		/* Usage Page (Generic Desktop) */
    0x09, 0x3a,		/* Usage (Counted Buffer) */
    0xa1, 0x02,		/* Collection (Logical) */
    0x75, 0x08,		/* Report Size (8) */
    0x95, 0x01,		/* Report Count (1) */
    0x81, 0x01,		/* Input (Constant) */
    0x75, 0x08,		/* Report Size (8) */
    0x95, 0x01,		/* Report Count (1) */
    0x05, 0x01,		/* Usage Page (Generic Desktop) */
    0x09, 0x3b,		/* Usage (Byte Count) */
    0x81, 0x01,		/* Input (Constant) */
    0x05, 0x01,		/* Usage Page (Generic Desktop) */
    0x09, 0x01,		/* Usage (Pointer) */
    0xa1, 0x00,		/* Collection (Physical) */
    0x75, 0x01,		/* Report Size (1) */
    0x15, 0x00,		/* Logical Minimum (0) */
    0x25, 0x01,		/* Logical Maximum (1) */
    0x35, 0x00,		/* Physical Minimum (0) */
    0x45, 0x01,		/* Physical Maximum (1) */
    0x95, 0x04,		/* Report Count (4) */
    0x05, 0x01,		/* Usage Page (Generic Desktop) */
    0x09, 0x90,		/* Usage (D-pad Up) */
    0x09, 0x91,		/* Usage (D-pad Down) */
    0x09, 0x93,		/* Usage (D-pad Left) */
    0x09, 0x92,		/* Usage (D-pad Right) */
    0x81, 0x02,		/* Input (Data,Variable,Absolute) */
    0xc0,		/* End Collection */
    0x75, 0x01,		/* Report Size (1) */
    0x15, 0x00,		/* Logical Minimum (0) */
    0x25, 0x01,		/* Logical Maximum (1) */
    0x35, 0x00,		/* Physical Minimum (0) */
    0x45, 0x01,		/* Physical Maximum (1) */
    0x95, 0x04,		/* Report Count (4) */
    0x05, 0x09,		/* Usage Page (Button) */
    0x19, 0x07,		/* Usage Minimum (Button 7) */
    0x29, 0x0a,		/* Usage Maximum (Button 10) */
    0x81, 0x02,		/* Input (Data,Variable,Absolute) */
    0x75, 0x01,		/* Report Size (1) */
    0x95, 0x08,		/* Report Count (8) */
    0x81, 0x01,		/* Input (Constant) */
    0x75, 0x08,		/* Report Size (8) */
    0x15, 0x00,		/* Logical Minimum (0) */
    0x26, 0xff, 0x00,	/* Logical Maximum (255) */
    0x35, 0x00,		/* Physical Minimum (0) */
    0x46, 0xff, 0x00,	/* Physical Maximum (255) */
    0x95, 0x06,		/* Report Count (6) */
    0x05, 0x09,		/* Usage Page (Button) */
    0x19, 0x01,		/* Usage Minimum (Button 1) */
    0x29, 0x06,		/* Usage Minimum (Button 6) */
    0x81, 0x02,		/* Input (Data,Variable,Absolute) */
    0x75, 0x08,		/* Report Size (8) */
    0x15, 0x00,		/* Logical Minimum (0) */
    0x26, 0xff, 0x00,	/* Logical Maximum (255) */
    0x35, 0x00,		/* Physical Minimum (0) */
    0x46, 0xff, 0x00,	/* Physical Maximum (255) */
    0x95, 0x02,		/* Report Count (2) */
    0x05, 0x01,		/* Usage Page (Generic Desktop) */
    0x09, 0x32,		/* Usage (Z) */
    0x09, 0x35,		/* Usage (Rz) */
    0x81, 0x02,		/* Input (Data,Variable,Absolute) */
    0x75, 0x10,		/* Report Size (16) */
    0x16, 0x00, 0x80,	/* Logical Minimum (-32768) */
    0x26, 0xff, 0x7f,	/* Logical Maximum (32767) */
    0x36, 0x00, 0x80,	/* Physical Minimum (-32768) */
    0x46, 0xff, 0x7f,	/* Physical Maximum (32767) */
    0x05, 0x01,		/* Usage Page (Generic Desktop) */
    0x09, 0x01,		/* Usage (Pointer) */
    0xa1, 0x00,		/* Collection (Physical) */
    0x95, 0x02,		/* Report Count (2) */
    0x05, 0x01,		/* Usage Page (Generic Desktop) */
    0x09, 0x30,		/* Usage (X) */
    0x09, 0x31,		/* Usage (Y) */
    0x81, 0x02,		/* Input (Data,Variable,Absolute) */
    0xc0,		/* End Collection */
    0x05, 0x01,		/* Usage Page (Generic Desktop) */
    0x09, 0x01,		/* Usage (Pointer) */
    0xa1, 0x00,		/* Collection (Physical) */
    0x95, 0x02,		/* Report Count (2) */
    0x05, 0x01,		/* Usage Page (Generic Desktop) */
    0x09, 0x33,		/* Usage (Rx) */
    0x09, 0x34,		/* Usage (Ry) */
    0x81, 0x02,		/* Input (Data,Variable,Absolute) */
    0xc0,		/* End Collection */
    0xc0,		/* End Collection */
    0x05, 0x01,		/* Usage Page (Generic Desktop) */
    0x09, 0x3a,		/* Usage (Counted Buffer) */
    0xa1, 0x02,		/* Collection (Logical) */
    0x75, 0x08,		/* Report Size (8) */
    0x95, 0x01,		/* Report Count (1) */
    0x91, 0x01,		/* Output (Constant) */
    0x75, 0x08,		/* Report Size (8) */
    0x95, 0x01,		/* Report Count (1) */
    0x05, 0x01,		/* Usage Page (Generic Desktop) */
    0x09, 0x3b,		/* Usage (Byte Count) */
    0x91, 0x01,		/* Output (Constant) */
    0x75, 0x08,		/* Report Size (8) */
    0x95, 0x01,		/* Report Count (1) */
    0x91, 0x01,		/* Output (Constant) */
    0x75, 0x08,		/* Report Size (8) */
    0x15, 0x00,		/* Logical Minimum (0) */
    0x26, 0xff, 0x00,	/* Logical Maximum (255) */
    0x35, 0x00,		/* Physical Minimum (0) */
    0x46, 0xff, 0x00,	/* Physical Maximum (255) */
    0x95, 0x01,		/* Report Count (1) */
    0x06, 0x00, 0xff,	/* Usage Page (vendor-defined) */
    0x09, 0x01,		/* Usage (1) */
    0x91, 0x02,		/* Output (Data,Variable,Absolute) */
    0x75, 0x08,		/* Report Size (8) */
    0x95, 0x01,		/* Report Count (1) */
    0x91, 0x01,		/* Output (Constant) */
    0x75, 0x08,		/* Report Size (8) */
    0x15, 0x00,		/* Logical Minimum (0) */
    0x26, 0xff, 0x00,	/* Logical Maximum (255) */
    0x35, 0x00,		/* Physical Minimum (0) */
    0x46, 0xff, 0x00,	/* Physical Maximum (255) */
    0x95, 0x01,		/* Report Count (1) */
    0x06, 0x00, 0xff,	/* Usage Page (vendor-defined) */
    0x09, 0x02,		/* Usage (2) */
    0x91, 0x02,		/* Output (Data,Variable,Absolute) */
    0xc0,		/* End Collection */
    0xc0,		/* End Collection */
};
