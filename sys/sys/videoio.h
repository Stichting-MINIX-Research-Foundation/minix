/* $NetBSD: videoio.h,v 1.8 2011/08/13 02:49:06 jakllsch Exp $ */

/*-
 * Copyright (c) 2005, 2008 Jared D. McNeill <jmcneill@invisible.ca>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* See http://v4l2spec.bytesex.org/ for Video4Linux 2 specifications */

#ifndef _HAVE_SYS_VIDEOIO_H
#define _HAVE_SYS_VIDEOIO_H

#include <sys/types.h>
#include <sys/time.h>
#ifdef _KERNEL
#include <compat/sys/time.h>
#endif

#ifndef _KERNEL
#define __u64	uint64_t
#define __u32	uint32_t
#define __u16	uint16_t
#define __u8	uint8_t
#define __s64	int64_t
#define __s32	int32_t
#define __s16	int16_t
#define __s8	int8_t
#endif

typedef uint64_t v4l2_std_id;
#define v4l2_fourcc(a,b,c,d) (((uint32_t)(a) << 0) |	\
			      ((uint32_t)(b) << 8) |	\
			      ((uint32_t)(c) << 16) |	\
			      ((uint32_t)(d) << 24))
#if 0
#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))
#endif

#define V4L2_CTRL_ID2CLASS(id)	((id >> 16) & 0xfff)
#define V4L2_CTRL_ID2CID(id)	(id & 0xffff)

enum v4l2_colorspace {
	V4L2_COLORSPACE_SMPTE170M = 1,
	V4L2_COLORSPACE_SMPTE240M,
	V4L2_COLORSPACE_REC709,
	V4L2_COLORSPACE_BT878,
	V4L2_COLORSPACE_470_SYSTEM_M,
	V4L2_COLORSPACE_470_SYSTEM_BG,
	V4L2_COLORSPACE_JPEG,
	V4L2_COLORSPACE_SRGB
};

enum v4l2_field {
	V4L2_FIELD_ANY = 0,
	V4L2_FIELD_NONE,
	V4L2_FIELD_TOP,
	V4L2_FIELD_BOTTOM,
	V4L2_FIELD_INTERLACED,
	V4L2_FIELD_SEQ_TB,
	V4L2_FIELD_SEQ_BT,
	V4L2_FIELD_ALTERNATE
};

enum v4l2_buf_type {
	V4L2_BUF_TYPE_VIDEO_CAPTURE = 1,
	V4L2_BUF_TYPE_VIDEO_OUTPUT,
	V4L2_BUF_TYPE_VIDEO_OVERLAY,
	V4L2_BUF_TYPE_VBI_CAPTURE,
	V4L2_BUF_TYPE_VBI_OUTPUT,
	V4L2_BUF_TYPE_PRIVATE = 0x80
};

enum v4l2_memory {
	V4L2_MEMORY_MMAP = 1,
	V4L2_MEMORY_USERPTR,
	V4L2_MEMORY_OVERLAY
};

enum v4l2_priority {
	V4L2_PRIORITY_UNSET = 0,
	V4L2_PRIORITY_BACKGROUND,
	V4L2_PRIORITY_INTERACTIVE,
	V4L2_PRIORITY_RECORD,
	V4L2_PRIORITY_DEFAULT = V4L2_PRIORITY_INTERACTIVE
};

enum v4l2_tuner_type {
	V4L2_TUNER_RADIO = 1,
	V4L2_TUNER_ANALOG_TV
};

enum v4l2_ctrl_type {
	V4L2_CTRL_TYPE_INTEGER = 1,
	V4L2_CTRL_TYPE_BOOLEAN,
	V4L2_CTRL_TYPE_MENU,
	V4L2_CTRL_TYPE_BUTTON
};

struct v4l2_timecode {
	uint32_t	type;
	uint32_t	flags;
	uint8_t		frames;
	uint8_t		seconds;
	uint8_t		minutes;
	uint8_t		hours;
	uint8_t		userbits[4];
};

struct v4l2_pix_format {
	uint32_t	width;
	uint32_t	height;
	uint32_t	pixelformat;
	enum v4l2_field	field;
	uint32_t	bytesperline;
	uint32_t	sizeimage;
	enum v4l2_colorspace colorspace;
	uint32_t	priv;
};

struct v4l2_buffer {
	uint32_t	index;
	enum v4l2_buf_type type;
	uint32_t	bytesused;
	uint32_t	flags;
	enum v4l2_field	field;
	struct timeval	timestamp;
	struct v4l2_timecode timecode;
	uint32_t	sequence;
	enum v4l2_memory memory;
	union {
		uint32_t	offset;
		unsigned long	userptr;
	} m;
	uint32_t	length;
	uint32_t	input;
	uint32_t	reserved;
};

#ifdef _KERNEL
struct v4l2_buffer50 {
	uint32_t	index;
	enum v4l2_buf_type type;
	uint32_t	bytesused;
	uint32_t	flags;
	enum v4l2_field	field;
	struct timeval50 timestamp;
	struct v4l2_timecode timecode;
	uint32_t	sequence;
	enum v4l2_memory memory;
	union {
		uint32_t	offset;
		unsigned long	userptr;
	} m;
	uint32_t	length;
	uint32_t	input;
	uint32_t	reserved;
};

#endif
struct v4l2_rect {
	int32_t		left;
	int32_t		top;
	int32_t		width;
	int32_t		height;
};

struct v4l2_fract {
	uint32_t	numerator;
	uint32_t	denominator;
};

struct v4l2_fmtdesc {
	uint32_t	index;
	enum v4l2_buf_type type;
	uint32_t	flags;
	uint8_t		description[32];
	uint32_t	pixelformat;
	uint32_t	reserved[4];
};

struct v4l2_clip {
	struct v4l2_rect c;
	struct v4l2_clip *next;
};

struct v4l2_window {
	struct v4l2_rect w;
	enum v4l2_field	field;
	uint32_t	chromakey;
	struct v4l2_clip *clips;
	uint32_t	clipcount;
	void		*bitmap;
};

struct v4l2_vbi_format {
	uint32_t	sampling_rate;
	uint32_t	offset;
	uint32_t	samples_per_line;
	uint32_t	sample_format;
	uint32_t	start[2];
	uint32_t	count[2];
	uint32_t	flags;
	uint32_t	reserved[2];
};

/* In the API docs, but not the Linux implementation
 *
 * struct v4l2_sliced_vbi_format {
 * 	uint32_t	service_set;
 * 	uint32_t	packet_size;
 * 	uint32_t	io_size;
 * 	uint32_t	reserved;
 * };
 *
 *
 * struct v4l2_sliced_data {
 * 	uint32_t	id;
 * 	uint32_t	line;
 * 	uint8_t		data[];
 * };
 */

struct v4l2_cropcap {
	enum v4l2_buf_type type;
	struct v4l2_rect bounds;
	struct v4l2_rect defrect;
	struct v4l2_fract pixelaspect;
};

struct v4l2_input {
	uint32_t	index;
	uint8_t		name[32];
	uint32_t	type;
	uint32_t	audioset;
	uint32_t	tuner;
	v4l2_std_id	std;
	uint32_t	status;
	uint32_t	reserved[4];
};

struct v4l2_output {
	uint32_t	index;
	uint8_t		name[32];
	uint32_t	type;
	uint32_t	audioset;
	uint32_t	modulator;
	v4l2_std_id	std;
	uint32_t	reserved[4];
};

struct v4l2_audio {
	uint32_t	index;
	uint8_t		name[32];
	uint32_t	capability;
	uint32_t	mode;
	uint32_t	reserved[2];
};

struct v4l2_audioout {
	uint32_t	index;
	uint8_t		name[32];
	uint32_t	capability;
	uint32_t	mode;
	uint32_t	reserved[2];
};

struct v4l2_compression {
	uint32_t	quality;
	uint32_t	keyframerate;
	uint32_t	pframerate;
	uint32_t	reserved[5];
};

struct v4l2_crop {
	enum v4l2_buf_type type;
	struct v4l2_rect c;
};

struct v4l2_control {
	uint32_t	id;
	int32_t		value;
};

struct v4l2_framebuffer {
	uint32_t	capability;
	uint32_t	flags;
	void		*base;
	struct v4l2_pix_format fmt;
};

struct v4l2_standard {
	uint32_t	index;
	v4l2_std_id	id;
	uint8_t		name[24];
	struct v4l2_fract frameperiod;
	uint32_t	framelines;
	uint32_t	reserved[4];
};

struct v4l2_format {
	enum v4l2_buf_type type;
	union {
		struct v4l2_pix_format pix;
		struct v4l2_window win;
		struct v4l2_vbi_format vbi;
		uint8_t		raw_data[200];
	} fmt;
};

struct v4l2_frequency {
	uint32_t	tuner;
	enum v4l2_tuner_type type;
	uint32_t	frequency;
	uint32_t	reserved[8];
};

struct v4l2_jpegcompression {
	int		quality;
	int		APPn;
	int		APP_len;
	char		APP_data[60];
	int		COM_len;
	char		COM_data[60];
	uint32_t	jpeg_markers;
};

struct v4l2_modulator {
	uint32_t	index;
	uint8_t		name[32];
	uint32_t	capability;
	uint32_t	rangelow;
	uint32_t	rangehigh;
	uint32_t	txsubchans;
	uint32_t	reserved[4];
};

struct v4l2_captureparm {
	uint32_t	capability;
	uint32_t	capturemode;
	struct v4l2_fract timeperframe;
	uint32_t	extendedmode;
	uint32_t	readbuffers;
	uint32_t	reserved[4];
};

struct v4l2_outputparm {
	uint32_t	capability;
	uint32_t	outputmode;
	struct v4l2_fract timeperframe;
	uint32_t	extendedmode;
	uint32_t	writebuffers;
	uint32_t	reserved[4];
};

struct v4l2_streamparm {
	enum v4l2_buf_type type;
	union {
		struct v4l2_captureparm capture;
		struct v4l2_outputparm output;
		uint8_t		raw_data[200];
	} parm;
};

struct v4l2_tuner {
	uint32_t	index;
	uint8_t		name[32];
	enum v4l2_tuner_type type;
	uint32_t	capability;
	uint32_t	rangelow;
	uint32_t	rangehigh;
	uint32_t	rxsubchans;
	uint32_t	audmode;
	uint32_t	signal;
	int32_t		afc;
	uint32_t	reserved[4];
};

struct v4l2_capability {
	uint8_t		driver[16];
	uint8_t		card[32];
	uint8_t		bus_info[32];
	uint32_t	version;
	uint32_t	capabilities;
	uint32_t	reserved[4];
};

struct v4l2_queryctrl {
	uint32_t	id;
	enum v4l2_ctrl_type type;
	uint8_t		name[32];
	int32_t		minimum;
	int32_t		maximum;
	int32_t		step;
	int32_t		default_value;
	uint32_t	flags;
	uint32_t	reserved[2];
};

struct v4l2_querymenu {
	uint32_t	id;
	uint32_t	index;
	uint8_t		name[32];
	uint32_t	reserved;
};

struct v4l2_requestbuffers {
	uint32_t	count;
	enum v4l2_buf_type type;
	enum v4l2_memory memory;
	uint32_t	reserved[2];
};

/* Timecode types */
#define V4L2_TC_TYPE_24FPS		1
#define V4L2_TC_TYPE_25FPS		2
#define V4L2_TC_TYPE_30FPS		3
#define V4L2_TC_TYPE_50FPS		4
#define V4L2_TC_TYPE_60FPS		5

/* Timecode flags */
#define V4L2_TC_FLAG_DROPFRAME		0x0001
#define V4L2_TC_FLAG_COLORFRAME		0x0002
#define V4L2_TC_USERBITS_field		0x000c
#define V4L2_TC_USERBITS_USERDEFINED	0x0000
#define V4L2_TC_USERBITS_8BITCHARS	0x0008

/* Buffer flags */
#define V4L2_BUF_FLAG_MAPPED		0x0001
#define V4L2_BUF_FLAG_QUEUED		0x0002
#define V4L2_BUF_FLAG_DONE		0x0004
#define V4L2_BUF_FLAG_KEYFRAME		0x0008
#define V4L2_BUF_FLAG_PFRAME		0x0010
#define V4L2_BUF_FLAG_BFRAME		0x0020
#define V4L2_BUF_FLAG_TIMECODE		0x0100
#define V4L2_BUF_FLAG_INPUT		0x0200

/* Image format description flags */
#define V4L2_FMT_FLAG_COMPRESSED	0x0001

/* Input types */
#define V4L2_INPUT_TYPE_TUNER		1
#define V4L2_INPUT_TYPE_CAMERA		2

/* Input status flags */
#define V4L2_IN_ST_NO_POWER		0x00000001
#define V4L2_IN_ST_NO_SIGNAL		0x00000002
#define V4L2_IN_ST_NO_COLOR		0x00000004
#define V4L2_IN_ST_NO_H_LOCK		0x00000100
#define V4L2_IN_ST_COLOR_KILL		0x00000200
#define V4L2_IN_ST_NO_SYNC		0x00010000
#define V4L2_IN_ST_NO_EQU		0x00020000
#define V4L2_IN_ST_NO_CARRIER		0x00040000
#define V4L2_IN_ST_MACROVISION		0x01000000
#define V4L2_IN_ST_NO_ACCESS		0x02000000
#define V4L2_IN_ST_VTR			0x04000000

/* Output types */
#define V4L2_OUTPUT_TYPE_MODULATOR		1
#define V4L2_OUTPUT_TYPE_ANALOG			2
#define V4L2_OUTPUT_TYPE_ANALOGVGAOVERLAY	3

/* Audio capability flags */
#define V4L2_AUDCAP_STEREO		0x00001
#define V4L2_AUDCAP_AVL			0x00002

/* Audio modes */
#define V4L2_AUDMODE_AVL		0x00001

/* Frame buffer capability flags */
#define V4L2_FBUF_CAP_EXTERNOVERLAY	0x0001
#define V4L2_FBUF_CAP_CHROMAKEY		0x0002
#define V4L2_FBUF_CAP_LIST_CLIPPING	0x0004
#define V4L2_FBUF_CAP_BITMAP_CLIPPING	0x0008

/* Frame buffer flags */
#define V4L2_FBUF_FLAG_PRIMARY		0x0001
#define V4L2_FBUF_FLAG_OVERLAY		0x0002
#define V4L2_FBUF_FLAG_CHROMAKEY	0x0004

/* JPEG markers flags */
#define V4L2_JPEG_MARKER_DHT		(1 << 3)
#define V4L2_JPEG_MARKER_DQT		(1 << 4)
#define V4L2_JPEG_MARKER_DRI		(1 << 5)
#define V4L2_JPEG_MARKER_COM		(1 << 6)
#define V4L2_JPEG_MARKER_APP		(1 << 7)

/* Streaming parameters capabilities */
#define V4L2_CAP_TIMEPERFRAME		0x1000

/* Capture parameters flags */
#define V4L2_MODE_HIGHQUALITY		0x0001

/* Tuner and modulator capability flags */
#define V4L2_TUNER_CAP_LOW		0x0001
#define V4L2_TUNER_CAP_NORM		0x0002
#define V4L2_TUNER_CAP_STEREO		0x0010
#define V4L2_TUNER_CAP_LANG2		0x0020
#define V4L2_TUNER_CAP_SAP		0x0020
#define V4L2_TUNER_CAP_LANG1		0x0040

/* Tuner and modulation audio transmission flags */
#define V4L2_TUNER_SUB_MONO		0x0001
#define V4L2_TUNER_SUB_STEREO		0x0002
#define V4L2_TUNER_SUB_LANG2		0x0004
#define V4L2_TUNER_SUB_SAP		0x0004
#define V4L2_TUNER_SUB_LANG1		0x0008

/* Tuner audio modes */
#define V4L2_TUNER_MODE_MONO		0
#define V4L2_TUNER_MODE_STEREO		1
#define V4L2_TUNER_MODE_LANG2		2
#define V4L2_TUNER_MODE_SAP		2
#define V4L2_TUNER_MODE_LANG1		3
#define V4L2_TUNER_MODE_LANG1_LANG2	4

/* Control flags */
#define V4L2_CTRL_FLAG_DISABLED		0x0001
#define V4L2_CTRL_FLAG_GRABBED		0x0002
#define V4L2_CTRL_FLAG_READ_ONLY	0x0004
#define V4L2_CTRL_FLAG_UPDATE		0x0008
#define V4L2_CTRL_FLAG_INACTIVE		0x0010
#define V4L2_CTRL_FLAG_SLIDER		0x0020

/* Control IDs defined by V4L2 */
#define V4L2_CID_BASE			0x00980900
#define V4L2_CID_PRIVATE_BASE		0x08000000

#define V4L2_CID_BRIGHTNESS		(V4L2_CID_BASE + 0)
#define V4L2_CID_CONTRAST		(V4L2_CID_BASE + 1)
#define V4L2_CID_SATURATION		(V4L2_CID_BASE + 2)
#define V4L2_CID_HUE			(V4L2_CID_BASE + 3)
#define V4L2_CID_AUDIO_VOLUME		(V4L2_CID_BASE + 5)
#define V4L2_CID_AUDIO_BALANCE		(V4L2_CID_BASE + 6)
#define V4L2_CID_AUDIO_BASS		(V4L2_CID_BASE + 7)
#define V4L2_CID_AUDIO_TREBLE		(V4L2_CID_BASE + 8)
#define V4L2_CID_AUDIO_MUTE		(V4L2_CID_BASE + 9)
#define V4L2_CID_AUDIO_LOUDNESS		(V4L2_CID_BASE + 10)
#define V4L2_CID_BLACK_LEVEL		(V4L2_CID_BASE + 11)
#define V4L2_CID_AUTO_WHITE_BALANCE	(V4L2_CID_BASE + 12)
#define V4L2_CID_DO_WHITE_BALANCE	(V4L2_CID_BASE + 13)
#define V4L2_CID_RED_BALANCE		(V4L2_CID_BASE + 14)
#define V4L2_CID_BLUE_BALANCE		(V4L2_CID_BASE + 15)
#define V4L2_CID_GAMMA			(V4L2_CID_BASE + 16)
#define V4L2_CID_WHITENESS		(V4L2_CID_GAMMA)
#define V4L2_CID_EXPOSURE		(V4L2_CID_BASE + 17)
#define V4L2_CID_AUTOGAIN		(V4L2_CID_BASE + 18)
#define V4L2_CID_GAIN			(V4L2_CID_BASE + 19)
#define V4L2_CID_HFLIP			(V4L2_CID_BASE + 20)
#define V4L2_CID_VFLIP			(V4L2_CID_BASE + 21)
#define V4L2_CID_HCENTER_DEPRECATED	(V4L2_CID_BASE + 22)
#define V4L2_CID_VCENTER_DEPRECATED	(V4L2_CID_BASE + 23)
#define V4L2_CID_HCENTER	V4L2_CID_HCENTER_DEPRECATED
#define V4L2_CID_VCENTER	V4L2_CID_VCENTER_DEPRECATED
#define V4L2_CID_POWER_LINE_FREQUENCY	(V4L2_CID_BASE + 24)
#define V4L2_CID_HUE_AUTO		(V4L2_CID_BASE + 25)
#define V4L2_CID_WHITE_BALANCE_TEMPERATURE (V4L2_CID_BASE + 26)
#define V4L2_CID_SHARPNESS		(V4L2_CID_BASE + 27)
#define V4L2_CID_BACKLIGHT_COMPENSATION	(V4L2_CID_BASE + 28)
#define V4L2_CID_LASTP1			(V4L2_CID_BASE + 29)

/* Pixel formats */
#define V4L2_PIX_FMT_RGB332	v4l2_fourcc('R', 'G', 'B', '1')
#define V4L2_PIX_FMT_RGB555	v4l2_fourcc('R', 'G', 'B', 'O')
#define V4L2_PIX_FMT_RGB565	v4l2_fourcc('R', 'G', 'B', 'P')
#define V4L2_PIX_FMT_RGB555X	v4l2_fourcc('R', 'G', 'B', 'Q')
#define V4L2_PIX_FMT_RGB565X	v4l2_fourcc('R', 'G', 'B', 'R')
#define V4L2_PIX_FMT_BGR24	v4l2_fourcc('B', 'G', 'R', '3')
#define V4L2_PIX_FMT_RGB24	v4l2_fourcc('R', 'G', 'B', '3')
#define V4L2_PIX_FMT_BGR32	v4l2_fourcc('B', 'G', 'R', '4')
#define V4L2_PIX_FMT_RGB32	v4l2_fourcc('R', 'G', 'B', '4')
#define V4L2_PIX_FMT_GREY	v4l2_fourcc('G', 'R', 'E', 'Y')
#define V4L2_PIX_FMT_YUYV	v4l2_fourcc('Y', 'U', 'Y', 'V')
#define V4L2_PIX_FMT_UYVY	v4l2_fourcc('U', 'Y', 'V', 'Y')
#define V4L2_PIX_FMT_Y41P	v4l2_fourcc('Y', '4', '1', 'P')
#define V4L2_PIX_FMT_YVU420	v4l2_fourcc('Y', 'V', '1', '2')
#define V4L2_PIX_FMT_YUV420	v4l2_fourcc('Y', 'U', '1', '2')
#define V4L2_PIX_FMT_YVU410	v4l2_fourcc('Y', 'V', 'U', '9')
#define V4L2_PIX_FMT_YUV410	v4l2_fourcc('Y', 'U', 'V', '9')
#define V4L2_PIX_FMT_YUV422P	v4l2_fourcc('4', '2', '2', 'P')
#define V4L2_PIX_FMT_YUV411P	v4l2_fourcc('Y', '1', '1', 'P')
#define V4L2_PIX_FMT_NV12	v4l2_fourcc('N', 'V', '1', '2')
#define V4L2_PIX_FMT_NV21	v4l2_fourcc('N', 'V', '2', '1')
/* http://www.siliconimaging.com/RGB%20Bayer.htm */
#define V4L2_PIX_FMT_SBGGR8	v4l2_fourcc('B', 'A', '8', '1')
/* Reserved pixel formats */
#define V4L2_PIX_FMT_YYUV	v4l2_fourcc('Y', 'Y', 'U', 'V')
#define V4L2_PIX_FMT_HI240	v4l2_fourcc('H', 'I', '2', '4')
#define V4L2_PIX_FMT_MJPEG	v4l2_fourcc('M', 'J', 'P', 'G')
#define V4L2_PIX_FMT_JPEG	v4l2_fourcc('J', 'P', 'E', 'G')
#define V4L2_PIX_FMT_DV		v4l2_fourcc('d', 'v', 's', 'd')
#define V4L2_PIX_FMT_MPEG	v4l2_fourcc('M', 'P', 'E', 'G')
#define V4L2_PIX_FMT_WNVA	v4l2_fourcc('W', 'N', 'V', 'A')
#define V4L2_PIX_FMT_SN9C10X	v4l2_fourcc('S', '9', '1', '0')

/* Video standards */
#define V4L2_STD_PAL_B		((v4l2_std_id)0x00000001)
#define V4L2_STD_PAL_B1		((v4l2_std_id)0x00000002)
#define V4L2_STD_PAL_G		((v4l2_std_id)0x00000004)
#define V4L2_STD_PAL_H		((v4l2_std_id)0x00000008)
#define V4L2_STD_PAL_I		((v4l2_std_id)0x00000010)
#define V4L2_STD_PAL_D		((v4l2_std_id)0x00000020)
#define V4L2_STD_PAL_D1		((v4l2_std_id)0x00000040)
#define V4L2_STD_PAL_K		((v4l2_std_id)0x00000080)
#define V4L2_STD_PAL_M		((v4l2_std_id)0x00000100)
#define V4L2_STD_PAL_N		((v4l2_std_id)0x00000200)
#define V4L2_STD_PAL_Nc		((v4l2_std_id)0x00000400)
#define V4L2_STD_PAL_60		((v4l2_std_id)0x00000800)
#define V4L2_STD_NTSC_M		((v4l2_std_id)0x00001000)
#define V4L2_STD_NTSC_M_JP	((v4l2_std_id)0x00002000)
#define V4L2_STD_SECAM_B	((v4l2_std_id)0x00010000)
#define V4L2_STD_SECAM_D	((v4l2_std_id)0x00020000)
#define V4L2_STD_SECAM_G	((v4l2_std_id)0x00040000)
#define V4L2_STD_SECAM_H	((v4l2_std_id)0x00080000)
#define V4L2_STD_SECAM_K	((v4l2_std_id)0x00100000)
#define V4L2_STD_SECAM_K1	((v4l2_std_id)0x00200000)
#define V4L2_STD_SECAM_L	((v4l2_std_id)0x00400000)
#define V4L2_STD_ATSC_8_VSB	((v4l2_std_id)0x01000000)
#define V4L2_STD_ATSC_16_VSB	((v4l2_std_id)0x02000000)
#define V4L2_STD_PAL_BG		(V4L2_STD_PAL_B |	\
				 V4L2_STD_PAL_B1 |	\
				 V4L2_STD_PAL_G)
#define V4L2_STD_PAL_DK		(V4L2_STD_PAL_D |	\
				 V4L2_STD_PAL_D1 |	\
				 V4L2_STD_PAL_K)
#define V4L2_STD_PAL		(V4L2_STD_PAL_BG |	\
				 V4L2_STD_PAL_DK |	\
				 V4L2_STD_PAL_H |	\
				 V4L2_STD_PAL_I)
#define V4L2_STD_NTSC		(V4L2_STD_NTSC_M |	\
				 V4L2_STD_NTSC_M_JP)
#define V4L2_STD_SECAM		(V4L2_STD_SECAM_B |	\
				 V4L2_STD_SECAM_D |	\
				 V4L2_STD_SECAM_G |	\
				 V4L2_STD_SECAM_H |	\
				 V4L2_STD_SECAM_K |	\
				 V4L2_STD_SECAM_K1 |	\
				 V4L2_STD_SECAM_L)
#define V4L2_STD_525_60		(V4L2_STD_PAL_M |	\
				 V4L2_STD_PAL_60 |	\
				 V4L2_STD_NTSC)
#define V4L2_STD_625_50		(V4L2_STD_PAL |		\
				 V4L2_STD_PAL_N |	\
				 V4L2_STD_PAL_Nc |	\
				 V4L2_STD_SECAM)
#define V4L2_STD_UNKNOWN	0
#define V4L2_STD_ALL		(V4L2_STD_525_60 |	\
				 V4L2_STD_625_50)

/* Raw VBI format flags */
#define V4L2_VBI_UNSYNC			0x0001
#define V4L2_VBI_INTERLACED		0x0002

/* Device capabilities */
#define V4L2_CAP_VIDEO_CAPTURE		0x00000001
#define V4L2_CAP_VIDEO_OUTPUT		0x00000002
#define V4L2_CAP_VIDEO_OVERLAY		0x00000004
#define V4L2_CAP_VBI_CAPTURE		0x00000010
#define V4L2_CAP_VBI_OUTPUT		0x00000020
#define V4L2_CAP_RDS_CAPTURE		0x00000100
#define V4L2_CAP_TUNER			0x00010000
#define V4L2_CAP_AUDIO			0x00020000
#define V4L2_CAP_READWRITE		0x01000000
#define V4L2_CAP_ASYNCIO		0x02000000
#define V4L2_CAP_STREAMING		0x04000000
#define V4L2_CAP_BITMASK	\
	"\20\1VIDEO_CAPTURE\2VIDEO_OUTPUT\3VIDEO_OVERLAY"	\
	"\5VBI_CAPTURE\6VBI_OUTPUT\10RDS_CAPTURE"		\
	"\21TUNER\22AUDIO\31READWRITE"				\
	"\32ASYNCIO\33STREAMING"

/* Device ioctls -- try to keep them the same as Linux for compat_linux */
#define VIDIOC_QUERYCAP		_IOR('V', 0, struct v4l2_capability)
#define VIDIOC_RESERVED		_IO('V', 1)
#define VIDIOC_ENUM_FMT		_IOWR('V', 2, struct v4l2_fmtdesc)
#define VIDIOC_G_FMT		_IOWR('V', 4, struct v4l2_format)
#define VIDIOC_S_FMT		_IOWR('V', 5, struct v4l2_format)
/* 6 and 7 are VIDIOC_[SG]_COMP, which are unsupported */
#define VIDIOC_REQBUFS		_IOWR('V', 8, struct v4l2_requestbuffers)
#define VIDIOC_QUERYBUF		_IOWR('V', 9, struct v4l2_buffer)
#define VIDIOC_G_FBUF		_IOR('V', 10, struct v4l2_framebuffer)
#define VIDIOC_S_FBUF		_IOW('V', 11, struct v4l2_framebuffer)
#define VIDIOC_OVERLAY		_IOW('V', 14, int)
#define VIDIOC_QBUF		_IOWR('V', 15, struct v4l2_buffer)
#define VIDIOC_DQBUF		_IOWR('V', 17, struct v4l2_buffer)
#define VIDIOC_STREAMON		_IOW('V', 18, int)
#define VIDIOC_STREAMOFF	_IOW('V', 19, int)
#define VIDIOC_G_PARM		_IOWR('V', 21, struct v4l2_streamparm)
#define VIDIOC_S_PARM		_IOWR('V', 22, struct v4l2_streamparm)
#define VIDIOC_G_STD		_IOR('V', 23, v4l2_std_id)
#define VIDIOC_S_STD		_IOW('V', 24, v4l2_std_id)
#define VIDIOC_ENUMSTD		_IOWR('V', 25, struct v4l2_standard)
#define VIDIOC_ENUMINPUT	_IOWR('V', 26, struct v4l2_input)
#define VIDIOC_G_CTRL		_IOWR('V', 27, struct v4l2_control)
#define VIDIOC_S_CTRL		_IOWR('V', 28, struct v4l2_control)
#define VIDIOC_G_TUNER		_IOWR('V', 29, struct v4l2_tuner)
#define VIDIOC_S_TUNER		_IOW('V', 30, struct v4l2_tuner)
#define VIDIOC_G_AUDIO		_IOR('V', 33, struct v4l2_audio)
#define VIDIOC_S_AUDIO		_IOW('V', 34, struct v4l2_audio)
#define VIDIOC_QUERYCTRL	_IOWR('V', 36, struct v4l2_queryctrl)
#define VIDIOC_QUERYMENU	_IOWR('V', 37, struct v4l2_querymenu)
#define VIDIOC_G_INPUT		_IOR('V', 38, int)
#define VIDIOC_S_INPUT		_IOWR('V', 39, int)
#define VIDIOC_G_OUTPUT		_IOR('V', 46, int)
#define VIDIOC_S_OUTPUT		_IOWR('V', 47, int)
#define VIDIOC_ENUMOUTPUT	_IOWR('V', 48, struct v4l2_output)
#define VIDIOC_G_AUDOUT		_IOR('V', 49, struct v4l2_audioout)
#define VIDIOC_S_AUDOUT		_IOW('V', 50, struct v4l2_audioout)
#define VIDIOC_G_MODULATOR	_IOWR('V', 54, struct v4l2_modulator)
#define VIDIOC_S_MODULATOR	_IOW('V', 55, struct v4l2_modulator)
#define VIDIOC_G_FREQUENCY	_IOWR('V', 56, struct v4l2_frequency)
#define VIDIOC_S_FREQUENCY	_IOW('V', 57, struct v4l2_frequency)
#define VIDIOC_CROPCAP		_IOWR('V', 58, struct v4l2_cropcap)
#define VIDIOC_G_CROP		_IOWR('V', 59, struct v4l2_crop)
#define VIDIOC_S_CROP		_IOW('V', 60, struct v4l2_crop)
#define VIDIOC_G_JPEGCOMP	_IOR('V', 61, struct v4l2_jpegcompression)
#define VIDIOC_S_JPEGCOMP	_IOW('V', 62, struct v4l2_jpegcompression)
#define VIDIOC_QUERYSTD		_IOR('V', 63, v4l2_std_id)
#define VIDIOC_TRY_FMT		_IOWR('V', 64, struct v4l2_format)
#define VIDIOC_ENUMAUDIO	_IOWR('V', 65, struct v4l2_audio)
#define VIDIOC_ENUMAUDOUT	_IOWR('V', 66, struct v4l2_audioout)
#define VIDIOC_G_PRIORITY	_IOR('V', 67, enum v4l2_priority)
#define VIDIOC_S_PRIORITY	_IOW('V', 68, enum v4l2_priority)

#ifdef _KERNEL
#define VIDIOC_QUERYBUF50	_IOWR('V', 9, struct v4l2_buffer50)
#define VIDIOC_QBUF50		_IOWR('V', 15, struct v4l2_buffer50)
#define VIDIOC_DQBUF50		_IOWR('V', 17, struct v4l2_buffer50)
#endif

#endif /* !_HAVE_SYS_VIDEOIO_H */
