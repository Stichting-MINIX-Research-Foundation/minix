/* VirtualBox driver - by D.C. van Moolenbroek */
/*
 * This driver currently performs two tasks:
 * - synchronizing to the host system time;
 * - providing an interface for HGCM communication with the host system.
 */
#include <minix/drivers.h>
#include <minix/driver.h>
#include <minix/optset.h>
#include <machine/pci.h>
#include <sys/time.h>

#include "vmmdev.h"
#include "proto.h"

#define DEFAULT_INTERVAL	1	/* check host time every second */
#define DEFAULT_DRIFT		2	/* update time if delta is >= 2 secs */

static void *vir_ptr;
static phys_bytes phys_ptr;
static port_t port;
static u32_t ticks;
static int interval;
static int drift;

static unsigned int irq;
static int hook_id;

static struct optset optset_table[] = {
	{ "interval",	OPT_INT,	&interval, 	10		},
	{ "drift",	OPT_INT,	&drift,		10		},
	{ NULL,		0,		NULL,		0		}
};

/*===========================================================================*
 *				vbox_request				     *
 *===========================================================================*/
int vbox_request(struct VMMDevRequestHeader *header, phys_bytes addr,
	int type, size_t size)
{
	/* Perform a VirtualBox backdoor request. */
	int r;

	header->size = size;
	header->version = VMMDEV_BACKDOOR_VERSION;
	header->type = type;
	header->result = VMMDEV_ERR_GENERIC;

	if ((r = sys_outl(port, addr)) != OK)
		panic("device I/O failed: %d", r);

	return header->result;
}

/*===========================================================================*
 *				vbox_init				     *
 *===========================================================================*/
static int vbox_init(int UNUSED(type), sef_init_info_t *UNUSED(info))
{
	/* Initialize the device. */
	int devind;
	u16_t vid, did;
	struct VMMDevReportGuestInfo *req;
	int r;

	interval = DEFAULT_INTERVAL;
	drift = DEFAULT_DRIFT;

	if (env_argc > 1)
		optset_parse(optset_table, env_argv[1]);

	pci_init();

	r = pci_first_dev(&devind, &vid, &did);

	for (;;) {
		if (r != 1)
			panic("backdoor device not found");

		if (vid == VMMDEV_PCI_VID && did == VMMDEV_PCI_DID)
			break;

		r = pci_next_dev(&devind, &vid, &did);
	}

	pci_reserve(devind);

	port = pci_attr_r32(devind, PCI_BAR) & PCI_BAR_IO_MASK;

	irq = pci_attr_r8(devind, PCI_ILR);
	hook_id = 0;

	if ((r = sys_irqsetpolicy(irq, 0 /* IRQ_REENABLE */, &hook_id)) != OK)
		panic("unable to register IRQ: %d", r);

	if ((r = sys_irqenable(&hook_id)) != OK)
		panic("unable to enable IRQ: %d", r);

	if ((vir_ptr = alloc_contig(VMMDEV_BUF_SIZE, 0, &phys_ptr)) == NULL)
		panic("unable to allocate memory");

	req = (struct VMMDevReportGuestInfo *) vir_ptr;
	req->add_version = VMMDEV_GUEST_VERSION;
	req->os_type = VMMDEV_GUEST_OS_OTHER;

	if ((r = vbox_request(&req->header, phys_ptr,
			VMMDEV_REQ_REPORTGUESTINFO, sizeof(*req))) !=
			VMMDEV_ERR_OK)
		panic("backdoor device not functioning");

	ticks = sys_hz() * interval;

	sys_setalarm(ticks, 0);

	return OK;
}

/*===========================================================================*
 *				vbox_intr				     *
 *===========================================================================*/
static void vbox_intr(void)
{
	/* Process an interrupt. */
	struct VMMDevEvents *req;
	int r;

	req = (struct VMMDevEvents *) vir_ptr;
	req->events = 0;

	/* If we cannot retrieve the events mask, we cannot do anything with
	 * this or any future interrupt either, so return without reenabling
	 * interrupts.
	 */
	if ((r = vbox_request(&req->header, phys_ptr,
			VMMDEV_REQ_ACKNOWLEDGEEVENTS, sizeof(*req))) !=
			VMMDEV_ERR_OK) {
		printf("VBOX: unable to retrieve event mask (%d)\n", r);

		return;
	}

	if (req->events & VMMDEV_EVENT_HGCM)
		hgcm_intr();

	if ((r = sys_irqenable(&hook_id)) != OK)
		panic("unable to reenable IRQ: %d", r);
}

/*===========================================================================*
 *				vbox_update_time			     *
 *===========================================================================*/
static void vbox_update_time(void)
{
	/* Update the current time if it has drifted too far. */
	struct VMMDevReqHostTime *req;
	time_t otime, ntime;

	req = (struct VMMDevReqHostTime *) vir_ptr;

	if (vbox_request(&req->header, phys_ptr, VMMDEV_REQ_HOSTTIME,
			sizeof(*req)) == VMMDEV_ERR_OK) {
		time(&otime);				/* old time */

		ntime = div64u(req->time, 1000);	/* new time */

		/* Make time go forward, if the difference exceeds the drift
		 * threshold. Never make time go backward.
		 */
		if ((int) (ntime - otime) >= drift)
			stime(&ntime);
	}

	sys_setalarm(ticks, 0);
}

/*===========================================================================*
 *				vbox_signal				     *
 *===========================================================================*/
static void vbox_signal(int signo)
{
	/* Process a signal. If it is a SIGTERM, terminate immediately. */

	if (signo != SIGTERM) return;

	exit(0);
}

/*===========================================================================*
 *				sef_local_startup			     *
 *===========================================================================*/
static void sef_local_startup(void)
{
	/* Perform local SEF initialization. */

	sef_setcb_init_fresh(vbox_init);
	sef_setcb_init_restart(vbox_init);

	sef_setcb_signal_handler(vbox_signal);

	sef_startup();
}

/*===========================================================================*
 *				main					     *
 *===========================================================================*/
int main(int argc, char **argv)
{
	/* The main message loop. */
	message m;
	int r, ipc_status;

	env_setargs(argc, argv);
	sef_local_startup();

	while (TRUE) {
		if ((r = driver_receive(ANY, &m, &ipc_status)) != OK)
			panic("driver_receive failed: %d", r);

		if (is_ipc_notify(ipc_status)) {
			switch (m.m_source) {
			case HARDWARE:
				vbox_intr();

				break;

			case CLOCK:
				vbox_update_time();

				break;

			default:
				printf("VBOX: received notify from %d\n",
					m.m_source);
			}

			continue;
		}

		hgcm_message(&m, ipc_status);
	}

	return 0;
}
