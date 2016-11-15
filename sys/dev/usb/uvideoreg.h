/*	$NetBSD: uvideoreg.h,v 1.4 2011/12/23 00:51:49 jakllsch Exp $	*/

/*
 * Copyright (c) 2008 Patrick Mahoney
 * All rights reserved.
 *
 * This code was written by Patrick Mahoney (pat@polycrystal.org) as
 * part of Google Summer of Code 2008.
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

#define UVIDEO_VERSION		0x001

/* This is the standard GUID / UUID.  In USB, it comes in the usual
 * little-endian packed format. */

typedef struct {
	uDWord		data1;
	uWord		data2;
	uWord		data3;
	uByte		data4[8];
} UPACKED usb_guid_t;

typedef struct {
	uint32_t	data1;
	uint16_t	data2;
	uint16_t	data3;
	uint8_t		data4[8];
} guid_t;
#define GUID_LEN 16

/*
 * Video Control descriptors
 */

#define UDESC_VC_HEADER		0x01
#define UDESC_INPUT_TERMINAL	0x02
#define UDESC_OUTPUT_TERMINAL	0x03
#define UDESC_SELECTOR_UNIT	0x04
#define UDESC_PROCESSING_UNIT	0x05
#define UDESC_EXTENSION_UNIT	0x06

#define UDESC_VC_INTERRUPT_ENDPOINT	0x03

/* Terminal Types */
#define UVDIEO_TT_VENDOR_SPECIFIC	0x0100
#define UVIDEO_TT_STREAMING		0x0101

/* Input Terminal Types */
#define UVIDEO_ITT_VENDOR_SPECIFIC	0x0200
#define UVIDEO_ITT_CAMERA		0x0201
#define UVIDEO_ITT_MEDIA_TRANSPORT_INPUT 0x0202

/* Output Terminal Types */
#define UVIDEO_OTT_VENDOR_SPECIFIC	0x0300
#define UVIDEO_OTT_DISPLAY		0x0301
#define UVIDEO_OTT_MEDIA_TRANSPORT_OUTPUT 0x0302

/* generic descriptor with Subtype */
typedef struct {
	uByte	bLength;
	uByte	bDescriptorType;
	uByte	bDescriptorSubtype;
} uvideo_descriptor_t;

/* Class-specific Video Control Interface Header Descriptor */
typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bDescriptorSubtype;
	uWord		bcdUVC;
	uWord		wTotalLength;
	uDWord		dwClockFrequency;
	uByte		bInCollection;
	/* followed by n bytes where n is equal to value of bInCollection */
	uByte		baInterfaceNr[];
} UPACKED uvideo_vc_header_descriptor_t;

/* Input Terminal Descriptor */
typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bDescriptorSubtype;
	uByte		bTerminalID;
	uWord		wTerminalType;
	uByte		bAssocTerminal;
	uByte		iTerminal;
	/* possibly more, depending on Terminal type */
} UPACKED uvideo_input_terminal_descriptor_t;

/* Output Terminal Descriptor */
typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bDescriptorSubtype;
	uByte		bTerminalID;
	uWord		wTerminalType;
	uByte		bAssocTerminal;
	uByte		bSourceID;
	uByte		iTerminal;
} UPACKED uvideo_output_terminal_descriptor_t;

/* Camera Terminal Descriptor */
typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bDescriptorSubtype; 	/* UDESC_VC_INPUT_TERMINAL */
	uByte		bTerminalID;
	uWord		wTerminalType;		/* UVIDEO_ITT_CAMERA */
	uByte		bAssocTerminal;
	uByte		iTerminal;
	uWord		wObjectiveFocalLengthMin;
	uWord		wObjectiveFocalLengthMax;
	uWord		wOcularFocalLength;
	uByte		bControlSize;
	uByte		bmControls[];
} UPACKED uvideo_camera_terminal_descriptor_t;

/* bmControls fields of uvideo_camera_terminal_descriptor_t */
#define UVIDEO_CAMERA_CONTROL_SCANNING_MODE		(1<<0)
#define UVIDEO_CAMERA_CONTROL_AUTO_EXPOSURE_MODE	(1<<1)
#define UVIDEO_CAMERA_CONTROL_AUTO_EXPOSURE_PRIO	(1<<2)
#define UVIDEO_CAMERA_CONTROL_EXPOSURE_TIME_ABSOLUTE	(1<<3)
#define UVIDEO_CAMERA_CONTROL_EXPOSURE_TIME_RELATIVE	(1<<4)
#define UVIDEO_CAMERA_CONTROL_FOCUS_ABSOLUTE		(1<<5)
#define UVIDEO_CAMERA_CONTROL_FOCUS_RELATIVE		(1<<6)
#define UVIDEO_CAMERA_CONTROL_IRIS_ABSOLUTE		(1<<7)
#define UVIDEO_CAMERA_CONTROL_IRIS_RELATIVE		(1<<8)
#define UVIDEO_CAMERA_CONTROL_ZOOM_ABSOLUTE		(1<<9)
#define UVIDEO_CAMERA_CONTROL_ZOOM_RELATIVE		(1<<10)
#define UVIDEO_CAMERA_CONTROL_PANTILT_ABSOLUTE		(1<<11)
#define UVIDEO_CAMERA_CONTROL_PANTILT_RELATIVE		(1<<12)
#define UVIDEO_CAMERA_CONTROL_ROLL_ABSOLUTE		(1<<13)
#define UVIDEO_CAMERA_CONTROL_ROLL_RELATIVE		(1<<14)
/* 15,16 reserved */
#define UVIDEO_CAMERA_CONTROL_FOCUS_AUTO		(1<<17)
#define UVIDEO_CAMERA_CONTROL_PRIVACY			(1<<18)

typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bDescriptorSubtype;
	uByte		bUnitID;
	uByte		bNrInPins;
	uByte		baSourceID[];
	/* The position of the next field is baSourceID[0] + bNrInPins
	 * and should be accessed via a function. */
/*      uByte           iSelector */
} UPACKED uvideo_selector_unit_descriptor_t;

typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bDescriptorSubtype;
	uByte		bUnitID;
	uByte		bSourceID;
	uWord		wMaxMultiplier;
	uByte		bControlSize;
	uByte		bmControls[];
/*      uByte           iProcessing */
/*      uByte           bmVideoStandards */
#define PU_GET_VIDEO_STANDARDS(desc)	\
	(*((desc)->bmControls + (desc)->bControlSize))
#define UVIDEO_STANDARD_NONE		(1<<0)
#define UVIDEO_STANDARD_NTSC_525_60	(1<<1)
#define UVIDEO_STANDARD_PAL_625_50	(1<<2)
#define UVIDEO_STANDARD_SECAM_625_50	(1<<3)
#define UVIDEO_STANDARD_NTSC_625_50	(1<<4)
#define UVIDEO_STANDARD_PAL_525_60	(1<<5)
} UPACKED uvideo_processing_unit_descriptor_t;

typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bDescriptorSubtype;
	uByte		bUnitID;
	usb_guid_t	guidExtensionCode;
	uByte		bNumControls;
	uByte		bNrInPins;
	uByte		baSourceID[];
/*      uByte           bControlSize */
/*      uByte           bmControls */
#define XU_GET_CONTROL_SIZE(desc)			\
	(*((desc)->baSourceID + (desc)->bNrInPins))
#define XU_GET_CONTROLS(desc)				\
	((desc)->baSourceID + (desc)->bNrInPins + 1)
/*      uByte           iExtension */
} UPACKED uvideo_extension_unit_descriptor_t;

typedef struct {
	uByte		bLength;
	uByte		bDescriptorType; /* UDESC_ENDPOINT */
	uByte		bDescriptorSubtype;
	uWord		wMaxTransferSize;
} UPACKED uvideo_vc_interrupt_endpoint_descriptor_t;



/*
 * Video Streaming descriptors
 */

#define UDESC_VS_INPUT_HEADER		0x01
#define UDESC_VS_OUTPUT_HEADER		0x02
#define UDESC_VS_STILL_IMAGE_FRAME	0x03
#define UDESC_VS_FORMAT_UNCOMPRESSED	0x04
#define UDESC_VS_FRAME_UNCOMPRESSED	0x05
#define UDESC_VS_FORMAT_MJPEG		0x06
#define UDESC_VS_FRAME_MJPEG		0x07
/* reserved in spec v1.1		0x08 */
/* reserved in spec v1.1		0x09 */
#define UDESC_VS_FORMAT_MPEG2TS		0x0A
/* reserved in spec v 1.1		0x0B */
#define UDESC_VS_FORMAT_DV		0x0C
#define UDESC_VS_COLORFORMAT		0x0D
/* reserved in spec v1.1		0x0E */
/* reserved in spec v1.1		0x0F */
#define UDESC_VS_FORMAT_FRAME_BASED	0x10
#define UDESC_VS_FRAME_FRAME_BASED	0x11
#define UDESC_VS_FORMAT_STREAM_BASED	0x12

/* Copy protection state */
#define UVIDEO_NO_RESTRICTIONS		0
#define UVIDEO_RESTRICT_DUP		1

typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bDescriptorSubtype;
	uByte		bNumFormats;
	uWord		wTotalLength;
	uByte		bEndpointAddress;
	uByte		bmInfo;
	uByte		bTerminalLink;
	uByte		bStillCaptureMethod;
	uByte		bTriggerSupport;
	uByte		bTriggerUsage;
	uByte		bControlSize;
	uByte		bmaControls[];
#define UVIDEO_VS_KEYFRAME_RATE	(1<<0)
#define UVIDEO_VS_PFRAME_RATE	(1<<1)
#define UVIDEO_VS_COMP_QUALITY	(1<<2)
#define UVIDEO_VS_COMP_WINDOW_SIZE	(1<<3)
#define UVIDEO_VS_GENERATE_KEYFRAME	(1<<4)
#define UVIDEO_VS_UPDATE_FRAME_SEGMENT	(1<<5)
} UPACKED uvideo_vs_input_header_descriptor_t;

typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bDescriptorSubtype;
	uByte		bNumFormats;
	uWord		wTotalLength;
	uByte		bEndpointAddress;
	uByte		bTerminalLink;
	uByte		bControlSize;
	uByte		bmaControls[];
} UPACKED uvideo_vs_output_header_descriptor_t;


typedef struct {
	uWord		wWidth;
	uWord		wHeight;
} UPACKED uvideo_still_image_frame_dimensions_t;

typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bDescriptorSubtype;
	uByte		bEndpointAddress;
	uByte		bNumImageSizePatterns;
	uvideo_still_image_frame_dimensions_t wwaDimensions[];
	/* position dependent on size of previous item */
	/* uByte	bNumCompressionPattern */
	/* uByte	bCompression[] */
} UPACKED uvideo_still_image_frame_descriptor_t;


/* Color matching information */

/* bColroPrimaries */
#define UVIDEO_COLOR_PRIMARIES_UNSPECIFIED	0
#define UVIDEO_COLOR_PRIMARIES_sRGB		1 /* same as BT709 */
#define UVIDEO_COLOR_PRIMARIES_BT709		1 /* default */
#define UVIDEO_COLOR_PRIMARIES_BT470_2_M       	2
#define UVIDEO_COLOR_PRIMARIES_BT470_2_BG      	3
#define UVIDEO_COLOR_PRIMARIES_SMPTE_170M      	4
#define UVIDEO_COLOR_PRIMARIES_SMPTE_240M      	5

/* bTransferCharacteristics */
#define UVIDEO_GAMMA_FUNCTION_UNSPECIFIED	0
#define UVIDEO_GAMMA_FUNCTION_BT709		1 /* default */
#define UVIDEO_GAMMA_FUNCTION_BT470_2_M       	2
#define UVIDEO_GAMMA_FUNCTION_BT470_2_BG      	3
#define UVIDEO_GAMMA_FUNCTION_SMPTE_170M      	4
#define UVIDEO_GAMMA_FUNCTION_SMPTE_240M      	5
#define UVIDEO_GAMMA_FUNCTION_LINEAR		6	
#define UVIDEO_GAMMA_FUNCTION_sRGB		7 /* similar to BT709 */

/* bMatrixCoefficients */
#define UVIDEO_LUMA_CHROMA_MATRIX_UNSPECIFIED	0
#define UVIDEO_LUMA_CHROMA_MATRIX_BT709		1
#define UVIDEO_LUMA_CHROMA_MATRIX_FCC       	2
#define UVIDEO_LUMA_CHROMA_MATRIX_BT470_2_BG	3
#define UVIDEO_LUMA_CHROMA_MATRIX_SMPTE_170M	4 /* default */
#define UVIDEO_LUMA_CHROMA_MATRIX_SMPTE_240M	5

typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bDescriptorSubtype;
	uByte		bColorPrimaries;
	uByte		bTransferCharacteristics;
	uByte		bMatrixCoefficients;
} UPACKED uvideo_color_matching_descriptor_t;

/*
 * Format and Frame descriptors
 */

#define UVIDEO_FRAME_CAP_STILL_IMAGE	1<<0
#define UVIDEO_FRAME_CAP_FIXED_RATE	1<<1

#define UVIDEO_FRAME_INTERVAL_CONTINUOUS 0

/* TODO: interlace flags */


typedef struct {
	uDWord		dwMinFrameInterval;
	uDWord		dwMaxFrameInterval;
	uDWord		dwFrameIntervalStep;
} UPACKED uvideo_frame_interval_continuous_t;

typedef struct {
	uDWord	dwFrameInterval[1]; /* length depends on bFrameIntervalType */
} UPACKED uvideo_frame_interval_discrete_t;

typedef union {
	uvideo_frame_interval_continuous_t	continuous;
	uvideo_frame_interval_discrete_t	discrete;
} uvideo_frame_interval_t;

/* generic format descriptor header */
typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bDescriptorSubtype;
	uByte		bFormatIndex;
} UPACKED uvideo_vs_format_descriptor_t;

/* generic frame descriptor header */
typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bDescriptorSubtype;
	uByte		bFrameIndex;
} UPACKED uvideo_vs_frame_descriptor_t;


/*  uncompressed format and frame descriptors */
static const guid_t uvideo_guid_format_yuy2 = {
	0x32595559,
	0x0000,
	0x0010,
	{0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71}
};

static const guid_t uvideo_guid_format_nv12 = {
	0x3231564E,
	0x0000,
	0x0010,
	{0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71}
};

static const guid_t uvideo_guid_format_uyvy = {
	0x59565955,
	0x0000,
	0x0010,
	{0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71}
};

typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bDescriptorSubtype;
	uByte		bFormatIndex;
	uByte		bNumFrameDescriptors;
	usb_guid_t	guidFormat;
	uByte		bBitsPerPixel;
	uByte		bDefaultFrameIndex;
	uByte		bAspectRatioX;
	uByte		bAspectRatioY;
	uByte		bmInterlaceFlags;
	uByte		bCopyProtect;
} UPACKED uvideo_vs_format_uncompressed_descriptor_t;

typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bDescriptorSubtype;
	uByte		bFrameIndex;
	uByte		bmCapabilities;
	uWord		wWidth;
	uWord		wHeight;
	uDWord		dwMinBitRate;
	uDWord		dwMaxBitRate;
	uDWord		dwMaxVideoFrameBufferSize;
	uDWord		dwDefaultFrameInterval;
	uByte		bFrameIntervalType;
	uvideo_frame_interval_t uFrameInterval;
} UPACKED uvideo_vs_frame_uncompressed_descriptor_t;


/* Frame based Format and Frame descriptors.  This is for generic
 * frame based payloads not covered by other types (e.g, uncompressed
 * or MJPEG). */

typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bDescriptorSubtype;
	uByte		bFormatIndex;
	uByte		bNumFrameDescriptors;
	usb_guid_t	guidFormat;
	uByte		bBitsPerPixel;
	uByte		bDefaultFrameIndex;
	uByte		bAspectRatioX;
	uByte		bAspectRatioY;
	uByte		bmInterlaceFlags;
	uByte		bCopyProtect;
} UPACKED uvideo_format_frame_based_descriptor_t;

typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bDescriptorSubtype;
	uByte		bFrameIndex;
	uByte		bmCapabilities;
	uWord		wWidth;
	uWord		wHeight;
	uDWord		dwMinBitRate;
	uDWord		dwMaxBitRate;
	uDWord		dwDefaultFrameInterval;
	uByte		bFrameIntervalType;
	uDWord		dwBytesPerLine;
	uvideo_frame_interval_t uFrameInterval;
} UPACKED uvideo_frame_frame_based_descriptor_t;


/* MJPEG format and frame descriptors */

typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bDescriptorSubtype;
	uByte		bFormatIndex;
	uByte		bNumFrameDescriptors;
	uByte		bmFlags;
#define UVIDEO_NO_FIXED_SIZE_SAMPLES 0
#define UVIDEO_FIXED_SIZE_SAMPLES 1
	uByte		bDefaultFrameIndex;
	uByte		bAspectRatioX;
	uByte		bAspectRatioY;
	uByte		bmInterlaceFlags;
	uByte		bCopyProtect;
} UPACKED uvideo_vs_format_mjpeg_descriptor_t;

typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bDescriptorSubtype;
	uByte		bFrameIndex;
	uByte		bmCapabilities;
	uWord		wWidth;
	uWord		wHeight;
	uDWord		dwMinBitRate;
	uDWord		dwMaxBitRate;
	uDWord		dwMaxVideoFrameBufferSize;
	uDWord		dwDefaultFrameInterval;
	uByte		bFrameIntervalType;
	uvideo_frame_interval_t uFrameInterval;
} UPACKED uvideo_vs_frame_mjpeg_descriptor_t;


typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bDescriptorSubtype;
	uByte		bFormatIndex;
	uDWord		dwMaxVideoFrameBufferSize;
	uByte		bFormatType;
#define UVIDEO_GET_DV_FREQ(ubyte) (((ubyte)>>7) & 1)
#define UVIDEO_DV_FORMAT_FREQ_50HZ 0
#define UVIDEO_DV_FORMAT_FREQ_60HZ 1
#define UVIDEO_GET_DV_FORMAT(ubyte) ((ubyte) & 0x3f)
#define UVIDEO_DV_FORMAT_SD_DV	0
#define UVIDEO_DV_FORMAT_SDL_DV	1
#define UVIDEO_DV_FORMAT_HD_DV	2
} UPACKED uvideo_vs_format_dv_descriptor_t;



/*
 * Video Control requests
 */

/* Pseudo bitmasks that only work when bitwise OR onto a zeroed value */
#define UVIDEO_REQUEST_TYPE_INTERFACE		(0x0001)
#define UVIDEO_REQUEST_TYPE_ENDPOINT		(0x0010)
#define UVIDEO_REQUEST_TYPE_CLASS_SPECIFIC	(0x01 << 5)
#define UVIDEO_REQUEST_TYPE_SET			(0x0 << 7)
#define UVIDEO_REQUEST_TYPE_GET			(0x1 << 7)

typedef enum {
	UVIDEO_REQUEST_DESC_INTERFACE,
	UVIDEO_REQUEST_DESC_ENDPOINT
} uvideo_request_descriptor;

typedef enum {
	UR_RC_UNDEFINED = 0x00,
	UR_SET_CUR	= 0x01,
	UR_GET_CUR	= 0x81,
	UR_GET_MIN	= 0x82,
	UR_GET_MAX	= 0x83,
	UR_GET_RES	= 0x84,
	UR_GET_LEN	= 0x85,
	UR_GET_INFO	= 0x86,
	UR_GET_DEF	= 0x87,
} uvideo_request;

/* camera terminal control selectors */
#define UVIDEO_CT_CONTROL_UNDEFINED		0x00
#define UVIDEO_CT_SCANNING_MODE_CONTROL		0x01
#define UVIDEO_CT_AE_MODE_CONTROL		0x02
#define UVIDEO_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL 0x04
#define UVIDEO_CT_EXPOSURE_TIME_RELATIVE_CONTROL 0x05
#define UVIDEO_CT_FOCUS_ABSOLUTE_CONTROL	0x06
#define UVIDEO_CT_FOCUS_RELATIVE_CONTROL	0x07
#define UVIDEO_CT_IRIS_ABSOLUTE_CONTROL		0x09
#define UVIDEO_CT_IRIS_RELATIVE_CONTROL		0x0A
#define UVIDEO_CT_ZOOM_ABSOLUTE_CONTROL		0x0B
#define UVIDEO_CT_ZOOM_RELATIVE_CONTROL		0x0C
#define UVIDEO_CT_PANTILT_ABSOLUTE_CONTROL	0x0D
#define UVIDEO_CT_PANTILT_RELATIVE_CONTROL	0x0E
#define UVIDEO_CT_ROLL_ABSOLUTE_CONTROL		0x0F
#define UVIDEO_CT_ROLL_RELATIVE_CONTROL		0x10
#define UVIDEO_CT_PRIVACY_CONTROL		0x11

/* processing unit control selectors */
#define UVIDEO_PU_CONTROL_UNDEFINED			0x00
#define UVIDEO_PU_BACKLIGHT_COMPENSATION_CONTROL	0x01
#define UVIDEO_PU_BRIGHTNESS_CONTROL			0x02
#define UVIDEO_PU_CONTRAST_CONTROL			0x03
#define UVIDEO_PU_GAIN_CONTROL				0x04
#define UVIDEO_PU_POWER_LINE_FREQUENCY_CONTROL		0x05
#define UVIDEO_PU_HUE_CONTROL				0x06
#define UVIDEO_PU_SATURATION_CONTROL			0x07
#define UVIDEO_PU_SHARPNESS_CONTROL			0x08
#define UVIDEO_PU_GAMMA_CONTROL				0x09
#define UVIDEO_PU_WHITE_BALANCE_TEMPERATURE_CONTROL	0x0A
#define UVIDEO_PU_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL 0x0B
#define UVIDEO_PU_WHITE_BALANCE_COMPONENT_CONTROL	0x0C
#define UVIDEO_PU_WHITE_BALANCE_COMPONENT_AUTO_CONTROL	0x0D
#define UVIDEO_PU_DIGITAL_MULTIPLIER_CONTROL		0x0E
#define UVIDEO_PU_DIGITAL_MULTIPLIER_LIMIT_CONTROL	0x0F
#define UVIDEO_PU_HUE_AUTO_CONTROL			0x10
#define UVIDEO_PU_ANALOG_VIDEO_STANDARD_CONTROL		0x11
#define UVIDEO_PU_ANALOG_LOCK_STATUS_CONTROL		0x12

/* extension unit control selectors */
#define UVIDEO_XU_CONTROL_UNDEFINED	0x00

/* VideoStreaming Interface control selectors */
#define UVIDEO_VS_CONTROL_UNDEFINED		0x00
#define UVIDEO_VS_PROBE_CONTROL			0x01
#define UVIDEO_VS_COMMIT_CONTROL		0x02
#define UVIDEO_VS_STILL_PROBE_CONTROL		0x03
#define UVIDEO_VS_STILL_COMMIT_CONTROL		0x04
#define UVIDEO_VS_STILL_IMAGE_TRIGGER_CONTROL	0x05
#define UVIDEO_VS_STREAM_ERROR_CODE_CONTROL	0x06
#define UVIDEO_VS_GENERATE_KEY_FRAME_CONTROL	0x07
#define UVIDEO_VS_UPDATE_FRAME_SEGMENT_CONTROL	0x08
#define UVIDEO_VS_SYNCH_DELAY_CONTROL		0x09

/* bitmask result of GET_INFO on a control */
#define UVIDEO_CONTROL_INFO_SUPPORTS_GET	(1<<0)
#define UVIDEO_CONTROL_INFO_SUPPORTS_SET	(1<<1)
#define UVIDEO_CONTROL_INFO_DISABLED		(1<<2)
#define UVIDEO_CONTROL_INFO_AUTOUPDATE		(1<<3)
#define UVIDEO_CONTROL_INFO_ASYNC		(1<<4)


/* Video Probe and Commit Controls request data */
typedef struct {
	uWord		bmHint;
#define UVIDEO_HINT_FRAME_INTERVAL	(1<<0)
#define UVIDEO_HINT_KEYFRAME_RATE	(1<<1)
#define UVIDEO_HINT_PFRAME_RATE		(1<<2)
#define UVIDEO_HINT_COMP_QUALITY	(1<<3)
#define UVIDEO_HINT_COMP_WINDOW_SIZE	(1<<4)
	uByte		bFormatIndex;
	uByte		bFrameIndex;
	uDWord		dwFrameInterval;
#define UVIDEO_100NS_PER_MS 10000
#define UVIDEO_FRAME_INTERVAL_UNITS_PER_USB_FRAME UVIDEO_100NS_PER_MS
	uWord		wKeyFrameRate;
	uWord		wPFrameRate;
	uWord		wCompQuality;
	uWord		wCompWindowSize;
	uWord		wDelay;
	uDWord		dwMaxVideoFrameSize;
	uDWord		dwMaxPayloadTransferSize;
	/* Following fields are not in v1.0 of UVC.  Will have to do
	 * UR_GET_LEN to discover the length of this descriptor. */
	uDWord		dwClockFrequency;
	uByte		bmFramingInfo;
#define UVIDEO_FRAMING_INFO_FID	(1<<0)
#define UVIDEO_FRAMING_INFO_EOF	(1<<1)
	uByte		bPreferedVersion;
	uByte		bMinVersion;
	uByte		bMaxVersion;
} UPACKED uvideo_probe_and_commit_data_t;

/* Video Still Probe and Still Commit Controls request data */
typedef struct {
	uByte		bFormatIndex;
	uByte		bFrameIndex;
	uByte		bCompressionIndex;
	uDWord		dwMaxVideoFrameSize;
	uDWord		dwMaxPayloadTransferSize;
} UPACKED uvideo_still_probe_and_still_commit_data_t;
#define UVIDEO_STILL_PROBE_AND_STILL_COMMIT_DATA_SIZE 11;



/* common header for Video Control and Video Stream status */
typedef struct {
	uByte		bStatusType;
#define UV_STATUS_TYPE_CONTROL	0x02
#define UV_STATUS_TYPE_STREAM	0x04
	uByte		bOriginator;
} UPACKED uvideo_status_t;

typedef struct {
	uByte		bStatusType;
	uByte		bOriginator;
	uByte		bEvent;
#define UV_CONTROL_CHANGE	0x00 /* any other value is Reserved */
	uByte		bSelector;
	uByte		bAttribute;
#define UV_CONTROL_VALUE_CHANGE		0x00
#define UV_CONTROL_INFO_CHANGE		0x01
#define UV_CONTROL_FAILURE_CHANGE	0x02
	uByte		bValue;
} UPACKED uvideo_control_status_t;

typedef struct {
	uByte		bStatusType;
	uByte		bOriginator;
	uByte		bEvent;
#define UV_BUTTON_PRESS	0x00 /* any other value is Stream Error */
	uByte		bValue;
#define UV_BUTTON_RELEASED	0x00
#define UV_BUTTON_PRESSED	0x01
} UPACKED uvideo_streaming_status_t;

typedef struct {
	uByte		bHeaderLength;
	uByte		bmHeaderInfo;
#define UV_FRAME_ID	1<<0
#define UV_END_OF_FRAME	1<<1
#define UV_PRES_TIME	1<<2
#define UV_SRC_CLOCK	1<<3
/* D4: Reserved */
#define UV_STILL_IMAGE	1<<5
#define UV_ERROR	1<<6
#define UV_END_OF_HDR	1<<7
/* other fields depend on which bits are set above and have no fixed offset */
/*	uDWord		dwPresentationTime; */
#define UVIDEO_PTS_SIZE 4
/*	uByte		scrSourceClock[UVIDEO_SOURCE_CLOCK_SIZE]; */
#define UVIDEO_SOURCE_CLOCK_SIZE 6
#define UV_GET_SOURCE_TIME_CLOCK(sc) (UGETDW(sc))
/* bits 42..32 */
#define UV_GET_SOF_COUNTER(sc) (((sc)[4] | ((sc)[5] << 8)) &0x7ff)
} UPACKED uvideo_payload_header_t;

/* Note: this might be larger depending on presence of source clock,
   SOF counter, or other things... bHeaderLength is actual length. */
#define UVIDEO_PAYLOAD_HEADER_SIZE 12

