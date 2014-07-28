#ifndef _MINIX_INPUTDRIVER_H
#define _MINIX_INPUTDRIVER_H

#include <minix/driver.h>
#include <minix/input.h>

/* Entry points into the device dependent code of input drivers. */
struct inputdriver {
	void (*idr_leds)(unsigned int leds);
	void (*idr_intr)(unsigned int mask);
	void (*idr_alarm)(clock_t stamp);
	void (*idr_other)(message *m_ptr, int ipc_status);
};

/* Functions defined by libinputdriver. */
void inputdriver_announce(unsigned int type);
void inputdriver_send_event(int mouse, unsigned short page,
	unsigned short code, int value, int flags);
void inputdriver_process(struct inputdriver *idp, message *m_ptr,
	int ipc_status);
void inputdriver_terminate(void);
void inputdriver_task(struct inputdriver *idp);

#endif /* !_MINIX_INPUTDRIVER_H */
