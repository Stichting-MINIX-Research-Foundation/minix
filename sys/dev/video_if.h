/* $NetBSD: video_if.h,v 1.7 2010/12/24 20:54:28 jmcneill Exp $ */

/*
 * Copyright (c) 2008 Patrick Mahoney <pat@polycrystal.org>
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

/*
 * This ia a Video4Linux 2 compatible /dev/video driver for NetBSD
 *
 * See http://v4l2spec.bytesex.org/ for Video4Linux 2 specifications
 */

#ifndef _SYS_DEV_VIDEO_IF_H_
#define _SYS_DEV_VIDEO_IF_H_

#include <sys/types.h>
#include <sys/videoio.h>

#if defined(_KERNEL_OPT)
#include "video.h"

#if (NVIDEO == 0)
#error "No 'video* at videobus?' configured"
#endif

#endif	/* _KERNEL_OPT */

struct video_softc;

/* Controls provide a way to query and set controls in the camera
 * hardware.  The control structure is the primitive unit.  Control
 * groups are arrays of controls that must be set together (e.g. pan
 * direction and pan speed).  Control descriptors describe a control
 * including minimum and maximum values, read-only state, etc.  A
 * control group descriptor is an array of control descriptors
 * corresponding to a control group array of controls.
 *
 * A control_group is made up of multiple controls meant to be set
 * together and is identified by a 16 bit group_id.  Each control is
 * identified by a group_id and a control_id.  Controls that are the
 * sole member of a control_group may ignore the control_id or
 * redundantly have the control_id equal to the group_id.
 *
 * The hardware driver only handles control_group's, many of which
 * will only have a single control.
 *
 * Some id's are defined here (closely following the USB Video Class
 * controls) with room for unspecified extended controls.  These id's
 * may be used for group_id's or control_id's as appropriate.
 */

enum video_control_id {
	VIDEO_CONTROL_UNDEFINED,
	/* camera hardware */
	VIDEO_CONTROL_SCANNING_MODE,
	VIDEO_CONTROL_AE_MODE,
	VIDEO_CONTROL_EXPOSURE_TIME_ABSOLUTE,
	VIDEO_CONTROL_EXPOSURE_TIME_RELATIVE,
	VIDEO_CONTROL_FOCUS_ABSOLUTE,
	VIDEO_CONTROL_FOCUS_RELATIVE,
	VIDEO_CONTROL_IRIS_ABSOLUTE,
	VIDEO_CONTROL_IRIS_RELATIVE,
	VIDEO_CONTROL_ZOOM_ABSOLUTE,
	VIDEO_CONTROL_ZOOM_RELATIVE,
	VIDEO_CONTROL_PANTILT_ABSOLUTE,
	VIDEO_CONTROL_PANTILT_RELATIVE,
	VIDEO_CONTROL_ROLL_ABSOLUTE,
	VIDEO_CONTROL_ROLL_RELATIVE,
	VIDEO_CONTROL_PRIVACY,
	/* video processing */
	VIDEO_CONTROL_BACKLIGHT_COMPENSATION,
	VIDEO_CONTROL_BRIGHTNESS,
	VIDEO_CONTROL_CONTRAST,
	VIDEO_CONTROL_GAIN,
	VIDEO_CONTROL_GAIN_AUTO, /* not in UVC */
	VIDEO_CONTROL_POWER_LINE_FREQUENCY,
	VIDEO_CONTROL_HUE,
	VIDEO_CONTROL_SATURATION,
	VIDEO_CONTROL_SHARPNESS,
	VIDEO_CONTROL_GAMMA,
	/* Generic WHITE_BALANCE controls applies to whichever type of
	 * white balance the hardware implements to either perform one
	 * white balance action or enable auto white balance. */
	VIDEO_CONTROL_WHITE_BALANCE_ACTION,
	VIDEO_CONTROL_WHITE_BALANCE_AUTO,
	VIDEO_CONTROL_WHITE_BALANCE_TEMPERATURE,
	VIDEO_CONTROL_WHITE_BALANCE_TEMPERATURE_AUTO,
	VIDEO_CONTROL_WHITE_BALANCE_COMPONENT,
	VIDEO_CONTROL_WHITE_BALANCE_COMPONENT_AUTO,
	VIDEO_CONTROL_DIGITAL_MULTIPLIER,
	VIDEO_CONTROL_DIGITAL_MULTIPLIER_LIMIT,
	VIDEO_CONTROL_HUE_AUTO,
	VIDEO_CONTROL_ANALOG_VIDEO_STANDARD,
	VIDEO_CONTROL_ANALOG_LOCK_STATUS,
	/* video stream */
	VIDEO_CONTROL_GENERATE_KEY_FRAME,
	VIDEO_CONTROL_UPDATE_FRAME_SEGMENT,
	/* misc, not in UVC */
	VIDEO_CONTROL_HFLIP,
	VIDEO_CONTROL_VFLIP,
	/* Custom controls start here; any controls beyond this are
	 * valid and condsidered "extended". */
	VIDEO_CONTROL_EXTENDED
};

enum video_control_type {
	VIDEO_CONTROL_TYPE_INT, /* signed 32 bit integer */
	VIDEO_CONTROL_TYPE_BOOL,
	VIDEO_CONTROL_TYPE_LIST,  /* V4L2 MENU */
	VIDEO_CONTROL_TYPE_ACTION /* V4L2 BUTTON */
};

#define VIDEO_CONTROL_FLAG_READ		(1<<0)
#define VIDEO_CONTROL_FLAG_WRITE	(1<<1)
#define VIDEO_CONTROL_FLAG_DISABLED	(1<<2) /* V4L2 INACTIVE */
#define VIDEO_CONTROL_FLAG_AUTOUPDATE	(1<<3)
#define VIDEO_CONTROL_FLAG_ASYNC	(1<<4)

struct video_control_desc {
	uint16_t	group_id;
	uint16_t	control_id;
	uint8_t		name[32];
	uint32_t	flags;
	enum video_control_type type;
	int32_t		min;
	int32_t		max;
	int32_t		step;
	int32_t		def;
};

/* array of struct video_control_value_info belonging to the same control */
struct video_control_desc_group {
	uint16_t	group_id;
	uint8_t		length;
	struct video_control_desc *desc;
};

struct video_control {
	uint16_t	group_id;
	uint16_t	control_id;
	int32_t		value;
};

/* array of struct video_control_value belonging to the same control */
struct video_control_group {
	uint16_t	group_id;
	uint8_t		length;
	struct video_control *control;
};

struct video_control_iter {
	struct video_control_desc *desc;
};

/* format of video data in a video sample */
enum video_pixel_format {
	VIDEO_FORMAT_UNDEFINED,
	
	/* uncompressed frame-based formats */
	VIDEO_FORMAT_YUY2,	/* packed 4:2:2 */
	VIDEO_FORMAT_NV12,	/* planar 4:2:0 */
	VIDEO_FORMAT_RGB24,
	VIDEO_FORMAT_RGB555,
	VIDEO_FORMAT_RGB565,
	VIDEO_FORMAT_YUV420,
	VIDEO_FORMAT_SBGGR8,
	VIDEO_FORMAT_UYVY,

	/* compressed frame-based formats */
	VIDEO_FORMAT_MJPEG,	/* frames of JPEG images */
	VIDEO_FORMAT_DV,

	/* stream-based formats */
	VIDEO_FORMAT_MPEG
};

/* video standards */
enum video_standard {
	VIDEO_STANDARD_PAL_B		= 0x00000001,
	VIDEO_STANDARD_PAL_B1		= 0x00000002,
	VIDEO_STANDARD_PAL_G		= 0x00000004,
	VIDEO_STANDARD_PAL_H		= 0x00000008,
	VIDEO_STANDARD_PAL_I		= 0x00000010,
	VIDEO_STANDARD_PAL_D		= 0x00000020,
	VIDEO_STANDARD_PAL_D1		= 0x00000040,
	VIDEO_STANDARD_PAL_K		= 0x00000080,
	VIDEO_STANDARD_PAL_M		= 0x00000100,
	VIDEO_STANDARD_PAL_N		= 0x00000200,
	VIDEO_STANDARD_PAL_Nc		= 0x00000400,
	VIDEO_STANDARD_PAL_60		= 0x00000800,
	VIDEO_STANDARD_NTSC_M		= 0x00001000,
	VIDEO_STANDARD_NTSC_M_JP	= 0x00002000,
	VIDEO_STANDARD_NTSC_443		= 0x00004000,
	VIDEO_STANDARD_NTSC_M_KR	= 0x00008000,
	VIDEO_STANDARD_SECAM_B		= 0x00010000,
	VIDEO_STANDARD_SECAM_D		= 0x00020000,
	VIDEO_STANDARD_SECAM_G		= 0x00040000,
	VIDEO_STANDARD_SECAM_H		= 0x00080000,
	VIDEO_STANDARD_SECAM_K		= 0x00100000,
	VIDEO_STANDARD_SECAM_K1		= 0x00200000,
	VIDEO_STANDARD_SECAM_L		= 0x00400000,

	VIDEO_STANDARD_UNKNOWN		= 0x00000000
};

/* interlace_flags bits are allocated like this:
      7 6 5 4 3 2 1 0
	    \_/ | | |interlaced or progressive
	     |  | |packing style of fields (interlaced or planar)
             |  |fields per sample (1 or 2)
             |pattern (F1 only, F2 only, F12, RND)
*/

/* two bits */
#define VIDEO_INTERLACED(iflags) (iflags & 1)
enum video_interlace_presence {
	VIDEO_INTERLACE_OFF = 0, /* progressive */
	VIDEO_INTERLACE_ON = 1,
	VIDEO_INTERLACE_ANY = 2	/* in requests, accept any interlacing */
};

/* one bit, not in UVC */
#define VIDEO_INTERLACE_PACKING(iflags) ((iflags >> 2) & 1)
enum video_interlace_packing {
	VIDEO_INTERLACE_INTERLACED = 0, /* F1 and F2 are interlaced */
	VIDEO_INTERLACE_PLANAR = 1 /* entire F1 is followed by F2 */
};

/* one bit, not in V4L2; Is this not redundant with PATTERN below?
 * For now, I'm assuming it describes where the "end-of-frame" markers
 * appear in the stream data: after every field or after every two
 * fields. */
#define VIDEO_INTERLACE_FIELDS_PER_SAMPLE(iflags) ((iflags >> 3) & 1)
enum video_interlace_fields_per_sample {
	VIDEO_INTERLACE_TWO_FIELDS_PER_SAMPLE = 0,
	VIDEO_INTERLACE_ONE_FIELD_PER_SAMPLE = 1
};

/* two bits */
#define VIDEO_INTERLACE_PATTERN(iflags) ((iflags >> 4) & 3)
enum video_interlace_pattern {
	VIDEO_INTERLACE_PATTERN_F1 = 0,
	VIDEO_INTERLACE_PATTERN_F2 = 1,
	VIDEO_INTERLACE_PATTERN_F12 = 2,
	VIDEO_INTERLACE_PATTERN_RND = 3
};

enum video_color_primaries {
	VIDEO_COLOR_PRIMARIES_UNSPECIFIED,
	VIDEO_COLOR_PRIMARIES_BT709, /* identical to sRGB */
	VIDEO_COLOR_PRIMARIES_BT470_2_M,
	VIDEO_COLOR_PRIMARIES_BT470_2_BG,
	VIDEO_COLOR_PRIMARIES_SMPTE_170M,
	VIDEO_COLOR_PRIMARIES_SMPTE_240M,
	VIDEO_COLOR_PRIMARIES_BT878 /* in V4L2 as broken BT878 chip */
};

enum video_gamma_function {
	VIDEO_GAMMA_FUNCTION_UNSPECIFIED,
	VIDEO_GAMMA_FUNCTION_BT709,
	VIDEO_GAMMA_FUNCTION_BT470_2_M,
	VIDEO_GAMMA_FUNCTION_BT470_2_BG,
	VIDEO_GAMMA_FUNCTION_SMPTE_170M,
	VIDEO_GAMMA_FUNCTION_SMPTE_240M,
	VIDEO_GAMMA_FUNCTION_LINEAR,
	VIDEO_GAMMA_FUNCTION_sRGB, /* similar but not identical to BT709 */
	VIDEO_GAMMA_FUNCTION_BT878 /* in V4L2 as broken BT878 chip */
};

/* Matrix coefficients for converting YUV to RGB */
enum video_matrix_coeff {
	VIDEO_MATRIX_COEFF_UNSPECIFIED,
	VIDEO_MATRIX_COEFF_BT709,
	VIDEO_MATRIX_COEFF_FCC,
	VIDEO_MATRIX_COEFF_BT470_2_BG,
	VIDEO_MATRIX_COEFF_SMPTE_170M,
	VIDEO_MATRIX_COEFF_SMPTE_240M,
	VIDEO_MATRIX_COEFF_BT878 /* in V4L2 as broken BT878 chip */
};

/* UVC spec separates these into three categories.  V4L2 does not. */
struct video_colorspace {
	enum video_color_primaries primaries;
	enum video_gamma_function gamma_function;
	enum video_matrix_coeff matrix_coeff;
};

#ifdef undef
/* Stucts for future split into format/frame/interval.  All functions
 * interacting with the hardware layer will deal with these structs.
 * This video layer will handle translating them to V4L2 structs as
 * necessary. */

struct video_format {
	enum video_pixel_format	vfo_pixel_format;
	uint8_t			vfo_aspect_x; /* aspect ratio x and y */
	uint8_t			vfo_aspect_y;
	struct video_colorspace	vfo_color;
	uint8_t			vfo_interlace_flags;
};

struct video_frame {
	uint32_t	vfr_width; /* dimensions in pixels */
	uint32_t	vfr_height;
	uint32_t	vfr_sample_size; /* max sample size */
	uint32_t	vfr_stride; /* length of one row of pixels in
				     * bytes; uncompressed formats
				     * only */
};

enum video_frame_interval_type {
	VIDEO_FRAME_INTERVAL_TYPE_CONTINUOUS,
	VIDEO_FRAME_INTERVAL_TYPE_DISCRETE
};

/* UVC spec frame interval units are 100s of nanoseconds.  V4L2 spec
 * uses a {32/32} bit struct fraction in seconds. We use 100ns units
 * here. */
#define VIDEO_FRAME_INTERVAL_UNITS_PER_US (10)
#define VIDEO_FRAME_INTERVAL_UNITS_PER_MS (10 * 1000)
#define VIDEO_FRAME_INTERVAL_UNITS_PER_S  (10 * 1000 * 1000)
struct video_frame_interval {
	enum video_frame_interval_type	vfi_type;
	union {
		struct {
			uint32_t min;
			uint32_t max;
			uint32_t step;
		} vfi_continuous;

		uint32_t	vfi_discrete;
	};
};
#endif /* undef */

/* Describes a video format.  For frame based formats, one sample is
 * equivalent to one frame.  For stream based formats such as MPEG, a
 * sample is logical unit of that streaming format.
 */
struct video_format {
	enum video_pixel_format pixel_format;
	uint32_t	width;	/* dimensions in pixels */
	uint32_t	height;
	uint8_t		aspect_x; /* aspect ratio x and y */
	uint8_t		aspect_y;
	uint32_t	sample_size; /* max sample size */
	uint32_t	stride;	     /* length of one row of pixels in
				      * bytes; uncompressed formats
				      * only */
	struct video_colorspace color;
	uint8_t		interlace_flags;
	uint32_t	priv;	/* For private use by hardware driver.
				 * Must be set to zero if not used. */
};

/* A payload is the smallest unit transfered from the hardware driver
 * to the video layer. Multiple video payloads make up one video
 * sample. */
struct video_payload {
	const uint8_t	*data;
	size_t		size;		/* size in bytes of this payload */
	int		frameno;	/* toggles between 0 and 1 */
	bool		end_of_frame;	/* set if this is the last
					 * payload in the frame. */
};

/* tuner frequency, frequencies are in units of 62.5 kHz */
struct video_frequency {
	uint32_t	tuner_index;
	uint32_t	frequency;
};

/* video tuner capability flags */
#define	VIDEO_TUNER_F_MONO	(1 << 0)
#define	VIDEO_TUNER_F_STEREO	(1 << 1)
#define	VIDEO_TUNER_F_LANG1	(1 << 2)
#define	VIDEO_TUNER_F_LANG2	(1 << 3)

/* Video tuner definition */
struct video_tuner {
	uint32_t	index;
	char		name[32];	/* tuner name */
	uint32_t	freq_lo;	/* lowest tunable frequency */
	uint32_t	freq_hi;	/* highest tunable frequency */
	uint32_t	caps;		/* capability flags */
	uint32_t	mode;		/* audio mode flags */
	uint32_t	signal;		/* signal strength */
	int32_t		afc;		/* automatic frequency control */
};

/* Video input capability flags */
enum video_input_type {
	VIDEO_INPUT_TYPE_TUNER,		/* RF demodulator */
	VIDEO_INPUT_TYPE_BASEBAND,	/* analog baseband */
	VIDEO_INPUT_TYPE_CAMERA = VIDEO_INPUT_TYPE_BASEBAND,
};

#define VIDEO_STATUS_NO_POWER		(1 << 0)
#define	VIDEO_STATUS_NO_SIGNAL		(1 << 1)
#define	VIDEO_STATUS_NO_COLOR		(1 << 2)
#define	VIDEO_STATUS_NO_HLOCK		(1 << 3)
#define	VIDEO_STATUS_MACROVISION	(1 << 4)

/* Video input definition */
struct video_input {
	uint32_t	index;
	char		name[32];	/* video input name */
	enum video_input_type type;	/* input type */
	uint32_t	audiomask;	/* bitmask of assoc. audio inputs */
	uint32_t	tuner_index;	/* tuner index if applicable */
	uint64_t	standards;	/* all supported standards */
	uint32_t	status;		/* input status */
};

/* Audio input capability flags */
#define	VIDEO_AUDIO_F_STEREO	(1 << 0)
#define	VIDEO_AUDIO_F_AVL	(1 << 1)

/* Audio input definition */
struct video_audio {
	uint32_t	index;
	char		name[32];	/* audio input name */
	uint32_t	caps;		/* capabilities flags */
	uint32_t	mode;		/* audio mode flags */
};

struct video_hw_if {
	int	(*open)(void *, int); /* open hardware */
	void	(*close)(void *);     /* close hardware */

	const char *	(*get_devname)(void *);
	const char *	(*get_businfo)(void *);

	int	(*enum_format)(void *, uint32_t, struct video_format *);
	int	(*get_format)(void *, struct video_format *);
	int	(*set_format)(void *, struct video_format *);
	int	(*try_format)(void *, struct video_format *);

	int	(*enum_standard)(void *, uint32_t, enum video_standard *);
	int	(*get_standard)(void *, enum video_standard *);
	int	(*set_standard)(void *, enum video_standard);

	int	(*start_transfer)(void *);
	int	(*stop_transfer)(void *);

	int	(*control_iter_init)(void *, struct video_control_iter *);
	int	(*control_iter_next)(void *, struct video_control_iter *);
	int	(*get_control_desc_group)(void *,
					  struct video_control_desc_group *);
	int	(*get_control_group)(void *, struct video_control_group *);
	int	(*set_control_group)(void *, const struct video_control_group *);

	int	(*enum_input)(void *, uint32_t, struct video_input *);
	int	(*get_input)(void *, struct video_input *);
	int	(*set_input)(void *, struct video_input *);

	int	(*enum_audio)(void *, uint32_t, struct video_audio *);
	int	(*get_audio)(void *, struct video_audio *);
	int	(*set_audio)(void *, struct video_audio *);

	int	(*get_tuner)(void *, struct video_tuner *);
	int	(*set_tuner)(void *, struct video_tuner *);

	int	(*get_frequency)(void *, struct video_frequency *);
	int	(*set_frequency)(void *, struct video_frequency *);
};

struct video_attach_args {
	const struct video_hw_if *hw_if;
};

device_t video_attach_mi(const struct video_hw_if *, device_t);
void video_submit_payload(device_t, const struct video_payload *);

#endif	/* _SYS_DEV_VIDEO_IF_H_ */
