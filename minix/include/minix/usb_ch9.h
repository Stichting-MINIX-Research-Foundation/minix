#ifndef MINIX_USB_CH9_H
#define MINIX_USB_CH9_H
/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
/* USB DESCRIPTORS */
/*
 * The USB records contain some unaligned little-endian word
 * components.  The U[SG]ETW macros take care of both the alignment
 * and endian problem and should always be used to access non-byte
 * values.
 */

#include <sys/types.h>

typedef u8_t uByte;
typedef u8_t uWord[2];
typedef u8_t uDWord[4];

#define USETW2(w,h,l) ((w)[0] = (u_int8_t)(l), (w)[1] = (u_int8_t)(h))

#if 1
#define UGETW(w) ((w)[0] | ((w)[1] << 8))
#define USETW(w,v) ((w)[0] = (u_int8_t)(v), (w)[1] = (u_int8_t)((v) >> 8))
#define UGETDW(w) ((w)[0] | ((w)[1] << 8) | ((w)[2] << 16) | ((w)[3] << 24))
#define USETDW(w,v) ((w)[0] = (u_int8_t)(v), \
		     (w)[1] = (u_int8_t)((v) >> 8), \
		     (w)[2] = (u_int8_t)((v) >> 16), \
		     (w)[3] = (u_int8_t)((v) >> 24))
#else
/*
 * On little-endian machines that can handle unanliged accesses
 * (e.g. i386) these macros can be replaced by the following.
 */
#define UGETW(w) (*(u_int16_t *)(w))
#define USETW(w,v) (*(u_int16_t *)(w) = (v))
#define UGETDW(w) (*(u_int32_t *)(w))
#define USETDW(w,v) (*(u_int32_t *)(w) = (v))
#endif

#define UPACKED __attribute__((__packed__))

/* Requests */
#define	UR_GET_STATUS		0x00
#define	UR_CLEAR_FEATURE	0x01
#define	UR_SET_FEATURE		0x03
#define	UR_SET_ADDRESS		0x05
#define	UR_GET_DESCRIPTOR	0x06
#define	UDESC_DEVICE		0x01
#define	UDESC_CONFIG		0x02
#define	UDESC_STRING		0x03
#define	USB_LANGUAGE_TABLE	0x00	/* language ID string index */
#define	UDESC_INTERFACE		0x04
#define	UDESC_ENDPOINT		0x05
#define	UDESC_DEVICE_QUALIFIER	0x06
#define	UDESC_OTHER_SPEED_CONFIGURATION 0x07
#define	UDESC_INTERFACE_POWER	0x08
#define	UDESC_OTG		0x09
#define	UDESC_DEBUG		0x0A
#define	UDESC_IFACE_ASSOC	0x0B	/* interface association */
#define	UDESC_BOS		0x0F	/* binary object store */
#define	UDESC_DEVICE_CAPABILITY	0x10
#define	UDESC_CS_DEVICE		0x21	/* class specific */
#define	UDESC_CS_CONFIG		0x22
#define	UDESC_CS_STRING		0x23
#define	UDESC_CS_INTERFACE	0x24
#define	UDESC_CS_ENDPOINT	0x25
#define	UDESC_HUB		0x29
#define	UDESC_ENDPOINT_SS_COMP	0x30	/* super speed */
#define	UR_SET_DESCRIPTOR	0x07
#define	UR_GET_CONFIG		0x08
#define	UR_SET_CONFIG		0x09
#define	UR_GET_INTERFACE	0x0a
#define	UR_SET_INTERFACE	0x0b
#define	UR_SYNCH_FRAME		0x0c
#define	UR_SET_SEL		0x30
#define	UR_ISOCH_DELAY		0x31



typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bDescriptorSubtype;
} UPACKED usb_descriptor_t;


typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uWord		bcdUSB;
#define UD_USB_2_0		0x0200
#define UD_IS_USB2(d) (UGETW((d)->bcdUSB) >= UD_USB_2_0)
	uByte		bDeviceClass;
	uByte		bDeviceSubClass;
	uByte		bDeviceProtocol;
	uByte		bMaxPacketSize;
	/* The fields below are not part of the initial descriptor. */
	uWord		idVendor;
	uWord		idProduct;
	uWord		bcdDevice;
	uByte		iManufacturer;
	uByte		iProduct;
	uByte		iSerialNumber;
	uByte		bNumConfigurations;
} UPACKED usb_device_descriptor_t;
#define USB_DEVICE_DESCRIPTOR_SIZE 18

typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uWord		wTotalLength;
	uByte		bNumInterface;
	uByte		bConfigurationValue;
	uByte		iConfiguration;
	uByte		bmAttributes;
#define UC_BUS_POWERED		0x80
#define UC_SELF_POWERED		0x40
#define UC_REMOTE_WAKEUP	0x20
	uByte		bMaxPower; /* max current in 2 mA units */
#define UC_POWER_FACTOR 2
} UPACKED usb_config_descriptor_t;
#define USB_CONFIG_DESCRIPTOR_SIZE 9

typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bInterfaceNumber;
	uByte		bAlternateSetting;
	uByte		bNumEndpoints;
	uByte		bInterfaceClass;
	uByte		bInterfaceSubClass;
	uByte		bInterfaceProtocol;
	uByte		iInterface;
} UPACKED usb_interface_descriptor_t;
#define USB_INTERFACE_DESCRIPTOR_SIZE 9

typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bEndpointAddress;
#define UE_GET_DIR(a)	((a) & 0x80)
#define UE_SET_DIR(a,d)	((a) | (((d)&1) << 7))
#define UE_DIR_IN	0x80
#define UE_DIR_OUT	0x00
#define UE_ADDR		0x0f
#define UE_GET_ADDR(a)	((a) & UE_ADDR)
	uByte		bmAttributes;
#define UE_XFERTYPE	0x03
#define  UE_CONTROL	0x00
#define  UE_ISOCHRONOUS	0x01
#define  UE_BULK	0x02
#define  UE_INTERRUPT	0x03
#define UE_GET_XFERTYPE(a)	((a) & UE_XFERTYPE)
#define UE_ISO_TYPE	0x0c
#define  UE_ISO_ASYNC	0x04
#define  UE_ISO_ADAPT	0x08
#define  UE_ISO_SYNC	0x0c
#define UE_GET_ISO_TYPE(a)	((a) & UE_ISO_TYPE)
	uWord		wMaxPacketSize;
	uByte		bInterval;
} UPACKED usb_endpoint_descriptor_t;
#define USB_ENDPOINT_DESCRIPTOR_SIZE 7

typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uWord		bString[127];
} UPACKED usb_string_descriptor_t;

#define USB_MAX_STRING_LEN 128
#define USB_MAX_ENCODED_STRING_LEN (USB_MAX_STRING_LEN * 3) /* UTF8 */

struct usb_device_request {
	uByte	bmRequestType;
	uByte	bRequest;
	uWord	wValue;
	uWord	wIndex;
	uWord	wLength;
} UPACKED;
typedef struct usb_device_request usb_device_request_t;


#endif
