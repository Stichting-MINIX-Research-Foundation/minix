/* Keyboard/mouse input server. */
#include <minix/drivers.h>
#include <minix/chardriver.h>
#include <minix/ds.h>
#include <sys/ioctl.h>
#include <sys/kbdio.h>

#include "input.h"

#define INPUT_DEBUG 0

static int input_open(devminor_t, int, endpoint_t);
static int input_close(devminor_t);
static ssize_t input_read(devminor_t, u64_t, endpoint_t, cp_grant_id_t, size_t,
	int, cdev_id_t);
static int input_ioctl(devminor_t, unsigned long, endpoint_t, cp_grant_id_t,
	int, endpoint_t, cdev_id_t);
static int input_cancel(devminor_t, endpoint_t, cdev_id_t);
static int input_select(devminor_t, unsigned int, endpoint_t);
static void input_other(message *, int);

static struct input_dev devs[INPUT_DEV_MAX];

#define input_dev_active(dev)		((dev)->owner != NONE || \
					 (dev)->minor == KBDMUX_MINOR || \
					 (dev)->minor == MOUSEMUX_MINOR)
#define input_dev_buf_empty(dev)	((dev)->count == 0)
#define input_dev_buf_full(dev)		((dev)->count == EVENTBUF_SIZE)

/* Entry points to the input driver. */
static struct chardriver input_tab = {
	.cdr_open	= input_open,
	.cdr_close	= input_close,
	.cdr_read	= input_read,
	.cdr_ioctl	= input_ioctl,
	.cdr_cancel	= input_cancel,
	.cdr_select	= input_select,
	.cdr_other	= input_other
};

/*
 * Map a minor number to an input device structure.
 */
static struct input_dev *
input_map(devminor_t minor)
{
	/*
	 * The minor device numbers were chosen not to be equal to the array
	 * slots, so that more keyboards can be added without breaking backward
	 * compatibility later.
	 */
	if (minor == KBDMUX_MINOR)
		return &devs[KBDMUX_DEV];
	else if (minor >= KBD0_MINOR && minor < KBD0_MINOR + KBD_MINORS)
		return &devs[FIRST_KBD_DEV + (minor - KBD0_MINOR)];
	else if (minor == MOUSEMUX_MINOR)
		return &devs[MOUSEMUX_DEV];
	else if (minor >= MOUSE0_MINOR && minor < MOUSE0_MINOR + MOUSE_MINORS)
		return &devs[FIRST_MOUSE_DEV + (minor - MOUSE0_MINOR)];
	else
		return NULL;
}

/*
 * Map an input device structure index to a minor number.
 */
static devminor_t
input_revmap(int id)
{
	if (id == KBDMUX_DEV)
		return KBDMUX_MINOR;
	else if (id >= FIRST_KBD_DEV && id <= LAST_KBD_DEV)
		return KBD0_MINOR + (id - FIRST_KBD_DEV);
	else if (id == MOUSEMUX_DEV)
		return MOUSEMUX_MINOR;
	else if (id >= FIRST_MOUSE_DEV && id <= LAST_MOUSE_DEV)
		return MOUSE0_MINOR + (id - FIRST_MOUSE_DEV);
	else
		panic("reverse-mapping invalid ID %d", id);
}

/*
 * Open an input device.
 */
static int
input_open(devminor_t minor, int UNUSED(access), endpoint_t UNUSED(user_endpt))
{
	struct input_dev *input_dev;

	if ((input_dev = input_map(minor)) == NULL)
		return ENXIO;

	if (!input_dev_active(input_dev))
		return ENXIO;

	if (input_dev->opened)
		return EBUSY;

	input_dev->opened = TRUE;

	return OK;
}

/*
 * Close an input device.
 */
static int
input_close(devminor_t minor)
{
	struct input_dev *input_dev;

	if ((input_dev = input_map(minor)) == NULL)
		return ENXIO;

	if (!input_dev->opened) {
		printf("INPUT: closing already-closed device %d\n", minor);
		return EINVAL;
	}

	input_dev->opened = FALSE;
	input_dev->tail = 0;
	input_dev->count = 0;

	return OK;
}

/*
 * Copy input events to a reader.
 */
static ssize_t
input_copy_events(endpoint_t endpt, cp_grant_id_t grant,
	unsigned int event_count, struct input_dev *input_dev)
{
	int r, nbytes, wrap_left;
	size_t event_size = sizeof(*input_dev->eventbuf);

	if (input_dev->count < event_count)
		panic("input_copy_events: not enough input is ready");

	wrap_left = input_dev->tail + event_count - EVENTBUF_SIZE;
	nbytes = (wrap_left <= 0 ? event_count :
	    EVENTBUF_SIZE - input_dev->tail) * event_size;

	if ((r = sys_safecopyto(endpt, grant, 0,
	    (vir_bytes)(input_dev->eventbuf + input_dev->tail), nbytes)) != OK)
		return r;

	/* Copy possible remaining part if we wrap over. */
	if (wrap_left > 0 && (r = sys_safecopyto(endpt, grant, nbytes,
	    (vir_bytes) input_dev->eventbuf, wrap_left * event_size)) != OK)
		return r;

	input_dev->tail = (input_dev->tail + event_count) % EVENTBUF_SIZE;
	input_dev->count -= event_count;

	return event_size * event_count; /* bytes copied */
}

/*
 * Read from an input device.
 */
static ssize_t
input_read(devminor_t minor, u64_t UNUSED(position), endpoint_t endpt,
	cp_grant_id_t grant, size_t size, int flags, cdev_id_t id)
{
	unsigned int event_count;
	struct input_dev *input_dev;

	if ((input_dev = input_map(minor)) == NULL)
		return ENXIO;

	/* We cannot accept more than one pending read request at once. */
	if (!input_dev_active(input_dev) || input_dev->suspended)
		return EIO;

	/* The caller's buffer must have room for at least one whole event. */
	event_count = size / sizeof(*input_dev->eventbuf);
	if (event_count == 0)
		return EIO;

	/* No data available? Suspend the caller, unless we shouldn't block. */
	if (input_dev_buf_empty(input_dev)) {
		if (flags & CDEV_NONBLOCK)
			return EAGAIN;

		input_dev->suspended = TRUE;
		input_dev->caller = endpt;
		input_dev->grant = grant;
		input_dev->req_id = id;

		/* We should now wake up any selector, but that's lame.. */
		return EDONTREPLY;
	}

	if (event_count > input_dev->count)
		event_count = input_dev->count;

	return input_copy_events(endpt, grant, event_count, input_dev);
}

/*
 * Set keyboard LEDs on one or all keyboards.
 */
static void
input_set_leds(devminor_t minor, unsigned int mask)
{
	struct input_dev *dev;
	message m;
	int i, r;

	/* Prepare the request message */
	memset(&m, 0, sizeof(m));

	m.m_type = INPUT_SETLEDS;
	m.m_input_linputdriver_setleds.led_mask = mask;

	/*
	 * Send the request to all matching keyboard devices.  As side effect,
	 * this approach discards the request on mouse devices.
	 */
	for (i = FIRST_KBD_DEV; i <= LAST_KBD_DEV; i++) {
		dev = &devs[i];

		if (minor != KBDMUX_MINOR && minor != dev->minor)
			continue;

		/* Save the new state; the driver might (re)start later. */
		dev->leds = mask;

		if (dev->owner != NONE) {
			if ((r = asynsend3(dev->owner, &m, AMF_NOREPLY)) != OK)
				printf("INPUT: asynsend to %u failed (%d)\n",
				    dev->owner, r);
		}
	}
}

/*
 * Process an IOCTL request.
 */
static int
input_ioctl(devminor_t minor, unsigned long request, endpoint_t endpt,
	cp_grant_id_t grant, int flags, endpoint_t user_endpt, cdev_id_t id)
{
	struct input_dev *input_dev;
	kio_leds_t leds;
	unsigned int mask;
	int r;

	if ((input_dev = input_map(minor)) == NULL)
		return ENXIO;

	if (!input_dev_active(input_dev))
		return EIO;

	switch (request) {
	case KIOCSLEDS:
		if ((r = sys_safecopyfrom(endpt, grant, 0, (vir_bytes) &leds,
		    sizeof(leds))) != OK)
			return r;

		mask = 0;
		if (leds.kl_bits & KBD_LEDS_NUM)
			mask |= (1 << INPUT_LED_NUMLOCK);
		if (leds.kl_bits & KBD_LEDS_CAPS)
			mask |= (1 << INPUT_LED_CAPSLOCK);
		if (leds.kl_bits & KBD_LEDS_SCROLL)
			mask |= (1 << INPUT_LED_SCROLLLOCK);

		input_set_leds(minor, mask);

		return OK;

	default:
		return ENOTTY;
	}
}

/*
 * Cancel a suspended read request.
 */
static int
input_cancel(devminor_t minor, endpoint_t endpt, cdev_id_t id)
{
	struct input_dev *input_dev;

	if ((input_dev = input_map(minor)) == NULL)
		return ENXIO;

	if (input_dev->suspended && input_dev->caller == endpt &&
	    input_dev->req_id == id) {
		input_dev->suspended = FALSE;

		return EINTR;
	}

	return EDONTREPLY;
}

/*
 * Perform a select call on an input device.
 */
static int
input_select(devminor_t minor, unsigned int ops, endpoint_t endpt)
{
	struct input_dev *input_dev;
	int ready_ops;

	if ((input_dev = input_map(minor)) == NULL)
		return ENXIO;

	ready_ops = 0;

	if (ops & CDEV_OP_RD) {
		if (!input_dev_active(input_dev) || input_dev->suspended)
			ready_ops |= CDEV_OP_RD;	/* immediate error */
		else if (!input_dev_buf_empty(input_dev))
			ready_ops |= CDEV_OP_RD;	/* data available */
		else if (ops & CDEV_NOTIFY)
			input_dev->selector = endpt;	/* report later */
	}

	if (ops & CDEV_OP_WR) ready_ops |= CDEV_OP_WR;	/* immediate error */

	return ready_ops;
}

/*
 * An input device receives an input event.  Enqueue it, and possibly unsuspend
 * a read request or wake up a selector.
 */
static void
input_process(struct input_dev *input_dev, const message *m)
{
	unsigned int next;
	int r;

	if (input_dev_buf_full(input_dev)) {
		/* Overflow.  Overwrite the oldest event. */
		input_dev->tail = (input_dev->tail + 1) % EVENTBUF_SIZE;
		input_dev->count--;

#if INPUT_DEBUG
		printf("INPUT: overflow on device %u\n", input_dev - devs);
#endif
	}
	next = (input_dev->tail + input_dev->count) % EVENTBUF_SIZE;
	input_dev->eventbuf[next].page = m->m_linputdriver_input_event.page;
	input_dev->eventbuf[next].code = m->m_linputdriver_input_event.code;
	input_dev->eventbuf[next].value = m->m_linputdriver_input_event.value;
	input_dev->eventbuf[next].flags = m->m_linputdriver_input_event.flags;
	input_dev->eventbuf[next].devid = m->m_linputdriver_input_event.id;
	input_dev->eventbuf[next].rsvd[0] = 0;
	input_dev->eventbuf[next].rsvd[1] = 0;
	input_dev->count++;

	/*
	 * There is new input.  Revive a suspended reader if there was one.
	 * Otherwise see if we should reply to a select query.
	 */
	if (input_dev->suspended) {
		r = input_copy_events(input_dev->caller, input_dev->grant, 1,
		    input_dev);
		chardriver_reply_task(input_dev->caller, input_dev->req_id, r);
		input_dev->suspended = FALSE;
	} else if (input_dev->selector != NONE) {
		chardriver_reply_select(input_dev->selector, input_dev->minor,
		    CDEV_OP_RD);
		input_dev->selector = NONE;
	}
}

/*
 * An input event has arrived from a driver.
 */
static void
input_event(message *m)
{
	struct input_dev *input_dev, *mux_dev;
	int r, id;

	/* Unlike minor numbers, device IDs are in fact array indices. */
	id = m->m_linputdriver_input_event.id;
	if (id < 0 || id >= INPUT_DEV_MAX)
		return;

	/* The sender must owner the device. */
	input_dev = &devs[id];
	if (input_dev->owner != m->m_source)
		return;

	/* Input events are also delivered to the respective multiplexer. */
	if (input_dev->minor >= KBD0_MINOR &&
	    input_dev->minor < KBD0_MINOR + KBD_MINORS)
		mux_dev = &devs[KBDMUX_DEV];
	else
		mux_dev = &devs[MOUSEMUX_DEV];

	/*
	 * Try to deliver the event to the input device or otherwise the
	 * corresponding multiplexer.  If neither are opened, forward the event
	 * to TTY.
	 */
	if (input_dev->opened)
		input_process(input_dev, m);
	else if (mux_dev->opened)
		input_process(mux_dev, m);
	else {
		message fwd;
		mess_input_tty_event *tty_event = &(fwd.m_input_tty_event);

		fwd.m_type = TTY_INPUT_EVENT;
		tty_event->id = m->m_linputdriver_input_event.id;
		tty_event->page = m->m_linputdriver_input_event.page;
		tty_event->code = m->m_linputdriver_input_event.code;
		tty_event->value = m->m_linputdriver_input_event.value;
		tty_event->flags = m->m_linputdriver_input_event.flags;

		if ((r = ipc_send(TTY_PROC_NR, &fwd)) != OK)
			printf("INPUT: send to TTY failed (%d)\n", r);
	}
}

/*
 * Allocate a device structure for an input driver of the given type, and
 * return its ID.  If the given label already owns a device ID of the right
 * type, update that entry instead.  If no device ID could be allocated, return
 * INVALID_INPUT_ID.
 */
static int
input_alloc_id(int mouse, endpoint_t owner, const char *label)
{
	int n, id, start, end;

	if (!mouse) {
		start = FIRST_KBD_DEV;
		end = LAST_KBD_DEV;
	} else {
		start = FIRST_MOUSE_DEV;
		end = LAST_MOUSE_DEV;
	}

	id = INVALID_INPUT_ID;
	for (n = start; n <= end; n++) {
		if (devs[n].owner != NONE) {
			if (!strcmp(devs[n].label, label)) {
				devs[n].owner = owner;
				return n;
			}
		/* Do not allocate the ID of a disconnected but open device. */
		} else if (!devs[n].opened && id == INVALID_INPUT_ID) {
			id = n;
		}
	}

	if (id != INVALID_INPUT_ID) {
		devs[id].owner = owner;
		strlcpy(devs[id].label, label, sizeof(devs[id].label));

#if INPUT_DEBUG
		printf("INPUT: connected device %u to %u (%s)\n", id,
		    owner, label);
#endif
	} else {
		printf("INPUT: out of %s slots for new driver %d\n",
		    mouse ? "mouse" : "keyboard", owner);
	}

	return id;
}

/*
 * Register keyboard and/or a mouse devices for a driver.
 */
static void
input_connect(endpoint_t owner, char *labelp, int typemask)
{
	message m;
	char label[DS_MAX_KEYLEN];
	int r, kbd_id, mouse_id;

#if INPUT_DEBUG
	printf("INPUT: connect request from %u (%s) for mask %x\n", owner,
	    labelp, typemask);
#endif

	/* Check the driver's label. */
	if ((r = ds_retrieve_label_name(label, owner)) != OK) {
		printf("INPUT: unable to get label for %u: %d\n", owner, r);
		return;
	}
	if (strcmp(label, labelp)) {
		printf("INPUT: ignoring driver %s label %s\n", label, labelp);
		return;
	}

	kbd_id = INVALID_INPUT_ID;
	mouse_id = INVALID_INPUT_ID;

	/*
	 * We ignore allocation failures here, thus possibly sending invalid
	 * IDs to the driver even for either or both the devices types it
	 * requested.  As a result, the driver will not send us input for these
	 * device types, possibly effectively disabling the driver altogether.
	 * Theoretically we could still admit events to the multiplexers for
	 * such drivers, but that would lead to unexpected behavior with
	 * respect to keyboard LEDs, for example.
	 */
	if (typemask & INPUT_DEV_KBD)
		kbd_id = input_alloc_id(FALSE /*mouse*/, owner, label);
	if (typemask & INPUT_DEV_MOUSE)
		mouse_id = input_alloc_id(TRUE /*mouse*/, owner, label);

	memset(&m, 0, sizeof(m));

	m.m_type = INPUT_CONF;
	m.m_input_linputdriver_input_conf.kbd_id = kbd_id;
	m.m_input_linputdriver_input_conf.mouse_id = mouse_id;
	m.m_input_linputdriver_input_conf.rsvd1_id = INVALID_INPUT_ID;	/* reserved (joystick?) */
	m.m_input_linputdriver_input_conf.rsvd2_id = INVALID_INPUT_ID;	/* reserved for future use */

	if ((r = asynsend3(owner, &m, AMF_NOREPLY)) != OK)
		printf("INPUT: asynsend to %u failed (%d)\n", owner, r);

	/* If a keyboard was registered, also set its initial LED state. */
	if (kbd_id != INVALID_INPUT_ID)
		input_set_leds(devs[kbd_id].minor, devs[kbd_id].leds);
}

/*
 * Disconnect a device.
 */
static void
input_disconnect(struct input_dev *input_dev)
{
#if INPUT_DEBUG
	printf("INPUT: disconnected device %u\n", input_dev - devs);
#endif

	if (input_dev->suspended) {
		chardriver_reply_task(input_dev->caller, input_dev->req_id,
		    EIO);
		input_dev->suspended = FALSE;
	}

	if (input_dev->selector != NONE) {
		chardriver_reply_select(input_dev->selector, input_dev->minor,
		    CDEV_OP_RD);
		input_dev->selector = NONE;
	}

	input_dev->owner = NONE;
}

/*
 * Check for driver status changes in the data store.
 */
static void
input_check(void)
{
	char key[DS_MAX_KEYLEN], *label;
	const char *driver_prefix = "drv.inp.";
	u32_t value;
	size_t len;
	int i, r, type;
	endpoint_t owner;

	len = strlen(driver_prefix);

	/* Check for new (input driver) entries. */
	while (ds_check(key, &type, &owner) == OK) {
		if ((r = ds_retrieve_u32(key, &value)) != OK) {
			printf("INPUT: ds_retrieve_u32 failed (%d)\n", r);
			continue;
		}

		/* Only check for input driver registration events. */
		if (strncmp(key, driver_prefix, len))
			continue;

		/* The prefix is followed by the driver's own label. */
		label = &key[len];

		input_connect(owner, label, value);
	}

	/* Check for removed (label) entries. */
	for (i = 0; i < INPUT_DEV_MAX; i++) {
		/* This also skips the multiplexers. */
		if (devs[i].owner == NONE)
			continue;

		r = ds_retrieve_label_endpt(devs[i].label, &owner);

		if (r == OK)
			devs[i].owner = owner;	/* not really necessary */
		else if (r == ESRCH)
			input_disconnect(&devs[i]);
		else
			printf("INPUT: ds_retrieve_label_endpt failed (%d)\n",
			    r);
	}
}

/*
 * Process messages not part of the character driver protocol.
 */
static void
input_other(message *m, int ipc_status)
{
	if (is_ipc_notify(ipc_status)) {
		switch (m->m_source) {
		case DS_PROC_NR:
			input_check();
			break;
		default:
			printf("INPUT: unexpected notify from %d\n",
			    m->m_source);
		}
		return;
	}

	/* An input event from a registered driver. */
	switch (m->m_type) {
	case INPUT_EVENT:
		input_event(m);

		break;

	case INPUT_SETLEDS:
		if (m->m_source == TTY_PROC_NR) {
			input_set_leds(KBDMUX_MINOR, m->m_input_linputdriver_setleds.led_mask);

			break;
		}
		/* FALLTHROUGH */
	default:
		printf("INPUT: unexpected message %d from %d\n",
		    m->m_type, m->m_source);
	}
}

/*
 * Initialize the input server.
 */
static int
input_init(int UNUSED(type), sef_init_info_t *UNUSED(info))
{
	message m;
	int i, r;

	/* Initialize input device structures. */
	for (i = 0; i < INPUT_DEV_MAX; i++) {
		devs[i].minor = input_revmap(i);
		devs[i].owner = NONE;
		devs[i].tail = 0;
		devs[i].count = 0;
		devs[i].opened = FALSE;
		devs[i].suspended = FALSE;
		devs[i].selector = NONE;
		devs[i].leds = 0;
	}

	/* Subscribe to driver registration events for input drivers. */
	if ((r = ds_subscribe("drv\\.inp\\..*", DSF_INITIAL)) != OK)
		panic("INPUT: can't subscribe to driver events (%d)", r);

	/* Announce our presence to VFS. */
	chardriver_announce();

	/* Announce our presence to TTY. */
	memset(&m, 0, sizeof(m));

	m.m_type = TTY_INPUT_UP;

	if ((r = ipc_send(TTY_PROC_NR, &m)) != OK)
		printf("INPUT: send to TTY failed (%d)\n", r);

	return OK;
}

/*
 * Set callbacks and invoke SEF startup.
 */
static void
input_startup(void)
{
	sef_setcb_init_fresh(input_init);

	sef_startup();
}

/*
 * Main program of the input server.
 */
int
main(void)
{
	input_startup();

	chardriver_task(&input_tab);

	return 0;
}
