/* VirtualBox driver - only does regular time sync - by D.C. van Moolenbroek */
#include <minix/drivers.h>
#include <minix/driver.h>
#include <minix/optset.h>
#include <machine/pci.h>
#include <sys/time.h>

#include "vbox.h"

#define DEFAULT_INTERVAL	1	/* check host time every second */
#define DEFAULT_DRIFT		2	/* update time if delta is >= 2 secs */

PRIVATE void *vir_ptr;
PRIVATE phys_bytes phys_ptr;
PRIVATE port_t port;
PRIVATE u32_t ticks;
PRIVATE int interval;
PRIVATE int drift;

PRIVATE struct optset optset_table[] = {
	{ "interval",	OPT_INT,	&interval, 	10		},
	{ "drift",	OPT_INT,	&drift,		10		},
	{ NULL,		0,		NULL,		0		}
};

/*===========================================================================*
 *				vbox_request				     *
 *===========================================================================*/
PRIVATE int vbox_request(int req_nr, size_t size)
{
	/* Perform a VirtualBox backdoor request. */
	struct VMMDevRequestHeader *hdr;
	int r;

	hdr = (struct VMMDevRequestHeader *) vir_ptr;
	hdr->size = size;
	hdr->version = VMMDEV_BACKDOOR_VERSION;
	hdr->type = req_nr;
	hdr->rc = VMMDEV_ERR_PERM;

	if ((r = sys_outl(port, phys_ptr)) != OK)
		panic("device I/O failed: %d", r);

	return hdr->rc;
}

/*===========================================================================*
 *				vbox_init				     *
 *===========================================================================*/
PRIVATE int vbox_init(int UNUSED(type), sef_init_info_t *UNUSED(info))
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

		if (vid == VBOX_PCI_VID && did == VBOX_PCI_DID)
			break;

		r = pci_next_dev(&devind, &vid, &did);
	}

	port = pci_attr_r16(devind, PCI_BAR) & 0xfffc;

	pci_reserve(devind);

	if ((vir_ptr = alloc_contig(VMMDEV_BUF_SIZE, 0, &phys_ptr)) == NULL)
		panic("unable to allocate memory");

	req = (struct VMMDevReportGuestInfo *) vir_ptr;
	req->guest_info.add_version = VMMDEV_GUEST_VERSION;
	req->guest_info.os_type = VMMDEV_GUEST_OS_OTHER;

	if ((r = vbox_request(VMMDEV_REQ_REPORTGUESTINFO, sizeof(*req))) !=
			VMMDEV_ERR_OK)
		panic("backdoor device not functioning");

	ticks = sys_hz() * interval;

	sys_setalarm(ticks, 0);

	return OK;
}

/*===========================================================================*
 *				vbox_update_time			     *
 *===========================================================================*/
PRIVATE void vbox_update_time(void)
{
	/* Update the current time if it has drifted too far. */
	struct VMMDevReqHostTime *req;
	time_t otime, ntime;

	req = (struct VMMDevReqHostTime *) vir_ptr;

	if (vbox_request(VMMDEV_REQ_HOSTTIME, sizeof(*req)) == VMMDEV_ERR_OK) {
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
PRIVATE void vbox_signal(int signo)
{
	/* Process a signal. If it is a SIGTERM, terminate immediately. */

	if (signo != SIGTERM) return;

	exit(0);
}

/*===========================================================================*
 *				sef_local_startup			     *
 *===========================================================================*/
PRIVATE void sef_local_startup(void)
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
PUBLIC int main(int argc, char **argv)
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
			case CLOCK:
				vbox_update_time();

				break;

			default:
				printf("VBOX: received notify from %d\n",
					m.m_source);
			}

			continue;
		}

		printf("VBOX: received message %d from %d\n",
			m.m_type, m.m_source);
	}

	return 0;
}
