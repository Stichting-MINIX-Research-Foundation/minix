/*
 * Linux evdev compatible event devices (see <minix/evdev.h>).
 *
 * The input server receives USB HID style events from its drivers.  While
 * one of the evdev devices is open, the events of its kind (keyboard or
 * mouse) of all drivers are translated into Linux evdev events here and
 * queued in Linux format, each followed by a SYN_REPORT.  An event arriving
 * in the same clock tick as the previous one, while that previous frame has
 * not been read yet, joins that frame (a PS/2 mouse packet thus becomes one
 * frame with REL_X, REL_Y and the button changes).  Timestamps are taken at
 * arrival, in clock ticks, and converted when the events are read.
 */
#include <minix/drivers.h>
#include <minix/chardriver.h>
#include <minix/ds.h>
#include <minix/evdev.h>
#include <sys/ioctl.h>
#include <time.h>

#include "input.h"

#define EVDEV_BUFSIZE		256	/* queued evdev events per device */
#define NR_EVDEVS		2

#define BITS_BYTES(n)		(((n) + 7) / 8)
#define KEY_BYTES		BITS_BYTES(EVDEV_KEY_MAX + 1)
#define REL_BYTES		BITS_BYTES(EVDEV_REL_MAX + 1)
#define LED_BYTES		BITS_BYTES(EVDEV_LED_MAX + 1)
#define EV_BYTES		BITS_BYTES(EVDEV_EV_MAX + 1)
#define PROP_BYTES		4

#define SET_BIT(a, b)		((a)[(b) / 8] |= (1 << ((b) % 8)))
#define CLR_BIT(a, b)		((a)[(b) / 8] &= ~(1 << ((b) % 8)))

struct evdev {
	devminor_t minor;
	int mouse;			/* mouse (TRUE) or keyboard device */
	const char *name;		/* device name (EVIOCGNAME) */
	struct evdev_id id;		/* device ID (EVIOCGID) */
	int opened;
	clockid_t clockid;		/* timestamp clock (EVIOCSCLOCKID) */

	struct evdev_event buf[EVDEV_BUFSIZE];	/* event ring buffer */
	clock_t when[EVDEV_BUFSIZE];	/* arrival time (uptime ticks) */
	unsigned int tail;
	unsigned int count;

	int suspended;			/* is a read suspended? */
	endpoint_t caller;
	cp_grant_id_t grant;
	size_t size;
	cdev_id_t req_id;
	endpoint_t selector;		/* read-selecting endpoint, or NONE */

	uint8_t ev_bits[EV_BYTES];	/* capabilities */
	uint8_t key_bits[KEY_BYTES];
	uint8_t rel_bits[REL_BYTES];
	uint8_t led_bits[LED_BYTES];

	uint8_t key_state[KEY_BYTES];	/* keys and buttons held down */
	uint8_t led_state[LED_BYTES];	/* LEDs lit */
};

static struct evdev evdevs[NR_EVDEVS];

static const uint8_t zeros[KEY_BYTES];

/*
 * USB HID keyboard page usages to Linux key codes, as Linux' hid-input
 * maps them.  Zero entries have no translation.
 */
static const uint16_t hid_keys[INPUT_KEY_RIGHT_GUI + 1] = {
	[INPUT_KEY_A] = 30,			/* KEY_A */
	[INPUT_KEY_B] = 48,			/* KEY_B */
	[INPUT_KEY_C] = 46,			/* KEY_C */
	[INPUT_KEY_D] = 32,			/* KEY_D */
	[INPUT_KEY_E] = 18,			/* KEY_E */
	[INPUT_KEY_F] = 33,			/* KEY_F */
	[INPUT_KEY_G] = 34,			/* KEY_G */
	[INPUT_KEY_H] = 35,			/* KEY_H */
	[INPUT_KEY_I] = 23,			/* KEY_I */
	[INPUT_KEY_J] = 36,			/* KEY_J */
	[INPUT_KEY_K] = 37,			/* KEY_K */
	[INPUT_KEY_L] = 38,			/* KEY_L */
	[INPUT_KEY_M] = 50,			/* KEY_M */
	[INPUT_KEY_N] = 49,			/* KEY_N */
	[INPUT_KEY_O] = 24,			/* KEY_O */
	[INPUT_KEY_P] = 25,			/* KEY_P */
	[INPUT_KEY_Q] = 16,			/* KEY_Q */
	[INPUT_KEY_R] = 19,			/* KEY_R */
	[INPUT_KEY_S] = 31,			/* KEY_S */
	[INPUT_KEY_T] = 20,			/* KEY_T */
	[INPUT_KEY_U] = 22,			/* KEY_U */
	[INPUT_KEY_V] = 47,			/* KEY_V */
	[INPUT_KEY_W] = 17,			/* KEY_W */
	[INPUT_KEY_X] = 45,			/* KEY_X */
	[INPUT_KEY_Y] = 21,			/* KEY_Y */
	[INPUT_KEY_Z] = 44,			/* KEY_Z */
	[INPUT_KEY_1] = 2,			/* KEY_1 */
	[INPUT_KEY_2] = 3,			/* KEY_2 */
	[INPUT_KEY_3] = 4,			/* KEY_3 */
	[INPUT_KEY_4] = 5,			/* KEY_4 */
	[INPUT_KEY_5] = 6,			/* KEY_5 */
	[INPUT_KEY_6] = 7,			/* KEY_6 */
	[INPUT_KEY_7] = 8,			/* KEY_7 */
	[INPUT_KEY_8] = 9,			/* KEY_8 */
	[INPUT_KEY_9] = 10,			/* KEY_9 */
	[INPUT_KEY_0] = 11,			/* KEY_0 */
	[INPUT_KEY_ENTER] = 28,			/* KEY_ENTER */
	[INPUT_KEY_ESCAPE] = 1,			/* KEY_ESC */
	[INPUT_KEY_BACKSPACE] = 14,		/* KEY_BACKSPACE */
	[INPUT_KEY_TAB] = 15,			/* KEY_TAB */
	[INPUT_KEY_SPACEBAR] = 57,		/* KEY_SPACE */
	[INPUT_KEY_DASH] = 12,			/* KEY_MINUS */
	[INPUT_KEY_EQUAL] = 13,			/* KEY_EQUAL */
	[INPUT_KEY_OPEN_BRACKET] = 26,		/* KEY_LEFTBRACE */
	[INPUT_KEY_CLOSE_BRACKET] = 27,		/* KEY_RIGHTBRACE */
	[INPUT_KEY_BACKSLASH] = 43,		/* KEY_BACKSLASH */
	[INPUT_KEY_EUROPE_1] = 43,		/* KEY_BACKSLASH */
	[INPUT_KEY_SEMICOLON] = 39,		/* KEY_SEMICOLON */
	[INPUT_KEY_APOSTROPH] = 40,		/* KEY_APOSTROPHE */
	[INPUT_KEY_GRAVE_ACCENT] = 41,		/* KEY_GRAVE */
	[INPUT_KEY_COMMA] = 51,			/* KEY_COMMA */
	[INPUT_KEY_PERIOD] = 52,		/* KEY_DOT */
	[INPUT_KEY_SLASH] = 53,			/* KEY_SLASH */
	[INPUT_KEY_CAPS_LOCK] = 58,		/* KEY_CAPSLOCK */
	[INPUT_KEY_F1] = 59,			/* KEY_F1 */
	[INPUT_KEY_F2] = 60,			/* KEY_F2 */
	[INPUT_KEY_F3] = 61,			/* KEY_F3 */
	[INPUT_KEY_F4] = 62,			/* KEY_F4 */
	[INPUT_KEY_F5] = 63,			/* KEY_F5 */
	[INPUT_KEY_F6] = 64,			/* KEY_F6 */
	[INPUT_KEY_F7] = 65,			/* KEY_F7 */
	[INPUT_KEY_F8] = 66,			/* KEY_F8 */
	[INPUT_KEY_F9] = 67,			/* KEY_F9 */
	[INPUT_KEY_F10] = 68,			/* KEY_F10 */
	[INPUT_KEY_F11] = 87,			/* KEY_F11 */
	[INPUT_KEY_F12] = 88,			/* KEY_F12 */
	[INPUT_KEY_PRINT_SCREEN] = 99,		/* KEY_SYSRQ */
	[INPUT_KEY_SCROLL_LOCK] = 70,		/* KEY_SCROLLLOCK */
	[INPUT_KEY_PAUSE] = 119,		/* KEY_PAUSE */
	[INPUT_KEY_INSERT] = 110,		/* KEY_INSERT */
	[INPUT_KEY_HOME] = 102,			/* KEY_HOME */
	[INPUT_KEY_PAGE_UP] = 104,		/* KEY_PAGEUP */
	[INPUT_KEY_DELETE] = 111,		/* KEY_DELETE */
	[INPUT_KEY_END] = 107,			/* KEY_END */
	[INPUT_KEY_PAGE_DOWN] = 109,		/* KEY_PAGEDOWN */
	[INPUT_KEY_RIGHT_ARROW] = 106,		/* KEY_RIGHT */
	[INPUT_KEY_LEFT_ARROW] = 105,		/* KEY_LEFT */
	[INPUT_KEY_DOWN_ARROW] = 108,		/* KEY_DOWN */
	[INPUT_KEY_UP_ARROW] = 103,		/* KEY_UP */
	[INPUT_KEY_NUM_LOCK] = 69,		/* KEY_NUMLOCK */
	[INPUT_KEY_KP_SLASH] = 98,		/* KEY_KPSLASH */
	[INPUT_KEY_KP_STAR] = 55,		/* KEY_KPASTERISK */
	[INPUT_KEY_KP_DASH] = 74,		/* KEY_KPMINUS */
	[INPUT_KEY_KP_PLUS] = 78,		/* KEY_KPPLUS */
	[INPUT_KEY_KP_ENTER] = 96,		/* KEY_KPENTER */
	[INPUT_KEY_KP_1] = 79,			/* KEY_KP1 */
	[INPUT_KEY_KP_2] = 80,			/* KEY_KP2 */
	[INPUT_KEY_KP_3] = 81,			/* KEY_KP3 */
	[INPUT_KEY_KP_4] = 75,			/* KEY_KP4 */
	[INPUT_KEY_KP_5] = 76,			/* KEY_KP5 */
	[INPUT_KEY_KP_6] = 77,			/* KEY_KP6 */
	[INPUT_KEY_KP_7] = 71,			/* KEY_KP7 */
	[INPUT_KEY_KP_8] = 72,			/* KEY_KP8 */
	[INPUT_KEY_KP_9] = 73,			/* KEY_KP9 */
	[INPUT_KEY_KP_0] = 82,			/* KEY_KP0 */
	[INPUT_KEY_KP_PERIOD] = 83,		/* KEY_KPDOT */
	[INPUT_KEY_EUROPE_2] = 86,		/* KEY_102ND */
	[INPUT_KEY_APPLICATION] = 127,		/* KEY_COMPOSE */
	[INPUT_KEY_POWER] = 116,		/* KEY_POWER */
	[INPUT_KEY_KP_EQUAL] = 117,		/* KEY_KPEQUAL */
	[INPUT_KEY_F13] = 183,			/* KEY_F13 */
	[INPUT_KEY_F14] = 184,			/* KEY_F14 */
	[INPUT_KEY_F15] = 185,			/* KEY_F15 */
	[INPUT_KEY_F16] = 186,			/* KEY_F16 */
	[INPUT_KEY_F17] = 187,			/* KEY_F17 */
	[INPUT_KEY_F18] = 188,			/* KEY_F18 */
	[INPUT_KEY_F19] = 189,			/* KEY_F19 */
	[INPUT_KEY_F20] = 190,			/* KEY_F20 */
	[INPUT_KEY_F21] = 191,			/* KEY_F21 */
	[INPUT_KEY_F22] = 192,			/* KEY_F22 */
	[INPUT_KEY_F23] = 193,			/* KEY_F23 */
	[INPUT_KEY_F24] = 194,			/* KEY_F24 */
	[INPUT_KEY_EXECUTE] = 134,		/* KEY_OPEN */
	[INPUT_KEY_HELP] = 138,			/* KEY_HELP */
	[INPUT_KEY_MENU] = 130,			/* KEY_PROPS */
	[INPUT_KEY_SELECT] = 132,		/* KEY_FRONT */
	[INPUT_KEY_STOP] = 128,			/* KEY_STOP */
	[INPUT_KEY_AGAIN] = 129,		/* KEY_AGAIN */
	[INPUT_KEY_UNDO] = 131,			/* KEY_UNDO */
	[INPUT_KEY_CUT] = 137,			/* KEY_CUT */
	[INPUT_KEY_COPY] = 133,			/* KEY_COPY */
	[INPUT_KEY_PASTE] = 135,		/* KEY_PASTE */
	[INPUT_KEY_FIND] = 136,			/* KEY_FIND */
	[INPUT_KEY_MUTE] = 113,			/* KEY_MUTE */
	[INPUT_KEY_VOLUME_UP] = 115,		/* KEY_VOLUMEUP */
	[INPUT_KEY_VOLUME_DOWN] = 114,		/* KEY_VOLUMEDOWN */
	[INPUT_KEY_KP_COMMA] = 121,		/* KEY_KPCOMMA */
	[INPUT_KEY_I10L_1] = 89,		/* KEY_RO */
	[INPUT_KEY_I10L_2] = 93,		/* KEY_KATAKANAHIRAGANA */
	[INPUT_KEY_I10L_3] = 124,		/* KEY_YEN */
	[INPUT_KEY_I10L_4] = 92,		/* KEY_HENKAN */
	[INPUT_KEY_I10L_5] = 94,		/* KEY_MUHENKAN */
	[INPUT_KEY_I10L_6] = 95,		/* KEY_KPJPCOMMA */
	[INPUT_KEY_LANG_1] = 122,		/* KEY_HANGEUL */
	[INPUT_KEY_LANG_2] = 123,		/* KEY_HANJA */
	[INPUT_KEY_LANG_3] = 90,		/* KEY_KATAKANA */
	[INPUT_KEY_LANG_4] = 91,		/* KEY_HIRAGANA */
	[INPUT_KEY_LANG_5] = 85,		/* KEY_ZENKAKUHANKAKU */
	[INPUT_KEY_KP_OPEN_PARENTHESIS] = 179,	/* KEY_KPLEFTPAREN */
	[INPUT_KEY_KP_CLOSE_PARENTHESIS] = 180,	/* KEY_KPRIGHTPAREN */
	[INPUT_KEY_LEFT_CTRL] = 29,		/* KEY_LEFTCTRL */
	[INPUT_KEY_LEFT_SHIFT] = 42,		/* KEY_LEFTSHIFT */
	[INPUT_KEY_LEFT_ALT] = 56,		/* KEY_LEFTALT */
	[INPUT_KEY_LEFT_GUI] = 125,		/* KEY_LEFTMETA */
	[INPUT_KEY_RIGHT_CTRL] = 97,		/* KEY_RIGHTCTRL */
	[INPUT_KEY_RIGHT_SHIFT] = 54,		/* KEY_RIGHTSHIFT */
	[INPUT_KEY_RIGHT_ALT] = 100,		/* KEY_RIGHTALT */
	[INPUT_KEY_RIGHT_GUI] = 126,		/* KEY_RIGHTMETA */
};

/* Consumer and generic desktop page usages that are keys. */
static const struct {
	uint16_t page, usage, key;
} hid_other_keys[] = {
	{ INPUT_PAGE_CONS, INPUT_CONS_SCAN_NEXT_TRACK,	163 },	/* KEY_NEXTSONG */
	{ INPUT_PAGE_CONS, INPUT_CONS_SCAN_PREVIOUS_TRACK, 165 }, /* KEY_PREVIOUSSONG */
	{ INPUT_PAGE_CONS, INPUT_CONS_STOP,		166 },	/* KEY_STOPCD */
	{ INPUT_PAGE_CONS, INPUT_CONS_PLAY_PAUSE,	164 },	/* KEY_PLAYPAUSE */
	{ INPUT_PAGE_CONS, INPUT_CONS_MUTE,		113 },	/* KEY_MUTE */
	{ INPUT_PAGE_CONS, INPUT_CONS_VOLUME_UP,	115 },	/* KEY_VOLUMEUP */
	{ INPUT_PAGE_CONS, INPUT_CONS_VOLUME_DOWN,	114 },	/* KEY_VOLUMEDOWN */
	{ INPUT_PAGE_CONS, INPUT_CONS_AL_MEDIA_SELECT,	171 },	/* KEY_CONFIG */
	{ INPUT_PAGE_CONS, INPUT_CONS_AL_EMAIL_READER,	155 },	/* KEY_MAIL */
	{ INPUT_PAGE_CONS, INPUT_CONS_AL_CALCULATOR,	140 },	/* KEY_CALC */
	{ INPUT_PAGE_CONS, INPUT_CONS_AL_LOCAL_BROWSER,	144 },	/* KEY_FILE */
	{ INPUT_PAGE_CONS, INPUT_CONS_AC_SEARCH,	217 },	/* KEY_SEARCH */
	{ INPUT_PAGE_CONS, INPUT_CONS_AC_GO_TO,		354 },	/* KEY_GOTO */
	{ INPUT_PAGE_CONS, INPUT_CONS_AC_HOME,		172 },	/* KEY_HOMEPAGE */
	{ INPUT_PAGE_CONS, INPUT_CONS_AC_BACK,		158 },	/* KEY_BACK */
	{ INPUT_PAGE_CONS, INPUT_CONS_AC_FORWARD,	159 },	/* KEY_FORWARD */
	{ INPUT_PAGE_CONS, INPUT_CONS_AC_STOP,		128 },	/* KEY_STOP */
	{ INPUT_PAGE_CONS, INPUT_CONS_AC_REFRESH,	173 },	/* KEY_REFRESH */
	{ INPUT_PAGE_CONS, INPUT_CONS_AC_BOOKMARKS,	156 },	/* KEY_BOOKMARKS */
	{ INPUT_PAGE_GD, INPUT_GD_SYSTEM_POWER_DOWN,	116 },	/* KEY_POWER */
	{ INPUT_PAGE_GD, INPUT_GD_SYSTEM_SLEEP,		142 },	/* KEY_SLEEP */
	{ INPUT_PAGE_GD, INPUT_GD_SYSTEM_WAKE_UP,	143 },	/* KEY_WAKEUP */
};

#define HID_GD_WHEEL		0x0038	/* generic desktop: wheel */
#define HID_CONS_AC_PAN		0x0238	/* consumer: horizontal wheel */

/* Mouse buttons, HID button 1..5 (primary, secondary, tertiary, ...). */
static const uint16_t hid_buttons[] = {
	EVDEV_BTN_LEFT, EVDEV_BTN_RIGHT, EVDEV_BTN_MIDDLE, EVDEV_BTN_SIDE,
	EVDEV_BTN_EXTRA
};

/*
 * Return the evdev device structure for a minor number, or NULL.
 */
static struct evdev *
evdev_map(devminor_t minor)
{
	int i;

	for (i = 0; i < NR_EVDEVS; i++)
		if (evdevs[i].minor == minor)
			return &evdevs[i];

	return NULL;
}

/*
 * Is the given minor number one of the evdev devices?
 */
int
evdev_minor(devminor_t minor)
{

	return evdev_map(minor) != NULL;
}

/*
 * Initialize the evdev devices.
 */
void
evdev_init(void)
{
	struct evdev *ev;
	unsigned int i;

	memset(evdevs, 0, sizeof(evdevs));

	/* Keyboards. */
	ev = &evdevs[0];
	ev->minor = EVDEV_KBD_MINOR;
	ev->mouse = FALSE;
	ev->name = "MINIX keyboard";
	ev->id.bustype = EVDEV_BUS_I8042;
	ev->id.vendor = 0x0001;
	ev->id.product = 0x0001;
	ev->id.version = 0x0001;
	SET_BIT(ev->ev_bits, EVDEV_EV_SYN);
	SET_BIT(ev->ev_bits, EVDEV_EV_KEY);
	SET_BIT(ev->ev_bits, EVDEV_EV_LED);
	for (i = 0; i < __arraycount(hid_keys); i++)
		if (hid_keys[i] != 0)
			SET_BIT(ev->key_bits, hid_keys[i]);
	for (i = 0; i < __arraycount(hid_other_keys); i++)
		SET_BIT(ev->key_bits, hid_other_keys[i].key);
	SET_BIT(ev->led_bits, EVDEV_LED_NUML);
	SET_BIT(ev->led_bits, EVDEV_LED_CAPSL);
	SET_BIT(ev->led_bits, EVDEV_LED_SCROLLL);

	/* Mice. */
	ev = &evdevs[1];
	ev->minor = EVDEV_MOUSE_MINOR;
	ev->mouse = TRUE;
	ev->name = "MINIX mouse";
	ev->id.bustype = EVDEV_BUS_I8042;
	ev->id.vendor = 0x0002;
	ev->id.product = 0x0001;
	ev->id.version = 0x0001;
	SET_BIT(ev->ev_bits, EVDEV_EV_SYN);
	SET_BIT(ev->ev_bits, EVDEV_EV_KEY);
	SET_BIT(ev->ev_bits, EVDEV_EV_REL);
	for (i = 0; i < __arraycount(hid_buttons); i++)
		SET_BIT(ev->key_bits, hid_buttons[i]);
	SET_BIT(ev->rel_bits, EVDEV_REL_X);
	SET_BIT(ev->rel_bits, EVDEV_REL_Y);
	SET_BIT(ev->rel_bits, EVDEV_REL_WHEEL);
	SET_BIT(ev->rel_bits, EVDEV_REL_HWHEEL);

	for (i = 0; i < NR_EVDEVS; i++) {
		evdevs[i].clockid = CLOCK_MONOTONIC;
		evdevs[i].selector = NONE;
	}
}

/*
 * Translate a HID style input event into an evdev event.  Return TRUE and
 * fill in the evdev type, code and value, or return FALSE if the event has
 * no translation.
 */
static int
evdev_translate(const struct evdev *ev, unsigned int page, unsigned int code,
	int value, unsigned int flags, uint16_t *typep, uint16_t *codep,
	int32_t *valuep)
{
	unsigned int i;

	if (ev->mouse) {
		switch (page) {
		case INPUT_PAGE_BUTTON:
			if (code < INPUT_BUTTON_1 ||
			    code - INPUT_BUTTON_1 >= __arraycount(hid_buttons))
				return FALSE;
			*typep = EVDEV_EV_KEY;
			*codep = hid_buttons[code - INPUT_BUTTON_1];
			*valuep = !!value;
			return TRUE;
		case INPUT_PAGE_GD:
			if (!(flags & INPUT_FLAG_REL))
				return FALSE;	/* no absolute axes */
			*typep = EVDEV_EV_REL;
			if (code == INPUT_GD_X)
				*codep = EVDEV_REL_X;
			else if (code == INPUT_GD_Y)
				*codep = EVDEV_REL_Y;
			else if (code == HID_GD_WHEEL)
				*codep = EVDEV_REL_WHEEL;
			else
				return FALSE;
			*valuep = value;
			return TRUE;
		case INPUT_PAGE_CONS:
			if (code != HID_CONS_AC_PAN || !(flags & INPUT_FLAG_REL))
				return FALSE;
			*typep = EVDEV_EV_REL;
			*codep = EVDEV_REL_HWHEEL;
			*valuep = value;
			return TRUE;
		default:
			return FALSE;
		}
	}

	*typep = EVDEV_EV_KEY;
	*valuep = (value != INPUT_RELEASE);

	if (page == INPUT_PAGE_KEY) {
		if (code >= __arraycount(hid_keys) || hid_keys[code] == 0)
			return FALSE;
		*codep = hid_keys[code];
		return TRUE;
	}

	for (i = 0; i < __arraycount(hid_other_keys); i++) {
		if (hid_other_keys[i].page == page &&
		    hid_other_keys[i].usage == code) {
			*codep = hid_other_keys[i].key;
			return TRUE;
		}
	}

	return FALSE;
}

/*
 * Return the ring buffer index of the given queue position.
 */
static unsigned int
evdev_slot(const struct evdev *ev, unsigned int pos)
{

	return (ev->tail + pos) % EVDEV_BUFSIZE;
}

/*
 * Append one event to the queue.  The caller has made room.
 */
static void
evdev_append(struct evdev *ev, uint16_t type, uint16_t code, int32_t value,
	clock_t when)
{
	unsigned int slot;

	slot = evdev_slot(ev, ev->count);
	memset(&ev->buf[slot], 0, sizeof(ev->buf[slot]));
	ev->buf[slot].type = type;
	ev->buf[slot].code = code;
	ev->buf[slot].value = value;
	ev->when[slot] = when;
	ev->count++;
}

/*
 * Queue an event as a frame of its own or, if possible, as part of the last
 * queued frame.
 */
static void
evdev_queue(struct evdev *ev, uint16_t type, uint16_t code, int32_t value,
	clock_t when)
{
	struct evdev_event *e;
	unsigned int pos, last;

	/*
	 * Join the last frame if it is still queued completely (its
	 * SYN_REPORT has not been read), stems from the same clock tick, and
	 * does not contain an event of the same type and code yet - relative
	 * motion of the same axis is summed up instead.
	 */
	e = NULL;
	if (ev->count > 0 && ev->count < EVDEV_BUFSIZE) {
		last = evdev_slot(ev, ev->count - 1);
		if (ev->buf[last].type == EVDEV_EV_SYN &&
		    ev->buf[last].code == EVDEV_SYN_REPORT &&
		    ev->when[last] == when) {
			for (pos = ev->count - 1; pos > 0; pos--) {
				e = &ev->buf[evdev_slot(ev, pos - 1)];
				if (e->type == EVDEV_EV_SYN)
					break;
				if (e->type == type && e->code == code)
					break;
			}
			if (pos == 0 || e == NULL || e->type == EVDEV_EV_SYN) {
				/* Reopen the frame: replace its SYN_REPORT. */
				ev->count--;
				evdev_append(ev, type, code, value, when);
				evdev_append(ev, EVDEV_EV_SYN,
				    EVDEV_SYN_REPORT, 0, when);
				return;
			}
			if (type == EVDEV_EV_REL) {
				e->value += value;
				return;
			}
		}
	}

	/*
	 * Overflow: like Linux, discard the queue, and tell the reader with
	 * SYN_DROPPED that it must resynchronize its state (EVIOCGKEY etc).
	 */
	if (ev->count + 2 > EVDEV_BUFSIZE - 1) {
		ev->tail = 0;
		ev->count = 0;
		evdev_append(ev, EVDEV_EV_SYN, EVDEV_SYN_DROPPED, 0, when);
	}

	evdev_append(ev, type, code, value, when);
	evdev_append(ev, EVDEV_EV_SYN, EVDEV_SYN_REPORT, 0, when);
}

/*
 * Copy up to 'max' queued events to a reader, converting their timestamps.
 * Return the number of bytes copied, or an error.
 */
static ssize_t
evdev_copy_out(struct evdev *ev, endpoint_t endpt, cp_grant_id_t grant,
	unsigned int max)
{
	struct evdev_event out[32];
	clock_t uptime, realtime, ticks;
	time_t boottime;
	unsigned int hz, n, i, done;
	int r;

	(void)getuptime(&uptime, &realtime, &boottime);
	hz = sys_hz();

	if (max > ev->count)
		max = ev->count;

	for (done = 0; done < max; done += n) {
		n = max - done;
		if (n > __arraycount(out))
			n = __arraycount(out);

		for (i = 0; i < n; i++) {
			out[i] = ev->buf[evdev_slot(ev, done + i)];
			ticks = ev->when[evdev_slot(ev, done + i)];
			/* As PM's clock_gettime(2) computes these clocks. */
			if (ev->clockid == CLOCK_REALTIME)
				ticks += realtime - uptime;
			out[i].time.tv_sec = boottime + ticks / hz;
			out[i].time.tv_usec =
			    (suseconds_t)((ticks % hz) * 1000000ULL / hz);
		}

		if ((r = sys_safecopyto(endpt, grant,
		    done * sizeof(out[0]), (vir_bytes)out,
		    n * sizeof(out[0]))) != OK)
			return r;
	}

	ev->tail = evdev_slot(ev, max);
	ev->count -= max;

	return max * sizeof(struct evdev_event);
}

/*
 * Deliver queued events to a suspended reader, or notify a selector.
 */
static void
evdev_wakeup(struct evdev *ev)
{
	ssize_t r;

	if (ev->count == 0)
		return;

	if (ev->suspended) {
		r = evdev_copy_out(ev, ev->caller, ev->grant,
		    ev->size / sizeof(struct evdev_event));
		chardriver_reply_task(ev->caller, ev->req_id, r);
		ev->suspended = FALSE;
	} else if (ev->selector != NONE) {
		chardriver_reply_select(ev->selector, ev->minor, CDEV_OP_RD);
		ev->selector = NONE;
	}
}

/*
 * An input event arrived from a driver.  If the evdev device of its kind is
 * open, queue the event there and return TRUE; otherwise return FALSE.
 */
int
evdev_input(int mouse, const message *m)
{
	struct evdev *ev;
	clock_t uptime;
	uint16_t type, code;
	int32_t value;

	ev = &evdevs[mouse ? 1 : 0];
	if (!ev->opened)
		return FALSE;

	if (!evdev_translate(ev, m->m_linputdriver_input_event.page,
	    m->m_linputdriver_input_event.code,
	    m->m_linputdriver_input_event.value,
	    m->m_linputdriver_input_event.flags, &type, &code, &value))
		return TRUE;	/* consumed, but nothing to report */

	if (type == EVDEV_EV_KEY) {
		if (value)
			SET_BIT(ev->key_state, code);
		else
			CLR_BIT(ev->key_state, code);
	}

	(void)getuptime(&uptime, NULL, NULL);

	evdev_queue(ev, type, code, value, uptime);

	evdev_wakeup(ev);

	return TRUE;
}

/*
 * Convert the evdev LED state to an input server LED mask.
 */
static unsigned int
evdev_led_mask(const struct evdev *ev)
{
	unsigned int mask;

	mask = 0;
	if (ev->led_state[0] & (1 << EVDEV_LED_NUML))
		mask |= (1 << INPUT_LED_NUMLOCK);
	if (ev->led_state[0] & (1 << EVDEV_LED_CAPSL))
		mask |= (1 << INPUT_LED_CAPSLOCK);
	if (ev->led_state[0] & (1 << EVDEV_LED_SCROLLL))
		mask |= (1 << INPUT_LED_SCROLLLOCK);

	return mask;
}

/*
 * Open an evdev device.
 */
int
evdev_open(devminor_t minor)
{
	struct evdev *ev;

	if ((ev = evdev_map(minor)) == NULL)
		return ENXIO;

	if (ev->opened)
		return EBUSY;

	ev->opened = TRUE;
	ev->clockid = CLOCK_MONOTONIC;
	ev->tail = 0;
	ev->count = 0;
	memset(ev->key_state, 0, sizeof(ev->key_state));
	memset(ev->led_state, 0, sizeof(ev->led_state));

	return OK;
}

/*
 * Close an evdev device.
 */
int
evdev_close(devminor_t minor)
{
	struct evdev *ev;

	if ((ev = evdev_map(minor)) == NULL)
		return ENXIO;

	if (!ev->opened)
		return EINVAL;

	ev->opened = FALSE;
	ev->tail = 0;
	ev->count = 0;

	/* Give the keyboard LEDs back to the console. */
	if (!ev->mouse)
		input_restore_leds();

	return OK;
}

/*
 * Read from an evdev device.
 */
ssize_t
evdev_read(devminor_t minor, endpoint_t endpt, cp_grant_id_t grant,
	size_t size, int flags, cdev_id_t id)
{
	struct evdev *ev;

	if ((ev = evdev_map(minor)) == NULL)
		return ENXIO;

	if (!ev->opened || ev->suspended)
		return EIO;

	if (size < sizeof(struct evdev_event))
		return EINVAL;

	if (ev->count == 0) {
		if (flags & CDEV_NONBLOCK)
			return EAGAIN;

		ev->suspended = TRUE;
		ev->caller = endpt;
		ev->grant = grant;
		ev->size = size;
		ev->req_id = id;
		return EDONTREPLY;
	}

	return evdev_copy_out(ev, endpt, grant,
	    size / sizeof(struct evdev_event));
}

/*
 * Write to an evdev device: EV_LED events set the keyboard LEDs, other
 * events are ignored.
 */
ssize_t
evdev_write(devminor_t minor, endpoint_t endpt, cp_grant_id_t grant,
	size_t size)
{
	struct evdev_event in[16];
	struct evdev *ev;
	size_t off, n, i;
	int r, leds_changed;

	if ((ev = evdev_map(minor)) == NULL)
		return ENXIO;

	if (!ev->opened)
		return EIO;

	if (size % sizeof(struct evdev_event) != 0)
		return EINVAL;

	leds_changed = FALSE;
	for (off = 0; off < size; off += n * sizeof(in[0])) {
		n = (size - off) / sizeof(in[0]);
		if (n > __arraycount(in))
			n = __arraycount(in);

		if ((r = sys_safecopyfrom(endpt, grant, off, (vir_bytes)in,
		    n * sizeof(in[0]))) != OK)
			return r;

		for (i = 0; i < n; i++) {
			if (ev->mouse || in[i].type != EVDEV_EV_LED ||
			    in[i].code > EVDEV_LED_SCROLLL)
				continue;
			if (in[i].value)
				SET_BIT(ev->led_state, in[i].code);
			else
				CLR_BIT(ev->led_state, in[i].code);
			leds_changed = TRUE;
		}
	}

	if (leds_changed)
		input_set_leds(KBDMUX_MINOR, evdev_led_mask(ev));

	return size;
}

/*
 * Copy (a prefix of) a bit array or string to the caller of an ioctl.
 * Like Linux, copy no more than the caller asked for.
 */
static int
evdev_ioctl_out(endpoint_t endpt, cp_grant_id_t grant, size_t len,
	const void *data, size_t datalen)
{

	if (len > datalen)
		len = datalen;
	if (len == 0)
		return OK;

	return sys_safecopyto(endpt, grant, 0, (vir_bytes)data, len);
}

/*
 * Process an ioctl on an evdev device.
 */
int
evdev_ioctl(devminor_t minor, unsigned long request, endpoint_t endpt,
	cp_grant_id_t grant)
{
	struct evdev *ev;
	size_t len;
	int r, val;
	unsigned int nr, type;

	if ((ev = evdev_map(minor)) == NULL)
		return ENXIO;

	if (!ev->opened)
		return EIO;

	/* Fixed-size requests. */
	switch (request) {
	case EVDEV_IOCGVERSION:
		val = EVDEV_VERSION;
		return sys_safecopyto(endpt, grant, 0, (vir_bytes)&val,
		    sizeof(val));
	case EVDEV_IOCGID:
		return sys_safecopyto(endpt, grant, 0, (vir_bytes)&ev->id,
		    sizeof(ev->id));
	case EVDEV_IOCGRAB:
		/* Input is always exclusive to an open evdev device. */
		if ((r = sys_safecopyfrom(endpt, grant, 0, (vir_bytes)&val,
		    sizeof(val))) != OK)
			return r;
		return OK;
	case EVDEV_IOCSCLOCKID:
		if ((r = sys_safecopyfrom(endpt, grant, 0, (vir_bytes)&val,
		    sizeof(val))) != OK)
			return r;
		if (val != CLOCK_MONOTONIC && val != CLOCK_REALTIME)
			return EINVAL;
		ev->clockid = val;
		return OK;
	}

	/* Variable-length requests: the length is part of the request. */
	if (IOCGROUP(request) != 'E' || (request & IOC_DIRMASK) != IOC_OUT)
		return ENOTTY;

	nr = EVDEV_IOC_NR(request);
	len = IOCPARM_LEN(request);

	switch (nr) {
	case EVDEV_IOC_NR(EVDEV_IOCGNAME(0)):
		return evdev_ioctl_out(endpt, grant, len, ev->name,
		    strlen(ev->name) + 1);
	case EVDEV_IOC_NR(EVDEV_IOCGPHYS(0)):
	case EVDEV_IOC_NR(EVDEV_IOCGUNIQ(0)):
		return ENOENT;
	case EVDEV_IOC_NR(EVDEV_IOCGPROP(0)):
		return evdev_ioctl_out(endpt, grant, len, zeros, PROP_BYTES);
	case EVDEV_IOC_NR(EVDEV_IOCGKEY(0)):
		return evdev_ioctl_out(endpt, grant, len, ev->key_state,
		    sizeof(ev->key_state));
	case EVDEV_IOC_NR(EVDEV_IOCGLED(0)):
		return evdev_ioctl_out(endpt, grant, len, ev->led_state,
		    sizeof(ev->led_state));
	case EVDEV_IOC_NR(EVDEV_IOCGSND(0)):
	case EVDEV_IOC_NR(EVDEV_IOCGSW(0)):
		return evdev_ioctl_out(endpt, grant, len, zeros, len);
	}

	if (nr >= EVDEV_IOC_NR(EVDEV_IOCGBIT(0, 0)) &&
	    nr <= EVDEV_IOC_NR(EVDEV_IOCGBIT(EVDEV_EV_MAX, 0))) {
		type = nr - EVDEV_IOC_NR(EVDEV_IOCGBIT(0, 0));
		switch (type) {
		case 0:
			return evdev_ioctl_out(endpt, grant, len, ev->ev_bits,
			    sizeof(ev->ev_bits));
		case EVDEV_EV_KEY:
			return evdev_ioctl_out(endpt, grant, len, ev->key_bits,
			    sizeof(ev->key_bits));
		case EVDEV_EV_REL:
			return evdev_ioctl_out(endpt, grant, len, ev->rel_bits,
			    sizeof(ev->rel_bits));
		case EVDEV_EV_LED:
			return evdev_ioctl_out(endpt, grant, len, ev->led_bits,
			    sizeof(ev->led_bits));
		default:
			return evdev_ioctl_out(endpt, grant, len, zeros, len);
		}
	}

	return EINVAL;	/* e.g. EVIOCGABS: there are no absolute axes */
}

/*
 * Cancel a suspended read.
 */
int
evdev_cancel(devminor_t minor, endpoint_t endpt, cdev_id_t id)
{
	struct evdev *ev;

	if ((ev = evdev_map(minor)) == NULL)
		return ENXIO;

	if (ev->suspended && ev->caller == endpt && ev->req_id == id) {
		ev->suspended = FALSE;
		return EINTR;
	}

	return EDONTREPLY;
}

/*
 * Select on an evdev device.
 */
int
evdev_select(devminor_t minor, unsigned int ops, endpoint_t endpt)
{
	struct evdev *ev;
	int ready_ops;

	if ((ev = evdev_map(minor)) == NULL)
		return ENXIO;

	ready_ops = 0;

	if (ops & CDEV_OP_RD) {
		if (!ev->opened || ev->suspended || ev->count > 0)
			ready_ops |= CDEV_OP_RD;
		else if (ops & CDEV_NOTIFY)
			ev->selector = endpt;
	}

	if (ops & CDEV_OP_WR)
		ready_ops |= CDEV_OP_WR;

	return ready_ops;
}
