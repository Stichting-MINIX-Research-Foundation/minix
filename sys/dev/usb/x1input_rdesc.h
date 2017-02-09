/* $NetBSD: x1input_rdesc.h,v 1.1 2015/02/08 19:22:45 jmcneill Exp $ */

/*-
 * Copyright (C) 2014 Loic Nageleisen
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the copyright holders nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define	USBIF_IS_X1INPUT(uaa)			\
	((uaa)->class == UICLASS_VENDOR && 	\
	 (uaa)->subclass == 0x47 &&		\
	 (uaa)->proto == 0xd0)

static const uByte uhid_x1input_report_descr[] = {
    0x05, 0x01,                     // Usage Page (Generic Desktop)
    0x09, 0x05,                     // Usage (Game Pad)
    0xa1, 0x01,                     // Collection (Application)

                                    //   # button packet
    0xa1, 0x00,                     //   Collection (Physical)
    0x85, 0x20,                     //     Report ID (0x20)

                                    //     # skip unknown field and counter
    0x05, 0x01,                     //     Usage Page (Generic Desktop)
    0x09, 0x00,                     //     Usage (Undefined)
    0x75, 0x08,                     //     Report Size (8)
    0x95, 0x02,                     //     Report Count (2)
    0x81, 0x03,                     //     Input (Constant, Variable, Absolute)

                                    //     payload size
    0x05, 0x01,                     //     Usage Page (Generic Desktop)
    0x09, 0x3b,                     //     Usage (Byte Count)
    0x75, 0x08,                     //     Report Size (8)
    0x95, 0x01,                     //     Report Count (1)
    0x81, 0x02,                     //     Input (Data, Variable, Absolute)

                                    //     # 16 buttons
    0x05, 0x09,                     //     Usage Page (Button)
    0x19, 0x01,                     //     Usage Minimum (Button 1)
    0x29, 0x10,                     //     Usage Maximum (Button 16)
    0x15, 0x00,                     //     Logical Minimum (0)
    0x25, 0x01,                     //     Logical Maximum (1)
    0x75, 0x01,                     //     Report Size (1)
    0x95, 0x10,                     //     Report Count (16)
    0x81, 0x02,                     //     Input (Data, Variable, Absolute)

                                    //     # triggers
    0x15, 0x00,                     //     Logical Minimum (0)
    0x26, 0xff, 0x03,               //     Logical Maximum (1023)
    0x35, 0x00,                     //     Physical Minimum (0)
    0x46, 0xff, 0x03,               //     Physical Maximum (1023)
    0x75, 0x10,                     //     Report Size (16)
    0x95, 0x02,                     //     Report Count (2)
    0x05, 0x01,                     //     Usage Page (Generic Desktop)
    0x09, 0x33,                     //     Usage (Rx)
    0x09, 0x34,                     //     Usage (Ry)
    0x81, 0x02,                     //     Input (Data, Variable, Absolute)

                                    //     # sticks
    0x75, 0x10,                     //     Report Size (16)
    0x16, 0x00, 0x80,               //     Logical Minimum (-32768)
    0x26, 0xff, 0x7f,               //     Logical Maximum (32767)
    0x36, 0x00, 0x80,               //     Physical Minimum (-32768)
    0x46, 0xff, 0x7f,               //     Physical Maximum (32767)
    0x05, 0x01,                     //     Usage Page (Generic Desktop)
    0x09, 0x01,                     //     Usage (Pointer)
    0xa1, 0x00,                     //     Collection (Physical)
    0x95, 0x02,                     //       Report Count (2)
    0x05, 0x01,                     //       Usage Page (Generic Desktop)
    0x09, 0x30,                     //       Usage (X)
    0x09, 0x31,                     //       Usage (Y)
    0x81, 0x02,                     //       Input (Data, Variable, Absolute)
    0xc0,                           //     End Collection
    0x05, 0x01,                     //     Usage Page (Generic Desktop)
    0x09, 0x01,                     //     Usage (Pointer)
    0xa1, 0x00,                     //     Collection (Physical)
    0x95, 0x02,                     //       Report Count (2)
    0x05, 0x01,                     //       Usage Page (Generic Desktop)
    0x09, 0x32,                     //       Usage (Z)
    0x09, 0x35,                     //       Usage (Rz)
    0x81, 0x02,                     //       Input (Data, Variable, Absolute)
    0xc0,                           //     End Collection
    0xc0,                           //   End Collection
};
