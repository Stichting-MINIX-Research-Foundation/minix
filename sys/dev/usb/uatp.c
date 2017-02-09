/*	$NetBSD: uatp.c,v 1.11 2015/03/07 20:20:55 mrg Exp $	*/

/*-
 * Copyright (c) 2011-2014 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Taylor R. Campbell.
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
 * uatp(4) - USB Apple Trackpad
 *
 * The uatp driver talks the protocol of the USB trackpads found in
 * Apple laptops since 2005, including PowerBooks, iBooks, MacBooks,
 * and MacBook Pros.  Some of these also present generic USB HID mice
 * on another USB report id, which the ums(4) driver can handle, but
 * Apple's protocol gives more detailed sensor data that lets us detect
 * multiple fingers to emulate multi-button mice and scroll wheels.
 */

/*
 * Protocol
 *
 * The device has a set of horizontal sensors, each being a column at a
 * particular position on the x axis that tells you whether there is
 * pressure anywhere on that column, and vertical sensors, each being a
 * row at a particular position on the y axis that tells you whether
 * there is pressure anywhere on that row.
 *
 * Whenever the device senses anything, it emits a readout of all of
 * the sensors, in some model-dependent order.  (For the order, see
 * read_sample_1 and read_sample_2.)  Each sensor datum is an unsigned
 * eight-bit quantity representing some measure of pressure.  (Of
 * course, it really measures capacitance, not pressure, but we'll call
 * it `pressure' here.)
 */

/*
 * Interpretation
 *
 * To interpret the finger's position on the trackpad, the driver
 * computes a weighted average over all possible positions, weighted by
 * the pressure at that position.  The weighted average is computed in
 * the dimensions of the screen, rather than the trackpad, in order to
 * admit a finer resolution of positions than the trackpad grid.
 *
 * To update the finger's position smoothly on the trackpad, the driver
 * computes a weighted average of the old raw position, the old
 * smoothed position, and the new smoothed position.  The weights are
 * given by the old_raw_weight, old_smoothed_weight, and new_raw_weight
 * sysctl knobs.
 *
 * Finally, to move the cursor, the driver takes the difference between
 * the old and new positions and accelerates it according to some
 * heuristic knobs that need to be reworked.
 *
 * Finally, there are some bells & whistles to detect tapping and to
 * emulate a three-button mouse by leaving two or three fingers on the
 * trackpad while pressing the button.
 */

/*
 * Future work
 *
 * With the raw sensor data available, we could implement fancier bells
 * & whistles too, such as pinch-to-zoom.  However, wsmouse supports
 * only four-dimensional mice with buttons, and we already use two
 * dimensions for mousing and two dimensions for scrolling, so there's
 * no straightforward way to report zooming and other gestures to the
 * operating system.  Probably a better way to do this would be just to
 * attach uhid(4) instead of uatp(4) and to read the raw sensors data
 * yourself -- but that requires hairy mode switching for recent models
 * (see geyser34_enable_raw_mode).
 *
 * XXX Rework the acceleration knobs.
 * XXX Implement edge scrolling.
 * XXX Fix sysctl setup; preserve knobs across suspend/resume.
 *     (uatp0 detaches and reattaches across suspend/resume, so as
 *     written, the sysctl tree is torn down and rebuilt, losing any
 *     state the user may have set.)
 * XXX Refactor motion state so I can understand it again.
 *     Should make a struct uatp_motion for all that state.
 * XXX Add hooks for ignoring trackpad input while typing.
 */

/*
 * Classifying devices
 *
 * I have only one MacBook to test this driver, but the driver should
 * be applicable to almost every Apple laptop made since the beginning
 * of 2005, so the driver reports lots of debugging output to help to
 * classify devices.  Boot with `boot -v' (verbose) and check the
 * output of `dmesg | grep uatp' to answer the following questions:
 *
 * - What devices (vendor, product, class, subclass, proto, USB HID
 *   report dump) fail to attach when you think they should work?
 *     (vendor not apple, class not hid, proto not mouse)
 *
 * - What devices have an unknown product id?
 *     `unknown vendor/product id'
 *
 * - What devices have the wrong screen-to-trackpad ratios?
 *     `... x sensors, scaled by ... for ... points on screen'
 *     `... y sensors, scaled by ... for ... points on screen'
 *   You can tweak hw.uatp0.x_ratio and hw.uatp0.y_ratio to adjust
 *   this, up to a maximum of 384 for each value.
 *
 * - What devices have the wrong input size?
 *     `expected input size ... but got ... for Apple trackpad'
 *
 * - What devices give wrong-sized packets?
 *     `discarding ...-byte input'
 *
 * - What devices split packets in chunks?
 *     `partial packet: ... bytes'
 *
 * - What devices develop large sensor readouts?
 *     `large sensor readout: ...'
 *
 * - What devices have the wrong number of sensors?  Are there parts of
 *   your trackpad that the system doesn't seem to notice?  You can
 *   tweak hw.uatp0.x_sensors and hw.uatp0.y_sensors, up to a maximum
 *   of 32 for each value.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uatp.c,v 1.11 2015/03/07 20:20:55 mrg Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/atomic.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/time.h>

/* Order is important here...sigh...  */
#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/uhidev.h>
#include <dev/usb/hid.h>
#include <dev/usb/usbhid.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsmousevar.h>

#define CHECK(condition, fail) do {					\
	if (! (condition)) {						\
		aprint_error_dev(uatp_dev(sc), "%s: check failed: %s\n",\
			__func__, #condition);				\
		fail;							\
	}								\
} while (0)

#define UATP_DEBUG_ATTACH	(1 << 0)
#define UATP_DEBUG_MISC		(1 << 1)
#define UATP_DEBUG_WSMOUSE	(1 << 2)
#define UATP_DEBUG_IOCTL	(1 << 3)
#define UATP_DEBUG_RESET	(1 << 4)
#define UATP_DEBUG_INTR		(1 << 5)
#define UATP_DEBUG_PARSE	(1 << 6)
#define UATP_DEBUG_TAP		(1 << 7)
#define UATP_DEBUG_EMUL_BUTTON	(1 << 8)
#define UATP_DEBUG_ACCUMULATE	(1 << 9)
#define UATP_DEBUG_STATUS	(1 << 10)
#define UATP_DEBUG_SPURINTR	(1 << 11)
#define UATP_DEBUG_MOVE		(1 << 12)
#define UATP_DEBUG_ACCEL	(1 << 13)
#define UATP_DEBUG_TRACK_DIST	(1 << 14)
#define UATP_DEBUG_PALM		(1 << 15)

#if UATP_DEBUG
#  define DPRINTF(sc, flags, format) do {				\
	if ((flags) & (sc)->sc_debug_flags) {				\
		printf("%s: %s: ", device_xname(uatp_dev(sc)), __func__); \
		printf format;						\
	}								\
} while (0)
#else
#  define DPRINTF(sc, flags, format) do {} while (0)
#endif

/* Maximum number of bytes in an incoming packet of sensor data.  */
#define UATP_MAX_INPUT_SIZE	81

/* Maximum number of sensors in each dimension.  */
#define UATP_MAX_X_SENSORS	32
#define UATP_MAX_Y_SENSORS	32
#define UATP_MAX_SENSORS	32
#define UATP_SENSORS		(UATP_MAX_X_SENSORS + UATP_MAX_Y_SENSORS)

/* Maximum accumulated sensor value.  */
#define UATP_MAX_ACC		0xff

/* Maximum screen dimension to sensor dimension ratios.  */
#define UATP_MAX_X_RATIO	0x180
#define UATP_MAX_Y_RATIO	0x180
#define UATP_MAX_RATIO		0x180

/* Maximum weight for positions in motion calculation.  */
#define UATP_MAX_WEIGHT		0x7f

/* Maximum possible trackpad position in a single dimension.  */
#define UATP_MAX_POSITION	(UATP_MAX_SENSORS * UATP_MAX_RATIO)

/* Bounds on acceleration.  */
#define UATP_MAX_MOTION_MULTIPLIER	16

/* Status bits transmitted in the last byte of an input packet.  */
#define UATP_STATUS_BUTTON	(1 << 0)	/* Button pressed */
#define UATP_STATUS_BASE	(1 << 2)	/* Base sensor data */
#define UATP_STATUS_POST_RESET	(1 << 4)	/* Post-reset */

/* Forward declarations */

struct uatp_softc;		/* Device driver state.  */
struct uatp_descriptor;		/* Descriptor for a particular model.  */
struct uatp_parameters;		/* Parameters common to a set of models.  */
struct uatp_knobs;		/* User-settable configuration knobs.  */
enum uatp_tap_state {
	TAP_STATE_INITIAL,
	TAP_STATE_TAPPING,
	TAP_STATE_TAPPED,
	TAP_STATE_DOUBLE_TAPPING,
	TAP_STATE_DRAGGING_DOWN,
	TAP_STATE_DRAGGING_UP,
	TAP_STATE_TAPPING_IN_DRAG,
};

static const struct uatp_descriptor *find_uatp_descriptor
    (const struct uhidev_attach_arg *);
static device_t uatp_dev(const struct uatp_softc *);
static uint8_t *uatp_x_sample(struct uatp_softc *);
static uint8_t *uatp_y_sample(struct uatp_softc *);
static int *uatp_x_acc(struct uatp_softc *);
static int *uatp_y_acc(struct uatp_softc *);
static void uatp_clear_position(struct uatp_softc *);
static unsigned int uatp_x_sensors(const struct uatp_softc *);
static unsigned int uatp_y_sensors(const struct uatp_softc *);
static unsigned int uatp_x_ratio(const struct uatp_softc *);
static unsigned int uatp_y_ratio(const struct uatp_softc *);
static unsigned int uatp_old_raw_weight(const struct uatp_softc *);
static unsigned int uatp_old_smoothed_weight(const struct uatp_softc *);
static unsigned int uatp_new_raw_weight(const struct uatp_softc *);
static int scale_motion(const struct uatp_softc *, int, int *,
    const unsigned int *, const unsigned int *);
static int uatp_scale_motion(const struct uatp_softc *, int, int *);
static int uatp_scale_fast_motion(const struct uatp_softc *, int, int *);
static int uatp_match(device_t, cfdata_t, void *);
static void uatp_attach(device_t, device_t, void *);
static void uatp_setup_sysctl(struct uatp_softc *);
static bool uatp_setup_sysctl_knob(struct uatp_softc *, int *, const char *,
    const char *);
static void uatp_childdet(device_t, device_t);
static int uatp_detach(device_t, int);
static int uatp_activate(device_t, enum devact);
static int uatp_enable(void *);
static void uatp_disable(void *);
static int uatp_ioctl(void *, unsigned long, void *, int, struct lwp *);
static void geyser34_enable_raw_mode(struct uatp_softc *);
static void geyser34_initialize(struct uatp_softc *);
static int geyser34_finalize(struct uatp_softc *);
static void geyser34_deferred_reset(struct uatp_softc *);
static void geyser34_reset_task(void *);
static void uatp_intr(struct uhidev *, void *, unsigned int);
static bool base_sample_softc_flag(const struct uatp_softc *, const uint8_t *);
static bool base_sample_input_flag(const struct uatp_softc *, const uint8_t *);
static void read_sample_1(uint8_t *, uint8_t *, const uint8_t *);
static void read_sample_2(uint8_t *, uint8_t *, const uint8_t *);
static void accumulate_sample_1(struct uatp_softc *);
static void accumulate_sample_2(struct uatp_softc *);
static void uatp_input(struct uatp_softc *, uint32_t, int, int, int, int);
static uint32_t uatp_tapped_buttons(struct uatp_softc *);
static bool interpret_input(struct uatp_softc *, int *, int *, int *, int *,
    uint32_t *);
static unsigned int interpret_dimension(struct uatp_softc *, const int *,
    unsigned int, unsigned int, unsigned int *, unsigned int *);
static void tap_initialize(struct uatp_softc *);
static void tap_finalize(struct uatp_softc *);
static void tap_enable(struct uatp_softc *);
static void tap_disable(struct uatp_softc *);
static void tap_transition(struct uatp_softc *, enum uatp_tap_state,
    const struct timeval *, unsigned int, unsigned int);
static void tap_transition_initial(struct uatp_softc *);
static void tap_transition_tapping(struct uatp_softc *, const struct timeval *,
    unsigned int);
static void tap_transition_double_tapping(struct uatp_softc *,
    const struct timeval *, unsigned int);
static void tap_transition_dragging_down(struct uatp_softc *);
static void tap_transition_tapping_in_drag(struct uatp_softc *,
    const struct timeval *, unsigned int);
static void tap_transition_tapped(struct uatp_softc *, const struct timeval *);
static void tap_transition_dragging_up(struct uatp_softc *);
static void tap_reset(struct uatp_softc *);
static void tap_reset_wait(struct uatp_softc *);
static void tap_touched(struct uatp_softc *, unsigned int);
static bool tap_released(struct uatp_softc *);
static void schedule_untap(struct uatp_softc *);
static void untap_callout(void *);
static uint32_t emulated_buttons(struct uatp_softc *, unsigned int);
static void update_position(struct uatp_softc *, unsigned int,
    unsigned int, unsigned int, int *, int *, int *, int *);
static void move_mouse(struct uatp_softc *, unsigned int, unsigned int,
    int *, int *);
static void scroll_wheel(struct uatp_softc *, unsigned int, unsigned int,
    int *, int *);
static void move(struct uatp_softc *, const char *, unsigned int, unsigned int,
    int *, int *, int *, int *, unsigned int *, unsigned int *, int *, int *);
static int smooth(struct uatp_softc *, unsigned int, unsigned int,
    unsigned int);
static bool motion_below_threshold(struct uatp_softc *, unsigned int,
    int, int);
static int accelerate(struct uatp_softc *, unsigned int, unsigned int,
    unsigned int, unsigned int, bool, int *);

struct uatp_knobs {
	/*
	 * Button emulation.  What do we do when two or three fingers
	 * are on the trackpad when the user presses the button?
	 */
	unsigned int two_finger_buttons;
	unsigned int three_finger_buttons;

#if 0
	/*
	 * Edge scrolling.
	 *
	 * XXX Implement this.  What units should these be in?
	 */
	unsigned int top_edge;
	unsigned int bottom_edge;
	unsigned int left_edge;
	unsigned int right_edge;
#endif

	/*
	 * Multifinger tracking.  What do we do with multiple fingers?
	 * 0. Ignore them.
	 * 1. Try to interpret them as ordinary mousing.
	 * 2. Act like a two-dimensional scroll wheel.
	 */
	unsigned int multifinger_track;

	/*
	 * Sensor parameters.
	 */
	unsigned int x_sensors;
	unsigned int x_ratio;
	unsigned int y_sensors;
	unsigned int y_ratio;
	unsigned int sensor_threshold;
	unsigned int sensor_normalizer;
	unsigned int palm_width;
	unsigned int old_raw_weight;
	unsigned int old_smoothed_weight;
	unsigned int new_raw_weight;

	/*
	 * Motion parameters.
	 *
	 * XXX There should be a more principled model of acceleration.
	 */
	unsigned int motion_remainder;
	unsigned int motion_threshold;
	unsigned int motion_multiplier;
	unsigned int motion_divisor;
	unsigned int fast_motion_threshold;
	unsigned int fast_motion_multiplier;
	unsigned int fast_motion_divisor;
	unsigned int fast_per_direction;
	unsigned int motion_delay;

	/*
	 * Tapping.
	 */
	unsigned int tap_limit_msec;
	unsigned int double_tap_limit_msec;
	unsigned int one_finger_tap_buttons;
	unsigned int two_finger_tap_buttons;
	unsigned int three_finger_tap_buttons;
	unsigned int tap_track_distance_limit;
};

static const struct uatp_knobs default_knobs = {
	/*
	 * Button emulation.  Fingers on the trackpad don't change it
	 * by default -- it's still the left button.
	 *
	 * XXX The left button should have a name.
	 */
	 .two_finger_buttons	= 1,
	 .three_finger_buttons	= 1,

#if 0
	/*
	 * Edge scrolling.  Off by default.
	 */
	.top_edge		= 0,
	.bottom_edge		= 0,
	.left_edge		= 0,
	.right_edge		= 0,
#endif

	/*
	 * Multifinger tracking.  Ignore by default.
	 */
	 .multifinger_track	= 0,

	/*
	 * Sensor parameters.
	 */
	.x_sensors		= 0,	/* default for model */
	.x_ratio		= 0,	/* default for model */
	.y_sensors		= 0,	/* default for model */
	.y_ratio		= 0,	/* default for model */
	.sensor_threshold	= 5,
	.sensor_normalizer	= 5,
	.palm_width		= 0,	/* palm detection disabled */
	.old_raw_weight		= 0,
	.old_smoothed_weight	= 5,
	.new_raw_weight		= 1,

	/*
	 * Motion parameters.
	 */
	.motion_remainder	= 1,
	.motion_threshold	= 0,
	.motion_multiplier	= 1,
	.motion_divisor		= 1,
	.fast_motion_threshold	= 10,
	.fast_motion_multiplier	= 3,
	.fast_motion_divisor	= 2,
	.fast_per_direction	= 0,
	.motion_delay		= 4,

	/*
	 * Tapping.  Disabled by default, with a reasonable time set
	 * nevertheless so that you can just set the buttons to enable
	 * it.
	 */
	.tap_limit_msec			= 100,
	.double_tap_limit_msec		= 200,
	.one_finger_tap_buttons		= 0,
	.two_finger_tap_buttons		= 0,
	.three_finger_tap_buttons	= 0,
	.tap_track_distance_limit	= 200,
};

struct uatp_softc {
	struct uhidev sc_hdev;		/* USB parent.  */
	device_t sc_wsmousedev;		/* Attached wsmouse device.  */
	const struct uatp_parameters *sc_parameters;
	struct uatp_knobs sc_knobs;
	struct sysctllog *sc_log;	/* Log for sysctl knobs.  */
	const struct sysctlnode *sc_node;	/* Our sysctl node.  */
	unsigned int sc_input_size;	/* Input packet size.  */
	uint8_t sc_input[UATP_MAX_INPUT_SIZE];	/* Buffer for a packet.   */
	unsigned int sc_input_index;	/* Current index into sc_input.  */
	int sc_acc[UATP_SENSORS];	/* Accumulated sensor state.  */
	uint8_t sc_base[UATP_SENSORS];	/* Base sample.  */
	uint8_t sc_sample[UATP_SENSORS];/* Current sample.  */
	unsigned int sc_motion_timer;	/* XXX describe; motion_delay  */
	int sc_x_raw;			/* Raw horiz. mouse position.  */
	int sc_y_raw;			/* Raw vert. mouse position.  */
	int sc_z_raw;			/* Raw horiz. scroll position.  */
	int sc_w_raw;			/* Raw vert. scroll position.  */
	int sc_x_smoothed;		/* Smoothed horiz. mouse position.  */
	int sc_y_smoothed;		/* Smoothed vert. mouse position.  */
	int sc_z_smoothed;		/* Smoothed horiz. scroll position.  */
	int sc_w_smoothed;		/* Smoothed vert. scroll position.  */
	int sc_x_remainder;		/* Remainders from acceleration.  */
	int sc_y_remainder;
	int sc_z_remainder;
	int sc_w_remainder;
	unsigned int sc_track_distance;	/* Distance^2 finger has tracked,
					 * squared to avoid sqrt in kernel.  */
	uint32_t sc_status;		/* Status flags:  */
#define UATP_ENABLED	(1 << 0)	/* . Is the wsmouse enabled?  */
#define UATP_DYING	(1 << 1)	/* . Have we been deactivated?  */
#define UATP_VALID	(1 << 2)	/* . Do we have valid sensor data?  */
	struct usb_task sc_reset_task;	/* Task for resetting device.  */

	callout_t sc_untap_callout;	/* Releases button after tap.  */
	kmutex_t sc_tap_mutex;		/* Protects the following fields.  */
	kcondvar_t sc_tap_cv;		/* Signalled by untap callout.  */
	enum uatp_tap_state sc_tap_state;	/* Current tap state.  */
	unsigned int sc_tapping_fingers;	/* No. fingers tapping.  */
	unsigned int sc_tapped_fingers;	/* No. fingers of last tap.  */
	struct timeval sc_tap_timer;	/* Timer for tap state transitions.  */
	uint32_t sc_buttons;		/* Physical buttons pressed.  */
	uint32_t sc_all_buttons;	/* Buttons pressed or tapped.  */

#if UATP_DEBUG
	uint32_t sc_debug_flags;	/* Debugging output enabled.  */
#endif
};

struct uatp_descriptor {
	uint16_t vendor;
	uint16_t product;
	const char *description;
	const struct uatp_parameters *parameters;
};

struct uatp_parameters {
	unsigned int x_ratio;		/* Screen width / trackpad width.  */
	unsigned int x_sensors;		/* Number of horizontal sensors.  */
	unsigned int x_sensors_17;	/* XXX Same, on a 17" laptop.  */
	unsigned int y_ratio;		/* Screen height / trackpad height.  */
	unsigned int y_sensors;		/* Number of vertical sensors.  */
	unsigned int input_size;	/* Size in bytes of input packets.  */

	/* Device-specific initialization routine.  May be null.  */
	void (*initialize)(struct uatp_softc *);

	/* Device-specific finalization routine.  May be null.  May fail.  */
	int (*finalize)(struct uatp_softc *);

	/* Tests whether this is a base sample.  Second argument is
	 * input_size bytes long.  */
	bool (*base_sample)(const struct uatp_softc *, const uint8_t *);

	/* Reads a sensor sample from an input packet.  First argument
	 * is UATP_MAX_X_SENSORS bytes long; second, UATP_MAX_Y_SENSORS
	 * bytes; third, input_size bytes.  */
	void (*read_sample)(uint8_t *, uint8_t *, const uint8_t *);

	/* Accumulates sensor state in sc->sc_acc.  */
	void (*accumulate)(struct uatp_softc *);

	/* Called on spurious interrupts to reset.  May be null.  */
	void (*reset)(struct uatp_softc *);
};

/* Known device parameters */

static const struct uatp_parameters fountain_parameters = {
	.x_ratio	= 64,	.x_sensors = 16,	.x_sensors_17 = 26,
	.y_ratio	= 43,	.y_sensors = 16,
	.input_size	= 81,
	.initialize	= NULL,
	.finalize	= NULL,
	.base_sample	= base_sample_softc_flag,
	.read_sample	= read_sample_1,
	.accumulate	= accumulate_sample_1,
	.reset		= NULL,
};

static const struct uatp_parameters geyser_1_parameters = {
	.x_ratio	= 64,	.x_sensors = 16,	.x_sensors_17 = 26,
	.y_ratio	= 43,	.y_sensors = 16,
	.input_size	= 81,
	.initialize	= NULL,
	.finalize	= NULL,
	.base_sample	= base_sample_softc_flag,
	.read_sample	= read_sample_1,
	.accumulate	= accumulate_sample_1,
	.reset		= NULL,
};

static const struct uatp_parameters geyser_2_parameters = {
	.x_ratio	= 64,	.x_sensors = 15,	.x_sensors_17 = 20,
	.y_ratio	= 43,	.y_sensors = 9,
	.input_size	= 64,
	.initialize	= NULL,
	.finalize	= NULL,
	.base_sample	= base_sample_softc_flag,
	.read_sample	= read_sample_2,
	.accumulate	= accumulate_sample_1,
	.reset		= NULL,
};

/*
 * The Geyser 3 and Geyser 4 share parameters.  They also present
 * generic USB HID mice on a different report id, so we have smaller
 * packets by one byte (uhidev handles multiplexing report ids) and
 * extra initialization work to switch the mode from generic USB HID
 * mouse to Apple trackpad.
 */

static const struct uatp_parameters geyser_3_4_parameters = {
	.x_ratio	= 64,	.x_sensors = 20, /* XXX */ .x_sensors_17 = 0,
	.y_ratio	= 64,	.y_sensors = 9,
	.input_size	= 63,	/* 64, minus one for the report id.  */
	.initialize	= geyser34_initialize,
	.finalize	= geyser34_finalize,
	.base_sample	= base_sample_input_flag,
	.read_sample	= read_sample_2,
	.accumulate	= accumulate_sample_2,
	.reset		= geyser34_deferred_reset,
};

/* Known device models */

#define APPLE_TRACKPAD(PRODUCT, DESCRIPTION, PARAMETERS)		\
	{								\
		.vendor = USB_VENDOR_APPLE,				\
		.product = (PRODUCT),					\
		.description = "Apple " DESCRIPTION " trackpad",	\
		.parameters = (& (PARAMETERS)),				\
	}

#define POWERBOOK_TRACKPAD(PRODUCT, PARAMETERS)				\
	APPLE_TRACKPAD(PRODUCT, "PowerBook/iBook", PARAMETERS)
#define MACBOOK_TRACKPAD(PRODUCT, PARAMETERS)				\
	APPLE_TRACKPAD(PRODUCT, "MacBook/MacBook Pro", PARAMETERS)

static const struct uatp_descriptor uatp_descriptors[] =
{
	POWERBOOK_TRACKPAD(0x020e, fountain_parameters),
	POWERBOOK_TRACKPAD(0x020f, fountain_parameters),
	POWERBOOK_TRACKPAD(0x030a, fountain_parameters),

	POWERBOOK_TRACKPAD(0x030b, geyser_1_parameters),

	POWERBOOK_TRACKPAD(0x0214, geyser_2_parameters),
	POWERBOOK_TRACKPAD(0x0215, geyser_2_parameters),
	POWERBOOK_TRACKPAD(0x0216, geyser_2_parameters),

	MACBOOK_TRACKPAD(0x0217, geyser_3_4_parameters), /* 3 */
	MACBOOK_TRACKPAD(0x0218, geyser_3_4_parameters), /* 3 */
	MACBOOK_TRACKPAD(0x0219, geyser_3_4_parameters), /* 3 */

	MACBOOK_TRACKPAD(0x021a, geyser_3_4_parameters), /* 4 */
	MACBOOK_TRACKPAD(0x021b, geyser_3_4_parameters), /* 4 */
	MACBOOK_TRACKPAD(0x021c, geyser_3_4_parameters), /* 4 */

	MACBOOK_TRACKPAD(0x0229, geyser_3_4_parameters), /* 4 */
	MACBOOK_TRACKPAD(0x022a, geyser_3_4_parameters), /* 4 */
	MACBOOK_TRACKPAD(0x022b, geyser_3_4_parameters), /* 4 */
};

#undef MACBOOK_TRACKPAD
#undef POWERBOOK_TRACKPAD
#undef APPLE_TRACKPAD

/* Miscellaneous utilities */

static const struct uatp_descriptor *
find_uatp_descriptor(const struct uhidev_attach_arg *uha)
{
	unsigned int i;

	for (i = 0; i < __arraycount(uatp_descriptors); i++)
		if ((uha->uaa->vendor == uatp_descriptors[i].vendor) &&
		    (uha->uaa->product == uatp_descriptors[i].product))
			return &uatp_descriptors[i];

	return NULL;
}

static device_t
uatp_dev(const struct uatp_softc *sc)
{
	return sc->sc_hdev.sc_dev;
}

static uint8_t *
uatp_x_sample(struct uatp_softc *sc)
{
	return &sc->sc_sample[0];
}

static uint8_t *
uatp_y_sample(struct uatp_softc *sc)
{
	return &sc->sc_sample[UATP_MAX_X_SENSORS];
}

static int *
uatp_x_acc(struct uatp_softc *sc)
{
	return &sc->sc_acc[0];
}

static int *
uatp_y_acc(struct uatp_softc *sc)
{
	return &sc->sc_acc[UATP_MAX_X_SENSORS];
}

static void
uatp_clear_position(struct uatp_softc *sc)
{
	memset(sc->sc_acc, 0, sizeof(sc->sc_acc));
	sc->sc_motion_timer = 0;
	sc->sc_x_raw = sc->sc_x_smoothed = -1;
	sc->sc_y_raw = sc->sc_y_smoothed = -1;
	sc->sc_z_raw = sc->sc_z_smoothed = -1;
	sc->sc_w_raw = sc->sc_w_smoothed = -1;
	sc->sc_x_remainder = 0;
	sc->sc_y_remainder = 0;
	sc->sc_z_remainder = 0;
	sc->sc_w_remainder = 0;
	sc->sc_track_distance = 0;
}

static unsigned int
uatp_x_sensors(const struct uatp_softc *sc)
{
	if ((0 < sc->sc_knobs.x_sensors) &&
	    (sc->sc_knobs.x_sensors <= UATP_MAX_X_SENSORS))
		return sc->sc_knobs.x_sensors;
	else
		return sc->sc_parameters->x_sensors;
}

static unsigned int
uatp_y_sensors(const struct uatp_softc *sc)
{
	if ((0 < sc->sc_knobs.y_sensors) &&
	    (sc->sc_knobs.y_sensors <= UATP_MAX_Y_SENSORS))
		return sc->sc_knobs.y_sensors;
	else
		return sc->sc_parameters->y_sensors;
}

static unsigned int
uatp_x_ratio(const struct uatp_softc *sc)
{
	/* XXX Reject bogus values in sysctl.  */
	if ((0 < sc->sc_knobs.x_ratio) &&
	    (sc->sc_knobs.x_ratio <= UATP_MAX_X_RATIO))
		return sc->sc_knobs.x_ratio;
	else
		return sc->sc_parameters->x_ratio;
}

static unsigned int
uatp_y_ratio(const struct uatp_softc *sc)
{
	/* XXX Reject bogus values in sysctl.  */
	if ((0 < sc->sc_knobs.y_ratio) &&
	    (sc->sc_knobs.y_ratio <= UATP_MAX_Y_RATIO))
		return sc->sc_knobs.y_ratio;
	else
		return sc->sc_parameters->y_ratio;
}

static unsigned int
uatp_old_raw_weight(const struct uatp_softc *sc)
{
	/* XXX Reject bogus values in sysctl.  */
	if (sc->sc_knobs.old_raw_weight <= UATP_MAX_WEIGHT)
		return sc->sc_knobs.old_raw_weight;
	else
		return 0;
}

static unsigned int
uatp_old_smoothed_weight(const struct uatp_softc *sc)
{
	/* XXX Reject bogus values in sysctl.  */
	if (sc->sc_knobs.old_smoothed_weight <= UATP_MAX_WEIGHT)
		return sc->sc_knobs.old_smoothed_weight;
	else
		return 0;
}

static unsigned int
uatp_new_raw_weight(const struct uatp_softc *sc)
{
	/* XXX Reject bogus values in sysctl.  */
	if ((0 < sc->sc_knobs.new_raw_weight) &&
	    (sc->sc_knobs.new_raw_weight <= UATP_MAX_WEIGHT))
		return sc->sc_knobs.new_raw_weight;
	else
		return 1;
}

static int
scale_motion(const struct uatp_softc *sc, int delta, int *remainder,
    const unsigned int *multiplier, const unsigned int *divisor)
{
	int product;

	/* XXX Limit the divisor?  */
	if (((*multiplier) == 0) ||
	    ((*multiplier) > UATP_MAX_MOTION_MULTIPLIER) ||
	    ((*divisor) == 0))
		DPRINTF(sc, UATP_DEBUG_ACCEL,
		    ("bad knobs; %d (+ %d) --> %d, rem 0\n",
			delta, *remainder, (delta + (*remainder))));
	else
		DPRINTF(sc, UATP_DEBUG_ACCEL,
		    ("scale %d (+ %d) by %u/%u --> %d, rem %d\n",
			delta, *remainder,
			(*multiplier), (*divisor),
			(((delta + (*remainder)) * ((int) (*multiplier)))
			    / ((int) (*divisor))),
			(((delta + (*remainder)) * ((int) (*multiplier)))
			    % ((int) (*divisor)))));

	if (sc->sc_knobs.motion_remainder)
		delta += *remainder;
	*remainder = 0;

	if (((*multiplier) == 0) ||
	    ((*multiplier) > UATP_MAX_MOTION_MULTIPLIER) ||
	    ((*divisor) == 0))
		return delta;

	product = (delta * ((int) (*multiplier)));
	*remainder = (product % ((int) (*divisor)));
	return (product / ((int) (*divisor)));
}

static int
uatp_scale_motion(const struct uatp_softc *sc, int delta, int *remainder)
{
	return scale_motion(sc, delta, remainder,
	    &sc->sc_knobs.motion_multiplier,
	    &sc->sc_knobs.motion_divisor);
}

static int
uatp_scale_fast_motion(const struct uatp_softc *sc, int delta, int *remainder)
{
	return scale_motion(sc, delta, remainder,
	    &sc->sc_knobs.fast_motion_multiplier,
	    &sc->sc_knobs.fast_motion_divisor);
}

/* Driver goop */

CFATTACH_DECL2_NEW(uatp, sizeof(struct uatp_softc), uatp_match, uatp_attach,
    uatp_detach, uatp_activate, NULL, uatp_childdet);

static const struct wsmouse_accessops uatp_accessops = {
	.enable = uatp_enable,
	.disable = uatp_disable,
	.ioctl = uatp_ioctl,
};

static int
uatp_match(device_t parent, cfdata_t match, void *aux)
{
	const struct uhidev_attach_arg *uha = aux;
	void *report_descriptor;
	int report_size, input_size;
	const struct uatp_descriptor *uatp_descriptor;

	aprint_debug("%s: vendor 0x%04x, product 0x%04x\n", __func__,
	    (unsigned int)uha->uaa->vendor,
	    (unsigned int)uha->uaa->product);
	aprint_debug("%s: class 0x%04x, subclass 0x%04x, proto 0x%04x\n",
	    __func__,
	    (unsigned int)uha->uaa->class,
	    (unsigned int)uha->uaa->subclass,
	    (unsigned int)uha->uaa->proto);

	uhidev_get_report_desc(uha->parent, &report_descriptor, &report_size);
	input_size = hid_report_size(report_descriptor, report_size,
	    hid_input, uha->reportid);
	aprint_debug("%s: reportid %d, input size %d\n", __func__,
	    (int)uha->reportid, input_size);

	/*
	 * Keyboards, trackpads, and eject buttons share common vendor
	 * and product ids, but not protocols: only the trackpad
	 * reports a mouse protocol.
	 */
	if (uha->uaa->proto != UIPROTO_MOUSE)
		return UMATCH_NONE;

	/* Check for a known vendor/product id.  */
	uatp_descriptor = find_uatp_descriptor(uha);
	if (uatp_descriptor == NULL) {
		aprint_debug("%s: unknown vendor/product id\n", __func__);
		return UMATCH_NONE;
	}

	/* Check for the expected input size.  */
	if ((input_size < 0) ||
	    ((unsigned int)input_size !=
		uatp_descriptor->parameters->input_size)) {
		aprint_debug("%s: expected input size %u\n", __func__,
		    uatp_descriptor->parameters->input_size);
		return UMATCH_NONE;
	}

	return UMATCH_VENDOR_PRODUCT_CONF_IFACE;
}

static void
uatp_attach(device_t parent, device_t self, void *aux)
{
	struct uatp_softc *sc = device_private(self);
	const struct uhidev_attach_arg *uha = aux;
	const struct uatp_descriptor *uatp_descriptor;
	void *report_descriptor;
	int report_size, input_size;
	struct wsmousedev_attach_args a;

	/* Set up uhidev state.  (Why doesn't uhidev do most of this?)  */
	sc->sc_hdev.sc_dev = self;
	sc->sc_hdev.sc_intr = uatp_intr;
	sc->sc_hdev.sc_parent = uha->parent;
	sc->sc_hdev.sc_report_id = uha->reportid;

	/* Identify ourselves to dmesg.  */
	uatp_descriptor = find_uatp_descriptor(uha);
	KASSERT(uatp_descriptor != NULL);
	aprint_normal(": %s\n", uatp_descriptor->description);
	aprint_naive(": %s\n", uatp_descriptor->description);
	aprint_verbose_dev(self,
	    "vendor 0x%04x, product 0x%04x, report id %d\n",
	    (unsigned int)uha->uaa->vendor, (unsigned int)uha->uaa->product,
	    (int)uha->reportid);

	uhidev_get_report_desc(uha->parent, &report_descriptor, &report_size);
	input_size = hid_report_size(report_descriptor, report_size, hid_input,
	    uha->reportid);
	KASSERT(0 < input_size);
	sc->sc_input_size = input_size;

	/* Initialize model-specific parameters.  */
	sc->sc_parameters = uatp_descriptor->parameters;
	KASSERT((int)sc->sc_parameters->input_size == input_size);
	KASSERT(sc->sc_parameters->x_sensors <= UATP_MAX_X_SENSORS);
	KASSERT(sc->sc_parameters->x_ratio <= UATP_MAX_X_RATIO);
	KASSERT(sc->sc_parameters->y_sensors <= UATP_MAX_Y_SENSORS);
	KASSERT(sc->sc_parameters->y_ratio <= UATP_MAX_Y_RATIO);
	aprint_verbose_dev(self,
	    "%u x sensors, scaled by %u for %u points on screen\n",
	    sc->sc_parameters->x_sensors, sc->sc_parameters->x_ratio,
	    sc->sc_parameters->x_sensors * sc->sc_parameters->x_ratio);
	aprint_verbose_dev(self,
	    "%u y sensors, scaled by %u for %u points on screen\n",
	    sc->sc_parameters->y_sensors, sc->sc_parameters->y_ratio,
	    sc->sc_parameters->y_sensors * sc->sc_parameters->y_ratio);
	if (sc->sc_parameters->initialize)
		sc->sc_parameters->initialize(sc);

	/* Register with pmf.  Nothing special for suspend/resume.  */
	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");

	/* Initialize knobs and create sysctl subtree to tweak them.  */
	sc->sc_knobs = default_knobs;
	uatp_setup_sysctl(sc);

	/* Initialize tapping.  */
	tap_initialize(sc);

	/* Attach wsmouse.  */
	a.accessops = &uatp_accessops;
	a.accesscookie = sc;
	sc->sc_wsmousedev = config_found_ia(self, "wsmousedev", &a,
	    wsmousedevprint);
}

/* Sysctl setup */

static void
uatp_setup_sysctl(struct uatp_softc *sc)
{
	int error;

	error = sysctl_createv(&sc->sc_log, 0, NULL, &sc->sc_node, 0,
	    CTLTYPE_NODE, device_xname(uatp_dev(sc)),
	    SYSCTL_DESCR("uatp configuration knobs"),
	    NULL, 0, NULL, 0,
	    CTL_HW, CTL_CREATE, CTL_EOL);
	if (error != 0) {
		aprint_error_dev(uatp_dev(sc),
		    "unable to set up sysctl tree hw.%s: %d\n",
		    device_xname(uatp_dev(sc)), error);
		goto err;
	}

#if UATP_DEBUG
	if (!uatp_setup_sysctl_knob(sc, &sc->sc_debug_flags, "debug",
		"uatp(4) debug flags"))
		goto err;
#endif

	/*
	 * Button emulation.
	 */
	if (!uatp_setup_sysctl_knob(sc, &sc->sc_knobs.two_finger_buttons,
		"two_finger_buttons",
		"buttons to emulate with two fingers on trackpad"))
		goto err;
	if (!uatp_setup_sysctl_knob(sc, &sc->sc_knobs.three_finger_buttons,
		"three_finger_buttons",
		"buttons to emulate with three fingers on trackpad"))
		goto err;

#if 0
	/*
	 * Edge scrolling.
	 */
	if (!uatp_setup_sysctl_knob(sc, &sc->sc_knobs.top_edge, "top_edge",
		"width of top edge for edge scrolling"))
		goto err;
	if (!uatp_setup_sysctl_knob(sc, &sc->sc_knobs.bottom_edge,
		"bottom_edge", "width of bottom edge for edge scrolling"))
		goto err;
	if (!uatp_setup_sysctl_knob(sc, &sc->sc_knobs.left_edge, "left_edge",
		"width of left edge for edge scrolling"))
		goto err;
	if (!uatp_setup_sysctl_knob(sc, &sc->sc_knobs.right_edge, "right_edge",
		"width of right edge for edge scrolling"))
		goto err;
#endif

	/*
	 * Multifinger tracking.
	 */
	if (!uatp_setup_sysctl_knob(sc, &sc->sc_knobs.multifinger_track,
		"multifinger_track",
		"0 to ignore multiple fingers, 1 to reset, 2 to scroll"))
		goto err;

	/*
	 * Sensor parameters.
	 */
	if (!uatp_setup_sysctl_knob(sc, &sc->sc_knobs.x_sensors, "x_sensors",
		"number of x sensors"))
		goto err;
	if (!uatp_setup_sysctl_knob(sc, &sc->sc_knobs.x_ratio, "x_ratio",
		"screen width to trackpad width ratio"))
		goto err;
	if (!uatp_setup_sysctl_knob(sc, &sc->sc_knobs.y_sensors, "y_sensors",
		"number of y sensors"))
		goto err;
	if (!uatp_setup_sysctl_knob(sc, &sc->sc_knobs.y_ratio, "y_ratio",
		"screen height to trackpad height ratio"))
		goto err;
	if (!uatp_setup_sysctl_knob(sc, &sc->sc_knobs.sensor_threshold,
		"sensor_threshold", "sensor threshold"))
		goto err;
	if (!uatp_setup_sysctl_knob(sc, &sc->sc_knobs.sensor_normalizer,
		"sensor_normalizer", "sensor normalizer"))
		goto err;
	if (!uatp_setup_sysctl_knob(sc, &sc->sc_knobs.palm_width,
		"palm_width", "lower bound on width/height of palm"))
		goto err;
	if (!uatp_setup_sysctl_knob(sc, &sc->sc_knobs.old_raw_weight,
		"old_raw_weight", "weight of old raw position"))
		goto err;
	if (!uatp_setup_sysctl_knob(sc, &sc->sc_knobs.old_smoothed_weight,
		"old_smoothed_weight", "weight of old smoothed position"))
		goto err;
	if (!uatp_setup_sysctl_knob(sc, &sc->sc_knobs.new_raw_weight,
		"new_raw_weight", "weight of new raw position"))
		goto err;
	if (!uatp_setup_sysctl_knob(sc, &sc->sc_knobs.motion_remainder,
		"motion_remainder", "remember motion division remainder"))
		goto err;
	if (!uatp_setup_sysctl_knob(sc, &sc->sc_knobs.motion_threshold,
		"motion_threshold", "threshold before finger moves cursor"))
		goto err;
	if (!uatp_setup_sysctl_knob(sc, &sc->sc_knobs.motion_multiplier,
		"motion_multiplier", "numerator of motion scale"))
		goto err;
	if (!uatp_setup_sysctl_knob(sc, &sc->sc_knobs.motion_divisor,
		"motion_divisor", "divisor of motion scale"))
		goto err;
	if (!uatp_setup_sysctl_knob(sc, &sc->sc_knobs.fast_motion_threshold,
		"fast_motion_threshold", "threshold before fast motion"))
		goto err;
	if (!uatp_setup_sysctl_knob(sc, &sc->sc_knobs.fast_motion_multiplier,
		"fast_motion_multiplier", "numerator of fast motion scale"))
		goto err;
	if (!uatp_setup_sysctl_knob(sc, &sc->sc_knobs.fast_motion_divisor,
		"fast_motion_divisor", "divisor of fast motion scale"))
		goto err;
	if (!uatp_setup_sysctl_knob(sc, &sc->sc_knobs.fast_per_direction,
		"fast_per_direction", "don't frobnitz the veeblefitzer!"))
		goto err;
	if (!uatp_setup_sysctl_knob(sc, &sc->sc_knobs.motion_delay,
		"motion_delay", "number of packets before motion kicks in"))
		goto err;

	/*
	 * Tapping.
	 */
	if (!uatp_setup_sysctl_knob(sc, &sc->sc_knobs.tap_limit_msec,
		"tap_limit_msec", "milliseconds before a touch is not a tap"))
		goto err;
	if (!uatp_setup_sysctl_knob(sc, &sc->sc_knobs.double_tap_limit_msec,
		"double_tap_limit_msec",
		"milliseconds before a second tap keeps the button down"))
		goto err;
	if (!uatp_setup_sysctl_knob(sc, &sc->sc_knobs.one_finger_tap_buttons,
		"one_finger_tap_buttons", "buttons for one-finger taps"))
		goto err;
	if (!uatp_setup_sysctl_knob(sc, &sc->sc_knobs.two_finger_tap_buttons,
		"two_finger_tap_buttons", "buttons for two-finger taps"))
		goto err;
	if (!uatp_setup_sysctl_knob(sc, &sc->sc_knobs.three_finger_tap_buttons,
		"three_finger_tap_buttons", "buttons for three-finger taps"))
		goto err;
	if (!uatp_setup_sysctl_knob(sc, &sc->sc_knobs.tap_track_distance_limit,
		"tap_track_distance_limit",
		"maximum distance^2 of tracking during tap"))
		goto err;

	return;

err:
	sysctl_teardown(&sc->sc_log);
	sc->sc_node = NULL;
}

static bool
uatp_setup_sysctl_knob(struct uatp_softc *sc, int *ptr, const char *name,
    const char *description)
{
	int error;

	error = sysctl_createv(&sc->sc_log, 0, NULL, NULL, CTLFLAG_READWRITE,
	    CTLTYPE_INT, name, SYSCTL_DESCR(description),
	    NULL, 0, ptr, 0,
	    CTL_HW, sc->sc_node->sysctl_num, CTL_CREATE, CTL_EOL);
	if (error != 0) {
		aprint_error_dev(uatp_dev(sc),
		    "unable to setup sysctl node hw.%s.%s: %d\n",
		    device_xname(uatp_dev(sc)), name, error);
		return false;
	}

	return true;
}

/* More driver goop */

static void
uatp_childdet(device_t self, device_t child)
{
	struct uatp_softc *sc = device_private(self);

	DPRINTF(sc, UATP_DEBUG_MISC, ("detaching child %s\n",
	    device_xname(child)));

	/* Our only child is the wsmouse device.  */
	if (child == sc->sc_wsmousedev)
		sc->sc_wsmousedev = NULL;
}

static int
uatp_detach(device_t self, int flags)
{
	struct uatp_softc *sc = device_private(self);

	DPRINTF(sc, UATP_DEBUG_MISC, ("detaching with flags %d\n", flags));

        if (sc->sc_status & UATP_ENABLED) {
		aprint_error_dev(uatp_dev(sc), "can't detach while enabled\n");
		return EBUSY;
        }

	if (sc->sc_parameters->finalize) {
		int error = sc->sc_parameters->finalize(sc);
		if (error != 0)
			return error;
	}

	pmf_device_deregister(self);

	sysctl_teardown(&sc->sc_log);
	sc->sc_node = NULL;

	tap_finalize(sc);

	return config_detach_children(self, flags);
}

static int
uatp_activate(device_t self, enum devact act)
{
	struct uatp_softc *sc = device_private(self);

	DPRINTF(sc, UATP_DEBUG_MISC, ("act %d\n", (int)act));

	if (act != DVACT_DEACTIVATE)
		return EOPNOTSUPP;

	sc->sc_status |= UATP_DYING;

	return 0;
}

/* wsmouse routines */

static int
uatp_enable(void *v)
{
	struct uatp_softc *sc = v;

	DPRINTF(sc, UATP_DEBUG_WSMOUSE, ("enabling wsmouse\n"));

	/* Refuse to enable if we've been deactivated.  */
	if (sc->sc_status & UATP_DYING) {
		DPRINTF(sc, UATP_DEBUG_WSMOUSE, ("busy dying\n"));
		return EIO;
	}

	/* Refuse to enable if we already are enabled.  */
	if (sc->sc_status & UATP_ENABLED) {
		DPRINTF(sc, UATP_DEBUG_WSMOUSE, ("already enabled\n"));
		return EBUSY;
	}

	sc->sc_status |= UATP_ENABLED;
	sc->sc_status &=~ UATP_VALID;
	sc->sc_input_index = 0;
	tap_enable(sc);
	uatp_clear_position(sc);

	DPRINTF(sc, UATP_DEBUG_MISC, ("uhidev_open(%p)\n", &sc->sc_hdev));
	return uhidev_open(&sc->sc_hdev);
}

static void
uatp_disable(void *v)
{
	struct uatp_softc *sc = v;

	DPRINTF(sc, UATP_DEBUG_WSMOUSE, ("disabling wsmouse\n"));

	if (!(sc->sc_status & UATP_ENABLED)) {
		DPRINTF(sc, UATP_DEBUG_WSMOUSE, ("not enabled\n"));
		return;
	}

	tap_disable(sc);
	sc->sc_status &=~ UATP_ENABLED;

	DPRINTF(sc, UATP_DEBUG_MISC, ("uhidev_close(%p)\n", &sc->sc_hdev));
	uhidev_close(&sc->sc_hdev);
}

static int
uatp_ioctl(void *v, unsigned long cmd, void *data, int flag, struct lwp *p)
{

	DPRINTF((struct uatp_softc*)v, UATP_DEBUG_IOCTL,
	    ("cmd %lx, data %p, flag %x, lwp %p\n", cmd, data, flag, p));

	/* XXX Implement any relevant wsmouse(4) ioctls.  */
	return EPASSTHROUGH;
}

/*
 * The Geyser 3 and 4 models talk the generic USB HID mouse protocol by
 * default.  This mode switch makes them give raw sensor data instead
 * so that we can implement tapping, two-finger scrolling, &c.
 */

#define GEYSER34_RAW_MODE		0x04
#define GEYSER34_MODE_REPORT_ID		0
#define GEYSER34_MODE_INTERFACE		0
#define GEYSER34_MODE_PACKET_SIZE	8

static void
geyser34_enable_raw_mode(struct uatp_softc *sc)
{
	usbd_device_handle udev = sc->sc_hdev.sc_parent->sc_udev;
	usb_device_request_t req;
	usbd_status status;
	uint8_t report[GEYSER34_MODE_PACKET_SIZE];

	req.bmRequestType = UT_READ_CLASS_INTERFACE;
	req.bRequest = UR_GET_REPORT;
	USETW2(req.wValue, UHID_FEATURE_REPORT, GEYSER34_MODE_REPORT_ID);
	USETW(req.wIndex, GEYSER34_MODE_INTERFACE);
	USETW(req.wLength, GEYSER34_MODE_PACKET_SIZE);

	DPRINTF(sc, UATP_DEBUG_RESET, ("get feature report\n"));
	status = usbd_do_request(udev, &req, report);
	if (status != USBD_NORMAL_COMPLETION) {
		aprint_error_dev(uatp_dev(sc),
		    "error reading feature report: %s\n", usbd_errstr(status));
		return;
	}

#if UATP_DEBUG
	if (sc->sc_debug_flags & UATP_DEBUG_RESET) {
		unsigned int i;
		DPRINTF(sc, UATP_DEBUG_RESET, ("old feature report:"));
		for (i = 0; i < GEYSER34_MODE_PACKET_SIZE; i++)
			printf(" %02x", (unsigned int)report[i]);
		printf("\n");
		/* Doing this twice is harmless here and lets this be
		 * one ifdef.  */
		report[0] = GEYSER34_RAW_MODE;
		DPRINTF(sc, UATP_DEBUG_RESET, ("new feature report:"));
		for (i = 0; i < GEYSER34_MODE_PACKET_SIZE; i++)
			printf(" %02x", (unsigned int)report[i]);
		printf("\n");
	}
#endif

	report[0] = GEYSER34_RAW_MODE;

	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UR_SET_REPORT;
	USETW2(req.wValue, UHID_FEATURE_REPORT, GEYSER34_MODE_REPORT_ID);
	USETW(req.wIndex, GEYSER34_MODE_INTERFACE);
	USETW(req.wLength, GEYSER34_MODE_PACKET_SIZE);

	DPRINTF(sc, UATP_DEBUG_RESET, ("set feature report\n"));
	status = usbd_do_request(udev, &req, report);
	if (status != USBD_NORMAL_COMPLETION) {
		aprint_error_dev(uatp_dev(sc),
		    "error writing feature report: %s\n", usbd_errstr(status));
		return;
	}
}

/*
 * The Geyser 3 and 4 need to be reset periodically after we detect a
 * continual flow of spurious interrupts.  We use a USB task for this.
 */

static void
geyser34_initialize(struct uatp_softc *sc)
{

	DPRINTF(sc, UATP_DEBUG_MISC, ("initializing\n"));
	geyser34_enable_raw_mode(sc);
	usb_init_task(&sc->sc_reset_task, &geyser34_reset_task, sc, 0);
}

static int
geyser34_finalize(struct uatp_softc *sc)
{

	DPRINTF(sc, UATP_DEBUG_MISC, ("finalizing\n"));
	usb_rem_task(sc->sc_hdev.sc_parent->sc_udev, &sc->sc_reset_task);

	return 0;
}

static void
geyser34_deferred_reset(struct uatp_softc *sc)
{

	DPRINTF(sc, UATP_DEBUG_RESET, ("deferring reset\n"));
	usb_add_task(sc->sc_hdev.sc_parent->sc_udev, &sc->sc_reset_task,
	    USB_TASKQ_DRIVER);
}

static void
geyser34_reset_task(void *arg)
{
	struct uatp_softc *sc = arg;

	DPRINTF(sc, UATP_DEBUG_RESET, ("resetting\n"));

	/* Reset by putting it into raw mode.  Not sure why.  */
	geyser34_enable_raw_mode(sc);
}

/* Interrupt handler */

static void
uatp_intr(struct uhidev *addr, void *ibuf, unsigned int len)
{
	struct uatp_softc *sc = (struct uatp_softc *)addr;
	uint8_t *input;
	int dx, dy, dz, dw;
	uint32_t buttons;

	DPRINTF(sc, UATP_DEBUG_INTR, ("softc %p, ibuf %p, len %u\n",
	    addr, ibuf, len));

	/*
	 * Some devices break packets up into chunks, so we accumulate
	 * input up to the expected packet length, or if it would
	 * overflow, discard the whole packet and start over.
	 */
	if (sc->sc_input_size < len) {
		aprint_error_dev(uatp_dev(sc),
		    "discarding %u-byte input packet\n", len);
		sc->sc_input_index = 0;
		return;
	} else if (sc->sc_input_size < (sc->sc_input_index + len)) {
		aprint_error_dev(uatp_dev(sc), "discarding %u-byte input\n",
		    (sc->sc_input_index + len));
		sc->sc_input_index = 0;
		return;
	}

#if UATP_DEBUG
	if (sc->sc_debug_flags & UATP_DEBUG_INTR) {
		unsigned int i;
		uint8_t *bytes = ibuf;
		DPRINTF(sc, UATP_DEBUG_INTR, ("raw"));
		for (i = 0; i < len; i++)
			printf(" %02x", (unsigned int)bytes[i]);
		printf("\n");
	}
#endif

	memcpy(&sc->sc_input[sc->sc_input_index], ibuf, len);
	sc->sc_input_index += len;
	if (sc->sc_input_index != sc->sc_input_size) {
		/* Wait until packet is complete.  */
		aprint_verbose_dev(uatp_dev(sc), "partial packet: %u bytes\n",
		    len);
		return;
	}

	/* Clear the buffer and process the now complete packet.  */
	sc->sc_input_index = 0;
	input = sc->sc_input;

	/* The last byte's first bit is set iff the button is pressed.
	 * XXX Left button should have a name.  */
	buttons = ((input[sc->sc_input_size - 1] & UATP_STATUS_BUTTON)
	    ? 1 : 0);

	/* Read the sample.  */
	memset(uatp_x_sample(sc), 0, UATP_MAX_X_SENSORS);
	memset(uatp_y_sample(sc), 0, UATP_MAX_Y_SENSORS);
	sc->sc_parameters->read_sample(uatp_x_sample(sc), uatp_y_sample(sc),
	    input);

#if UATP_DEBUG
	if (sc->sc_debug_flags & UATP_DEBUG_INTR) {
		unsigned int i;
		DPRINTF(sc, UATP_DEBUG_INTR, ("x sensors"));
		for (i = 0; i < uatp_x_sensors(sc); i++)
			printf(" %02x", (unsigned int)uatp_x_sample(sc)[i]);
		printf("\n");
		DPRINTF(sc, UATP_DEBUG_INTR, ("y sensors"));
		for (i = 0; i < uatp_y_sensors(sc); i++)
			printf(" %02x", (unsigned int)uatp_y_sample(sc)[i]);
		printf("\n");
	} else if ((sc->sc_debug_flags & UATP_DEBUG_STATUS) &&
		(input[sc->sc_input_size - 1] &~
		    (UATP_STATUS_BUTTON | UATP_STATUS_BASE |
			UATP_STATUS_POST_RESET)))
		DPRINTF(sc, UATP_DEBUG_STATUS, ("status byte: %02x\n",
		    input[sc->sc_input_size - 1]));
#endif

	/*
	 * If this is a base sample, initialize the state to interpret
	 * subsequent samples relative to it, and stop here.
	 */
	if (sc->sc_parameters->base_sample(sc, input)) {
		DPRINTF(sc, UATP_DEBUG_PARSE,
		    ("base sample, buttons %"PRIx32"\n", buttons));
		/* XXX Should the valid bit ever be reset?  */
		sc->sc_status |= UATP_VALID;
		uatp_clear_position(sc);
		memcpy(sc->sc_base, sc->sc_sample, sizeof(sc->sc_base));
		/* XXX Perform 17" size detection like Linux?  */
		return;
	}

	/* If not, accumulate the change in the sensors.  */
	sc->sc_parameters->accumulate(sc);

#if UATP_DEBUG
	if (sc->sc_debug_flags & UATP_DEBUG_ACCUMULATE) {
		unsigned int i;
		DPRINTF(sc, UATP_DEBUG_ACCUMULATE, ("accumulated x state:"));
		for (i = 0; i < uatp_x_sensors(sc); i++)
			printf(" %02x", (unsigned int)uatp_x_acc(sc)[i]);
		printf("\n");
		DPRINTF(sc, UATP_DEBUG_ACCUMULATE, ("accumulated y state:"));
		for (i = 0; i < uatp_y_sensors(sc); i++)
			printf(" %02x", (unsigned int)uatp_y_acc(sc)[i]);
		printf("\n");
	}
#endif

	/* Compute the change in coordinates and buttons.  */
	dx = dy = dz = dw = 0;
	if ((!interpret_input(sc, &dx, &dy, &dz, &dw, &buttons)) &&
	    /* If there's no input because we're releasing a button,
	     * then it's not spurious.  XXX Mutex?  */
	    (sc->sc_buttons == 0)) {
		DPRINTF(sc, UATP_DEBUG_SPURINTR, ("spurious interrupt\n"));
		if (sc->sc_parameters->reset)
			sc->sc_parameters->reset(sc);
		return;
	}

	/* Report to wsmouse.  */
	DPRINTF(sc, UATP_DEBUG_INTR,
	    ("buttons %"PRIx32", dx %d, dy %d, dz %d, dw %d\n",
		buttons, dx, dy, dz, dw));
	mutex_enter(&sc->sc_tap_mutex);
	uatp_input(sc, buttons, dx, dy, dz, dw);
	mutex_exit(&sc->sc_tap_mutex);
}

/*
 * Different ways to discern the base sample initializing the state.
 * `base_sample_softc_flag' uses a state flag stored in the softc;
 * `base_sample_input_flag' checks a flag at the end of the input
 * packet.
 */

static bool
base_sample_softc_flag(const struct uatp_softc *sc, const uint8_t *input)
{
	return !(sc->sc_status & UATP_VALID);
}

static bool
base_sample_input_flag(const struct uatp_softc *sc, const uint8_t *input)
{
	/* XXX Should we also check the valid flag?  */
	return !!(input[sc->sc_input_size - 1] & UATP_STATUS_BASE);
}

/*
 * Pick apart the horizontal sensors from the vertical sensors.
 * Different models interleave them in different orders.
 */

static void
read_sample_1(uint8_t *x, uint8_t *y, const uint8_t *input)
{
	unsigned int i;

	for (i = 0; i < 8; i++) {
		x[i] = input[5 * i + 2];
		x[i + 8] = input[5 * i + 4];
		x[i + 16] = input[5 * i + 42];
		if (i < 2)
			x[i + 24] = input[5 * i + 44];

		y[i] = input[5 * i + 1];
		y[i + 8] = input[5 * i + 3];
	}
}

static void
read_sample_2(uint8_t *x, uint8_t *y, const uint8_t *input)
{
	unsigned int i, j;

	for (i = 0, j = 19; i < 20; i += 2, j += 3) {
		x[i] = input[j];
		x[i + 1] = input[j + 1];
	}

	for (i = 0, j = 1; i < 9; i += 2, j += 3) {
		y[i] = input[j];
		y[i + 1] = input[j + 1];
	}
}

static void
accumulate_sample_1(struct uatp_softc *sc)
{
	unsigned int i;

	for (i = 0; i < UATP_SENSORS; i++) {
		sc->sc_acc[i] += (int8_t)(sc->sc_sample[i] - sc->sc_base[i]);
		if (sc->sc_acc[i] < 0) {
			sc->sc_acc[i] = 0;
		} else if (UATP_MAX_ACC < sc->sc_acc[i]) {
			DPRINTF(sc, UATP_DEBUG_ACCUMULATE,
			    ("overflow %d\n", sc->sc_acc[i]));
			sc->sc_acc[i] = UATP_MAX_ACC;
		}
	}

	memcpy(sc->sc_base, sc->sc_sample, sizeof(sc->sc_base));
}

static void
accumulate_sample_2(struct uatp_softc *sc)
{
	unsigned int i;

	for (i = 0; i < UATP_SENSORS; i++) {
		sc->sc_acc[i] = (int8_t)(sc->sc_sample[i] - sc->sc_base[i]);
		if (sc->sc_acc[i] < -0x80) {
			DPRINTF(sc, UATP_DEBUG_ACCUMULATE,
			    ("underflow %u - %u = %d\n",
				(unsigned int)sc->sc_sample[i],
				(unsigned int)sc->sc_base[i],
				sc->sc_acc[i]));
			sc->sc_acc[i] += 0x100;
		}
		if (0x7f < sc->sc_acc[i]) {
			DPRINTF(sc, UATP_DEBUG_ACCUMULATE,
			    ("overflow %u - %u = %d\n",
				(unsigned int)sc->sc_sample[i],
				(unsigned int)sc->sc_base[i],
				sc->sc_acc[i]));
			sc->sc_acc[i] -= 0x100;
		}
		if (sc->sc_acc[i] < 0)
			sc->sc_acc[i] = 0;
	}
}

/*
 * Report input to wsmouse, if there is anything interesting to report.
 * We must take into consideration the current tap-and-drag button
 * state.
 */

static void
uatp_input(struct uatp_softc *sc, uint32_t buttons,
    int dx, int dy, int dz, int dw)
{
	uint32_t all_buttons;

	KASSERT(mutex_owned(&sc->sc_tap_mutex));
	all_buttons = buttons | uatp_tapped_buttons(sc);

	if ((sc->sc_wsmousedev != NULL) &&
	    ((dx != 0) || (dy != 0) || (dz != 0) || (dw != 0) ||
		(all_buttons != sc->sc_all_buttons))) {
		int s = spltty();
		DPRINTF(sc, UATP_DEBUG_WSMOUSE, ("wsmouse input:"
		    " buttons %"PRIx32", dx %d, dy %d, dz %d, dw %d\n",
		    all_buttons, dx, -dy, dz, -dw));
		wsmouse_input(sc->sc_wsmousedev, all_buttons, dx, -dy, dz, -dw,
		    WSMOUSE_INPUT_DELTA);
		splx(s);
	}
	sc->sc_buttons = buttons;
	sc->sc_all_buttons = all_buttons;
}

/*
 * Interpret the current tap state to decide whether the tap buttons
 * are currently pressed.
 */

static uint32_t
uatp_tapped_buttons(struct uatp_softc *sc)
{
	KASSERT(mutex_owned(&sc->sc_tap_mutex));
	switch (sc->sc_tap_state) {
	case TAP_STATE_INITIAL:
	case TAP_STATE_TAPPING:
		return 0;

	case TAP_STATE_TAPPED:
	case TAP_STATE_DOUBLE_TAPPING:
	case TAP_STATE_DRAGGING_DOWN:
	case TAP_STATE_DRAGGING_UP:
	case TAP_STATE_TAPPING_IN_DRAG:
		CHECK((0 < sc->sc_tapped_fingers), return 0);
		switch (sc->sc_tapped_fingers) {
		case 1: return sc->sc_knobs.one_finger_tap_buttons;
		case 2: return sc->sc_knobs.two_finger_tap_buttons;
		case 3:
		default: return sc->sc_knobs.three_finger_tap_buttons;
		}

	default:
		aprint_error_dev(uatp_dev(sc), "%s: invalid tap state: %d\n",
		    __func__, sc->sc_tap_state);
		return 0;
	}
}

/*
 * Interpret the current input state to find a difference in all the
 * relevant coordinates and buttons to pass on to wsmouse, and update
 * any internal driver state necessary to interpret subsequent input
 * relative to this one.
 */

static bool
interpret_input(struct uatp_softc *sc, int *dx, int *dy, int *dz, int *dw,
    uint32_t *buttons)
{
	unsigned int x_pressure, x_raw, x_fingers;
	unsigned int y_pressure, y_raw, y_fingers;
	unsigned int fingers;

	x_pressure = interpret_dimension(sc, uatp_x_acc(sc),
	    uatp_x_sensors(sc), uatp_x_ratio(sc), &x_raw, &x_fingers);
	y_pressure = interpret_dimension(sc, uatp_y_acc(sc),
	    uatp_y_sensors(sc), uatp_y_ratio(sc), &y_raw, &y_fingers);

	DPRINTF(sc, UATP_DEBUG_PARSE,
	    ("x %u @ %u, %uf; y %u @ %u, %uf; buttons %"PRIx32"\n",
		x_pressure, x_raw, x_fingers,
		y_pressure, y_raw, y_fingers,
		*buttons));

	if ((x_pressure == 0) && (y_pressure == 0)) {
		bool ok;
		/* No fingers: clear position and maybe report a tap.  */
		DPRINTF(sc, UATP_DEBUG_INTR,
		    ("no position detected; clearing position\n"));
		if (*buttons == 0) {
			ok = tap_released(sc);
		} else {
			tap_reset(sc);
			/* Button pressed: interrupt is not spurious.  */
			ok = true;
		}
		/*
		 * Don't clear the position until after tap_released,
		 * which needs to know the track distance.
		 */
		uatp_clear_position(sc);
		return ok;
	} else if ((x_pressure == 0) || (y_pressure == 0)) {
		/* XXX What to do here?  */
		DPRINTF(sc, UATP_DEBUG_INTR,
		    ("pressure in only one dimension; ignoring\n"));
		return true;
	} else if ((x_pressure == 1) && (y_pressure == 1)) {
		fingers = max(x_fingers, y_fingers);
		CHECK((0 < fingers), return false);
		if (*buttons == 0)
			tap_touched(sc, fingers);
		else if (fingers == 1)
			tap_reset(sc);
		else		/* Multiple fingers, button pressed.  */
			*buttons = emulated_buttons(sc, fingers);
		update_position(sc, fingers, x_raw, y_raw, dx, dy, dz, dw);
		return true;
	} else {
		/* Palm detected in either or both of the dimensions.  */
		DPRINTF(sc, UATP_DEBUG_INTR, ("palm detected; ignoring\n"));
		return true;
	}
}

/*
 * Interpret the accumulated sensor state along one dimension to find
 * the number, mean position, and pressure of fingers.  Returns 0 to
 * indicate no pressure, returns 1 and sets *position and *fingers to
 * indicate fingers, and returns 2 to indicate palm.
 *
 * XXX Give symbolic names to the return values.
 */

static unsigned int
interpret_dimension(struct uatp_softc *sc, const int *acc,
    unsigned int n_sensors, unsigned int ratio,
    unsigned int *position, unsigned int *fingers)
{
	unsigned int i, v, n_fingers, sum;
	unsigned int total[UATP_MAX_SENSORS];
	unsigned int weighted[UATP_MAX_SENSORS];
	unsigned int sensor_threshold = sc->sc_knobs.sensor_threshold;
	unsigned int sensor_normalizer = sc->sc_knobs.sensor_normalizer;
	unsigned int width = 0;	/* GCC is not smart enough.  */
	unsigned int palm_width = sc->sc_knobs.palm_width;
	enum { none, nondecreasing, decreasing } state = none;

	if (sensor_threshold < sensor_normalizer)
		sensor_normalizer = sensor_threshold;
	if (palm_width == 0)	/* Effectively disable palm detection.  */
		palm_width = UATP_MAX_POSITION;

#define CHECK_(condition) CHECK(condition, return 0)

	/*
	 * Arithmetic bounds:
	 * . n_sensors is at most UATP_MAX_SENSORS,
	 * . n_fingers is at most UATP_MAX_SENSORS,
	 * . i is at most UATP_MAX_SENSORS,
	 * . sc->sc_acc[i] is at most UATP_MAX_ACC,
	 * . i * sc->sc_acc[i] is at most UATP_MAX_SENSORS * UATP_MAX_ACC,
	 * . each total[j] is at most UATP_MAX_SENSORS * UATP_MAX_ACC,
	 * . each weighted[j] is at most UATP_MAX_SENSORS^2 * UATP_MAX_ACC,
	 * . ratio is at most UATP_MAX_RATIO,
	 * . each weighted[j] * ratio is at most
	 *     UATP_MAX_SENSORS^2 * UATP_MAX_ACC * UATP_MAX_RATIO,
	 *   which is #x5fa0000 with the current values of the constants,
	 *   and
	 * . the sum of the positions is at most
	 *     UATP_MAX_SENSORS * UATP_MAX_POSITION,
	 *   which is #x60000 with the current values of the constants.
	 * Hence all of the arithmetic here fits in int (and thus also
	 * unsigned int).  If you change the constants, though, you
	 * must update the analysis.
	 */
	__CTASSERT(0x5fa0000 == (UATP_MAX_SENSORS * UATP_MAX_SENSORS *
		UATP_MAX_ACC * UATP_MAX_RATIO));
	__CTASSERT(0x60000 == (UATP_MAX_SENSORS * UATP_MAX_POSITION));
	CHECK_(n_sensors <= UATP_MAX_SENSORS);
	CHECK_(ratio <= UATP_MAX_RATIO);

	/*
	 * Detect each finger by looking for a consecutive sequence of
	 * increasing and then decreasing pressures above the sensor
	 * threshold.  Compute the finger's position as the weighted
	 * average of positions, weighted by the pressure at that
	 * position.  Finally, return the average finger position.
	 */

	n_fingers = 0;
	memset(weighted, 0, sizeof weighted);
	memset(total, 0, sizeof total);

	for (i = 0; i < n_sensors; i++) {
		CHECK_(0 <= acc[i]);
		v = acc[i];

		/* Ignore values outside a sensible interval.  */
		if (v <= sensor_threshold) {
			state = none;
			continue;
		} else if (UATP_MAX_ACC < v) {
			aprint_verbose_dev(uatp_dev(sc),
			    "ignoring large accumulated sensor state: %u\n",
			    v);
			continue;
		}

		switch (state) {
		case none:
			n_fingers += 1;
			CHECK_(n_fingers <= n_sensors);
			state = nondecreasing;
			width = 1;
			break;

		case nondecreasing:
		case decreasing:
			CHECK_(0 < i);
			CHECK_(0 <= acc[i - 1]);
			width += 1;
			if (palm_width <= (width * ratio)) {
				DPRINTF(sc, UATP_DEBUG_PALM,
				    ("palm detected\n"));
				return 2;
			} else if ((state == nondecreasing) &&
			    ((unsigned int)acc[i - 1] > v)) {
				state = decreasing;
			} else if ((state == decreasing) &&
			    ((unsigned int)acc[i - 1] < v)) {
				n_fingers += 1;
				CHECK_(n_fingers <= n_sensors);
				state = nondecreasing;
				width = 1;
			}
			break;

		default:
			aprint_error_dev(uatp_dev(sc),
			    "bad finger detection state: %d", state);
			return 0;
		}

		v -= sensor_normalizer;
		total[n_fingers - 1] += v;
		weighted[n_fingers - 1] += (i * v);
		CHECK_(total[n_fingers - 1] <=
		    (UATP_MAX_SENSORS * UATP_MAX_ACC));
		CHECK_(weighted[n_fingers - 1] <=
		    (UATP_MAX_SENSORS * UATP_MAX_SENSORS * UATP_MAX_ACC));
	}

	if (n_fingers == 0)
		return 0;

	sum = 0;
	for (i = 0; i < n_fingers; i++) {
		DPRINTF(sc, UATP_DEBUG_PARSE,
		    ("finger at %u\n", ((weighted[i] * ratio) / total[i])));
		sum += ((weighted[i] * ratio) / total[i]);
		CHECK_(sum <= UATP_MAX_SENSORS * UATP_MAX_POSITION);
	}

	*fingers = n_fingers;
	*position = (sum / n_fingers);
	return 1;

#undef CHECK_
}

/* Tapping */

/*
 * There is a very hairy state machine for detecting taps.  At every
 * touch, we record the maximum number of fingers touched, and don't
 * reset it to zero until the finger is released.
 *
 * INITIAL STATE
 * (no tapping fingers; no tapped fingers)
 * - On touch, go to TAPPING STATE.
 * - On any other input, remain in INITIAL STATE.
 *
 * TAPPING STATE: Finger touched; might be tap.
 * (tapping fingers; no tapped fingers)
 * - On release within the tap limit, go to TAPPED STATE.
 * - On release after the tap limit, go to INITIAL STATE.
 * - On any other input, remain in TAPPING STATE.
 *
 * TAPPED STATE: Finger recently tapped, and might double-tap.
 * (no tapping fingers; tapped fingers)
 * - On touch within the double-tap limit, go to DOUBLE-TAPPING STATE.
 * - On touch after the double-tap limit, go to TAPPING STATE.
 * - On no event after the double-tap limit, go to INITIAL STATE.
 * - On any other input, remain in TAPPED STATE.
 *
 * DOUBLE-TAPPING STATE: Finger touched soon after tap; might be double-tap.
 * (tapping fingers; tapped fingers)
 * - On release within the tap limit, release button and go to TAPPED STATE.
 * - On release after the tap limit, go to DRAGGING UP STATE.
 * - On touch after the tap limit, go to DRAGGING DOWN STATE.
 * - On any other input, remain in DOUBLE-TAPPING STATE.
 *
 * DRAGGING DOWN STATE: Finger has double-tapped and is dragging, not tapping.
 * (no tapping fingers; tapped fingers)
 * - On release, go to DRAGGING UP STATE.
 * - On any other input, remain in DRAGGING DOWN STATE.
 *
 * DRAGGING UP STATE: Finger has double-tapped and is up.
 * (no tapping fingers; tapped fingers)
 * - On touch, go to TAPPING IN DRAG STATE.
 * - On any other input, remain in DRAGGING UP STATE.
 *
 * TAPPING IN DRAG STATE: Tap-dancing while cross-dressed.
 * (tapping fingers; tapped fingers)
 * - On release within the tap limit, go to TAPPED STATE.
 * - On release after the tap limit, go to DRAGGING UP STATE.
 * - On any other input, remain in TAPPING IN DRAG STATE.
 *
 * Warning:  The graph of states is split into two components, those
 * with tapped fingers and those without.  The only path from any state
 * without tapped fingers to a state with tapped fingers must pass
 * through TAPPED STATE.  Also, the only transitions into TAPPED STATE
 * must be from states with tapping fingers, which become the tapped
 * fingers.  If you edit the state machine, you must either preserve
 * these properties, or globally transform the state machine to avoid
 * the bad consequences of violating these properties.
 */

static void
uatp_tap_limit(const struct uatp_softc *sc, struct timeval *limit)
{
	unsigned int msec = sc->sc_knobs.tap_limit_msec;
	limit->tv_sec = 0;
	limit->tv_usec = ((msec < 1000) ? (1000 * msec) : 100000);
}

#if UATP_DEBUG

#  define TAP_DEBUG_PRE(sc)	tap_debug((sc), __func__, "")
#  define TAP_DEBUG_POST(sc)	tap_debug((sc), __func__, " ->")

static void
tap_debug(struct uatp_softc *sc, const char *caller, const char *prefix)
{
	char buffer[128];
	const char *state;

	KASSERT(mutex_owned(&sc->sc_tap_mutex));
	switch (sc->sc_tap_state) {
	case TAP_STATE_INITIAL:		state = "initial";		break;
	case TAP_STATE_TAPPING:		state = "tapping";		break;
	case TAP_STATE_TAPPED:		state = "tapped";		break;
	case TAP_STATE_DOUBLE_TAPPING:	state = "double-tapping";	break;
	case TAP_STATE_DRAGGING_DOWN:	state = "dragging-down";	break;
	case TAP_STATE_DRAGGING_UP:	state = "dragging-up";		break;
	case TAP_STATE_TAPPING_IN_DRAG:	state = "tapping-in-drag";	break;
	default:
		snprintf(buffer, sizeof buffer, "unknown (%d)",
		    sc->sc_tap_state);
		state = buffer;
		break;
	}

	DPRINTF(sc, UATP_DEBUG_TAP,
	    ("%s:%s state %s, %u tapping, %u tapped\n",
		caller, prefix, state,
		sc->sc_tapping_fingers, sc->sc_tapped_fingers));
}

#else	/* !UATP_DEBUG */

#  define TAP_DEBUG_PRE(sc)	do {} while (0)
#  define TAP_DEBUG_POST(sc)	do {} while (0)

#endif

static void
tap_initialize(struct uatp_softc *sc)
{
	callout_init(&sc->sc_untap_callout, 0);
	callout_setfunc(&sc->sc_untap_callout, untap_callout, sc);
	mutex_init(&sc->sc_tap_mutex, MUTEX_DEFAULT, IPL_USB);
	cv_init(&sc->sc_tap_cv, "uatptap");
}

static void
tap_finalize(struct uatp_softc *sc)
{
	/* XXX Can the callout still be scheduled here?  */
	callout_destroy(&sc->sc_untap_callout);
	mutex_destroy(&sc->sc_tap_mutex);
	cv_destroy(&sc->sc_tap_cv);
}

static void
tap_enable(struct uatp_softc *sc)
{
	mutex_enter(&sc->sc_tap_mutex);
	tap_transition_initial(sc);
	sc->sc_buttons = 0;	/* XXX Not the right place?  */
	sc->sc_all_buttons = 0;
	mutex_exit(&sc->sc_tap_mutex);
}

static void
tap_disable(struct uatp_softc *sc)
{
	/* Reset tapping, and wait for any callouts to complete.  */
	tap_reset_wait(sc);
}

/*
 * Reset tap state.  If the untap callout has just fired, it may signal
 * a harmless button release event before this returns.
 */

static void
tap_reset(struct uatp_softc *sc)
{
	callout_stop(&sc->sc_untap_callout);
	mutex_enter(&sc->sc_tap_mutex);
	tap_transition_initial(sc);
	mutex_exit(&sc->sc_tap_mutex);
}

/* Reset, but don't return until the callout is done running.  */

static void
tap_reset_wait(struct uatp_softc *sc)
{
	bool fired = callout_stop(&sc->sc_untap_callout);

	mutex_enter(&sc->sc_tap_mutex);
	if (fired)
		while (sc->sc_tap_state == TAP_STATE_TAPPED)
			if (cv_timedwait(&sc->sc_tap_cv, &sc->sc_tap_mutex,
				mstohz(1000))) {
				aprint_error_dev(uatp_dev(sc),
				    "tap timeout\n");
				break;
			}
	if (sc->sc_tap_state == TAP_STATE_TAPPED)
		aprint_error_dev(uatp_dev(sc), "%s error\n", __func__);
	tap_transition_initial(sc);
	mutex_exit(&sc->sc_tap_mutex);
}

static const struct timeval zero_timeval;

static void
tap_transition(struct uatp_softc *sc, enum uatp_tap_state tap_state,
    const struct timeval *start_time,
    unsigned int tapping_fingers, unsigned int tapped_fingers)
{
	KASSERT(mutex_owned(&sc->sc_tap_mutex));
	sc->sc_tap_state = tap_state;
	sc->sc_tap_timer = *start_time;
	sc->sc_tapping_fingers = tapping_fingers;
	sc->sc_tapped_fingers = tapped_fingers;
}

static void
tap_transition_initial(struct uatp_softc *sc)
{
	/*
	 * No checks.  This state is always kosher, and sometimes a
	 * fallback in case of failure.
	 */
	tap_transition(sc, TAP_STATE_INITIAL, &zero_timeval, 0, 0);
}

/* Touch transitions */

static void
tap_transition_tapping(struct uatp_softc *sc, const struct timeval *start_time,
    unsigned int fingers)
{
	CHECK((sc->sc_tapping_fingers <= fingers),
	    do { tap_transition_initial(sc); return; } while (0));
	tap_transition(sc, TAP_STATE_TAPPING, start_time, fingers, 0);
}

static void
tap_transition_double_tapping(struct uatp_softc *sc,
    const struct timeval *start_time, unsigned int fingers)
{
	CHECK((sc->sc_tapping_fingers <= fingers),
	    do { tap_transition_initial(sc); return; } while (0));
	CHECK((0 < sc->sc_tapped_fingers),
	    do { tap_transition_initial(sc); return; } while (0));
	tap_transition(sc, TAP_STATE_DOUBLE_TAPPING, start_time, fingers,
	    sc->sc_tapped_fingers);
}

static void
tap_transition_dragging_down(struct uatp_softc *sc)
{
	CHECK((0 < sc->sc_tapped_fingers),
	    do { tap_transition_initial(sc); return; } while (0));
	tap_transition(sc, TAP_STATE_DRAGGING_DOWN, &zero_timeval, 0,
	    sc->sc_tapped_fingers);
}

static void
tap_transition_tapping_in_drag(struct uatp_softc *sc,
    const struct timeval *start_time, unsigned int fingers)
{
	CHECK((sc->sc_tapping_fingers <= fingers),
	    do { tap_transition_initial(sc); return; } while (0));
	CHECK((0 < sc->sc_tapped_fingers),
	    do { tap_transition_initial(sc); return; } while (0));
	tap_transition(sc, TAP_STATE_TAPPING_IN_DRAG, start_time, fingers,
	    sc->sc_tapped_fingers);
}

/* Release transitions */

static void
tap_transition_tapped(struct uatp_softc *sc, const struct timeval *start_time)
{
	/*
	 * The fingers that were tapping -- of which there must have
	 * been at least one -- are now the fingers that have tapped,
	 * and there are no longer fingers tapping.
	 */
	CHECK((0 < sc->sc_tapping_fingers),
	    do { tap_transition_initial(sc); return; } while (0));
	tap_transition(sc, TAP_STATE_TAPPED, start_time, 0,
	    sc->sc_tapping_fingers);
	schedule_untap(sc);
}

static void
tap_transition_dragging_up(struct uatp_softc *sc)
{
	CHECK((0 < sc->sc_tapped_fingers),
	    do { tap_transition_initial(sc); return; } while (0));
	tap_transition(sc, TAP_STATE_DRAGGING_UP, &zero_timeval, 0,
	    sc->sc_tapped_fingers);
}

static void
tap_touched(struct uatp_softc *sc, unsigned int fingers)
{
	struct timeval now, diff, limit;

	CHECK((0 < fingers), return);
	callout_stop(&sc->sc_untap_callout);
	mutex_enter(&sc->sc_tap_mutex);
	TAP_DEBUG_PRE(sc);
	/*
	 * Guarantee that the number of tapping fingers never decreases
	 * except when it is reset to zero on release.
	 */
	if (fingers < sc->sc_tapping_fingers)
		fingers = sc->sc_tapping_fingers;
	switch (sc->sc_tap_state) {
	case TAP_STATE_INITIAL:
		getmicrouptime(&now);
		tap_transition_tapping(sc, &now, fingers);
		break;

	case TAP_STATE_TAPPING:
		/*
		 * Number of fingers may have increased, so transition
		 * even though we're already in TAPPING.
		 */
		tap_transition_tapping(sc, &sc->sc_tap_timer, fingers);
		break;

	case TAP_STATE_TAPPED:
		getmicrouptime(&now);
		/*
		 * If the double-tap time limit has passed, it's the
		 * callout's responsibility to handle that event, so we
		 * assume the limit has not passed yet.
		 */
		tap_transition_double_tapping(sc, &now, fingers);
		break;

	case TAP_STATE_DOUBLE_TAPPING:
		getmicrouptime(&now);
		timersub(&now, &sc->sc_tap_timer, &diff);
		uatp_tap_limit(sc, &limit);
		if (timercmp(&diff, &limit, >) ||
		    (sc->sc_track_distance >
			sc->sc_knobs.tap_track_distance_limit))
			tap_transition_dragging_down(sc);
		break;

	case TAP_STATE_DRAGGING_DOWN:
		break;

	case TAP_STATE_DRAGGING_UP:
		getmicrouptime(&now);
		tap_transition_tapping_in_drag(sc, &now, fingers);
		break;

	case TAP_STATE_TAPPING_IN_DRAG:
		/*
		 * Number of fingers may have increased, so transition
		 * even though we're already in TAPPING IN DRAG.
		 */
		tap_transition_tapping_in_drag(sc, &sc->sc_tap_timer, fingers);
		break;

	default:
		aprint_error_dev(uatp_dev(sc), "%s: invalid tap state: %d\n",
		    __func__, sc->sc_tap_state);
		tap_transition_initial(sc);
		break;
	}
	TAP_DEBUG_POST(sc);
	mutex_exit(&sc->sc_tap_mutex);
}

static bool
tap_released(struct uatp_softc *sc)
{
	struct timeval now, diff, limit;
	void (*non_tapped_transition)(struct uatp_softc *);
	bool ok, temporary_release;

	mutex_enter(&sc->sc_tap_mutex);
	TAP_DEBUG_PRE(sc);
	switch (sc->sc_tap_state) {
	case TAP_STATE_INITIAL:
	case TAP_STATE_TAPPED:
	case TAP_STATE_DRAGGING_UP:
		/* Spurious interrupt: fingers are already off.  */
		ok = false;
		break;

	case TAP_STATE_TAPPING:
		temporary_release = false;
		non_tapped_transition = &tap_transition_initial;
		goto maybe_tap;

	case TAP_STATE_DOUBLE_TAPPING:
		temporary_release = true;
		non_tapped_transition = &tap_transition_dragging_up;
		goto maybe_tap;

	case TAP_STATE_TAPPING_IN_DRAG:
		temporary_release = false;
		non_tapped_transition = &tap_transition_dragging_up;
		goto maybe_tap;

	maybe_tap:
		getmicrouptime(&now);
		timersub(&now, &sc->sc_tap_timer, &diff);
		uatp_tap_limit(sc, &limit);
		if (timercmp(&diff, &limit, <=) &&
		    (sc->sc_track_distance <=
			sc->sc_knobs.tap_track_distance_limit)) {
			if (temporary_release) {
				/*
				 * XXX Kludge: Temporarily transition
				 * to a tap state that uatp_input will
				 * interpret as `no buttons tapped',
				 * saving the tapping fingers.  There
				 * should instead be a separate routine
				 * uatp_input_untapped.
				 */
				unsigned int fingers = sc->sc_tapping_fingers;
				tap_transition_initial(sc);
				uatp_input(sc, 0, 0, 0, 0, 0);
				sc->sc_tapping_fingers = fingers;
			}
			tap_transition_tapped(sc, &now);
		} else {
			(*non_tapped_transition)(sc);
		}
		ok = true;
		break;

	case TAP_STATE_DRAGGING_DOWN:
		tap_transition_dragging_up(sc);
		ok = true;
		break;

	default:
		aprint_error_dev(uatp_dev(sc), "%s: invalid tap state: %d\n",
		    __func__, sc->sc_tap_state);
		tap_transition_initial(sc);
		ok = false;
		break;
	}
	TAP_DEBUG_POST(sc);
	mutex_exit(&sc->sc_tap_mutex);
	return ok;
}

/* Untapping: Releasing the button after a tap */

static void
schedule_untap(struct uatp_softc *sc)
{
	unsigned int ms = sc->sc_knobs.double_tap_limit_msec;
	if (ms <= 1000)
		callout_schedule(&sc->sc_untap_callout, mstohz(ms));
	else			/* XXX Reject bogus values in sysctl.  */
		aprint_error_dev(uatp_dev(sc),
		    "double-tap delay too long: %ums\n", ms);
}

static void
untap_callout(void *arg)
{
	struct uatp_softc *sc = arg;

	mutex_enter(&sc->sc_tap_mutex);
	TAP_DEBUG_PRE(sc);
	switch (sc->sc_tap_state) {
	case TAP_STATE_TAPPED:
		tap_transition_initial(sc);
		/*
		 * XXX Kludge: Call uatp_input after the state transition
		 * to make sure that it will actually release the button.
		 */
		uatp_input(sc, 0, 0, 0, 0, 0);

	case TAP_STATE_INITIAL:
	case TAP_STATE_TAPPING:
	case TAP_STATE_DOUBLE_TAPPING:
	case TAP_STATE_DRAGGING_UP:
	case TAP_STATE_DRAGGING_DOWN:
	case TAP_STATE_TAPPING_IN_DRAG:
		/*
		 * Somebody else got in and changed the state before we
		 * untapped.  Let them take over; do nothing here.
		 */
		break;

	default:
		aprint_error_dev(uatp_dev(sc), "%s: invalid tap state: %d\n",
		    __func__, sc->sc_tap_state);
		tap_transition_initial(sc);
		/* XXX Just in case...?  */
		uatp_input(sc, 0, 0, 0, 0, 0);
		break;
	}
	TAP_DEBUG_POST(sc);
	/* XXX Broadcast only if state was TAPPED?  */
	cv_broadcast(&sc->sc_tap_cv);
	mutex_exit(&sc->sc_tap_mutex);
}

/*
 * Emulate different buttons if the user holds down n fingers while
 * pressing the physical button.  (This is unrelated to tapping.)
 */

static uint32_t
emulated_buttons(struct uatp_softc *sc, unsigned int fingers)
{
	CHECK((1 < fingers), return 0);

	switch (fingers) {
	case 2:
		DPRINTF(sc, UATP_DEBUG_EMUL_BUTTON,
		    ("2-finger emulated button: %"PRIx32"\n",
			sc->sc_knobs.two_finger_buttons));
		return sc->sc_knobs.two_finger_buttons;

	case 3:
	default:
		DPRINTF(sc, UATP_DEBUG_EMUL_BUTTON,
		    ("3-finger emulated button: %"PRIx32"\n",
			sc->sc_knobs.three_finger_buttons));
		return sc->sc_knobs.three_finger_buttons;
	}
}

/*
 * Update the position known to the driver based on the position and
 * number of fingers.  dx, dy, dz, and dw are expected to hold zero;
 * update_position may store nonzero changes in position in them.
 */

static void
update_position(struct uatp_softc *sc, unsigned int fingers,
    unsigned int x_raw, unsigned int y_raw,
    int *dx, int *dy, int *dz, int *dw)
{
	CHECK((0 < fingers), return);

	if ((fingers == 1) || (sc->sc_knobs.multifinger_track == 1))
		move_mouse(sc, x_raw, y_raw, dx, dy);
	else if (sc->sc_knobs.multifinger_track == 2)
		scroll_wheel(sc, x_raw, y_raw, dz, dw);
}

/*
 * XXX Scrolling needs to use a totally different motion model.
 */

static void
move_mouse(struct uatp_softc *sc, unsigned int x_raw, unsigned int y_raw,
    int *dx, int *dy)
{
	move(sc, "mouse", x_raw, y_raw, &sc->sc_x_raw, &sc->sc_y_raw,
	    &sc->sc_x_smoothed, &sc->sc_y_smoothed,
	    &sc->sc_x_remainder, &sc->sc_y_remainder,
	    dx, dy);
}

static void
scroll_wheel(struct uatp_softc *sc, unsigned int x_raw, unsigned int y_raw,
    int *dz, int *dw)
{
	move(sc, "scroll", x_raw, y_raw, &sc->sc_z_raw, &sc->sc_w_raw,
	    &sc->sc_z_smoothed, &sc->sc_w_smoothed,
	    &sc->sc_z_remainder, &sc->sc_w_remainder,
	    dz, dw);
}

static void
move(struct uatp_softc *sc, const char *ctx, unsigned int a, unsigned int b,
    int *a_raw, int *b_raw,
    int *a_smoothed, int *b_smoothed,
    unsigned int *a_remainder, unsigned int *b_remainder,
    int *da, int *db)
{
#define CHECK_(condition) CHECK(condition, return)

	int old_a_raw = *a_raw, old_a_smoothed = *a_smoothed;
	int old_b_raw = *b_raw, old_b_smoothed = *b_smoothed;
	unsigned int a_dist, b_dist, dist_squared;
	bool a_fast, b_fast;

	/*
	 * Make sure the quadratics in motion_below_threshold and
	 * tracking distance don't overflow int arithmetic.
	 */
	__CTASSERT(0x12000000 == (2 * UATP_MAX_POSITION * UATP_MAX_POSITION));

	CHECK_(a <= UATP_MAX_POSITION);
	CHECK_(b <= UATP_MAX_POSITION);
	*a_raw = a;
	*b_raw = b;
	if ((old_a_raw < 0) || (old_b_raw < 0)) {
		DPRINTF(sc, UATP_DEBUG_MOVE,
		    ("initialize %s position (%d, %d) -> (%d, %d)\n", ctx,
			old_a_raw, old_b_raw, a, b));
		return;
	}

	if ((old_a_smoothed < 0) || (old_b_smoothed < 0)) {
		/* XXX Does this make sense?  */
		old_a_smoothed = old_a_raw;
		old_b_smoothed = old_b_raw;
	}

	CHECK_(0 <= old_a_raw);
	CHECK_(0 <= old_b_raw);
	CHECK_(old_a_raw <= UATP_MAX_POSITION);
	CHECK_(old_b_raw <= UATP_MAX_POSITION);
	CHECK_(0 <= old_a_smoothed);
	CHECK_(0 <= old_b_smoothed);
	CHECK_(old_a_smoothed <= UATP_MAX_POSITION);
	CHECK_(old_b_smoothed <= UATP_MAX_POSITION);
	CHECK_(0 <= *a_raw);
	CHECK_(0 <= *b_raw);
	CHECK_(*a_raw <= UATP_MAX_POSITION);
	CHECK_(*b_raw <= UATP_MAX_POSITION);
	*a_smoothed = smooth(sc, old_a_raw, old_a_smoothed, *a_raw);
	*b_smoothed = smooth(sc, old_b_raw, old_b_smoothed, *b_raw);
	CHECK_(0 <= *a_smoothed);
	CHECK_(0 <= *b_smoothed);
	CHECK_(*a_smoothed <= UATP_MAX_POSITION);
	CHECK_(*b_smoothed <= UATP_MAX_POSITION);

	if (sc->sc_motion_timer < sc->sc_knobs.motion_delay) {
		DPRINTF(sc, UATP_DEBUG_MOVE, ("delay motion %u\n",
			sc->sc_motion_timer));
		sc->sc_motion_timer += 1;
		return;
	}

	/* XXX Use raw distances or smoothed distances?  Acceleration?  */
	if (*a_smoothed < old_a_smoothed)
		a_dist = old_a_smoothed - *a_smoothed;
	else
		a_dist = *a_smoothed - old_a_smoothed;

	if (*b_smoothed < old_b_smoothed)
		b_dist = old_b_smoothed - *b_smoothed;
	else
		b_dist = *b_smoothed - old_b_smoothed;

	dist_squared = (a_dist * a_dist) + (b_dist * b_dist);
	if (dist_squared < ((2 * UATP_MAX_POSITION * UATP_MAX_POSITION)
		- sc->sc_track_distance))
		sc->sc_track_distance += dist_squared;
	else
		sc->sc_track_distance = (2 * UATP_MAX_POSITION *
		    UATP_MAX_POSITION);
	DPRINTF(sc, UATP_DEBUG_TRACK_DIST, ("finger has tracked %u units^2\n",
		sc->sc_track_distance));

	/*
	 * The checks above guarantee that the differences here are at
	 * most UATP_MAX_POSITION in magnitude, since both minuend and
	 * subtrahend are nonnegative and at most UATP_MAX_POSITION.
	 */
	if (motion_below_threshold(sc, sc->sc_knobs.motion_threshold,
		(int)(*a_smoothed - old_a_smoothed),
		(int)(*b_smoothed - old_b_smoothed))) {
		DPRINTF(sc, UATP_DEBUG_MOVE,
		    ("%s motion too small: (%d, %d) -> (%d, %d)\n", ctx,
			old_a_smoothed, old_b_smoothed,
			*a_smoothed, *b_smoothed));
		return;
	}
	if (sc->sc_knobs.fast_per_direction == 0) {
		a_fast = b_fast = !motion_below_threshold(sc,
		    sc->sc_knobs.fast_motion_threshold,
		    (int)(*a_smoothed - old_a_smoothed),
		    (int)(*b_smoothed - old_b_smoothed));
	} else {
		a_fast = !motion_below_threshold(sc,
		    sc->sc_knobs.fast_motion_threshold,
		    (int)(*a_smoothed - old_a_smoothed),
		    0);
		b_fast = !motion_below_threshold(sc,
		    sc->sc_knobs.fast_motion_threshold,
		    0,
		    (int)(*b_smoothed - old_b_smoothed));
	}
	*da = accelerate(sc, old_a_raw, *a_raw, old_a_smoothed, *a_smoothed,
	    a_fast, a_remainder);
	*db = accelerate(sc, old_b_raw, *b_raw, old_b_smoothed, *b_smoothed,
	    b_fast, b_remainder);
	DPRINTF(sc, UATP_DEBUG_MOVE,
	    ("update %s position (%d, %d) -> (%d, %d), move by (%d, %d)\n",
		ctx, old_a_smoothed, old_b_smoothed, *a_smoothed, *b_smoothed,
		*da, *db));

#undef CHECK_
}

static int
smooth(struct uatp_softc *sc, unsigned int old_raw, unsigned int old_smoothed,
    unsigned int raw)
{
#define CHECK_(condition) CHECK(condition, return old_raw)

	/*
	 * Arithmetic bounds:
	 * . the weights are at most UATP_MAX_WEIGHT;
	 * . the positions are at most UATP_MAX_POSITION; and so
	 * . the numerator of the average is at most
	 *     3 * UATP_MAX_WEIGHT * UATP_MAX_POSITION,
	 *   which is #x477000, fitting comfortably in an int.
	 */
	__CTASSERT(0x477000 == (3 * UATP_MAX_WEIGHT * UATP_MAX_POSITION));
	unsigned int old_raw_weight = uatp_old_raw_weight(sc);
	unsigned int old_smoothed_weight = uatp_old_smoothed_weight(sc);
	unsigned int new_raw_weight = uatp_new_raw_weight(sc);
	CHECK_(old_raw_weight <= UATP_MAX_WEIGHT);
	CHECK_(old_smoothed_weight <= UATP_MAX_WEIGHT);
	CHECK_(new_raw_weight <= UATP_MAX_WEIGHT);
	CHECK_(old_raw <= UATP_MAX_POSITION);
	CHECK_(old_smoothed <= UATP_MAX_POSITION);
	CHECK_(raw <= UATP_MAX_POSITION);
	return (((old_raw_weight * old_raw) +
		(old_smoothed_weight * old_smoothed) +
		(new_raw_weight * raw))
	    / (old_raw_weight + old_smoothed_weight + new_raw_weight));

#undef CHECK_
}

static bool
motion_below_threshold(struct uatp_softc *sc, unsigned int threshold,
    int x, int y)
{
	unsigned int x_squared, y_squared;

	/* Caller guarantees the multiplication will not overflow.  */
	KASSERT(-UATP_MAX_POSITION <= x);
	KASSERT(-UATP_MAX_POSITION <= y);
	KASSERT(x <= UATP_MAX_POSITION);
	KASSERT(y <= UATP_MAX_POSITION);
	__CTASSERT(0x12000000 == (2 * UATP_MAX_POSITION * UATP_MAX_POSITION));

	x_squared = (x * x);
	y_squared = (y * y);

	return ((x_squared + y_squared) < threshold);
}

static int
accelerate(struct uatp_softc *sc, unsigned int old_raw, unsigned int raw,
    unsigned int old_smoothed, unsigned int smoothed, bool fast,
    int *remainder)
{
#define CHECK_(condition) CHECK(condition, return 0)

	/* Guarantee that the scaling won't overflow.  */
	__CTASSERT(0x30000 ==
	    (UATP_MAX_POSITION * UATP_MAX_MOTION_MULTIPLIER));

	CHECK_(old_raw <= UATP_MAX_POSITION);
	CHECK_(raw <= UATP_MAX_POSITION);
	CHECK_(old_smoothed <= UATP_MAX_POSITION);
	CHECK_(smoothed <= UATP_MAX_POSITION);

	return (fast ? uatp_scale_fast_motion : uatp_scale_motion)
	    (sc, (((int) smoothed) - ((int) old_smoothed)), remainder);

#undef CHECK_
}

MODULE(MODULE_CLASS_DRIVER, uatp, NULL);

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
uatp_modcmd(modcmd_t cmd, void *aux)
{
	int error = 0;

	switch (cmd) {
	case MODULE_CMD_INIT:
#ifdef _MODULE
		error = config_init_component(cfdriver_ioconf_uatp,
		    cfattach_ioconf_uatp, cfdata_ioconf_uatp);
#endif
		return error;
	case MODULE_CMD_FINI:
#ifdef _MODULE
		error = config_fini_component(cfdriver_ioconf_uatp,
		    cfattach_ioconf_uatp, cfdata_ioconf_uatp);
#endif
		return error;
	default:
		return ENOTTY;
	}
}
