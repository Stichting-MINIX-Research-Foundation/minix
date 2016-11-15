/*	$NetBSD: sl811hs.c,v 1.47 2013/10/17 21:24:24 christos Exp $	*/

/*
 * Not (c) 2007 Matthew Orgass
 * This file is public domain, meaning anyone can make any use of part or all
 * of this file including copying into other works without credit.  Any use,
 * modified or not, is solely the responsibility of the user.  If this file is
 * part of a collection then use in the collection is governed by the terms of
 * the collection.
 */

/*
 * Cypress/ScanLogic SL811HS/T USB Host Controller
 * Datasheet, Errata, and App Note available at www.cypress.com
 *
 * Uses: Ratoc CFU1U PCMCIA USB Host Controller, Nereid X68k USB HC, ISA
 * HCs.  The Ratoc CFU2 uses a different chip.
 *
 * This chip puts the serial in USB.  It implements USB by means of an eight
 * bit I/O interface.  It can be used for ISA, PCMCIA/CF, parallel port,
 * serial port, or any eight bit interface.  It has 256 bytes of memory, the
 * first 16 of which are used for register access.  There are two sets of
 * registers for sending individual bus transactions.  Because USB is polled,
 * this organization means that some amount of card access must often be made
 * when devices are attached, even if when they are not directly being used.
 * A per-ms frame interrupt is necessary and many devices will poll with a
 * per-frame bulk transfer.
 *
 * It is possible to write a little over two bytes to the chip (auto
 * incremented) per full speed byte time on the USB.  Unfortunately,
 * auto-increment does not work reliably so write and bus speed is
 * approximately the same for full speed devices.
 *
 * In addition to the 240 byte packet size limit for isochronous transfers,
 * this chip has no means of determining the current frame number other than
 * getting all 1ms SOF interrupts, which is not always possible even on a fast
 * system.  Isochronous transfers guarantee that transfers will never be
 * retried in a later frame, so this can cause problems with devices beyond
 * the difficulty in actually performing the transfer most frames.  I tried
 * implementing isoc transfers and was able to play CD-derrived audio via an
 * iMic on a 2GHz PC, however it would still be interrupted at times and
 * once interrupted, would stay out of sync.  All isoc support has been
 * removed.
 *
 * BUGS: all chip revisions have problems with low speed devices through hubs.
 * The chip stops generating SOF with hubs that send SE0 during SOF.  See
 * comment in dointr().  All performance enhancing features of this chip seem
 * not to work properly, most confirmed buggy in errata doc.
 *
 */

/*
 * The hard interrupt is the main entry point.  Start, callbacks, and repeat
 * are the only others called frequently.
 *
 * Since this driver attaches to pcmcia, card removal at any point should be
 * expected and not cause panics or infinite loops.
 */

/*
 * XXX TODO:
 *   copy next output packet while transfering
 *   usb suspend
 *   could keep track of known values of all buffer space?
 *   combined print/log function for errors
 *
 *   use_polling support is untested and may not work
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sl811hs.c,v 1.47 2013/10/17 21:24:24 christos Exp $");

#include "opt_slhci.h"

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/gcq.h>
#include <sys/intr.h>
#include <sys/cpu.h>
#include <sys/bus.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/usbroothub_subr.h>

#include <dev/ic/sl811hsreg.h>
#include <dev/ic/sl811hsvar.h>

#define Q_CB 0				/* Control/Bulk */
#define Q_NEXT_CB 1
#define Q_MAX_XFER Q_CB
#define Q_CALLBACKS 2
#define Q_MAX Q_CALLBACKS

#define F_AREADY		(0x00000001)
#define F_BREADY		(0x00000002)
#define F_AINPROG		(0x00000004)
#define F_BINPROG		(0x00000008)
#define F_LOWSPEED		(0x00000010)
#define F_UDISABLED		(0x00000020) /* Consider disabled for USB */
#define F_NODEV			(0x00000040)
#define F_ROOTINTR		(0x00000080)
#define F_REALPOWER		(0x00000100) /* Actual power state */
#define F_POWER			(0x00000200) /* USB reported power state */
#define F_ACTIVE		(0x00000400)
#define F_CALLBACK		(0x00000800) /* Callback scheduled */
#define F_SOFCHECK1		(0x00001000)
#define F_SOFCHECK2		(0x00002000)
#define F_CRESET		(0x00004000) /* Reset done not reported */
#define F_CCONNECT		(0x00008000) /* Connect change not reported */
#define F_RESET			(0x00010000)
#define F_ISOC_WARNED		(0x00020000)
#define F_LSVH_WARNED		(0x00040000)

#define F_DISABLED		(F_NODEV|F_UDISABLED)
#define F_CHANGE		(F_CRESET|F_CCONNECT)

#ifdef SLHCI_TRY_LSVH
unsigned int slhci_try_lsvh = 1;
#else
unsigned int slhci_try_lsvh = 0;
#endif

#define ADR 0
#define LEN 1
#define PID 2
#define DEV 3
#define STAT 2
#define CONT 3

#define A 0
#define B 1

static const uint8_t slhci_tregs[2][4] =
{{SL11_E0ADDR, SL11_E0LEN, SL11_E0PID, SL11_E0DEV },
 {SL11_E1ADDR, SL11_E1LEN, SL11_E1PID, SL11_E1DEV }};

#define PT_ROOT_CTRL	0
#define PT_ROOT_INTR	1
#define PT_CTRL_SETUP	2
#define PT_CTRL_DATA	3
#define PT_CTRL_STATUS	4
#define PT_INTR		5
#define PT_BULK		6
#define PT_MAX		6

#ifdef SLHCI_DEBUG
#define SLHCI_MEM_ACCOUNTING
static const char *
pnames(int ptype)
{
	static const char * const names[] = { "ROOT Ctrl", "ROOT Intr",
	    "Control (setup)", "Control (data)", "Control (status)",
	    "Interrupt", "Bulk", "BAD PTYPE" };

	KASSERT(sizeof(names) / sizeof(names[0]) == PT_MAX + 2);
	if (ptype > PT_MAX)
		ptype = PT_MAX + 1;
	return names[ptype];
}
#endif

#define SLHCI_XFER_TYPE(x) (((struct slhci_pipe *)((x)->pipe))->ptype)

/*
 * Maximum allowable reserved bus time.  Since intr/isoc transfers have
 * unconditional priority, this is all that ensures control and bulk transfers
 * get a chance.  It is a single value for all frames since all transfers can
 * use multiple consecutive frames if an error is encountered.  Note that it
 * is not really possible to fill the bus with transfers, so this value should
 * be on the low side.  Defaults to giving a warning unless SLHCI_NO_OVERTIME
 * is defined.  Full time is 12000 - END_BUSTIME.
 */
#ifndef SLHCI_RESERVED_BUSTIME
#define SLHCI_RESERVED_BUSTIME 5000
#endif

/*
 * Rate for "exceeds reserved bus time" warnings (default) or errors.
 * Warnings only happen when an endpoint open causes the time to go above
 * SLHCI_RESERVED_BUSTIME, not if it is already above.
 */
#ifndef SLHCI_OVERTIME_WARNING_RATE
#define SLHCI_OVERTIME_WARNING_RATE { 60, 0 } /* 60 seconds */
#endif
static const struct timeval reserved_warn_rate = SLHCI_OVERTIME_WARNING_RATE;

/* Rate for overflow warnings */
#ifndef SLHCI_OVERFLOW_WARNING_RATE
#define SLHCI_OVERFLOW_WARNING_RATE { 60, 0 } /* 60 seconds */
#endif
static const struct timeval overflow_warn_rate = SLHCI_OVERFLOW_WARNING_RATE;

/*
 * For EOF, the spec says 42 bit times, plus (I think) a possible hub skew of
 * 20 bit times.  By default leave 66 bit times to start the transfer beyond
 * the required time.  Units are full-speed bit times (a bit over 5us per 64).
 * Only multiples of 64 are significant.
 */
#define SLHCI_STANDARD_END_BUSTIME 128
#ifndef SLHCI_EXTRA_END_BUSTIME
#define SLHCI_EXTRA_END_BUSTIME 0
#endif

#define SLHCI_END_BUSTIME (SLHCI_STANDARD_END_BUSTIME+SLHCI_EXTRA_END_BUSTIME)

/*
 * This is an approximation of the USB worst-case timings presented on p. 54 of
 * the USB 1.1 spec translated to full speed bit times.
 * FS = full speed with handshake, FSII = isoc in, FSIO = isoc out,
 * FSI = isoc (worst case), LS = low speed
 */
#define SLHCI_FS_CONST		114
#define SLHCI_FSII_CONST	92
#define SLHCI_FSIO_CONST	80
#define SLHCI_FSI_CONST		92
#define SLHCI_LS_CONST		804
#ifndef SLHCI_PRECICE_BUSTIME
/*
 * These values are < 3% too high (compared to the multiply and divide) for
 * max sized packets.
 */
#define SLHCI_FS_DATA_TIME(len) (((u_int)(len)<<3)+(len)+((len)>>1))
#define SLHCI_LS_DATA_TIME(len) (((u_int)(len)<<6)+((u_int)(len)<<4))
#else
#define SLHCI_FS_DATA_TIME(len) (56*(len)/6)
#define SLHCI_LS_DATA_TIME(len) (449*(len)/6)
#endif

/*
 * Set SLHCI_WAIT_SIZE to the desired maximum size of single FS transfer
 * to poll for after starting a transfer.  64 gets all full speed transfers.
 * Note that even if 0 polling will occur if data equal or greater than the
 * transfer size is copied to the chip while the transfer is in progress.
 * Setting SLHCI_WAIT_TIME to -12000 will disable polling.
 */
#ifndef SLHCI_WAIT_SIZE
#define SLHCI_WAIT_SIZE 8
#endif
#ifndef SLHCI_WAIT_TIME
#define SLHCI_WAIT_TIME (SLHCI_FS_CONST + \
    SLHCI_FS_DATA_TIME(SLHCI_WAIT_SIZE))
#endif
const int slhci_wait_time = SLHCI_WAIT_TIME;

/* Root hub intr endpoint */
#define ROOT_INTR_ENDPT        1

#ifndef SLHCI_MAX_RETRIES
#define SLHCI_MAX_RETRIES 3
#endif

/* Check IER values for corruption after this many unrecognized interrupts. */
#ifndef SLHCI_IER_CHECK_FREQUENCY
#ifdef SLHCI_DEBUG
#define SLHCI_IER_CHECK_FREQUENCY 1
#else
#define SLHCI_IER_CHECK_FREQUENCY 100
#endif
#endif

/* Note that buffer points to the start of the buffer for this transfer.  */
struct slhci_pipe {
	struct usbd_pipe pipe;
	struct usbd_xfer *xfer;		/* xfer in progress */
	uint8_t		*buffer;	/* I/O buffer (if needed) */
	struct gcq 	ap;		/* All pipes */
	struct gcq 	to;		/* Timeout list */
	struct gcq 	xq;		/* Xfer queues */
	unsigned int	pflags;		/* Pipe flags */
#define PF_GONE		(0x01)		/* Pipe is on disabled device */
#define PF_TOGGLE 	(0x02)		/* Data toggle status */
#define PF_LS		(0x04)		/* Pipe is low speed */
#define PF_PREAMBLE	(0x08)		/* Needs preamble */
	Frame		to_frame;	/* Frame number for timeout */
	Frame		frame;		/* Frame number for intr xfer */
	Frame		lastframe;	/* Previous frame number for intr */
	uint16_t	bustime;	/* Worst case bus time usage */
	uint16_t	newbustime[2];	/* new bustimes (see index below) */
	uint8_t		tregs[4];	/* ADR, LEN, PID, DEV */
	uint8_t		newlen[2];	/* 0 = short data, 1 = ctrl data */
	uint8_t		newpid;		/* for ctrl */
	uint8_t		wantshort;	/* last xfer must be short */
	uint8_t		control;	/* Host control register settings */
	uint8_t		nerrs;		/* Current number of errors */
	uint8_t 	ptype;		/* Pipe type */
};

#ifdef SLHCI_PROFILE_TRANSFER
#if defined(__mips__)
/*
 * MIPS cycle counter does not directly count cpu cycles but is a different
 * fraction of cpu cycles depending on the cpu.
 */
typedef u_int32_t cc_type;
#define CC_TYPE_FMT "%u"
#define slhci_cc_set(x) __asm volatile ("mfc0 %[cc], $9\n\tnop\n\tnop\n\tnop" \
    : [cc] "=r"(x))
#elif defined(__i386__)
typedef u_int64_t cc_type;
#define CC_TYPE_FMT "%llu"
#define slhci_cc_set(x) __asm volatile ("rdtsc" : "=A"(x))
#else
#error "SLHCI_PROFILE_TRANSFER not implemented on this MACHINE_ARCH (see sys/dev/ic/sl811hs.c)"
#endif
struct slhci_cc_time {
	cc_type start;
	cc_type stop;
	unsigned int miscdata;
};
#ifndef SLHCI_N_TIMES
#define SLHCI_N_TIMES 200
#endif
struct slhci_cc_times {
	struct slhci_cc_time times[SLHCI_N_TIMES];
	int current;
	int wraparound;
};

static struct slhci_cc_times t_ab[2];
static struct slhci_cc_times t_abdone;
static struct slhci_cc_times t_copy_to_dev;
static struct slhci_cc_times t_copy_from_dev;
static struct slhci_cc_times t_intr;
static struct slhci_cc_times t_lock;
static struct slhci_cc_times t_delay;
static struct slhci_cc_times t_hard_int;
static struct slhci_cc_times t_callback;

static inline void
start_cc_time(struct slhci_cc_times *times, unsigned int misc) {
	times->times[times->current].miscdata = misc;
	slhci_cc_set(times->times[times->current].start);
}
static inline void
stop_cc_time(struct slhci_cc_times *times) {
	slhci_cc_set(times->times[times->current].stop);
	if (++times->current >= SLHCI_N_TIMES) {
		times->current = 0;
		times->wraparound = 1;
	}
}

void slhci_dump_cc_times(int);

void
slhci_dump_cc_times(int n) {
	struct slhci_cc_times *times;
	int i;

	switch (n) {
	default:
	case 0:
		printf("USBA start transfer to intr:\n");
		times = &t_ab[A];
		break;
	case 1:
		printf("USBB start transfer to intr:\n");
		times = &t_ab[B];
		break;
	case 2:
		printf("abdone:\n");
		times = &t_abdone;
		break;
	case 3:
		printf("copy to device:\n");
		times = &t_copy_to_dev;
		break;
	case 4:
		printf("copy from device:\n");
		times = &t_copy_from_dev;
		break;
	case 5:
		printf("intr to intr:\n");
		times = &t_intr;
		break;
	case 6:
		printf("lock to release:\n");
		times = &t_lock;
		break;
	case 7:
		printf("delay time:\n");
		times = &t_delay;
		break;
	case 8:
		printf("hard interrupt enter to exit:\n");
		times = &t_hard_int;
		break;
	case 9:
		printf("callback:\n");
		times = &t_callback;
		break;
	}

	if (times->wraparound)
		for (i = times->current + 1; i < SLHCI_N_TIMES; i++)
			printf("start " CC_TYPE_FMT " stop " CC_TYPE_FMT
			    " difference %8i miscdata %#x\n",
			    times->times[i].start, times->times[i].stop,
			    (int)(times->times[i].stop -
			    times->times[i].start), times->times[i].miscdata);

	for (i = 0; i < times->current; i++)
		printf("start " CC_TYPE_FMT " stop " CC_TYPE_FMT
		    " difference %8i miscdata %#x\n", times->times[i].start,
		    times->times[i].stop, (int)(times->times[i].stop -
		    times->times[i].start), times->times[i].miscdata);
}
#else
#define start_cc_time(x, y)
#define stop_cc_time(x)
#endif /* SLHCI_PROFILE_TRANSFER */

typedef usbd_status (*LockCallFunc)(struct slhci_softc *, struct slhci_pipe
    *, struct usbd_xfer *);

usbd_status slhci_allocm(struct usbd_bus *, usb_dma_t *, u_int32_t);
void slhci_freem(struct usbd_bus *, usb_dma_t *);
struct usbd_xfer * slhci_allocx(struct usbd_bus *);
void slhci_freex(struct usbd_bus *, struct usbd_xfer *);
static void slhci_get_lock(struct usbd_bus *, kmutex_t **);

usbd_status slhci_transfer(struct usbd_xfer *);
usbd_status slhci_start(struct usbd_xfer *);
usbd_status slhci_root_start(struct usbd_xfer *);
usbd_status slhci_open(struct usbd_pipe *);

/*
 * slhci_supported_rev, slhci_preinit, slhci_attach, slhci_detach,
 * slhci_activate
 */

void slhci_abort(struct usbd_xfer *);
void slhci_close(struct usbd_pipe *);
void slhci_clear_toggle(struct usbd_pipe *);
void slhci_poll(struct usbd_bus *);
void slhci_done(struct usbd_xfer *);
void slhci_void(void *);

/* lock entry functions */

#ifdef SLHCI_MEM_ACCOUNTING
void slhci_mem_use(struct usbd_bus *, int);
#endif

void slhci_reset_entry(void *);
usbd_status slhci_lock_call(struct slhci_softc *, LockCallFunc,
    struct slhci_pipe *, struct usbd_xfer *);
void slhci_start_entry(struct slhci_softc *, struct slhci_pipe *);
void slhci_callback_entry(void *arg);
void slhci_do_callback(struct slhci_softc *, struct usbd_xfer *);

/* slhci_intr */

void slhci_main(struct slhci_softc *);

/* in lock functions */

static void slhci_write(struct slhci_softc *, uint8_t, uint8_t);
static uint8_t slhci_read(struct slhci_softc *, uint8_t);
static void slhci_write_multi(struct slhci_softc *, uint8_t, uint8_t *, int);
static void slhci_read_multi(struct slhci_softc *, uint8_t, uint8_t *, int);

static void slhci_waitintr(struct slhci_softc *, int);
static int slhci_dointr(struct slhci_softc *);
static void slhci_abdone(struct slhci_softc *, int);
static void slhci_tstart(struct slhci_softc *);
static void slhci_dotransfer(struct slhci_softc *);

static void slhci_callback(struct slhci_softc *);
static void slhci_enter_xfer(struct slhci_softc *, struct slhci_pipe *);
static void slhci_enter_xfers(struct slhci_softc *);
static void slhci_queue_timed(struct slhci_softc *, struct slhci_pipe *);
static void slhci_xfer_timer(struct slhci_softc *, struct slhci_pipe *);

static void slhci_do_repeat(struct slhci_softc *, struct usbd_xfer *);
static void slhci_callback_schedule(struct slhci_softc *);
static void slhci_do_callback_schedule(struct slhci_softc *);
#if 0
void slhci_pollxfer(struct slhci_softc *, struct usbd_xfer *); /* XXX */
#endif

static usbd_status slhci_do_poll(struct slhci_softc *, struct slhci_pipe *,
    struct usbd_xfer *);
static usbd_status slhci_lsvh_warn(struct slhci_softc *, struct slhci_pipe *,
    struct usbd_xfer *);
static usbd_status slhci_isoc_warn(struct slhci_softc *, struct slhci_pipe *,
    struct usbd_xfer *);
static usbd_status slhci_open_pipe(struct slhci_softc *, struct slhci_pipe *,
    struct usbd_xfer *);
static usbd_status slhci_close_pipe(struct slhci_softc *, struct slhci_pipe *,
    struct usbd_xfer *);
static usbd_status slhci_do_abort(struct slhci_softc *, struct slhci_pipe *,
    struct usbd_xfer *);
static usbd_status slhci_halt(struct slhci_softc *, struct slhci_pipe *,
    struct usbd_xfer *);

static void slhci_intrchange(struct slhci_softc *, uint8_t);
static void slhci_drain(struct slhci_softc *);
static void slhci_reset(struct slhci_softc *);
static int slhci_reserve_bustime(struct slhci_softc *, struct slhci_pipe *,
    int);
static void slhci_insert(struct slhci_softc *);

static usbd_status slhci_clear_feature(struct slhci_softc *, unsigned int);
static usbd_status slhci_set_feature(struct slhci_softc *, unsigned int);
static void slhci_get_status(struct slhci_softc *, usb_port_status_t *);
static usbd_status slhci_root(struct slhci_softc *, struct slhci_pipe *,
    struct usbd_xfer *);

#ifdef SLHCI_DEBUG
void slhci_log_buffer(struct usbd_xfer *);
void slhci_log_req(usb_device_request_t *);
void slhci_log_req_hub(usb_device_request_t *);
void slhci_log_dumpreg(void);
void slhci_log_xfer(struct usbd_xfer *);
void slhci_log_spipe(struct slhci_pipe *);
void slhci_print_intr(void);
void slhci_log_sc(void);
void slhci_log_slreq(struct slhci_pipe *);

extern int usbdebug;

/* Constified so you can read the values from ddb */
const int SLHCI_D_TRACE =	0x0001;
const int SLHCI_D_MSG = 	0x0002;
const int SLHCI_D_XFER =	0x0004;
const int SLHCI_D_MEM = 	0x0008;
const int SLHCI_D_INTR =	0x0010;
const int SLHCI_D_SXFER =	0x0020;
const int SLHCI_D_ERR = 	0x0080;
const int SLHCI_D_BUF = 	0x0100;
const int SLHCI_D_SOFT =	0x0200;
const int SLHCI_D_WAIT =	0x0400;
const int SLHCI_D_ROOT =	0x0800;
/* SOF/NAK alone normally ignored, SOF also needs D_INTR */
const int SLHCI_D_SOF =		0x1000;
const int SLHCI_D_NAK =		0x2000;

int slhci_debug = 0x1cbc; /* 0xc8c; */ /* 0xffff; */ /* 0xd8c; */
struct slhci_softc *ssc;
#ifdef USB_DEBUG
int slhci_usbdebug = -1; /* value to set usbdebug on attach, -1 = leave alone */
#endif

/*
 * XXXMRG the SLHCI UVMHIST code has been converted to KERNHIST, but it has
 * not been tested.  the extra instructions to enable it can probably be
 * commited to the kernhist code, and these instructions reduced to simply
 * enabling SLHCI_DEBUG.
 */

/*
 * Add KERNHIST history for debugging:
 *
 *   Before kern_hist in sys/kern/subr_kernhist.c add:
 *      KERNHIST_DECL(slhcihist);
 *
 *   In kern_hist add:
 *      if ((bitmask & KERNHIST_SLHCI))
 *              hists[i++] = &slhcihist;
 *
 *   In sys/sys/kernhist.h add KERNHIST_SLHCI define.
 */

#include <sys/kernhist.h>
KERNHIST_DECL(slhcihist);

#if !defined(KERNHIST) || !defined(KERNHIST_SLHCI)
#error "SLHCI_DEBUG requires KERNHIST (with modifications, see sys/dev/ic/sl81hs.c)"
#endif

#ifndef SLHCI_NHIST
#define SLHCI_NHIST 409600
#endif
const unsigned int SLHCI_HISTMASK = KERNHIST_SLHCI;
struct kern_history_ent slhci_he[SLHCI_NHIST];

#define SLHCI_DEXEC(x, y) do { if ((slhci_debug & SLHCI_ ## x)) { y; } \
} while (/*CONSTCOND*/ 0)
#define DDOLOG(f, a, b, c, d) do { const char *_kernhist_name = __func__; \
    u_long _kernhist_call = 0; KERNHIST_LOG(slhcihist, f, a, b, c, d);	     \
} while (/*CONSTCOND*/0)
#define DLOG(x, f, a, b, c, d) SLHCI_DEXEC(x, DDOLOG(f, a, b, c, d))
/*
 * DLOGFLAG8 is a macro not a function so that flag name expressions are not
 * evaluated unless the flag bit is set (which could save a register read).
 * x is debug mask, y is flag identifier, z is flag variable,
 * a-h are flag names (must evaluate to string constants, msb first).
 */
#define DDOLOGFLAG8(y, z, a, b, c, d, e, f, g, h) do { uint8_t _DLF8 = (z);   \
    const char *_kernhist_name = __func__; u_long _kernhist_call = 0;	      \
    if (_DLF8 & 0xf0) KERNHIST_LOG(slhcihist, y " %s %s %s %s", _DLF8 & 0x80 ?  \
    (a) : "", _DLF8 & 0x40 ? (b) : "", _DLF8 & 0x20 ? (c) : "", _DLF8 & 0x10 ? \
    (d) : ""); if (_DLF8 & 0x0f) KERNHIST_LOG(slhcihist, y " %s %s %s %s",      \
    _DLF8 & 0x08 ? (e) : "", _DLF8 & 0x04 ? (f) : "", _DLF8 & 0x02 ? (g) : "", \
    _DLF8 & 0x01 ? (h) : "");		      				       \
} while (/*CONSTCOND*/ 0)
#define DLOGFLAG8(x, y, z, a, b, c, d, e, f, g, h) \
    SLHCI_DEXEC(x, DDOLOGFLAG8(y, z, a, b, c, d, e, f, g, h))
/*
 * DDOLOGBUF logs a buffer up to 8 bytes at a time. No identifier so that we
 * can make it a real function.
 */
static void
DDOLOGBUF(uint8_t *buf, unsigned int length)
{
	int i;

	for(i=0; i+8 <= length; i+=8)
		DDOLOG("%.4x %.4x %.4x %.4x", (buf[i] << 8) | buf[i+1],
		    (buf[i+2] << 8) | buf[i+3], (buf[i+4] << 8) | buf[i+5],
		    (buf[i+6] << 8) | buf[i+7]);
	if (length == i+7)
		DDOLOG("%.4x %.4x %.4x %.2x", (buf[i] << 8) | buf[i+1],
		    (buf[i+2] << 8) | buf[i+3], (buf[i+4] << 8) | buf[i+5],
		    buf[i+6]);
	else if (length == i+6)
		DDOLOG("%.4x %.4x %.4x", (buf[i] << 8) | buf[i+1],
		    (buf[i+2] << 8) | buf[i+3], (buf[i+4] << 8) | buf[i+5], 0);
	else if (length == i+5)
		DDOLOG("%.4x %.4x %.2x", (buf[i] << 8) | buf[i+1],
		    (buf[i+2] << 8) | buf[i+3], buf[i+4], 0);
	else if (length == i+4)
		DDOLOG("%.4x %.4x", (buf[i] << 8) | buf[i+1],
		    (buf[i+2] << 8) | buf[i+3], 0,0);
	else if (length == i+3)
		DDOLOG("%.4x %.2x", (buf[i] << 8) | buf[i+1], buf[i+2], 0,0);
	else if (length == i+2)
		DDOLOG("%.4x", (buf[i] << 8) | buf[i+1], 0,0,0);
	else if (length == i+1)
		DDOLOG("%.2x", buf[i], 0,0,0);
}
#define DLOGBUF(x, b, l) SLHCI_DEXEC(x, DDOLOGBUF(b, l))
#else /* now !SLHCI_DEBUG */
#define slhci_log_spipe(spipe) ((void)0)
#define slhci_log_xfer(xfer) ((void)0)
#define SLHCI_DEXEC(x, y) ((void)0)
#define DDOLOG(f, a, b, c, d) ((void)0)
#define DLOG(x, f, a, b, c, d) ((void)0)
#define DDOLOGFLAG8(y, z, a, b, c, d, e, f, g, h) ((void)0)
#define DLOGFLAG8(x, y, z, a, b, c, d, e, f, g, h) ((void)0)
#define DDOLOGBUF(b, l) ((void)0)
#define DLOGBUF(x, b, l) ((void)0)
#endif /* SLHCI_DEBUG */

#ifdef DIAGNOSTIC
#define LK_SLASSERT(exp, sc, spipe, xfer, ext) do {			\
	if (!(exp)) {							\
		printf("%s: assertion %s failed line %u function %s!"	\
		" halted\n", SC_NAME(sc), #exp, __LINE__, __func__);\
		DDOLOG("%s: assertion %s failed line %u function %s!"	\
		" halted\n", SC_NAME(sc), #exp, __LINE__, __func__);\
		slhci_halt(sc, spipe, xfer);				\
		ext;							\
	}								\
} while (/*CONSTCOND*/0)
#define UL_SLASSERT(exp, sc, spipe, xfer, ext) do {			\
	if (!(exp)) {							\
		printf("%s: assertion %s failed line %u function %s!"	\
		" halted\n", SC_NAME(sc), #exp, __LINE__, __func__);	\
		DDOLOG("%s: assertion %s failed line %u function %s!"	\
		" halted\n", SC_NAME(sc), #exp, __LINE__, __func__);	\
		slhci_lock_call(sc, &slhci_halt, spipe, xfer);		\
		ext;							\
	}								\
} while (/*CONSTCOND*/0)
#else
#define LK_SLASSERT(exp, sc, spipe, xfer, ext) ((void)0)
#define UL_SLASSERT(exp, sc, spipe, xfer, ext) ((void)0)
#endif

const struct usbd_bus_methods slhci_bus_methods = {
	.open_pipe = slhci_open,
	.soft_intr = slhci_void,
	.do_poll = slhci_poll,
	.allocm = slhci_allocm,
	.freem = slhci_freem,
	.allocx = slhci_allocx,
	.freex = slhci_freex,
	.get_lock = slhci_get_lock,
	NULL, /* new_device */
};

const struct usbd_pipe_methods slhci_pipe_methods = {
	.transfer = slhci_transfer,
	.start = slhci_start,
	.abort = slhci_abort,
	.close = slhci_close,
	.cleartoggle = slhci_clear_toggle,
	.done = slhci_done,
};

const struct usbd_pipe_methods slhci_root_methods = {
	.transfer = slhci_transfer,
	.start = slhci_root_start,
	.abort = slhci_abort,
	.close = (void (*)(struct usbd_pipe *))slhci_void, /* XXX safe? */
	.cleartoggle = slhci_clear_toggle,
	.done = slhci_done,
};

/* Queue inlines */

#define GOT_FIRST_TO(tvar, t) \
    GCQ_GOT_FIRST_TYPED(tvar, &(t)->to, struct slhci_pipe, to)

#define FIND_TO(var, t, tvar, cond) \
    GCQ_FIND_TYPED(var, &(t)->to, tvar, struct slhci_pipe, to, cond)

#define FOREACH_AP(var, t, tvar) \
    GCQ_FOREACH_TYPED(var, &(t)->ap, tvar, struct slhci_pipe, ap)

#define GOT_FIRST_TIMED_COND(tvar, t, cond) \
    GCQ_GOT_FIRST_COND_TYPED(tvar, &(t)->timed, struct slhci_pipe, xq, cond)

#define GOT_FIRST_CB(tvar, t) \
    GCQ_GOT_FIRST_TYPED(tvar, &(t)->q[Q_CB], struct slhci_pipe, xq)

#define DEQUEUED_CALLBACK(tvar, t) \
    GCQ_DEQUEUED_FIRST_TYPED(tvar, &(t)->q[Q_CALLBACKS], struct slhci_pipe, xq)

#define FIND_TIMED(var, t, tvar, cond) \
   GCQ_FIND_TYPED(var, &(t)->timed, tvar, struct slhci_pipe, xq, cond)

#define DEQUEUED_WAITQ(tvar, sc) \
    GCQ_DEQUEUED_FIRST_TYPED(tvar, &(sc)->sc_waitq, struct slhci_pipe, xq)

static inline void
enter_waitq(struct slhci_softc *sc, struct slhci_pipe *spipe)
{
	gcq_insert_tail(&sc->sc_waitq, &spipe->xq);
}

static inline void
enter_q(struct slhci_transfers *t, struct slhci_pipe *spipe, int i)
{
	gcq_insert_tail(&t->q[i], &spipe->xq);
}

static inline void
enter_callback(struct slhci_transfers *t, struct slhci_pipe *spipe)
{
	gcq_insert_tail(&t->q[Q_CALLBACKS], &spipe->xq);
}

static inline void
enter_all_pipes(struct slhci_transfers *t, struct slhci_pipe *spipe)
{
	gcq_insert_tail(&t->ap, &spipe->ap);
}

/* Start out of lock functions. */

struct slhci_mem {
	usb_dma_block_t block;
	uint8_t data[];
};

/*
 * The SL811HS does not do DMA as a host controller, but NetBSD's USB interface
 * assumes DMA is used.  So we fake the DMA block.
 */
usbd_status
slhci_allocm(struct usbd_bus *bus, usb_dma_t *dma, u_int32_t size)
{
	struct slhci_mem *mem;

	mem = malloc(sizeof(struct slhci_mem) + size, M_USB, M_NOWAIT|M_ZERO);

	DLOG(D_MEM, "allocm %p", mem, 0,0,0);

	if (mem == NULL)
		return USBD_NOMEM;

	dma->block = &mem->block;
	dma->block->kaddr = mem->data;

	/* dma->offs = 0; */
	dma->block->nsegs = 1;
	dma->block->size = size;
	dma->block->align = size;
	dma->block->flags |= USB_DMA_FULLBLOCK;

#ifdef SLHCI_MEM_ACCOUNTING
	slhci_mem_use(bus, 1);
#endif

	return USBD_NORMAL_COMPLETION;
}

void
slhci_freem(struct usbd_bus *bus, usb_dma_t *dma)
{
	DLOG(D_MEM, "freem %p", dma->block, 0,0,0);

#ifdef SLHCI_MEM_ACCOUNTING
	slhci_mem_use(bus, -1);
#endif

	free(dma->block, M_USB);
}

struct usbd_xfer *
slhci_allocx(struct usbd_bus *bus)
{
	struct usbd_xfer *xfer;

	xfer = malloc(sizeof(*xfer), M_USB, M_NOWAIT|M_ZERO);

	DLOG(D_MEM, "allocx %p", xfer, 0,0,0);

#ifdef SLHCI_MEM_ACCOUNTING
	slhci_mem_use(bus, 1);
#endif
#ifdef DIAGNOSTIC
	if (xfer != NULL)
		xfer->busy_free = XFER_BUSY;
#endif
	return xfer;
}

void
slhci_freex(struct usbd_bus *bus, struct usbd_xfer *xfer)
{
	DLOG(D_MEM, "freex xfer %p spipe %p", xfer, xfer->pipe,0,0);

#ifdef SLHCI_MEM_ACCOUNTING
	slhci_mem_use(bus, -1);
#endif
#ifdef DIAGNOSTIC
	if (xfer->busy_free != XFER_BUSY) {
		struct slhci_softc *sc = bus->hci_private;
		printf("%s: slhci_freex: xfer=%p not busy, %#08x halted\n",
		    SC_NAME(sc), xfer, xfer->busy_free);
		DDOLOG("%s: slhci_freex: xfer=%p not busy, %#08x halted\n",
		    SC_NAME(sc), xfer, xfer->busy_free, 0);
		slhci_lock_call(sc, &slhci_halt, NULL, NULL);
		return;
	}
	xfer->busy_free = XFER_FREE;
#endif

	free(xfer, M_USB);
}

static void
slhci_get_lock(struct usbd_bus *bus, kmutex_t **lock)
{
	struct slhci_softc *sc = bus->hci_private;

	*lock = &sc->sc_lock;
}

usbd_status
slhci_transfer(struct usbd_xfer *xfer)
{
	struct slhci_softc *sc = xfer->pipe->device->bus->hci_private;
	usbd_status error;

	DLOG(D_TRACE, "%s transfer xfer %p spipe %p ",
	    pnames(SLHCI_XFER_TYPE(xfer)), xfer, xfer->pipe,0);

	/* Insert last in queue */
	mutex_enter(&sc->sc_lock);
	error = usb_insert_transfer(xfer);
	mutex_exit(&sc->sc_lock);
	if (error) {
		if (error != USBD_IN_PROGRESS)
			DLOG(D_ERR, "usb_insert_transfer returns %d!", error,
			    0,0,0);
		return error;
	}

	/*
	 * Pipe isn't running (otherwise error would be USBD_INPROG),
	 * so start it first.
	 */

	/*
	 * Start will take the lock.
	 */
	error = xfer->pipe->methods->start(SIMPLEQ_FIRST(&xfer->pipe->queue));

	return error;
}

/* It is not safe for start to return anything other than USBD_INPROG. */
usbd_status
slhci_start(struct usbd_xfer *xfer)
{
	struct slhci_softc *sc = xfer->pipe->device->bus->hci_private;
	struct usbd_pipe *pipe = xfer->pipe;
	struct slhci_pipe *spipe = (struct slhci_pipe *)pipe;
	struct slhci_transfers *t = &sc->sc_transfers;
;	usb_endpoint_descriptor_t *ed = pipe->endpoint->edesc;
	unsigned int max_packet;

	mutex_enter(&sc->sc_lock);

	max_packet = UGETW(ed->wMaxPacketSize);

	DLOG(D_TRACE, "%s start xfer %p spipe %p length %d",
	    pnames(spipe->ptype), xfer, spipe, xfer->length);

	/* root transfers use slhci_root_start */

	KASSERT(spipe->xfer == NULL); /* not SLASSERT */

	xfer->actlen = 0;
	xfer->status = USBD_IN_PROGRESS;

	spipe->xfer = xfer;

	spipe->nerrs = 0;
	spipe->frame = t->frame;
	spipe->control = SL11_EPCTRL_ARM_ENABLE;
	spipe->tregs[DEV] = pipe->device->address;
	spipe->tregs[PID] = spipe->newpid = UE_GET_ADDR(ed->bEndpointAddress)
	    | (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN ? SL11_PID_IN :
	    SL11_PID_OUT);
	spipe->newlen[0] = xfer->length % max_packet;
	spipe->newlen[1] = min(xfer->length, max_packet);

	if (spipe->ptype == PT_BULK || spipe->ptype == PT_INTR) {
		if (spipe->pflags & PF_TOGGLE)
			spipe->control |= SL11_EPCTRL_DATATOGGLE;
		spipe->tregs[LEN] = spipe->newlen[1];
		if (spipe->tregs[LEN])
			spipe->buffer = KERNADDR(&xfer->dmabuf, 0);
		else
			spipe->buffer = NULL;
		spipe->lastframe = t->frame;
#if defined(DEBUG) || defined(SLHCI_DEBUG)
		if (__predict_false(spipe->ptype == PT_INTR &&
		    xfer->length > spipe->tregs[LEN])) {
			printf("%s: Long INTR transfer not supported!\n",
			    SC_NAME(sc));
			DDOLOG("%s: Long INTR transfer not supported!\n",
			    SC_NAME(sc), 0,0,0);
			xfer->status = USBD_INVAL;
		}
#endif
	} else {
		/* ptype may be currently set to any control transfer type. */
		SLHCI_DEXEC(D_TRACE, slhci_log_xfer(xfer));

		/* SETUP contains IN/OUT bits also */
		spipe->tregs[PID] |= SL11_PID_SETUP;
		spipe->tregs[LEN] = 8;
		spipe->buffer = (uint8_t *)&xfer->request;
		DLOGBUF(D_XFER, spipe->buffer, spipe->tregs[LEN]);
		spipe->ptype = PT_CTRL_SETUP;
		spipe->newpid &= ~SL11_PID_BITS;
		if (xfer->length == 0 || (xfer->request.bmRequestType &
		    UT_READ))
			spipe->newpid |= SL11_PID_IN;
		else
			spipe->newpid |= SL11_PID_OUT;
	}

	if (xfer->flags & USBD_FORCE_SHORT_XFER && spipe->tregs[LEN] ==
	    max_packet && (spipe->newpid & SL11_PID_BITS) == SL11_PID_OUT)
		spipe->wantshort = 1;
	else
		spipe->wantshort = 0;

	/*
	 * The goal of newbustime and newlen is to avoid bustime calculation
	 * in the interrupt.  The calculations are not too complex, but they
	 * complicate the conditional logic somewhat and doing them all in the
	 * same place shares constants. Index 0 is "short length" for bulk and
	 * ctrl data and 1 is "full length" for ctrl data (bulk/intr are
	 * already set to full length).
	 */
	if (spipe->pflags & PF_LS) {
		/*
		 * Setting PREAMBLE for directly connnected LS devices will
		 * lock up the chip.
		 */
		if (spipe->pflags & PF_PREAMBLE)
			spipe->control |= SL11_EPCTRL_PREAMBLE;
		if (max_packet <= 8) {
			spipe->bustime = SLHCI_LS_CONST +
			    SLHCI_LS_DATA_TIME(spipe->tregs[LEN]);
			spipe->newbustime[0] = SLHCI_LS_CONST +
			    SLHCI_LS_DATA_TIME(spipe->newlen[0]);
			spipe->newbustime[1] = SLHCI_LS_CONST +
			    SLHCI_LS_DATA_TIME(spipe->newlen[1]);
		} else
			xfer->status = USBD_INVAL;
	} else {
		UL_SLASSERT(pipe->device->speed == USB_SPEED_FULL, sc,
		    spipe, xfer, return USBD_IN_PROGRESS);
		if (max_packet <= SL11_MAX_PACKET_SIZE) {
			spipe->bustime = SLHCI_FS_CONST +
			    SLHCI_FS_DATA_TIME(spipe->tregs[LEN]);
			spipe->newbustime[0] = SLHCI_FS_CONST +
			    SLHCI_FS_DATA_TIME(spipe->newlen[0]);
			spipe->newbustime[1] = SLHCI_FS_CONST +
			    SLHCI_FS_DATA_TIME(spipe->newlen[1]);
		} else
			xfer->status = USBD_INVAL;
	}

	/*
	 * The datasheet incorrectly indicates that DIRECTION is for
	 * "transmit to host".  It is for OUT and SETUP.  The app note
	 * describes its use correctly.
	 */
	if ((spipe->tregs[PID] & SL11_PID_BITS) != SL11_PID_IN)
		spipe->control |= SL11_EPCTRL_DIRECTION;

	slhci_start_entry(sc, spipe);

	mutex_exit(&sc->sc_lock);

	return USBD_IN_PROGRESS;
}

usbd_status
slhci_root_start(struct usbd_xfer *xfer)
{
	struct slhci_softc *sc;
	struct slhci_pipe *spipe;

	spipe = (struct slhci_pipe *)xfer->pipe;
	sc = xfer->pipe->device->bus->hci_private;

	return slhci_lock_call(sc, &slhci_root, spipe, xfer);
}

usbd_status
slhci_open(struct usbd_pipe *pipe)
{
	struct usbd_device *dev;
	struct slhci_softc *sc;
	struct slhci_pipe *spipe;
	usb_endpoint_descriptor_t *ed;
	struct slhci_transfers *t;
	unsigned int max_packet, pmaxpkt;

	dev = pipe->device;
	sc = dev->bus->hci_private;
	spipe = (struct slhci_pipe *)pipe;
	ed = pipe->endpoint->edesc;
	t = &sc->sc_transfers;

	DLOG(D_TRACE, "slhci_open(addr=%d,ep=%d,rootaddr=%d)",
		dev->address, ed->bEndpointAddress, t->rootaddr, 0);

	spipe->pflags = 0;
	spipe->frame = 0;
	spipe->lastframe = 0;
	spipe->xfer = NULL;
	spipe->buffer = NULL;

	gcq_init(&spipe->ap);
	gcq_init(&spipe->to);
	gcq_init(&spipe->xq);

	/*
	 * The endpoint descriptor will not have been set up yet in the case
	 * of the standard control pipe, so the max packet checks are also
	 * necessary in start.
	 */

	max_packet = UGETW(ed->wMaxPacketSize);

	if (dev->speed == USB_SPEED_LOW) {
		spipe->pflags |= PF_LS;
		if (dev->myhub->address != t->rootaddr) {
			spipe->pflags |= PF_PREAMBLE;
			if (!slhci_try_lsvh)
				return slhci_lock_call(sc, &slhci_lsvh_warn,
				    spipe, NULL);
		}
		pmaxpkt = 8;
	} else
		pmaxpkt = SL11_MAX_PACKET_SIZE;

	if (max_packet > pmaxpkt) {
		DLOG(D_ERR, "packet too large! size %d spipe %p", max_packet,
		    spipe, 0,0);
		return USBD_INVAL;
	}

	if (dev->address == t->rootaddr) {
		switch (ed->bEndpointAddress) {
		case USB_CONTROL_ENDPOINT:
			spipe->ptype = PT_ROOT_CTRL;
			pipe->interval = 0;
			break;
		case UE_DIR_IN | ROOT_INTR_ENDPT:
			spipe->ptype = PT_ROOT_INTR;
			pipe->interval = 1;
			break;
		default:
			printf("%s: Invalid root endpoint!\n", SC_NAME(sc));
			DDOLOG("%s: Invalid root endpoint!\n", SC_NAME(sc),
			    0,0,0);
			return USBD_INVAL;
		}
		pipe->methods = __UNCONST(&slhci_root_methods);
		return USBD_NORMAL_COMPLETION;
	} else {
		switch (ed->bmAttributes & UE_XFERTYPE) {
		case UE_CONTROL:
			spipe->ptype = PT_CTRL_SETUP;
			pipe->interval = 0;
			break;
		case UE_INTERRUPT:
			spipe->ptype = PT_INTR;
			if (pipe->interval == USBD_DEFAULT_INTERVAL)
				pipe->interval = ed->bInterval;
			break;
		case UE_ISOCHRONOUS:
			return slhci_lock_call(sc, &slhci_isoc_warn, spipe,
			    NULL);
		case UE_BULK:
			spipe->ptype = PT_BULK;
			pipe->interval = 0;
			break;
		}

		DLOG(D_MSG, "open pipe %s interval %d", pnames(spipe->ptype),
		    pipe->interval, 0,0);

		pipe->methods = __UNCONST(&slhci_pipe_methods);

		return slhci_lock_call(sc, &slhci_open_pipe, spipe, NULL);
	}
}

int
slhci_supported_rev(uint8_t rev)
{
	return (rev >= SLTYPE_SL811HS_R12 && rev <= SLTYPE_SL811HS_R15);
}

/*
 * Must be called before the ISR is registered. Interrupts can be shared so
 * slhci_intr could be called as soon as the ISR is registered.
 * Note max_current argument is actual current, but stored as current/2
 */
void
slhci_preinit(struct slhci_softc *sc, PowerFunc pow, bus_space_tag_t iot,
    bus_space_handle_t ioh, uint16_t max_current, uint32_t stride)
{
	struct slhci_transfers *t;
	int i;

	t = &sc->sc_transfers;

#ifdef SLHCI_DEBUG
	KERNHIST_INIT_STATIC(slhcihist, slhci_he);
#endif
	mutex_init(&sc->sc_lock, MUTEX_DEFAULT, IPL_SOFTUSB);
	mutex_init(&sc->sc_intr_lock, MUTEX_DEFAULT, IPL_SCHED);

	/* sc->sc_ier = 0;	*/
	/* t->rootintr = NULL;	*/
	t->flags = F_NODEV|F_UDISABLED;
	t->pend = INT_MAX;
	KASSERT(slhci_wait_time != INT_MAX);
	t->len[0] = t->len[1] = -1;
	if (max_current > 500)
		max_current = 500;
	t->max_current = (uint8_t)(max_current / 2);
	sc->sc_enable_power = pow;
	sc->sc_iot = iot;
	sc->sc_ioh = ioh;
	sc->sc_stride = stride;

	KASSERT(Q_MAX+1 == sizeof(t->q) / sizeof(t->q[0]));

	for (i = 0; i <= Q_MAX; i++)
		gcq_init_head(&t->q[i]);
	gcq_init_head(&t->timed);
	gcq_init_head(&t->to);
	gcq_init_head(&t->ap);
	gcq_init_head(&sc->sc_waitq);
}

int
slhci_attach(struct slhci_softc *sc)
{
	struct slhci_transfers *t;
	const char *rev;

	t = &sc->sc_transfers;

	/* Detect and check the controller type */
	t->sltype = SL11_GET_REV(slhci_read(sc, SL11_REV));

	/* SL11H not supported */
	if (!slhci_supported_rev(t->sltype)) {
		if (t->sltype == SLTYPE_SL11H)
			printf("%s: SL11H unsupported or bus error!\n",
			    SC_NAME(sc));
		else
			printf("%s: Unknown chip revision!\n", SC_NAME(sc));
		return -1;
	}

	callout_init(&sc->sc_timer, CALLOUT_MPSAFE);
	callout_setfunc(&sc->sc_timer, slhci_reset_entry, sc);

	/*
	 * It is not safe to call the soft interrupt directly as
	 * usb_schedsoftintr does in the use_polling case (due to locking).
	 */
	sc->sc_cb_softintr = softint_establish(SOFTINT_NET,
	    slhci_callback_entry, sc);

#ifdef SLHCI_DEBUG
	ssc = sc;
#ifdef USB_DEBUG
	if (slhci_usbdebug >= 0)
		usbdebug = slhci_usbdebug;
#endif
#endif

	if (t->sltype == SLTYPE_SL811HS_R12)
		rev = " (rev 1.2)";
	else if (t->sltype == SLTYPE_SL811HS_R14)
		rev = " (rev 1.4 or 1.5)";
	else
		rev = " (unknown revision)";

	aprint_normal("%s: ScanLogic SL811HS/T USB Host Controller %s\n",
	    SC_NAME(sc), rev);

	aprint_normal("%s: Max Current %u mA (value by code, not by probe)\n",
	    SC_NAME(sc), t->max_current * 2);

#if defined(SLHCI_DEBUG) || defined(SLHCI_NO_OVERTIME) || \
    defined(SLHCI_TRY_LSVH) || defined(SLHCI_PROFILE_TRANSFER)
	aprint_normal("%s: driver options:"
#ifdef SLHCI_DEBUG
	" SLHCI_DEBUG"
#endif
#ifdef SLHCI_TRY_LSVH
	" SLHCI_TRY_LSVH"
#endif
#ifdef SLHCI_NO_OVERTIME
	" SLHCI_NO_OVERTIME"
#endif
#ifdef SLHCI_PROFILE_TRANSFER
	" SLHCI_PROFILE_TRANSFER"
#endif
	"\n", SC_NAME(sc));
#endif
	sc->sc_bus.usbrev = USBREV_1_1;
	sc->sc_bus.methods = __UNCONST(&slhci_bus_methods);
	sc->sc_bus.pipe_size = sizeof(struct slhci_pipe);

	if (!sc->sc_enable_power)
		t->flags |= F_REALPOWER;

	t->flags |= F_ACTIVE;

	/* Attach usb and uhub. */
	sc->sc_child = config_found(SC_DEV(sc), &sc->sc_bus, usbctlprint);

	if (!sc->sc_child)
		return -1;
	else
		return 0;
}

int
slhci_detach(struct slhci_softc *sc, int flags)
{
	struct slhci_transfers *t;
	int ret;

	t = &sc->sc_transfers;

	/* By this point bus access is no longer allowed. */

	KASSERT(!(t->flags & F_ACTIVE));

	/*
	 * To be MPSAFE is not sufficient to cancel callouts and soft
	 * interrupts and assume they are dead since the code could already be
	 * running or about to run.  Wait until they are known to be done.
	 */
	while (t->flags & (F_RESET|F_CALLBACK))
		tsleep(&sc, PPAUSE, "slhci_detach", hz);

	softint_disestablish(sc->sc_cb_softintr);

	mutex_destroy(&sc->sc_lock);
	mutex_destroy(&sc->sc_intr_lock);

	ret = 0;

	if (sc->sc_child)
		ret = config_detach(sc->sc_child, flags);

#ifdef SLHCI_MEM_ACCOUNTING
	if (sc->sc_mem_use) {
		printf("%s: Memory still in use after detach! mem_use (count)"
		    " = %d\n", SC_NAME(sc), sc->sc_mem_use);
		DDOLOG("%s: Memory still in use after detach! mem_use (count)"
		    " = %d\n", SC_NAME(sc), sc->sc_mem_use, 0,0);
	}
#endif

	return ret;
}

int
slhci_activate(device_t self, enum devact act)
{
	struct slhci_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		slhci_lock_call(sc, &slhci_halt, NULL, NULL);
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

void
slhci_abort(struct usbd_xfer *xfer)
{
	struct slhci_softc *sc;
	struct slhci_pipe *spipe;

	spipe = (struct slhci_pipe *)xfer->pipe;

	if (spipe == NULL)
		goto callback;

	sc = spipe->pipe.device->bus->hci_private;

	KASSERT(mutex_owned(&sc->sc_lock));

	DLOG(D_TRACE, "%s abort xfer %p spipe %p spipe->xfer %p",
	    pnames(spipe->ptype), xfer, spipe, spipe->xfer);

	slhci_lock_call(sc, &slhci_do_abort, spipe, xfer);

callback:
	xfer->status = USBD_CANCELLED;
	/* Abort happens at IPL_USB. */
	usb_transfer_complete(xfer);
}

void
slhci_close(struct usbd_pipe *pipe)
{
	struct slhci_softc *sc;
	struct slhci_pipe *spipe;

	sc = pipe->device->bus->hci_private;
	spipe = (struct slhci_pipe *)pipe;

	DLOG(D_TRACE, "%s close spipe %p spipe->xfer %p",
	    pnames(spipe->ptype), spipe, spipe->xfer, 0);

	slhci_lock_call(sc, &slhci_close_pipe, spipe, NULL);
}

void
slhci_clear_toggle(struct usbd_pipe *pipe)
{
	struct slhci_pipe *spipe;

	spipe = (struct slhci_pipe *)pipe;

	DLOG(D_TRACE, "%s toggle spipe %p", pnames(spipe->ptype),
	    spipe,0,0);

	spipe->pflags &= ~PF_TOGGLE;

#ifdef DIAGNOSTIC
	if (spipe->xfer != NULL) {
		struct slhci_softc *sc = (struct slhci_softc
		    *)pipe->device->bus;

		printf("%s: Clear toggle on transfer in progress! halted\n",
		    SC_NAME(sc));
		DDOLOG("%s: Clear toggle on transfer in progress! halted\n",
		    SC_NAME(sc), 0,0,0);
		slhci_halt(sc, NULL, NULL);
	}
#endif
}

void
slhci_poll(struct usbd_bus *bus) /* XXX necessary? */
{
	struct slhci_softc *sc;

	sc = bus->hci_private;

	DLOG(D_TRACE, "slhci_poll", 0,0,0,0);

	slhci_lock_call(sc, &slhci_do_poll, NULL, NULL);
}

void
slhci_done(struct usbd_xfer *xfer)
{
	/* xfer may not be valid here */
}

void
slhci_void(void *v) {}

/* End out of lock functions. Start lock entry functions. */

#ifdef SLHCI_MEM_ACCOUNTING
void
slhci_mem_use(struct usbd_bus *bus, int val)
{
	struct slhci_softc *sc = bus->hci_private;
	int s;

	mutex_enter(&sc->sc_intr_lock);
	sc->sc_mem_use += val;
	mutex_exit(&sc->sc_intr_lock);
}
#endif

void
slhci_reset_entry(void *arg)
{
	struct slhci_softc *sc = arg;

	mutex_enter(&sc->sc_intr_lock);
	slhci_reset(sc);
	/*
	 * We cannot call the callback directly since we could then be reset
	 * again before finishing and need the callout delay for timing.
	 * Scheduling the callout again before we exit would defeat the reap
	 * mechanism since we could be unlocked while the reset flag is not
	 * set. The callback code will check the wait queue.
	 */
	slhci_callback_schedule(sc);
	mutex_exit(&sc->sc_intr_lock);
}

usbd_status
slhci_lock_call(struct slhci_softc *sc, LockCallFunc lcf, struct slhci_pipe
    *spipe, struct usbd_xfer *xfer)
{
	usbd_status ret;

	mutex_enter(&sc->sc_intr_lock);
	ret = (*lcf)(sc, spipe, xfer);
	slhci_main(sc);
	mutex_exit(&sc->sc_intr_lock);

	return ret;
}

void
slhci_start_entry(struct slhci_softc *sc, struct slhci_pipe *spipe)
{
	struct slhci_transfers *t;

	mutex_enter(&sc->sc_intr_lock);
	t = &sc->sc_transfers;

	if (!(t->flags & (F_AINPROG|F_BINPROG))) {
		slhci_enter_xfer(sc, spipe);
		slhci_dotransfer(sc);
		slhci_main(sc);
	} else {
		enter_waitq(sc, spipe);
	}
	mutex_exit(&sc->sc_intr_lock);
}

void
slhci_callback_entry(void *arg)
{
	struct slhci_softc *sc;
	struct slhci_transfers *t;

	sc = (struct slhci_softc *)arg;

	mutex_enter(&sc->sc_intr_lock);
	t = &sc->sc_transfers;
	DLOG(D_SOFT, "callback_entry flags %#x", t->flags, 0,0,0);

repeat:
	slhci_callback(sc);

	if (!gcq_empty(&sc->sc_waitq)) {
		slhci_enter_xfers(sc);
		slhci_dotransfer(sc);
		slhci_waitintr(sc, 0);
		goto repeat;
	}

	t->flags &= ~F_CALLBACK;
	mutex_exit(&sc->sc_intr_lock);
}

void
slhci_do_callback(struct slhci_softc *sc, struct usbd_xfer *xfer)
{
	KASSERT(mutex_owned(&sc->sc_intr_lock));

	int repeat;

	start_cc_time(&t_callback, (u_int)xfer);
	mutex_exit(&sc->sc_intr_lock);

	mutex_enter(&sc->sc_lock);
	repeat = xfer->pipe->repeat;
	usb_transfer_complete(xfer);
	mutex_exit(&sc->sc_lock);

	mutex_enter(&sc->sc_intr_lock);
	stop_cc_time(&t_callback);

	if (repeat && !sc->sc_bus.use_polling)
		slhci_do_repeat(sc, xfer);
}

int
slhci_intr(void *arg)
{
	struct slhci_softc *sc = arg;
	int ret;

	start_cc_time(&t_hard_int, (unsigned int)arg);
	mutex_enter(&sc->sc_intr_lock);

	ret = slhci_dointr(sc);
	slhci_main(sc);
	mutex_exit(&sc->sc_intr_lock);

	stop_cc_time(&t_hard_int);
	return ret;
}

/* called with main lock only held, returns with locks released. */
void
slhci_main(struct slhci_softc *sc)
{
	struct slhci_transfers *t;

	t = &sc->sc_transfers;

	KASSERT(mutex_owned(&sc->sc_intr_lock));

waitcheck:
	slhci_waitintr(sc, slhci_wait_time);

	/*
	 * The direct call is needed in the use_polling and disabled cases
	 * since the soft interrupt is not available.  In the disabled case,
	 * this code can be reached from the usb detach, after the reaping of
	 * the soft interrupt.  That test could be !F_ACTIVE, but there is no
	 * reason not to make the callbacks directly in the other DISABLED
	 * cases.
	 */
	if ((t->flags & F_ROOTINTR) || !gcq_empty(&t->q[Q_CALLBACKS])) {
		if (__predict_false(sc->sc_bus.use_polling ||
		    t->flags & F_DISABLED))
			slhci_callback(sc);
		else
			slhci_callback_schedule(sc);
	}

	if (!gcq_empty(&sc->sc_waitq)) {
		slhci_enter_xfers(sc);
		slhci_dotransfer(sc);
		goto waitcheck;
	}
}

/* End lock entry functions. Start in lock function. */

/* Register read/write routines and barriers. */
#ifdef SLHCI_BUS_SPACE_BARRIERS
#define BSB(a, b, c, d, e) bus_space_barrier(a, b, c, d, BUS_SPACE_BARRIER_ # e)
#define BSB_SYNC(a, b, c, d) bus_space_barrier(a, b, c, d, BUS_SPACE_BARRIER_SYNC)
#else /* now !SLHCI_BUS_SPACE_BARRIERS */
#define BSB(a, b, c, d, e) __USE(d)
#define BSB_SYNC(a, b, c, d)
#endif /* SLHCI_BUS_SPACE_BARRIERS */

static void
slhci_write(struct slhci_softc *sc, uint8_t addr, uint8_t data)
{
	bus_size_t paddr, pdata, pst, psz;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;

	paddr = pst = 0;
	pdata = sc->sc_stride;
	psz = pdata * 2;
	iot = sc->sc_iot;
	ioh = sc->sc_ioh;

	bus_space_write_1(iot, ioh, paddr, addr);
	BSB(iot, ioh, pst, psz, WRITE_BEFORE_WRITE);
	bus_space_write_1(iot, ioh, pdata, data);
	BSB(iot, ioh, pst, psz, WRITE_BEFORE_WRITE);
}

static uint8_t
slhci_read(struct slhci_softc *sc, uint8_t addr)
{
	bus_size_t paddr, pdata, pst, psz;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	uint8_t data;

	paddr = pst = 0;
	pdata = sc->sc_stride;
	psz = pdata * 2;
	iot = sc->sc_iot;
	ioh = sc->sc_ioh;

	bus_space_write_1(iot, ioh, paddr, addr);
	BSB(iot, ioh, pst, psz, WRITE_BEFORE_READ);
	data = bus_space_read_1(iot, ioh, pdata);
	BSB(iot, ioh, pst, psz, READ_BEFORE_WRITE);
	return data;
}

#if 0 /* auto-increment mode broken, see errata doc */
static void
slhci_write_multi(struct slhci_softc *sc, uint8_t addr, uint8_t *buf, int l)
{
	bus_size_t paddr, pdata, pst, psz;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;

	paddr = pst = 0;
	pdata = sc->sc_stride;
	psz = pdata * 2;
	iot = sc->sc_iot;
	ioh = sc->sc_ioh;

	bus_space_write_1(iot, ioh, paddr, addr);
	BSB(iot, ioh, pst, psz, WRITE_BEFORE_WRITE);
	bus_space_write_multi_1(iot, ioh, pdata, buf, l);
	BSB(iot, ioh, pst, psz, WRITE_BEFORE_WRITE);
}

static void
slhci_read_multi(struct slhci_softc *sc, uint8_t addr, uint8_t *buf, int l)
{
	bus_size_t paddr, pdata, pst, psz;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;

	paddr = pst = 0;
	pdata = sc->sc_stride;
	psz = pdata * 2;
	iot = sc->sc_iot;
	ioh = sc->sc_ioh;

	bus_space_write_1(iot, ioh, paddr, addr);
	BSB(iot, ioh, pst, psz, WRITE_BEFORE_READ);
	bus_space_read_multi_1(iot, ioh, pdata, buf, l);
	BSB(iot, ioh, pst, psz, READ_BEFORE_WRITE);
}
#else
static void
slhci_write_multi(struct slhci_softc *sc, uint8_t addr, uint8_t *buf, int l)
{
#if 1
	for (; l; addr++, buf++, l--)
		slhci_write(sc, addr, *buf);
#else
	bus_size_t paddr, pdata, pst, psz;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;

	paddr = pst = 0;
	pdata = sc->sc_stride;
	psz = pdata * 2;
	iot = sc->sc_iot;
	ioh = sc->sc_ioh;

	for (; l; addr++, buf++, l--) {
		bus_space_write_1(iot, ioh, paddr, addr);
		BSB(iot, ioh, pst, psz, WRITE_BEFORE_WRITE);
		bus_space_write_1(iot, ioh, pdata, *buf);
		BSB(iot, ioh, pst, psz, WRITE_BEFORE_WRITE);
	}
#endif
}

static void
slhci_read_multi(struct slhci_softc *sc, uint8_t addr, uint8_t *buf, int l)
{
#if 1
	for (; l; addr++, buf++, l--)
		*buf = slhci_read(sc, addr);
#else
	bus_size_t paddr, pdata, pst, psz;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;

	paddr = pst = 0;
	pdata = sc->sc_stride;
	psz = pdata * 2;
	iot = sc->sc_iot;
	ioh = sc->sc_ioh;

	for (; l; addr++, buf++, l--) {
		bus_space_write_1(iot, ioh, paddr, addr);
		BSB(iot, ioh, pst, psz, WRITE_BEFORE_READ);
		*buf = bus_space_read_1(iot, ioh, pdata);
		BSB(iot, ioh, pst, psz, READ_BEFORE_WRITE);
	}
#endif
}
#endif

/*
 * After calling waitintr it is necessary to either call slhci_callback or
 * schedule the callback if necessary.  The callback cannot be called directly
 * from the hard interrupt since it interrupts at a high IPL and callbacks
 * can do copyout and such.
 */
static void
slhci_waitintr(struct slhci_softc *sc, int wait_time)
{
	struct slhci_transfers *t;

	t = &sc->sc_transfers;

	KASSERT(mutex_owned(&sc->sc_intr_lock));

	if (__predict_false(sc->sc_bus.use_polling))
		wait_time = 12000;

	while (t->pend <= wait_time) {
		DLOG(D_WAIT, "waiting... frame %d pend %d flags %#x",
		    t->frame, t->pend, t->flags, 0);
		LK_SLASSERT(t->flags & F_ACTIVE, sc, NULL, NULL, return);
		LK_SLASSERT(t->flags & (F_AINPROG|F_BINPROG), sc, NULL, NULL,
		    return);
		slhci_dointr(sc);
	}
}

static int
slhci_dointr(struct slhci_softc *sc)
{
	struct slhci_transfers *t;
	struct slhci_pipe *tosp;
	uint8_t r;

	t = &sc->sc_transfers;

	KASSERT(mutex_owned(&sc->sc_intr_lock));

	if (sc->sc_ier == 0)
		return 0;

	r = slhci_read(sc, SL11_ISR);

#ifdef SLHCI_DEBUG
	if (slhci_debug & SLHCI_D_INTR && r & sc->sc_ier &&
	    ((r & ~(SL11_ISR_SOF|SL11_ISR_DATA)) || slhci_debug &
	    SLHCI_D_SOF)) {
		uint8_t e, f;

		e = slhci_read(sc, SL11_IER);
		f = slhci_read(sc, SL11_CTRL);
		DDOLOG("Flags=%#x IER=%#x ISR=%#x", t->flags, e, r, 0);
		DDOLOGFLAG8("Status=", r, "D+", (f & SL11_CTRL_SUSPEND) ?
		    "RESUME" : "NODEV", "INSERT", "SOF", "res", "BABBLE",
		    "USBB", "USBA");
	}
#endif

	/*
	 * check IER for corruption occasionally.  Assume that the above
	 * sc_ier == 0 case works correctly.
	 */
	if (__predict_false(sc->sc_ier_check++ > SLHCI_IER_CHECK_FREQUENCY)) {
		sc->sc_ier_check = 0;
		if (sc->sc_ier != slhci_read(sc, SL11_IER)) {
			printf("%s: IER value corrupted! halted\n",
			    SC_NAME(sc));
			DDOLOG("%s: IER value corrupted! halted\n",
			    SC_NAME(sc), 0,0,0);
			slhci_halt(sc, NULL, NULL);
			return 1;
		}
	}

	r &= sc->sc_ier;

	if (r == 0)
		return 0;

	sc->sc_ier_check = 0;

	slhci_write(sc, SL11_ISR, r);
	BSB_SYNC(sc->iot, sc->ioh, sc->pst, sc->psz);

	/* If we have an insertion event we do not care about anything else. */
	if (__predict_false(r & SL11_ISR_INSERT)) {
		slhci_insert(sc);
		return 1;
	}

	stop_cc_time(&t_intr);
	start_cc_time(&t_intr, r);

	if (r & SL11_ISR_SOF) {
		t->frame++;

		gcq_merge_tail(&t->q[Q_CB], &t->q[Q_NEXT_CB]);

		/*
		 * SOFCHECK flags are cleared in tstart.  Two flags are needed
		 * since the first SOF interrupt processed after the transfer
		 * is started might have been generated before the transfer
		 * was started.
		 */
		if (__predict_false(t->flags & F_SOFCHECK2 && t->flags &
		    (F_AINPROG|F_BINPROG))) {
			printf("%s: Missed transfer completion. halted\n",
			    SC_NAME(sc));
			DDOLOG("%s: Missed transfer completion. halted\n",
			    SC_NAME(sc), 0,0,0);
			slhci_halt(sc, NULL, NULL);
			return 1;
		} else if (t->flags & F_SOFCHECK1) {
			t->flags |= F_SOFCHECK2;
		} else
			t->flags |= F_SOFCHECK1;

		if (t->flags & F_CHANGE)
			t->flags |= F_ROOTINTR;

		while (__predict_true(GOT_FIRST_TO(tosp, t)) &&
		    __predict_false(tosp->to_frame <= t->frame)) {
			tosp->xfer->status = USBD_TIMEOUT;
			slhci_do_abort(sc, tosp, tosp->xfer);
			enter_callback(t, tosp);
		}

		/*
		 * Start any waiting transfers right away.  If none, we will
		 * start any new transfers later.
		 */
		slhci_tstart(sc);
	}

	if (r & (SL11_ISR_USBA|SL11_ISR_USBB)) {
		int ab;

		if ((r & (SL11_ISR_USBA|SL11_ISR_USBB)) ==
		    (SL11_ISR_USBA|SL11_ISR_USBB)) {
			if (!(t->flags & (F_AINPROG|F_BINPROG)))
				return 1; /* presume card pulled */

			LK_SLASSERT((t->flags & (F_AINPROG|F_BINPROG)) !=
			    (F_AINPROG|F_BINPROG), sc, NULL, NULL, return 1);

			/*
			 * This should never happen (unless card removal just
			 * occurred) but appeared frequently when both
			 * transfers were started at the same time and was
			 * accompanied by data corruption.  It still happens
			 * at times.  I have not seen data correption except
			 * when the STATUS bit gets set, which now causes the
			 * driver to halt, however this should still not
			 * happen so the warning is kept.  See comment in
			 * abdone, below.
			 */
			printf("%s: Transfer reported done but not started! "
			    "Verify data integrity if not detaching. "
			    " flags %#x r %x\n", SC_NAME(sc), t->flags, r);

			if (!(t->flags & F_AINPROG))
				r &= ~SL11_ISR_USBA;
			else
				r &= ~SL11_ISR_USBB;
		}
		t->pend = INT_MAX;

		if (r & SL11_ISR_USBA)
			ab = A;
		else
			ab = B;

		/*
		 * This happens when a low speed device is attached to
		 * a hub with chip rev 1.5.  SOF stops, but a few transfers
		 * still work before causing this error.
		 */
		if (!(t->flags & (ab ? F_BINPROG : F_AINPROG))) {
			printf("%s: %s done but not in progress! halted\n",
			    SC_NAME(sc), ab ? "B" : "A");
			DDOLOG("%s: %s done but not in progress! halted\n",
			    SC_NAME(sc), ab ? "B" : "A", 0,0);
			slhci_halt(sc, NULL, NULL);
			return 1;
		}

		t->flags &= ~(ab ? F_BINPROG : F_AINPROG);
		slhci_tstart(sc);
		stop_cc_time(&t_ab[ab]);
		start_cc_time(&t_abdone, t->flags);
		slhci_abdone(sc, ab);
		stop_cc_time(&t_abdone);
	}

	slhci_dotransfer(sc);

	return 1;
}

static void
slhci_abdone(struct slhci_softc *sc, int ab)
{
	struct slhci_transfers *t;
	struct slhci_pipe *spipe;
	struct usbd_xfer *xfer;
	uint8_t status, buf_start;
	uint8_t *target_buf;
	unsigned int actlen;
	int head;

	t = &sc->sc_transfers;

	KASSERT(mutex_owned(&sc->sc_intr_lock));

	DLOG(D_TRACE, "ABDONE flags %#x", t->flags, 0,0,0);

	DLOG(D_MSG, "DONE %s spipe %p len %d xfer %p", ab ? "B" : "A",
	    t->spipe[ab], t->len[ab], t->spipe[ab] ?
	    t->spipe[ab]->xfer : NULL);

	spipe = t->spipe[ab];

	/*
	 * skip this one if aborted; do not call return from the rest of the
	 * function unless halting, else t->len will not be cleared.
	 */
	if (spipe == NULL)
		goto done;

	t->spipe[ab] = NULL;

	xfer = spipe->xfer;

	gcq_remove(&spipe->to);

	LK_SLASSERT(xfer != NULL, sc, spipe, NULL, return);

	status = slhci_read(sc, slhci_tregs[ab][STAT]);

	/*
	 * I saw no status or remaining length greater than the requested
	 * length in early driver versions in circumstances I assumed caused
	 * excess power draw.  I am no longer able to reproduce this when
	 * causing excess power draw circumstances.
	 *
	 * Disabling a power check and attaching aue to a keyboard and hub
	 * that is directly attached (to CFU1U, 100mA max, aue 160mA, keyboard
	 * 98mA) sometimes works and sometimes fails to configure.  After
	 * removing the aue and attaching a self-powered umass dvd reader
	 * (unknown if it draws power from the host also) soon a single Error
	 * status occurs then only timeouts. The controller soon halts freeing
	 * memory due to being ONQU instead of BUSY.  This may be the same
	 * basic sequence that caused the no status/bad length errors.  The
	 * umass device seems to work (better at least) with the keyboard hub
	 * when not first attaching aue (tested once reading an approximately
	 * 200MB file).
	 *
	 * Overflow can indicate that the device and host disagree about how
	 * much data has been transfered.  This may indicate a problem at any
	 * point during the transfer, not just when the error occurs.  It may
	 * indicate data corruption.  A warning message is printed.
	 *
	 * Trying to use both A and B transfers at the same time results in
	 * incorrect transfer completion ISR reports and the status will then
	 * include SL11_EPSTAT_SETUP, which is apparently set while the
	 * transfer is in progress.  I also noticed data corruption, even
	 * after waiting for the transfer to complete. The driver now avoids
	 * trying to start both at the same time.
	 *
	 * I had accidently initialized the B registers before they were valid
	 * in some driver versions.  Since every other performance enhancing
	 * feature has been confirmed buggy in the errata doc, I have not
	 * tried both transfers at once again with the documented
	 * initialization order.
	 *
	 * However, I have seen this problem again ("done but not started"
	 * errors), which in some cases cases the SETUP status bit to remain
	 * set on future transfers.  In other cases, the SETUP bit is not set
	 * and no data corruption occurs.  This occured while using both umass
	 * and aue on a powered hub (maybe triggered by some local activity
	 * also) and needs several reads of the 200MB file to trigger.  The
	 * driver now halts if SETUP is detected.
 	 */

	actlen = 0;

	if (__predict_false(!status)) {
		DDOLOG("no status! xfer %p spipe %p", xfer, spipe, 0,0);
		printf("%s: no status! halted\n", SC_NAME(sc));
		slhci_halt(sc, spipe, xfer);
		return;
	}

#ifdef SLHCI_DEBUG
	if (slhci_debug & SLHCI_D_NAK || (status & SL11_EPSTAT_ERRBITS) !=
	    SL11_EPSTAT_NAK)
		DLOGFLAG8(D_XFER, "STATUS=", status, "STALL", "NAK",
		    "Overflow", "Setup", "Data Toggle", "Timeout", "Error",
		    "ACK");
#endif

	if (!(status & SL11_EPSTAT_ERRBITS)) {
		unsigned int cont;
		cont = slhci_read(sc, slhci_tregs[ab][CONT]);
		if (cont != 0)
			DLOG(D_XFER, "cont %d len %d", cont,
			    spipe->tregs[LEN], 0,0);
		if (__predict_false(cont > spipe->tregs[LEN])) {
			DDOLOG("cont > len! cont %d len %d xfer->length %d "
			    "spipe %p", cont, spipe->tregs[LEN], xfer->length,
			    spipe);
			printf("%s: cont > len! cont %d len %d xfer->length "
			    "%d", SC_NAME(sc), cont, spipe->tregs[LEN],
			    xfer->length);
			slhci_halt(sc, spipe, xfer);
			return;
		} else {
			spipe->nerrs = 0;
			actlen = spipe->tregs[LEN] - cont;
		}
	}

	/* Actual copyin done after starting next transfer. */
	if (actlen && (spipe->tregs[PID] & SL11_PID_BITS) == SL11_PID_IN) {
		target_buf = spipe->buffer;
		buf_start = spipe->tregs[ADR];
	} else {
		target_buf = NULL;
		buf_start = 0; /* XXX gcc uninitialized warnings */
	}

	if (status & SL11_EPSTAT_ERRBITS) {
		status &= SL11_EPSTAT_ERRBITS;
		if (status & SL11_EPSTAT_SETUP) {
			printf("%s: Invalid controller state detected! "
			    "halted\n", SC_NAME(sc));
			DDOLOG("%s: Invalid controller state detected! "
			    "halted\n", SC_NAME(sc), 0,0,0);
			slhci_halt(sc, spipe, xfer);
			return;
		} else if (__predict_false(sc->sc_bus.use_polling)) {
			if (status == SL11_EPSTAT_STALL)
				xfer->status = USBD_STALLED;
			else if (status == SL11_EPSTAT_TIMEOUT)
				xfer->status = USBD_TIMEOUT;
			else if (status == SL11_EPSTAT_NAK)
				xfer->status = USBD_TIMEOUT; /*XXX*/
			else
				xfer->status = USBD_IOERROR;
			head = Q_CALLBACKS;
		} else if (status == SL11_EPSTAT_NAK) {
			if (spipe->pipe.interval) {
				spipe->lastframe = spipe->frame =
				    t->frame + spipe->pipe.interval;
				slhci_queue_timed(sc, spipe);
				goto queued;
			}
			head = Q_NEXT_CB;
		} else if (++spipe->nerrs > SLHCI_MAX_RETRIES ||
		    status == SL11_EPSTAT_STALL) {
			if (status == SL11_EPSTAT_STALL)
				xfer->status = USBD_STALLED;
			else if (status == SL11_EPSTAT_TIMEOUT)
				xfer->status = USBD_TIMEOUT;
			else
				xfer->status = USBD_IOERROR;

			DLOG(D_ERR, "Max retries reached! status %#x "
			    "xfer->status %#x", status, xfer->status, 0,0);
			DLOGFLAG8(D_ERR, "STATUS=", status, "STALL",
			    "NAK", "Overflow", "Setup", "Data Toggle",
			    "Timeout", "Error", "ACK");

			if (status == SL11_EPSTAT_OVERFLOW &&
			    ratecheck(&sc->sc_overflow_warn_rate,
			    &overflow_warn_rate)) {
				printf("%s: Overflow condition: "
				    "data corruption possible\n",
				    SC_NAME(sc));
				DDOLOG("%s: Overflow condition: "
				    "data corruption possible\n",
				    SC_NAME(sc), 0,0,0);
			}
			head = Q_CALLBACKS;
		} else {
			head = Q_NEXT_CB;
		}
	} else if (spipe->ptype == PT_CTRL_SETUP) {
		spipe->tregs[PID] = spipe->newpid;

		if (xfer->length) {
			LK_SLASSERT(spipe->newlen[1] != 0, sc, spipe, xfer,
			    return);
			spipe->tregs[LEN] = spipe->newlen[1];
			spipe->bustime = spipe->newbustime[1];
			spipe->buffer = KERNADDR(&xfer->dmabuf, 0);
			spipe->ptype = PT_CTRL_DATA;
		} else {
status_setup:
			/* CTRL_DATA swaps direction in PID then jumps here */
			spipe->tregs[LEN] = 0;
			if (spipe->pflags & PF_LS)
				spipe->bustime = SLHCI_LS_CONST;
			else
				spipe->bustime = SLHCI_FS_CONST;
			spipe->ptype = PT_CTRL_STATUS;
			spipe->buffer = NULL;
		}

		/* Status or first data packet must be DATA1. */
		spipe->control |= SL11_EPCTRL_DATATOGGLE;
		if ((spipe->tregs[PID] & SL11_PID_BITS) == SL11_PID_IN)
			spipe->control &= ~SL11_EPCTRL_DIRECTION;
		else
			spipe->control |= SL11_EPCTRL_DIRECTION;

		head = Q_CB;
	} else if (spipe->ptype == PT_CTRL_STATUS) {
		head = Q_CALLBACKS;
	} else { /* bulk, intr, control data */
		xfer->actlen += actlen;
		spipe->control ^= SL11_EPCTRL_DATATOGGLE;

		if (actlen == spipe->tregs[LEN] && (xfer->length >
		    xfer->actlen || spipe->wantshort)) {
			spipe->buffer += actlen;
			LK_SLASSERT(xfer->length >= xfer->actlen, sc,
			    spipe, xfer, return);
			if (xfer->length - xfer->actlen < actlen) {
				spipe->wantshort = 0;
				spipe->tregs[LEN] = spipe->newlen[0];
				spipe->bustime = spipe->newbustime[0];
				LK_SLASSERT(xfer->actlen +
				    spipe->tregs[LEN] == xfer->length, sc,
				    spipe, xfer, return);
			}
			head = Q_CB;
		} else if (spipe->ptype == PT_CTRL_DATA) {
			spipe->tregs[PID] ^= SLHCI_PID_SWAP_IN_OUT;
			goto status_setup;
		} else {
			if (spipe->ptype == PT_INTR) {
				spipe->lastframe +=
				    spipe->pipe.interval;
				/*
				 * If ack, we try to keep the
				 * interrupt rate by using lastframe
				 * instead of the current frame.
				 */
				spipe->frame = spipe->lastframe +
				    spipe->pipe.interval;
			}

			/*
			 * Set the toggle for the next transfer.  It
			 * has already been toggled above, so the
			 * current setting will apply to the next
			 * transfer.
			 */
			if (spipe->control & SL11_EPCTRL_DATATOGGLE)
				spipe->pflags |= PF_TOGGLE;
			else
				spipe->pflags &= ~PF_TOGGLE;

			head = Q_CALLBACKS;
		}
	}

	if (head == Q_CALLBACKS) {
		gcq_remove(&spipe->to);

	 	if (xfer->status == USBD_IN_PROGRESS) {
			LK_SLASSERT(xfer->actlen <= xfer->length, sc,
			    spipe, xfer, return);
			xfer->status = USBD_NORMAL_COMPLETION;
#if 0 /* usb_transfer_complete will do this */
			if (xfer->length == xfer->actlen || xfer->flags &
			    USBD_SHORT_XFER_OK)
				xfer->status = USBD_NORMAL_COMPLETION;
			else
				xfer->status = USBD_SHORT_XFER;
#endif
		}
	}

	enter_q(t, spipe, head);

queued:
	if (target_buf != NULL) {
		slhci_dotransfer(sc);
		start_cc_time(&t_copy_from_dev, actlen);
		slhci_read_multi(sc, buf_start, target_buf, actlen);
		stop_cc_time(&t_copy_from_dev);
		DLOGBUF(D_BUF, target_buf, actlen);
		t->pend -= SLHCI_FS_CONST + SLHCI_FS_DATA_TIME(actlen);
	}

done:
	t->len[ab] = -1;
}

static void
slhci_tstart(struct slhci_softc *sc)
{
	struct slhci_transfers *t;
	struct slhci_pipe *spipe;
	int remaining_bustime;

	t = &sc->sc_transfers;

	KASSERT(mutex_owned(&sc->sc_intr_lock));

	if (!(t->flags & (F_AREADY|F_BREADY)))
		return;

	if (t->flags & (F_AINPROG|F_BINPROG|F_DISABLED))
		return;

	/*
	 * We have about 6 us to get from the bus time check to
	 * starting the transfer or we might babble or the chip might fail to
	 * signal transfer complete.  This leaves no time for any other
	 * interrupts.
	 */
	remaining_bustime = (int)(slhci_read(sc, SL811_CSOF)) << 6;
	remaining_bustime -= SLHCI_END_BUSTIME;

	/*
	 * Start one transfer only, clearing any aborted transfers that are
	 * not yet in progress and skipping missed isoc. It is easier to copy
	 * & paste most of the A/B sections than to make the logic work
	 * otherwise and this allows better constant use.
	 */
	if (t->flags & F_AREADY) {
		spipe = t->spipe[A];
		if (spipe == NULL) {
			t->flags &= ~F_AREADY;
			t->len[A] = -1;
		} else if (remaining_bustime >= spipe->bustime) {
			t->flags &= ~(F_AREADY|F_SOFCHECK1|F_SOFCHECK2);
			t->flags |= F_AINPROG;
			start_cc_time(&t_ab[A], spipe->tregs[LEN]);
			slhci_write(sc, SL11_E0CTRL, spipe->control);
			goto pend;
		}
	}
	if (t->flags & F_BREADY) {
		spipe = t->spipe[B];
		if (spipe == NULL) {
			t->flags &= ~F_BREADY;
			t->len[B] = -1;
		} else if (remaining_bustime >= spipe->bustime) {
			t->flags &= ~(F_BREADY|F_SOFCHECK1|F_SOFCHECK2);
			t->flags |= F_BINPROG;
			start_cc_time(&t_ab[B], spipe->tregs[LEN]);
			slhci_write(sc, SL11_E1CTRL, spipe->control);
pend:
			t->pend = spipe->bustime;
		}
	}
}

static void
slhci_dotransfer(struct slhci_softc *sc)
{
	struct slhci_transfers *t;
	struct slhci_pipe *spipe;
	int ab, i;

	t = &sc->sc_transfers;

	KASSERT(mutex_owned(&sc->sc_intr_lock));

 	while ((t->len[A] == -1 || t->len[B] == -1) &&
	    (GOT_FIRST_TIMED_COND(spipe, t, spipe->frame <= t->frame) ||
	    GOT_FIRST_CB(spipe, t))) {
		LK_SLASSERT(spipe->xfer != NULL, sc, spipe, NULL, return);
		LK_SLASSERT(spipe->ptype != PT_ROOT_CTRL && spipe->ptype !=
		    PT_ROOT_INTR, sc, spipe, NULL, return);

		/* Check that this transfer can fit in the remaining memory. */
		if (t->len[A] + t->len[B] + spipe->tregs[LEN] + 1 >
		    SL11_MAX_PACKET_SIZE) {
			DLOG(D_XFER, "Transfer does not fit. alen %d blen %d "
			    "len %d", t->len[A], t->len[B], spipe->tregs[LEN],
			    0);
			return;
		}

		gcq_remove(&spipe->xq);

		if (t->len[A] == -1) {
			ab = A;
			spipe->tregs[ADR] = SL11_BUFFER_START;
		} else {
			ab = B;
			spipe->tregs[ADR] = SL11_BUFFER_END -
			    spipe->tregs[LEN];
		}

		t->len[ab] = spipe->tregs[LEN];

		if (spipe->tregs[LEN] && (spipe->tregs[PID] & SL11_PID_BITS)
		    != SL11_PID_IN) {
			start_cc_time(&t_copy_to_dev,
			    spipe->tregs[LEN]);
			slhci_write_multi(sc, spipe->tregs[ADR],
			    spipe->buffer, spipe->tregs[LEN]);
			stop_cc_time(&t_copy_to_dev);
			t->pend -= SLHCI_FS_CONST +
			    SLHCI_FS_DATA_TIME(spipe->tregs[LEN]);
		}

		DLOG(D_MSG, "NEW TRANSFER %s flags %#x alen %d blen %d",
		    ab ? "B" : "A", t->flags, t->len[0], t->len[1]);

		if (spipe->tregs[LEN])
			i = 0;
		else
			i = 1;

		for (; i <= 3; i++)
			if (t->current_tregs[ab][i] != spipe->tregs[i]) {
				t->current_tregs[ab][i] = spipe->tregs[i];
				slhci_write(sc, slhci_tregs[ab][i],
				    spipe->tregs[i]);
			}

		DLOG(D_SXFER, "Transfer len %d pid %#x dev %d type %s",
		    spipe->tregs[LEN], spipe->tregs[PID], spipe->tregs[DEV],
	    	    pnames(spipe->ptype));

		t->spipe[ab] = spipe;
		t->flags |= ab ? F_BREADY : F_AREADY;

		slhci_tstart(sc);
	}
}

/*
 * slhci_callback is called after the lock is taken from splusb.
 */
static void
slhci_callback(struct slhci_softc *sc)
{
	struct slhci_transfers *t;
	struct slhci_pipe *spipe;
	struct usbd_xfer *xfer;

	t = &sc->sc_transfers;

	KASSERT(mutex_owned(&sc->sc_intr_lock));

	DLOG(D_SOFT, "CB flags %#x", t->flags, 0,0,0);
	for (;;) {
		if (__predict_false(t->flags & F_ROOTINTR)) {
			t->flags &= ~F_ROOTINTR;
			if (t->rootintr != NULL) {
				u_char *p;

				p = KERNADDR(&t->rootintr->dmabuf, 0);
				p[0] = 2;
				t->rootintr->actlen = 1;
				t->rootintr->status = USBD_NORMAL_COMPLETION;
				xfer = t->rootintr;
				goto do_callback;
			}
		}


		if (!DEQUEUED_CALLBACK(spipe, t))
			return;

		xfer = spipe->xfer;
		LK_SLASSERT(xfer != NULL, sc, spipe, NULL, return);
		spipe->xfer = NULL;
		DLOG(D_XFER, "xfer callback length %d actlen %d spipe %x "
		    "type %s", xfer->length, xfer->actlen, spipe,
		    pnames(spipe->ptype));
do_callback:
		slhci_do_callback(sc, xfer);
	}
}

static void
slhci_enter_xfer(struct slhci_softc *sc, struct slhci_pipe *spipe)
{
	struct slhci_transfers *t;

	t = &sc->sc_transfers;

	KASSERT(mutex_owned(&sc->sc_intr_lock));

	if (__predict_false(t->flags & F_DISABLED) ||
	    __predict_false(spipe->pflags & PF_GONE)) {
		DLOG(D_MSG, "slhci_enter_xfer: DISABLED or GONE", 0,0,0,0);
		spipe->xfer->status = USBD_CANCELLED;
	}

	if (spipe->xfer->status == USBD_IN_PROGRESS) {
		if (spipe->xfer->timeout) {
			spipe->to_frame = t->frame + spipe->xfer->timeout;
			slhci_xfer_timer(sc, spipe);
		}
		if (spipe->pipe.interval)
			slhci_queue_timed(sc, spipe);
		else
			enter_q(t, spipe, Q_CB);
	} else
		enter_callback(t, spipe);
}

static void
slhci_enter_xfers(struct slhci_softc *sc)
{
	struct slhci_pipe *spipe;

	KASSERT(mutex_owned(&sc->sc_intr_lock));

	while (DEQUEUED_WAITQ(spipe, sc))
		slhci_enter_xfer(sc, spipe);
}

static void
slhci_queue_timed(struct slhci_softc *sc, struct slhci_pipe *spipe)
{
	struct slhci_transfers *t;
	struct gcq *q;
	struct slhci_pipe *spp;

	t = &sc->sc_transfers;

	KASSERT(mutex_owned(&sc->sc_intr_lock));

	FIND_TIMED(q, t, spp, spp->frame > spipe->frame);
	gcq_insert_before(q, &spipe->xq);
}

static void
slhci_xfer_timer(struct slhci_softc *sc, struct slhci_pipe *spipe)
{
	struct slhci_transfers *t;
	struct gcq *q;
	struct slhci_pipe *spp;

	t = &sc->sc_transfers;

	KASSERT(mutex_owned(&sc->sc_intr_lock));

	FIND_TO(q, t, spp, spp->to_frame >= spipe->to_frame);
	gcq_insert_before(q, &spipe->to);
}

static void
slhci_do_repeat(struct slhci_softc *sc, struct usbd_xfer *xfer)
{
	struct slhci_transfers *t;
	struct slhci_pipe *spipe;

	t = &sc->sc_transfers;
	spipe = (struct slhci_pipe *)xfer->pipe;

	if (xfer == t->rootintr)
		return;

	DLOG(D_TRACE, "REPEAT: xfer %p actlen %d frame %u now %u",
	    xfer, xfer->actlen, spipe->frame, sc->sc_transfers.frame);

	xfer->actlen = 0;
	spipe->xfer = xfer;
	if (spipe->tregs[LEN])
		KASSERT(spipe->buffer == KERNADDR(&xfer->dmabuf, 0));
	slhci_queue_timed(sc, spipe);
	slhci_dotransfer(sc);
}

static void
slhci_callback_schedule(struct slhci_softc *sc)
{
	struct slhci_transfers *t;

	t = &sc->sc_transfers;

	KASSERT(mutex_owned(&sc->sc_intr_lock));

	if (t->flags & F_ACTIVE)
		slhci_do_callback_schedule(sc);
}

static void
slhci_do_callback_schedule(struct slhci_softc *sc)
{
	struct slhci_transfers *t;

	t = &sc->sc_transfers;

	KASSERT(mutex_owned(&sc->sc_intr_lock));

	if (!(t->flags & F_CALLBACK)) {
		t->flags |= F_CALLBACK;
		softint_schedule(sc->sc_cb_softintr);
	}
}

#if 0
/* must be called with lock taken from IPL_USB */
/* XXX static */ void
slhci_pollxfer(struct slhci_softc *sc, struct usbd_xfer *xfer)
{
	KASSERT(mutex_owned(&sc->sc_intr_lock));
	slhci_dotransfer(sc);
	do {
		slhci_dointr(sc);
	} while (xfer->status == USBD_IN_PROGRESS);
	slhci_do_callback(sc, xfer);
}
#endif

static usbd_status
slhci_do_poll(struct slhci_softc *sc, struct slhci_pipe *spipe, struct
    usbd_xfer *xfer)
{
	slhci_waitintr(sc, 0);

	return USBD_NORMAL_COMPLETION;
}

static usbd_status
slhci_lsvh_warn(struct slhci_softc *sc, struct slhci_pipe *spipe, struct
    usbd_xfer *xfer)
{
	struct slhci_transfers *t;

	t = &sc->sc_transfers;

	if (!(t->flags & F_LSVH_WARNED)) {
		printf("%s: Low speed device via hub disabled, "
		    "see slhci(4)\n", SC_NAME(sc));
		DDOLOG("%s: Low speed device via hub disabled, "
		    "see slhci(4)\n", SC_NAME(sc), 0,0,0);
		t->flags |= F_LSVH_WARNED;
	}
	return USBD_INVAL;
}

static usbd_status
slhci_isoc_warn(struct slhci_softc *sc, struct slhci_pipe *spipe, struct
    usbd_xfer *xfer)
{
	struct slhci_transfers *t;

	t = &sc->sc_transfers;

	if (!(t->flags & F_ISOC_WARNED)) {
		printf("%s: ISOC transfer not supported "
		    "(see slhci(4))\n", SC_NAME(sc));
		DDOLOG("%s: ISOC transfer not supported "
		    "(see slhci(4))\n", SC_NAME(sc), 0,0,0);
		t->flags |= F_ISOC_WARNED;
	}
	return USBD_INVAL;
}

static usbd_status
slhci_open_pipe(struct slhci_softc *sc, struct slhci_pipe *spipe, struct
    usbd_xfer *xfer)
{
	struct slhci_transfers *t;
	struct usbd_pipe *pipe;

	t = &sc->sc_transfers;
	pipe = &spipe->pipe;

	if (t->flags & F_DISABLED)
		return USBD_CANCELLED;
	else if (pipe->interval && !slhci_reserve_bustime(sc, spipe, 1))
		return USBD_PENDING_REQUESTS;
	else {
		enter_all_pipes(t, spipe);
		return USBD_NORMAL_COMPLETION;
	}
}

static usbd_status
slhci_close_pipe(struct slhci_softc *sc, struct slhci_pipe *spipe, struct
    usbd_xfer *xfer)
{
	struct usbd_pipe *pipe;

	pipe = &spipe->pipe;

	if (pipe->interval && spipe->ptype != PT_ROOT_INTR)
		slhci_reserve_bustime(sc, spipe, 0);
	gcq_remove(&spipe->ap);
	return USBD_NORMAL_COMPLETION;
}

static usbd_status
slhci_do_abort(struct slhci_softc *sc, struct slhci_pipe *spipe, struct
    usbd_xfer *xfer)
{
	struct slhci_transfers *t;

	t = &sc->sc_transfers;

	KASSERT(mutex_owned(&sc->sc_intr_lock));

	if (spipe->xfer == xfer) {
		if (spipe->ptype == PT_ROOT_INTR) {
			if (t->rootintr == spipe->xfer) /* XXX assert? */
				t->rootintr = NULL;
		} else {
			gcq_remove(&spipe->to);
			gcq_remove(&spipe->xq);

			if (t->spipe[A] == spipe) {
				t->spipe[A] = NULL;
				if (!(t->flags & F_AINPROG))
					t->len[A] = -1;
			} else if (t->spipe[B] == spipe) {
					t->spipe[B] = NULL;
				if (!(t->flags & F_BINPROG))
					t->len[B] = -1;
			}
		}

		if (xfer->status != USBD_TIMEOUT) {
			spipe->xfer = NULL;
			spipe->pipe.repeat = 0; /* XXX timeout? */
		}
	}

	return USBD_NORMAL_COMPLETION;
}

/*
 * Called to deactivate or stop use of the controller instead of panicking.
 * Will cancel the xfer correctly even when not on a list.
 */
static usbd_status
slhci_halt(struct slhci_softc *sc, struct slhci_pipe *spipe, struct usbd_xfer
    *xfer)
{
	struct slhci_transfers *t;

	KASSERT(mutex_owned(&sc->sc_intr_lock));

	t = &sc->sc_transfers;

	DDOLOG("Halt! sc %p spipe %p xfer %p", sc, spipe, xfer, 0);

	if (spipe != NULL)
		slhci_log_spipe(spipe);

	if (xfer != NULL)
		slhci_log_xfer(xfer);

	if (spipe != NULL && xfer != NULL && spipe->xfer == xfer &&
	    !gcq_onlist(&spipe->xq) && t->spipe[A] != spipe && t->spipe[B] !=
	    spipe) {
		xfer->status = USBD_CANCELLED;
		enter_callback(t, spipe);
	}

	if (t->flags & F_ACTIVE) {
		slhci_intrchange(sc, 0);
		/*
		 * leave power on when halting in case flash devices or disks
		 * are attached, which may be writing and could be damaged
		 * by abrupt power loss.  The root hub clear power feature
		 * should still work after halting.
		 */
	}

	t->flags &= ~F_ACTIVE;
	t->flags |= F_UDISABLED;
	if (!(t->flags & F_NODEV))
		t->flags |= F_NODEV|F_CCONNECT|F_ROOTINTR;
	slhci_drain(sc);

	/* One last callback for the drain and device removal. */
	slhci_do_callback_schedule(sc);

	return USBD_NORMAL_COMPLETION;
}

/*
 * There are three interrupt states: no interrupts during reset and after
 * device deactivation, INSERT only for no device present but power on, and
 * SOF, INSERT, ADONE, and BDONE when device is present.
 */
static void
slhci_intrchange(struct slhci_softc *sc, uint8_t new_ier)
{
	KASSERT(mutex_owned(&sc->sc_intr_lock));
	if (sc->sc_ier != new_ier) {
		sc->sc_ier = new_ier;
		slhci_write(sc, SL11_IER, new_ier);
		BSB_SYNC(sc->iot, sc->ioh, sc->pst, sc->psz);
	}
}

/*
 * Drain: cancel all pending transfers and put them on the callback list and
 * set the UDISABLED flag.  UDISABLED is cleared only by reset.
 */
static void
slhci_drain(struct slhci_softc *sc)
{
	struct slhci_transfers *t;
	struct slhci_pipe *spipe;
	struct gcq *q;
	int i;

 	KASSERT(mutex_owned(&sc->sc_intr_lock));

	t = &sc->sc_transfers;

	DLOG(D_MSG, "DRAIN flags %#x", t->flags, 0,0,0);

	t->pend = INT_MAX;

	for (i=0; i<=1; i++) {
		t->len[i] = -1;
		if (t->spipe[i] != NULL) {
			enter_callback(t, t->spipe[i]);
			t->spipe[i] = NULL;
		}
	}

	/* Merge the queues into the callback queue. */
	gcq_merge_tail(&t->q[Q_CALLBACKS], &t->q[Q_CB]);
	gcq_merge_tail(&t->q[Q_CALLBACKS], &t->q[Q_NEXT_CB]);
	gcq_merge_tail(&t->q[Q_CALLBACKS], &t->timed);

	/*
	 * Cancel all pipes.  Note that not all of these may be on the
	 * callback queue yet; some could be in slhci_start, for example.
	 */
	FOREACH_AP(q, t, spipe) {
		spipe->pflags |= PF_GONE;
		spipe->pipe.repeat = 0;
		spipe->pipe.aborting = 1;
		if (spipe->xfer != NULL)
			spipe->xfer->status = USBD_CANCELLED;
	}

	gcq_remove_all(&t->to);

	t->flags |= F_UDISABLED;
	t->flags &= ~(F_AREADY|F_BREADY|F_AINPROG|F_BINPROG|F_LOWSPEED);
}

/*
 * RESET: SL11_CTRL_RESETENGINE=1 and SL11_CTRL_JKSTATE=0 for 50ms
 * reconfigure SOF after reset, must wait 2.5us before USB bus activity (SOF)
 * check attached device speed.
 * must wait 100ms before USB transaction according to app note, 10ms
 * by spec.  uhub does this delay
 *
 * Started from root hub set feature reset, which does step one.
 * use_polling will call slhci_reset directly, otherwise the callout goes
 * through slhci_reset_entry.
 */
void
slhci_reset(struct slhci_softc *sc)
{
	struct slhci_transfers *t;
	struct slhci_pipe *spipe;
	struct gcq *q;
	uint8_t r, pol, ctrl;

	t = &sc->sc_transfers;
	KASSERT(mutex_owned(&sc->sc_intr_lock));

	stop_cc_time(&t_delay);

	KASSERT(t->flags & F_ACTIVE);

	start_cc_time(&t_delay, 0);
	stop_cc_time(&t_delay);

	slhci_write(sc, SL11_CTRL, 0);
	start_cc_time(&t_delay, 3);
	DELAY(3);
	stop_cc_time(&t_delay);
	slhci_write(sc, SL11_ISR, 0xff);

	r = slhci_read(sc, SL11_ISR);

	if (r & SL11_ISR_INSERT)
		slhci_write(sc, SL11_ISR, SL11_ISR_INSERT);

	if (r & SL11_ISR_NODEV) {
		DLOG(D_MSG, "NC", 0,0,0,0);
		/*
		 * Normally, the hard interrupt insert routine will issue
		 * CCONNECT, however we need to do it here if the detach
		 * happened during reset.
		 */
		if (!(t->flags & F_NODEV))
			t->flags |= F_CCONNECT|F_ROOTINTR|F_NODEV;
		slhci_intrchange(sc, SL11_IER_INSERT);
	} else {
		if (t->flags & F_NODEV)
			t->flags |= F_CCONNECT;
		t->flags &= ~(F_NODEV|F_LOWSPEED);
		if (r & SL11_ISR_DATA) {
			DLOG(D_MSG, "FS", 0,0,0,0);
			pol = ctrl = 0;
		} else {
			DLOG(D_MSG, "LS", 0,0,0,0);
			pol  = SL811_CSOF_POLARITY;
			ctrl = SL11_CTRL_LOWSPEED;
			t->flags |= F_LOWSPEED;
		}

		/* Enable SOF auto-generation */
		t->frame = 0;	/* write to SL811_CSOF will reset frame */
		slhci_write(sc, SL11_SOFTIME, 0xe0);
		slhci_write(sc, SL811_CSOF, pol|SL811_CSOF_MASTER|0x2e);
		slhci_write(sc, SL11_CTRL, ctrl|SL11_CTRL_ENABLESOF);

		/*
		 * According to the app note, ARM must be set
		 * for SOF generation to work.  We initialize all
		 * USBA registers here for current_tregs.
		 */
		slhci_write(sc, SL11_E0ADDR, SL11_BUFFER_START);
		slhci_write(sc, SL11_E0LEN, 0);
		slhci_write(sc, SL11_E0PID, SL11_PID_SOF);
		slhci_write(sc, SL11_E0DEV, 0);
		slhci_write(sc, SL11_E0CTRL, SL11_EPCTRL_ARM);

		/*
		 * Initialize B registers.  This can't be done earlier since
		 * they are not valid until the SL811_CSOF register is written
		 * above due to SL11H compatability.
		 */
		slhci_write(sc, SL11_E1ADDR, SL11_BUFFER_END - 8);
		slhci_write(sc, SL11_E1LEN, 0);
		slhci_write(sc, SL11_E1PID, 0);
		slhci_write(sc, SL11_E1DEV, 0);

		t->current_tregs[0][ADR] = SL11_BUFFER_START;
		t->current_tregs[0][LEN] = 0;
		t->current_tregs[0][PID] = SL11_PID_SOF;
		t->current_tregs[0][DEV] = 0;
		t->current_tregs[1][ADR] = SL11_BUFFER_END - 8;
		t->current_tregs[1][LEN] = 0;
		t->current_tregs[1][PID] = 0;
		t->current_tregs[1][DEV] = 0;

		/* SOF start will produce USBA interrupt */
		t->len[A] = 0;
		t->flags |= F_AINPROG;

		slhci_intrchange(sc, SLHCI_NORMAL_INTERRUPTS);
	}

	t->flags &= ~(F_UDISABLED|F_RESET);
	t->flags |= F_CRESET|F_ROOTINTR;
	FOREACH_AP(q, t, spipe) {
		spipe->pflags &= ~PF_GONE;
		spipe->pipe.aborting = 0;
	}
	DLOG(D_MSG, "RESET done flags %#x", t->flags, 0,0,0);
}

/* returns 1 if succeeded, 0 if failed, reserve == 0 is unreserve */
static int
slhci_reserve_bustime(struct slhci_softc *sc, struct slhci_pipe *spipe, int
    reserve)
{
	struct slhci_transfers *t;
	int bustime, max_packet;

	KASSERT(mutex_owned(&sc->sc_intr_lock));

	t = &sc->sc_transfers;
	max_packet = UGETW(spipe->pipe.endpoint->edesc->wMaxPacketSize);

	if (spipe->pflags & PF_LS)
		bustime = SLHCI_LS_CONST + SLHCI_LS_DATA_TIME(max_packet);
	else
		bustime = SLHCI_FS_CONST + SLHCI_FS_DATA_TIME(max_packet);

	if (!reserve) {
		t->reserved_bustime -= bustime;
#ifdef DIAGNOSTIC
		if (t->reserved_bustime < 0) {
			printf("%s: reserved_bustime %d < 0!\n",
			    SC_NAME(sc), t->reserved_bustime);
			DDOLOG("%s: reserved_bustime %d < 0!\n",
			    SC_NAME(sc), t->reserved_bustime, 0,0);
			t->reserved_bustime = 0;
		}
#endif
		return 1;
	}

	if (t->reserved_bustime + bustime > SLHCI_RESERVED_BUSTIME) {
		if (ratecheck(&sc->sc_reserved_warn_rate,
		    &reserved_warn_rate))
#ifdef SLHCI_NO_OVERTIME
		{
			printf("%s: Max reserved bus time exceeded! "
			    "Erroring request.\n", SC_NAME(sc));
			DDOLOG("%s: Max reserved bus time exceeded! "
			    "Erroring request.\n", SC_NAME(sc), 0,0,0);
		}
		return 0;
#else
		{
			printf("%s: Reserved bus time exceeds %d!\n",
			    SC_NAME(sc), SLHCI_RESERVED_BUSTIME);
			DDOLOG("%s: Reserved bus time exceeds %d!\n",
			    SC_NAME(sc), SLHCI_RESERVED_BUSTIME, 0,0);
		}
#endif
	}

	t->reserved_bustime += bustime;
	return 1;
}

/* Device insertion/removal interrupt */
static void
slhci_insert(struct slhci_softc *sc)
{
	struct slhci_transfers *t;

	t = &sc->sc_transfers;

	KASSERT(mutex_owned(&sc->sc_intr_lock));

	if (t->flags & F_NODEV)
		slhci_intrchange(sc, 0);
	else {
		slhci_drain(sc);
		slhci_intrchange(sc, SL11_IER_INSERT);
	}
	t->flags ^= F_NODEV;
	t->flags |= F_ROOTINTR|F_CCONNECT;
	DLOG(D_MSG, "INSERT intr: flags after %#x", t->flags, 0,0,0);
}

/*
 * Data structures and routines to emulate the root hub.
 */
static const usb_device_descriptor_t slhci_devd = {
	USB_DEVICE_DESCRIPTOR_SIZE,
	UDESC_DEVICE,		/* type */
	{0x01, 0x01},		/* USB version */
	UDCLASS_HUB,		/* class */
	UDSUBCLASS_HUB,		/* subclass */
	0,			/* protocol */
	64,			/* max packet */
	{USB_VENDOR_SCANLOGIC & 0xff,	/* vendor ID (low)  */
	 USB_VENDOR_SCANLOGIC >> 8  },	/* vendor ID (high) */
	{0} /* ? */,		/* product ID */
	{0},			/* device */
	1,			/* index to manufacturer */
	2,			/* index to product */
	0,			/* index to serial number */
	1			/* number of configurations */
};

static const struct slhci_confd_t {
	const usb_config_descriptor_t confd;
	const usb_interface_descriptor_t ifcd;
	const usb_endpoint_descriptor_t endpd;
} UPACKED slhci_confd = {
	{ /* Configuration */
		USB_CONFIG_DESCRIPTOR_SIZE,
		UDESC_CONFIG,
		{USB_CONFIG_DESCRIPTOR_SIZE +
		 USB_INTERFACE_DESCRIPTOR_SIZE +
		 USB_ENDPOINT_DESCRIPTOR_SIZE},
		1,			/* number of interfaces */
		1,			/* configuration value */
		0,			/* index to configuration */
		UC_SELF_POWERED,	/* attributes */
		0			/* max current, filled in later */
	}, { /* Interface */
		USB_INTERFACE_DESCRIPTOR_SIZE,
		UDESC_INTERFACE,
		0,			/* interface number */
		0,			/* alternate setting */
		1,			/* number of endpoint */
		UICLASS_HUB,		/* class */
		UISUBCLASS_HUB,		/* subclass */
		0,			/* protocol */
		0			/* index to interface */
	}, { /* Endpoint */
		USB_ENDPOINT_DESCRIPTOR_SIZE,
		UDESC_ENDPOINT,
		UE_DIR_IN | ROOT_INTR_ENDPT,	/* endpoint address */
		UE_INTERRUPT,			/* attributes */
		{240, 0},			/* max packet size */
		255				/* interval */
	}
};

static const usb_hub_descriptor_t slhci_hubd = {
	USB_HUB_DESCRIPTOR_SIZE,
	UDESC_HUB,
	1,			/* number of ports */
	{UHD_PWR_INDIVIDUAL | UHD_OC_NONE, 0},	/* hub characteristics */
	50,			/* 5:power on to power good, units of 2ms */
	0,			/* 6:maximum current, filled in later */
	{ 0x00 },		/* port is removable */
	{ 0x00 }		/* port power control mask */
};

static usbd_status
slhci_clear_feature(struct slhci_softc *sc, unsigned int what)
{
	struct slhci_transfers *t;
	usbd_status error;

	t = &sc->sc_transfers;
	error = USBD_NORMAL_COMPLETION;

	KASSERT(mutex_owned(&sc->sc_intr_lock));

	if (what == UHF_PORT_POWER) {
		DLOG(D_MSG, "POWER_OFF", 0,0,0,0);
		t->flags &= ~F_POWER;
		if (!(t->flags & F_NODEV))
			t->flags |= F_NODEV|F_CCONNECT|F_ROOTINTR;
		/* for x68k Nereid USB controller */
		if (sc->sc_enable_power && (t->flags & F_REALPOWER)) {
			t->flags &= ~F_REALPOWER;
			sc->sc_enable_power(sc, POWER_OFF);
		}
		slhci_intrchange(sc, 0);
		slhci_drain(sc);
	} else if (what == UHF_C_PORT_CONNECTION) {
		t->flags &= ~F_CCONNECT;
	} else if (what == UHF_C_PORT_RESET) {
		t->flags &= ~F_CRESET;
	} else if (what == UHF_PORT_ENABLE) {
		slhci_drain(sc);
	} else if (what != UHF_PORT_SUSPEND) {
		DDOLOG("ClrPortFeatERR:value=%#.4x", what, 0,0,0);
		error = USBD_IOERROR;
	}

	return error;
}

static usbd_status
slhci_set_feature(struct slhci_softc *sc, unsigned int what)
{
	struct slhci_transfers *t;
	uint8_t r;

	t = &sc->sc_transfers;

	KASSERT(mutex_owned(&sc->sc_intr_lock));

	if (what == UHF_PORT_RESET) {
		if (!(t->flags & F_ACTIVE)) {
			DDOLOG("SET PORT_RESET when not ACTIVE!",
			    0,0,0,0);
			return USBD_INVAL;
		}
		if (!(t->flags & F_POWER)) {
			DDOLOG("SET PORT_RESET without PORT_POWER! flags %p",
			    t->flags, 0,0,0);
			return USBD_INVAL;
		}
		if (t->flags & F_RESET)
			return USBD_NORMAL_COMPLETION;
		DLOG(D_MSG, "RESET flags %#x", t->flags, 0,0,0);
		slhci_intrchange(sc, 0);
		slhci_drain(sc);
		slhci_write(sc, SL11_CTRL, SL11_CTRL_RESETENGINE);
		/* usb spec says delay >= 10ms, app note 50ms */
 		start_cc_time(&t_delay, 50000);
		if (sc->sc_bus.use_polling) {
			DELAY(50000);
			slhci_reset(sc);
		} else {
			t->flags |= F_RESET;
			callout_schedule(&sc->sc_timer, max(mstohz(50), 2));
		}
	} else if (what == UHF_PORT_SUSPEND) {
		printf("%s: USB Suspend not implemented!\n", SC_NAME(sc));
		DDOLOG("%s: USB Suspend not implemented!\n", SC_NAME(sc),
		    0,0,0);
	} else if (what == UHF_PORT_POWER) {
		DLOG(D_MSG, "PORT_POWER", 0,0,0,0);
		/* for x68k Nereid USB controller */
		if (!(t->flags & F_ACTIVE))
			return USBD_INVAL;
		if (t->flags & F_POWER)
			return USBD_NORMAL_COMPLETION;
		if (!(t->flags & F_REALPOWER)) {
			if (sc->sc_enable_power)
				sc->sc_enable_power(sc, POWER_ON);
			t->flags |= F_REALPOWER;
		}
		t->flags |= F_POWER;
		r = slhci_read(sc, SL11_ISR);
		if (r & SL11_ISR_INSERT)
			slhci_write(sc, SL11_ISR, SL11_ISR_INSERT);
		if (r & SL11_ISR_NODEV) {
			slhci_intrchange(sc, SL11_IER_INSERT);
			t->flags |= F_NODEV;
		} else {
			t->flags &= ~F_NODEV;
			t->flags |= F_CCONNECT|F_ROOTINTR;
		}
	} else {
		DDOLOG("SetPortFeatERR=%#.8x", what, 0,0,0);
		return USBD_IOERROR;
	}

	return USBD_NORMAL_COMPLETION;
}

static void
slhci_get_status(struct slhci_softc *sc, usb_port_status_t *ps)
{
	struct slhci_transfers *t;
	unsigned int status, change;

	t = &sc->sc_transfers;

	KASSERT(mutex_owned(&sc->sc_intr_lock));

	/*
	 * We do not have a way to detect over current or bable and
	 * suspend is currently not implemented, so connect and reset
	 * are the only changes that need to be reported.
	 */
	change = 0;
	if (t->flags & F_CCONNECT)
		change |= UPS_C_CONNECT_STATUS;
	if (t->flags & F_CRESET)
		change |= UPS_C_PORT_RESET;

	status = 0;
	if (!(t->flags & F_NODEV))
		status |= UPS_CURRENT_CONNECT_STATUS;
	if (!(t->flags & F_UDISABLED))
		status |= UPS_PORT_ENABLED;
	if (t->flags & F_RESET)
		status |= UPS_RESET;
	if (t->flags & F_POWER)
		status |= UPS_PORT_POWER;
	if (t->flags & F_LOWSPEED)
		status |= UPS_LOW_SPEED;
	USETW(ps->wPortStatus, status);
	USETW(ps->wPortChange, change);
	DLOG(D_ROOT, "status=%#.4x, change=%#.4x", status, change, 0,0);
}

static usbd_status
slhci_root(struct slhci_softc *sc, struct slhci_pipe *spipe, struct usbd_xfer
    *xfer)
{
	struct slhci_transfers *t;
	usb_device_request_t *req;
	unsigned int len, value, index, actlen, type;
	uint8_t *buf;
	usbd_status error;

	t = &sc->sc_transfers;
	buf = NULL;

	LK_SLASSERT(spipe != NULL && xfer != NULL, sc, spipe, xfer, return
	    USBD_CANCELLED);

	DLOG(D_TRACE, "%s start", pnames(SLHCI_XFER_TYPE(xfer)), 0,0,0);
	KASSERT(mutex_owned(&sc->sc_intr_lock));

	if (spipe->ptype == PT_ROOT_INTR) {
		LK_SLASSERT(t->rootintr == NULL, sc, spipe, xfer, return
		    USBD_CANCELLED);
		t->rootintr = xfer;
		if (t->flags & F_CHANGE)
			t->flags |= F_ROOTINTR;
		return USBD_IN_PROGRESS;
	}

	error = USBD_IOERROR; /* XXX should be STALL */
	actlen = 0;
	req = &xfer->request;

	len = UGETW(req->wLength);
	value = UGETW(req->wValue);
	index = UGETW(req->wIndex);

	type = req->bmRequestType;

	if (len)
		buf = KERNADDR(&xfer->dmabuf, 0);

	SLHCI_DEXEC(D_TRACE, slhci_log_req_hub(req));

	/*
	 * USB requests for hubs have two basic types, standard and class.
	 * Each could potentially have recipients of device, interface,
	 * endpoint, or other.  For the hub class, CLASS_OTHER means the port
	 * and CLASS_DEVICE means the hub.  For standard requests, OTHER
	 * is not used.  Standard request are described in section 9.4 of the
	 * standard, hub class requests in 11.16.  Each request is either read
	 * or write.
	 *
	 * Clear Feature, Set Feature, and Status are defined for each of the
	 * used recipients.  Get Descriptor and Set Descriptor are defined for
	 * both standard and hub class types with different descriptors.
	 * Other requests have only one defined recipient and type.  These
	 * include: Get/Set Address, Get/Set Configuration, Get/Set Interface,
	 * and Synch Frame for standard requests and Get Bus State for hub
	 * class.
	 *
	 * When a device is first powered up it has address 0 until the
	 * address is set.
	 *
	 * Hubs are only allowed to support one interface and may not have
	 * isochronous endpoints.  The results of the related requests are
	 * undefined.
	 *
	 * The standard requires invalid or unsupported requests to return
	 * STALL in the data stage, however this does not work well with
	 * current error handling. XXX
	 *
	 * Some unsupported fields:
	 * Clear Hub Feature is for C_HUB_LOCAL_POWER and C_HUB_OVER_CURRENT
	 * Set Device Features is for ENDPOINT_HALT and DEVICE_REMOTE_WAKEUP
	 * Get Bus State is optional sample of D- and D+ at EOF2
	 */

	switch (req->bRequest) {
	/* Write Requests */
	case UR_CLEAR_FEATURE:
		if (type == UT_WRITE_CLASS_OTHER) {
			if (index == 1 /* Port */)
				error = slhci_clear_feature(sc, value);
			else
				DLOG(D_ROOT, "Clear Port Feature "
				    "index = %#.4x", index, 0,0,0);
		}
		break;
	case UR_SET_FEATURE:
		if (type == UT_WRITE_CLASS_OTHER) {
			if (index == 1 /* Port */)
				error = slhci_set_feature(sc, value);
			else
				DLOG(D_ROOT, "Set Port Feature "
				    "index = %#.4x", index, 0,0,0);
		} else if (type != UT_WRITE_CLASS_DEVICE)
			DLOG(D_ROOT, "Set Device Feature "
			    "ENDPOINT_HALT or DEVICE_REMOTE_WAKEUP "
			    "not supported", 0,0,0,0);
		break;
	case UR_SET_ADDRESS:
		if (type == UT_WRITE_DEVICE) {
			DLOG(D_ROOT, "Set Address %#.4x", value, 0,0,0);
			if (value < USB_MAX_DEVICES) {
				t->rootaddr = value;
				error = USBD_NORMAL_COMPLETION;
			}
		}
		break;
	case UR_SET_CONFIG:
		if (type == UT_WRITE_DEVICE) {
			DLOG(D_ROOT, "Set Config %#.4x", value, 0,0,0);
			if (value == 0 || value == 1) {
				t->rootconf = value;
				error = USBD_NORMAL_COMPLETION;
			}
		}
		break;
	/* Read Requests */
	case UR_GET_STATUS:
		if (type == UT_READ_CLASS_OTHER) {
			if (index == 1 /* Port */ && len == /* XXX >=? */
			    sizeof(usb_port_status_t)) {
				slhci_get_status(sc, (usb_port_status_t *)
				    buf);
				actlen = sizeof(usb_port_status_t);
				error = USBD_NORMAL_COMPLETION;
			} else
				DLOG(D_ROOT, "Get Port Status index = %#.4x "
				    "len = %#.4x", index, len, 0,0);
		} else if (type == UT_READ_CLASS_DEVICE) { /* XXX index? */
			if (len == sizeof(usb_hub_status_t)) {
				DLOG(D_ROOT, "Get Hub Status",
				    0,0,0,0);
				actlen = sizeof(usb_hub_status_t);
				memset(buf, 0, actlen);
				error = USBD_NORMAL_COMPLETION;
			} else
				DLOG(D_ROOT, "Get Hub Status bad len %#.4x",
				    len, 0,0,0);
		} else if (type == UT_READ_DEVICE) {
			if (len >= 2) {
				USETW(((usb_status_t *)buf)->wStatus, UDS_SELF_POWERED);
				actlen = 2;
				error = USBD_NORMAL_COMPLETION;
			}
		} else if (type == (UT_READ_INTERFACE|UT_READ_ENDPOINT)) {
			if (len >= 2) {
				USETW(((usb_status_t *)buf)->wStatus, 0);
				actlen = 2;
				error = USBD_NORMAL_COMPLETION;
			}
		}
		break;
	case UR_GET_CONFIG:
		if (type == UT_READ_DEVICE) {
			DLOG(D_ROOT, "Get Config", 0,0,0,0);
			if (len > 0) {
				*buf = t->rootconf;
				actlen = 1;
				error = USBD_NORMAL_COMPLETION;
			}
		}
		break;
	case UR_GET_INTERFACE:
		if (type == UT_READ_INTERFACE) {
			if (len > 0) {
				*buf = 0;
				actlen = 1;
				error = USBD_NORMAL_COMPLETION;
			}
		}
		break;
	case UR_GET_DESCRIPTOR:
		if (type == UT_READ_DEVICE) {
			/* value is type (&0xff00) and index (0xff) */
			if (value == (UDESC_DEVICE<<8)) {
				actlen = min(len, sizeof(slhci_devd));
				memcpy(buf, &slhci_devd, actlen);
				error = USBD_NORMAL_COMPLETION;
			} else if (value == (UDESC_CONFIG<<8)) {
				actlen = min(len, sizeof(slhci_confd));
				memcpy(buf, &slhci_confd, actlen);
				if (actlen > offsetof(usb_config_descriptor_t,
				    bMaxPower))
					((usb_config_descriptor_t *)
					    buf)->bMaxPower = t->max_current;
					    /* 2 mA units */
				error = USBD_NORMAL_COMPLETION;
			} else if (value == (UDESC_STRING<<8)) {
				/* language table XXX */
			} else if (value == ((UDESC_STRING<<8)|1)) {
				/* Vendor */
				actlen = usb_makestrdesc((usb_string_descriptor_t *)
				    buf, len, "ScanLogic/Cypress");
				error = USBD_NORMAL_COMPLETION;
			} else if (value == ((UDESC_STRING<<8)|2)) {
				/* Product */
				actlen = usb_makestrdesc((usb_string_descriptor_t *)
				    buf, len, "SL811HS/T root hub");
				error = USBD_NORMAL_COMPLETION;
			} else
				DDOLOG("Unknown Get Descriptor %#.4x",
				    value, 0,0,0);
		} else if (type == UT_READ_CLASS_DEVICE) {
			/* Descriptor number is 0 */
			if (value == (UDESC_HUB<<8)) {
				actlen = min(len, sizeof(slhci_hubd));
				memcpy(buf, &slhci_hubd, actlen);
				if (actlen > offsetof(usb_config_descriptor_t,
				    bMaxPower))
					((usb_hub_descriptor_t *)
					    buf)->bHubContrCurrent = 500 -
					    t->max_current;
				error = USBD_NORMAL_COMPLETION;
			} else
				DDOLOG("Unknown Get Hub Descriptor %#.4x",
				    value, 0,0,0);
		}
		break;
	}

	if (error == USBD_NORMAL_COMPLETION)
		xfer->actlen = actlen;
	xfer->status = error;
	KASSERT(spipe->xfer == NULL);
	spipe->xfer = xfer;
	enter_callback(t, spipe);

	return USBD_IN_PROGRESS;
}

/* End in lock functions. Start debug functions. */

#ifdef SLHCI_DEBUG
void
slhci_log_buffer(struct usbd_xfer *xfer)
{
	u_char *buf;

	if(xfer->length > 0 &&
	    UE_GET_DIR(xfer->pipe->endpoint->edesc->bEndpointAddress) ==
	    UE_DIR_IN) {
		buf = KERNADDR(&xfer->dmabuf, 0);
		DDOLOGBUF(buf, xfer->actlen);
		DDOLOG("len %d actlen %d short %d", xfer->length,
		    xfer->actlen, xfer->length - xfer->actlen, 0);
	}
}

void
slhci_log_req(usb_device_request_t *r)
{
	static const char *xmes[]={
		"GETSTAT",
		"CLRFEAT",
		"res",
		"SETFEAT",
		"res",
		"SETADDR",
		"GETDESC",
		"SETDESC",
		"GETCONF",
		"SETCONF",
		"GETIN/F",
		"SETIN/F",
		"SYNC_FR",
		"UNKNOWN"
	};
	int req, mreq, type, value, index, len;

	req   = r->bRequest;
	mreq  = (req > 13) ? 13 : req;
	type  = r->bmRequestType;
	value = UGETW(r->wValue);
	index = UGETW(r->wIndex);
	len   = UGETW(r->wLength);

	DDOLOG("request: %s %#x", xmes[mreq], type, 0,0);
	DDOLOG("request: r=%d,v=%d,i=%d,l=%d ", req, value, index, len);
}

void
slhci_log_req_hub(usb_device_request_t *r)
{
	static const struct {
		int req;
		int type;
		const char *str;
	} conf[] = {
		{ 1, 0x20, "ClrHubFeat"  },
		{ 1, 0x23, "ClrPortFeat" },
		{ 2, 0xa3, "GetBusState" },
		{ 6, 0xa0, "GetHubDesc"  },
		{ 0, 0xa0, "GetHubStat"  },
		{ 0, 0xa3, "GetPortStat" },
		{ 7, 0x20, "SetHubDesc"  },
		{ 3, 0x20, "SetHubFeat"  },
		{ 3, 0x23, "SetPortFeat" },
		{-1, 0, NULL},
	};
	int i;
	int value, index, len;
	const char *str;

	value = UGETW(r->wValue);
	index = UGETW(r->wIndex);
	len   = UGETW(r->wLength);
	for (i = 0; ; i++) {
		if (conf[i].req == -1 ) {
			slhci_log_req(r);
			return;
		}
		if (r->bmRequestType == conf[i].type && r->bRequest == conf[i].req) {
			str = conf[i].str;
			break;
		}
	}
	DDOLOG("hub request: %s v=%d,i=%d,l=%d ", str, value, index, len);
}

void
slhci_log_dumpreg(void)
{
	uint8_t r;
	unsigned int aaddr, alen, baddr, blen;
	static u_char buf[240];

	r = slhci_read(ssc, SL11_E0CTRL);
	DDOLOG("USB A Host Control = %#.2x", r, 0,0,0);
	DDOLOGFLAG8("E0CTRL=", r, "Preamble", "Data Toggle",  "SOF Sync",
	    "ISOC", "res", "Out", "Enable", "Arm");
	aaddr = slhci_read(ssc, SL11_E0ADDR);
	DDOLOG("USB A Base Address = %u", aaddr, 0,0,0);
	alen = slhci_read(ssc, SL11_E0LEN);
	DDOLOG("USB A Length = %u", alen, 0,0,0);
	r = slhci_read(ssc, SL11_E0STAT);
	DDOLOG("USB A Status = %#.2x", r, 0,0,0);
	DDOLOGFLAG8("E0STAT=", r, "STALL", "NAK", "Overflow", "Setup",
	    "Data Toggle", "Timeout", "Error", "ACK");
	r = slhci_read(ssc, SL11_E0CONT);
	DDOLOG("USB A Remaining or Overflow Length = %u", r, 0,0,0);
	r = slhci_read(ssc, SL11_E1CTRL);
	DDOLOG("USB B Host Control = %#.2x", r, 0,0,0);
	DDOLOGFLAG8("E1CTRL=", r, "Preamble", "Data Toggle",  "SOF Sync",
	    "ISOC", "res", "Out", "Enable", "Arm");
	baddr = slhci_read(ssc, SL11_E1ADDR);
	DDOLOG("USB B Base Address = %u", baddr, 0,0,0);
	blen = slhci_read(ssc, SL11_E1LEN);
	DDOLOG("USB B Length = %u", blen, 0,0,0);
	r = slhci_read(ssc, SL11_E1STAT);
	DDOLOG("USB B Status = %#.2x", r, 0,0,0);
	DDOLOGFLAG8("E1STAT=", r, "STALL", "NAK", "Overflow", "Setup",
	    "Data Toggle", "Timeout", "Error", "ACK");
	r = slhci_read(ssc, SL11_E1CONT);
	DDOLOG("USB B Remaining or Overflow Length = %u", r, 0,0,0);

	r = slhci_read(ssc, SL11_CTRL);
	DDOLOG("Control = %#.2x", r, 0,0,0);
	DDOLOGFLAG8("CTRL=", r, "res", "Suspend", "LOW Speed",
	    "J-K State Force", "Reset", "res", "res", "SOF");
	r = slhci_read(ssc, SL11_IER);
	DDOLOG("Interrupt Enable = %#.2x", r, 0,0,0);
	DDOLOGFLAG8("IER=", r, "D+ **IER!**", "Device Detect/Resume",
	    "Insert/Remove", "SOF", "res", "res", "USBB", "USBA");
	r = slhci_read(ssc, SL11_ISR);
	DDOLOG("Interrupt Status = %#.2x", r, 0,0,0);
	DDOLOGFLAG8("ISR=", r, "D+", "Device Detect/Resume",
	    "Insert/Remove", "SOF", "res", "res", "USBB", "USBA");
	r = slhci_read(ssc, SL11_REV);
	DDOLOG("Revision = %#.2x", r, 0,0,0);
	r = slhci_read(ssc, SL811_CSOF);
	DDOLOG("SOF Counter = %#.2x", r, 0,0,0);

	if (alen && aaddr >= SL11_BUFFER_START && aaddr < SL11_BUFFER_END &&
	    alen <= SL11_MAX_PACKET_SIZE && aaddr + alen <= SL11_BUFFER_END) {
		slhci_read_multi(ssc, aaddr, buf, alen);
		DDOLOG("USBA Buffer: start %u len %u", aaddr, alen, 0,0);
		DDOLOGBUF(buf, alen);
	} else if (alen)
		DDOLOG("USBA Buffer Invalid", 0,0,0,0);

	if (blen && baddr >= SL11_BUFFER_START && baddr < SL11_BUFFER_END &&
	    blen <= SL11_MAX_PACKET_SIZE && baddr + blen <= SL11_BUFFER_END) {
		slhci_read_multi(ssc, baddr, buf, blen);
		DDOLOG("USBB Buffer: start %u len %u", baddr, blen, 0,0);
		DDOLOGBUF(buf, blen);
	} else if (blen)
		DDOLOG("USBB Buffer Invalid", 0,0,0,0);
}

void
slhci_log_xfer(struct usbd_xfer *xfer)
{
	DDOLOG("xfer: length=%u, actlen=%u, flags=%#x, timeout=%u,",
		xfer->length, xfer->actlen, xfer->flags, xfer->timeout);
	if (xfer->dmabuf.block)
		DDOLOG("buffer=%p", KERNADDR(&xfer->dmabuf, 0), 0,0,0);
	slhci_log_req_hub(&xfer->request);
}

void
slhci_log_spipe(struct slhci_pipe *spipe)
{
	DDOLOG("spipe %p onlists: %s %s %s", spipe, gcq_onlist(&spipe->ap) ?
	    "AP" : "", gcq_onlist(&spipe->to) ? "TO" : "",
	    gcq_onlist(&spipe->xq) ? "XQ" : "");
	DDOLOG("spipe: xfer %p buffer %p pflags %#x ptype %s",
	    spipe->xfer, spipe->buffer, spipe->pflags, pnames(spipe->ptype));
}

void
slhci_print_intr(void)
{
	unsigned int ier, isr;
	ier = slhci_read(ssc, SL11_IER);
	isr = slhci_read(ssc, SL11_ISR);
	printf("IER: %#x ISR: %#x \n", ier, isr);
}

#if 0
void
slhci_log_sc(void)
{
	struct slhci_transfers *t;
	int i;

	t = &ssc->sc_transfers;

	DDOLOG("Flags=%#x", t->flags, 0,0,0);
	DDOLOG("a = %p Alen=%d b = %p Blen=%d", t->spipe[0], t->len[0],
	    t->spipe[1], t->len[1]);

	for (i=0; i<=Q_MAX; i++)
		DDOLOG("Q %d: %p", i, gcq_first(&t->q[i]), 0,0);

	DDOLOG("TIMED: %p", GCQ_ITEM(gcq_first(&t->to),
	    struct slhci_pipe, to), 0,0,0);

	DDOLOG("frame=%d rootintr=%p", t->frame, t->rootintr, 0,0);

	DDOLOG("use_polling=%d", ssc->sc_bus.use_polling, 0, 0, 0);
}

void
slhci_log_slreq(struct slhci_pipe *r)
{
	DDOLOG("next: %p", r->q.next.sqe_next, 0,0,0);
	DDOLOG("xfer: %p", r->xfer, 0,0,0);
	DDOLOG("buffer: %p", r->buffer, 0,0,0);
	DDOLOG("bustime: %u", r->bustime, 0,0,0);
	DDOLOG("control: %#x", r->control, 0,0,0);
	DDOLOGFLAG8("control=", r->control, "Preamble", "Data Toggle",
	    "SOF Sync", "ISOC", "res", "Out", "Enable", "Arm");
	DDOLOG("pid: %#x", r->tregs[PID], 0,0,0);
	DDOLOG("dev: %u", r->tregs[DEV], 0,0,0);
	DDOLOG("len: %u", r->tregs[LEN], 0,0,0);

	if (r->xfer)
		slhci_log_xfer(r->xfer);
}
#endif
#endif /* SLHCI_DEBUG */
/* End debug functions. */
