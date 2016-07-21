/* Keyboard driver for PCs and ATs. */
#include <minix/drivers.h>
#include <minix/input.h>
#include <minix/inputdriver.h>

#include "pckbd.h"

/*
 * Data that is to be sent to the keyboard. Each byte is ACKed by the keyboard.
 * This is currently somewhat overpowered for its only purpose: setting LEDs.
 */
static struct kbdout {
	unsigned char buf[KBD_OUT_BUFSZ];
	int offset;
	int avail;
	int expect_ack;
} kbdout;

static int kbd_watchdog_set = 0;
static int kbd_alive = 1;
static minix_timer_t tmr_kbd_wd;

static int irq_hook_id = -1;
static int aux_irq_hook_id = -1;

static int kbd_state = 0;

static unsigned char aux_bytes[3];
static unsigned char aux_state = 0;
static int aux_counter = 0;
static int aux_available = 0;

static void pckbd_leds(unsigned int);
static void pckbd_intr(unsigned int);
static void pckbd_alarm(clock_t);

static struct inputdriver pckbd_tab = {
	.idr_leds	= pckbd_leds,
	.idr_intr	= pckbd_intr,
	.idr_alarm	= pckbd_alarm
};

/*
 * The watchdog timer function, implementing all but the actual reset.
 */
static void
kbd_watchdog(int arg __unused)
{
	kbd_watchdog_set = 0;
	if (!kbdout.avail)
		return;	/* Watchdog is no longer needed */

	if (!kbd_alive)
		printf("PCKBD: watchdog should reset keyboard\n");
	kbd_alive = 0;

	set_timer(&tmr_kbd_wd, sys_hz(), kbd_watchdog, 0);

	kbd_watchdog_set = 1;
}

/*
 * Send queued data to the keyboard.
 */
static void
kbd_send(void)
{
	u32_t sb;
	int r;

	if (!kbdout.avail)
		return;
	if (kbdout.expect_ack)
		return;

	if ((r = sys_inb(KB_STATUS, &sb)) != OK)
		printf("PCKBD: send sys_inb() failed (1): %d\n", r);

	if (sb & (KB_OUT_FULL | KB_IN_FULL)) {
		printf("PCKBD: not sending (1): sb = 0x%x\n", sb);
		return;
	}
	micro_delay(KBC_IN_DELAY);
	if ((r = sys_inb(KB_STATUS, &sb)) != OK)
		printf("PCKBD: send sys_inb() failed (2): %d\n", r);
	if (sb & (KB_OUT_FULL | KB_IN_FULL)) {
		printf("PCKBD: not sending (2): sb = 0x%x\n", sb);
		return;
	}

	/* Okay, buffer is really empty */
	if ((r = sys_outb(KEYBD, kbdout.buf[kbdout.offset])) != OK)
		printf("PCKBD: send sys_outb() failed: %d\n", r);
	kbdout.offset++;
	kbdout.avail--;
	kbdout.expect_ack = 1;

	kbd_alive = 1;
	if (kbd_watchdog_set) {
		/* Set a watchdog timer for one second. */
		set_timer(&tmr_kbd_wd, sys_hz(), kbd_watchdog, 0);

		kbd_watchdog_set = 1;
	}
}

/*
 * Try to obtain input from the keyboard.
 */
static int
scan_keyboard(unsigned char *bp, int *isauxp)
{
	u32_t b, sb;
	int r;

	if ((r = sys_inb(KB_STATUS, &sb)) != OK) {
		printf("PCKBD: scan sys_inb() failed (1): %d\n", r);
		return FALSE;
	}
	if (!(sb & KB_OUT_FULL)) {
		if (kbdout.avail && !kbdout.expect_ack)
			kbd_send();
		return FALSE;
	}
	if ((r = sys_inb(KEYBD, &b)) != OK) {
		printf("PCKBD: scan sys_inb() failed (2): %d\n", r);
		return FALSE;
	}
	if (!(sb & 0x40) && b == KB_ACK && kbdout.expect_ack) {
		kbdout.expect_ack = 0;
		micro_delay(KBC_IN_DELAY);
		kbd_send();
		return FALSE;
	}
	if (bp)
		*bp = b;
	if (isauxp)
		*isauxp = !!(sb & KB_AUX_BYTE);
	if (kbdout.avail && !kbdout.expect_ack) {
		micro_delay(KBC_IN_DELAY);
		kbd_send();
	}
	return TRUE;
}

/*
 * Wait until the controller is ready.  Return TRUE on success, FALSE on
 * timeout.  Since this may discard input, only use during initialization.
 */
static int
kb_wait(void)
{
	spin_t spin;
	u32_t status;
	int r, isaux;
	unsigned char byte;

	SPIN_FOR(&spin, KBC_WAIT_TIME) {
		if ((r = sys_inb(KB_STATUS, &status)) != OK)
			printf("PCKBD: wait sys_inb() failed: %d\n", r);
		if (status & KB_OUT_FULL)
			(void) scan_keyboard(&byte, &isaux);
		if (!(status & (KB_IN_FULL | KB_OUT_FULL)))
			return TRUE;		/* wait until ready */
	}

	printf("PCKBD: wait timeout\n");
	return FALSE;
}

/*
 * Set the LEDs on the caps, num, and scroll lock keys.
 */
static void
set_leds(unsigned char ledmask)
{
	if (kbdout.avail == 0)
		kbdout.offset = 0;
	if (kbdout.offset + kbdout.avail + 2 > KBD_OUT_BUFSZ) {
		/*
		 * The output buffer is full.  Ignore this command.  Reset the
		 * ACK flag.
		 */
		kbdout.expect_ack = 0;
	} else {
		kbdout.buf[kbdout.offset+kbdout.avail] = LED_CODE;
		kbdout.buf[kbdout.offset+kbdout.avail+1] = ledmask;
		kbdout.avail += 2;
	}
	if (!kbdout.expect_ack)
		kbd_send();
}

/*
 * Send a command to the keyboard.
 */
static void
kbc_cmd0(int cmd)
{
	int r;

	kb_wait();
	if ((r = sys_outb(KB_COMMAND, cmd)) != OK)
		printf("PCKBD: cmd0 sys_outb() failed: %d\n", r);
}

/*
 * Send a command to the keyboard, including data.
 */
static void
kbc_cmd1(int cmd, int data)
{
	int r;

	kb_wait();
	if ((r = sys_outb(KB_COMMAND, cmd)) != OK)
		printf("PCKBD: cmd1 sys_outb() failed (1): %d\n", r);
	kb_wait();
	if ((r = sys_outb(KEYBD, data)) != OK)
		printf("PCKBD: cmd1 sys_outb() failed (2): %d\n", r);
}

/*
 * Wait at most one second for a byte from the keyboard or the controller.
 */
static int
kbc_read(void)
{
	u32_t byte, status;
	spin_t spin;
	int r;

	SPIN_FOR(&spin, KBC_READ_TIME) {
		if ((r = sys_inb(KB_STATUS, &status)) != OK)
			printf("PCKBD: read sys_inb() failed (1): %d\n", r);
		if (status & KB_OUT_FULL) {
			micro_delay(KBC_IN_DELAY);
			if ((r = sys_inb(KEYBD, &byte)) != OK)
				printf("PCKBD: read sys_inb() failed (2): "
				    "%d\n", r);
			if (status & KB_AUX_BYTE)
				printf("PCKBD: read got aux 0x%x\n", byte);
			return byte;
		}
	}

	panic("kbc_read failed to complete");
}

/*
 * Initialize the keyboard hardware.
 */
static int
kb_init(void)
{
	int r, ccb;

	/* Disable the keyboard and AUX. */
	kbc_cmd0(KBC_DI_KBD);
	kbc_cmd0(KBC_DI_AUX);

	/* Discard leftover keystroke. */
	scan_keyboard(NULL, NULL);

	/* Get the current configuration byte. */
	kbc_cmd0(KBC_RD_RAM_CCB);
	ccb = kbc_read();

	/* If bit 5 is clear, it is a single channel controler for sure.. */
	aux_available = (ccb & 0x10);

	/* Execute Controller Self Test. */
	kbc_cmd0(0xAA);
	r = kbc_read();
	if (r != 0x55){
		printf("PCKBD: Controller self-test failed.\n");
		return EGENERIC;
	}

	/* Set interrupt handler and enable keyboard IRQ. */
	irq_hook_id = KEYBOARD_IRQ;	/* id to be returned on interrupt */
	r = sys_irqsetpolicy(KEYBOARD_IRQ, IRQ_REENABLE, &irq_hook_id);
	if (r != OK)
		panic("Couldn't set keyboard IRQ policy: %d", r);
	if ((r = sys_irqenable(&irq_hook_id)) != OK)
		panic("Couldn't enable keyboard IRQs: %d", r);

	/* Activate IRQ bit for the keyboard. */
	ccb |= 0x1;

	if (aux_available != 0) {
		/* Set AUX interrupt handler and enable AUX IRQ. */
		aux_irq_hook_id = KBD_AUX_IRQ;	/* id to be returned on interrupt */
		r = sys_irqsetpolicy(KBD_AUX_IRQ, IRQ_REENABLE, &aux_irq_hook_id);
		if (r != OK)
			panic("Couldn't set AUX IRQ policy: %d", r);
		if ((r = sys_irqenable(&aux_irq_hook_id)) != OK)
			panic("Couldn't enable AUX IRQs: %d", r);

		/* Activate IRQ for AUX. */
		ccb |= 0x2;
	}

	/* Enable interrupt(s). */
	kbc_cmd1(KBC_WR_RAM_CCB, ccb);

	/* Re-enable the keyboard device. */
	kbc_cmd0(KBC_EN_KBD);

	if (aux_available != 0) {
		/* Enable the AUX device. */
		kbc_cmd0(KBC_EN_AUX);
		kbc_cmd1(0xD4, 0xF6);
		kbc_cmd1(0xD4, 0xF4);
	}

	/* Set the initial LED state. */
	kb_wait();

	set_leds(0);
	return OK;
}

/*
 * Process a keyboard scancode.
 */
static void
kbd_process(unsigned char scode)
{
	int press, index, page, code;

	press = !(scode & SCAN_RELEASE) ? INPUT_PRESS : INPUT_RELEASE;
	index = scode & ~SCAN_RELEASE;

	switch (kbd_state) {
	case 1:
		page = scanmap_escaped[index].page;
		code = scanmap_escaped[index].code;
		break;
	case 2:
		kbd_state = (index == SCAN_CTRL) ? 3 : 0;
		return;
	case 3:
		if (index == SCAN_NUMLOCK) {
			page = INPUT_PAGE_KEY;
			code = INPUT_KEY_PAUSE;
			break;
		}
		/* FALLTHROUGH */
	default:
		switch (scode) {
		case SCAN_EXT0:
			kbd_state = 1;
			return;
		case SCAN_EXT1:
			kbd_state = 2;
			return;
		}
		page = scanmap_normal[index].page;
		code = scanmap_normal[index].code;
		break;
	}

	if (page)
		inputdriver_send_event(FALSE /*mouse*/, page, code, press, 0);

	kbd_state = 0;
}

/*
 * Process an auxiliary (mouse) scancode.
 */
static void
kbdaux_process(unsigned char scode)
{
	u32_t delta;
	int i;

	if (aux_counter == 0 && !(scode & 0x08))
		return;	/* resync */

	aux_bytes[aux_counter++] = scode;

	if (aux_counter < 3)
		return; /* need more first */

	aux_counter = 0;

	/* Send an event for each button state change. */
	for (i = 0; i < 3; i++) {
		if ((aux_state ^ aux_bytes[0]) & (1 << i)) {
			aux_state ^= (1 << i);

			inputdriver_send_event(TRUE /*mouse*/,
			    INPUT_PAGE_BUTTON, INPUT_BUTTON_1 + i,
			    !!(aux_state & (1 << i)), 0);
		}
	}

	/* Send an event for each relative mouse movement, X and/or Y. */
	for (i = 0; i < 2; i++) {
		delta = aux_bytes[1 + i];
		if (delta != 0) {
			if (aux_bytes[0] & (0x10 << i))
				delta |= 0xFFFFFF00; /* make signed */

			inputdriver_send_event(TRUE /*mouse*/, INPUT_PAGE_GD,
			    !i ? INPUT_GD_X : INPUT_GD_Y, delta,
			    INPUT_FLAG_REL);
		}
	}
}

/*
 * Set keyboard LEDs.
 */
static void
pckbd_leds(unsigned int leds)
{
	unsigned char b;

	b = 0;
	if (leds & (1 << INPUT_LED_NUMLOCK)) b |= LED_NUM_LOCK;
	if (leds & (1 << INPUT_LED_CAPSLOCK)) b |= LED_CAPS_LOCK;
	if (leds & (1 << INPUT_LED_SCROLLLOCK)) b |= LED_SCROLL_LOCK;

	set_leds(b);
}

/*
 * Process a keyboard interrupt.
 */
static void
pckbd_intr(unsigned int UNUSED(mask))
{
	unsigned char scode;
	int isaux;

	/* Fetch a character from the keyboard hardware and acknowledge it. */
	if (!scan_keyboard(&scode, &isaux))
		return;

	if (!isaux) {
		/* A keyboard key press or release. */
		kbd_process(scode);
	} else {
		/* A mouse event. */
		kbdaux_process(scode);
	}
}

/*
 * Process a timer signal.
 */
static void
pckbd_alarm(clock_t stamp)
{
	expire_timers(stamp);
}

/*
 * Initialize the driver.
 */
static int
pckbd_init(int UNUSED(type), sef_init_info_t *UNUSED(info))
{
	int flags = INPUT_DEV_KBD;
	/* Initialize the watchdog timer. */
	init_timer(&tmr_kbd_wd);

	/* Initialize the keyboard. */
	int r;
	if((r = kb_init())!=OK){
		return r;
	}

	/* Announce the driver's presence. */
	if (aux_available != 0)
		flags |= INPUT_DEV_MOUSE;
	inputdriver_announce(flags);

	return OK;
}

/*
 * Set callback routines and let SEF initialize.
 */
static void
pckbd_startup(void)
{
	sef_setcb_init_fresh(pckbd_init);

	sef_startup();
}

/*
 * PC keyboard/mouse driver task.
 */
int
main(void)
{
	pckbd_startup();

	inputdriver_task(&pckbd_tab);

	return 0;
}
