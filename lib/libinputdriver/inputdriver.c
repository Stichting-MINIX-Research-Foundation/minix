/* This file contains the device independent input driver interface. */
/*
 * Changes:
 *   Sep 22, 2013   created  (D.C. van Moolenbroek)
 */

#include <minix/drivers.h>
#include <minix/inputdriver.h>
#include <minix/ds.h>

static endpoint_t input_endpt = NONE;
static int kbd_id = INVALID_INPUT_ID;
static int mouse_id = INVALID_INPUT_ID;

static int running;

/*
 * Announce that we are up after a fresh start or restart.
 */
void
inputdriver_announce(unsigned int type)
{
	const char *driver_prefix = "drv.inp.";
	char key[DS_MAX_KEYLEN];
	char label[DS_MAX_KEYLEN];
	int r;

	/* Publish a driver up event. */
	if ((r = ds_retrieve_label_name(label, sef_self())) != OK)
		panic("libinputdriver: unable to retrieve own label: %d", r);

	snprintf(key, sizeof(key), "%s%s", driver_prefix, label);
	if ((r = ds_publish_u32(key, type, DSF_OVERWRITE)) != OK)
		panic("libinputdriver: unable to publish up event: %d", r);

	/* Now we wait for the input server to contact us. */
}

/*
 * Send an event to the input server.
 */
void
inputdriver_send_event(int mouse, unsigned short page, unsigned short code,
	int value, int flags)
{
	message m;
	int id;

	if (input_endpt == NONE)
		return;

	id = mouse ? mouse_id : kbd_id;
	if (id == INVALID_INPUT_ID)
		return;

	memset(&m, 0, sizeof(m));

	m.m_type = INPUT_EVENT;
	m.m_linputdriver_input_event.id = id;
	m.m_linputdriver_input_event.page = page;
	m.m_linputdriver_input_event.code = code;
	m.m_linputdriver_input_event.value = value;
	m.m_linputdriver_input_event.flags = flags;

	/*
	 * Use a blocking send call, for two reasons.  First, this avoids the
	 * situation that we ever end up queuing too many asynchronous messages
	 * to the input server.  Second, it allows us to detect trivially if
	 * the input server has crashed, in which case we should stop sending
	 * more messages to it.
	 */
	if (ipc_send(input_endpt, &m) != OK)
		input_endpt = NONE;
}

/*
 * The input server requests that we configure the driver.  This request should
 * be sent to us once, although it may be sent multiple times if the input
 * server crashes and recovers.  The configuration consists of device IDs for
 * use in keyboard and/or mouse events, one per each device type.
 */
static void
do_conf(message *m_ptr)
{
	endpoint_t ep;
	int r;

	/* Make sure that the sender is actually the input server. */
	if ((r = ds_retrieve_label_endpt("input", &ep)) != OK) {
		printf("libinputdriver: unable to get input endpoint (%d)\n",
		    r);

		return; /* ignore message */
	}

	if (ep != m_ptr->m_source) {
		printf("libinputdriver: ignoring CONF request from %u\n",
		    m_ptr->m_source);

		return;
	}

	/* Save the new state. */
	input_endpt = m_ptr->m_source;
	kbd_id = m_ptr->m_input_linputdriver_input_conf.kbd_id;
	mouse_id = m_ptr->m_input_linputdriver_input_conf.mouse_id;

	/* If the input server is "full" there's nothing for us to do. */
	if (kbd_id == INVALID_INPUT_ID && mouse_id == INVALID_INPUT_ID)
		printf("libinputdriver: no IDs given, driver disabled\n");
}

/*
 * The input server is telling us to change the LEDs to a particular mask.
 * For now this is for keyboards only, so no device type is provided.
 * This approach was chosen over sending toggle events for the individual LEDs
 * for convenience reasons only.
 */
static void
do_setleds(struct inputdriver *idp, message *m_ptr)
{
	unsigned int mask;

	if (m_ptr->m_source != input_endpt) {
		printf("libinputdriver: ignoring SETLEDS request from %u\n",
			m_ptr->m_source);

		return;
	}

	mask = m_ptr->m_input_linputdriver_setleds.led_mask;

	if (idp->idr_leds)
		idp->idr_leds(mask);
}

/*
 * Call the appropriate driver function, based on the type of message.
 * All messages in the input protocol are one-way, so we never send a reply.
 */
void
inputdriver_process(struct inputdriver *idp, message *m_ptr, int ipc_status)
{
	/* Check for notifications first. */
	if (is_ipc_notify(ipc_status)) {
		switch (_ENDPOINT_P(m_ptr->m_source)) {
		case HARDWARE:
			if (idp->idr_intr)
				idp->idr_intr(m_ptr->m_notify.interrupts);
			break;

		case CLOCK:
			if (idp->idr_alarm)
				idp->idr_alarm(m_ptr->m_notify.timestamp);
			break;

		default:
			if (idp->idr_other)
				idp->idr_other(m_ptr, ipc_status);
		}

		return;
	}

	switch (m_ptr->m_type) {
	case INPUT_CONF:		do_conf(m_ptr);		break;
	case INPUT_SETLEDS:		do_setleds(idp, m_ptr);	break;
	default:
		if (idp->idr_other)
			idp->idr_other(m_ptr, ipc_status);
	}
}

/*
 * Break out of the main loop after finishing the current request.
 */
void
inputdriver_terminate(void)
{
	running = FALSE;

	sef_cancel();
}

/*
 * Main program of any input driver task.
 */
void
inputdriver_task(struct inputdriver *idp)
{
	message m;
	int r, ipc_status;

	running = TRUE;

	while (running) {
		if ((r = sef_receive_status(ANY, &m, &ipc_status)) != OK) {
			if (r == EINTR && !running)
				break;

			panic("libinputdriver: receive failed: %d", r);
		}

		inputdriver_process(idp, &m, ipc_status);
	}
}
