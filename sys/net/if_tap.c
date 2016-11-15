/*	$NetBSD: if_tap.c,v 1.82 2015/08/20 14:40:19 christos Exp $	*/

/*
 *  Copyright (c) 2003, 2004, 2008, 2009 The NetBSD Foundation.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 *  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 *  TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *  PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 *  BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * tap(4) is a virtual Ethernet interface.  It appears as a real Ethernet
 * device to the system, but can also be accessed by userland through a
 * character device interface, which allows reading and injecting frames.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_tap.c,v 1.82 2015/08/20 14:40:19 christos Exp $");

#if defined(_KERNEL_OPT)

#include "opt_modular.h"
#include "opt_compat_netbsd.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/cprng.h>
#include <sys/device.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/poll.h>
#include <sys/proc.h>
#include <sys/select.h>
#include <sys/sockio.h>
#if defined(COMPAT_40) || defined(MODULAR)
#include <sys/sysctl.h>
#endif
#include <sys/kauth.h>
#include <sys/mutex.h>
#include <sys/intr.h>
#include <sys/stat.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_ether.h>
#include <net/if_media.h>
#include <net/if_tap.h>
#include <net/bpf.h>

#include <compat/sys/sockio.h>

#include "ioconf.h"

#if defined(COMPAT_40) || defined(MODULAR)
/*
 * sysctl node management
 *
 * It's not really possible to use a SYSCTL_SETUP block with
 * current module implementation, so it is easier to just define
 * our own function.
 *
 * The handler function is a "helper" in Andrew Brown's sysctl
 * framework terminology.  It is used as a gateway for sysctl
 * requests over the nodes.
 *
 * tap_log allows the module to log creations of nodes and
 * destroy them all at once using sysctl_teardown.
 */
static int tap_node;
static int	tap_sysctl_handler(SYSCTLFN_PROTO);
SYSCTL_SETUP_PROTO(sysctl_tap_setup);
#endif

/*
 * Since we're an Ethernet device, we need the 2 following
 * components: a struct ethercom and a struct ifmedia
 * since we don't attach a PHY to ourselves.
 * We could emulate one, but there's no real point.
 */

struct tap_softc {
	device_t	sc_dev;
	struct ifmedia	sc_im;
	struct ethercom	sc_ec;
	int		sc_flags;
#define	TAP_INUSE	0x00000001	/* tap device can only be opened once */
#define TAP_ASYNCIO	0x00000002	/* user is using async I/O (SIGIO) on the device */
#define TAP_NBIO	0x00000004	/* user wants calls to avoid blocking */
#define TAP_GOING	0x00000008	/* interface is being destroyed */
	struct selinfo	sc_rsel;
	pid_t		sc_pgid; /* For async. IO */
	kmutex_t	sc_rdlock;
	kmutex_t	sc_kqlock;
	void		*sc_sih;
	struct timespec sc_atime;
	struct timespec sc_mtime;
	struct timespec sc_btime;
};

/* autoconf(9) glue */

static int	tap_match(device_t, cfdata_t, void *);
static void	tap_attach(device_t, device_t, void *);
static int	tap_detach(device_t, int);

CFATTACH_DECL_NEW(tap, sizeof(struct tap_softc),
    tap_match, tap_attach, tap_detach, NULL);
extern struct cfdriver tap_cd;

/* Real device access routines */
static int	tap_dev_close(struct tap_softc *);
static int	tap_dev_read(int, struct uio *, int);
static int	tap_dev_write(int, struct uio *, int);
static int	tap_dev_ioctl(int, u_long, void *, struct lwp *);
static int	tap_dev_poll(int, int, struct lwp *);
static int	tap_dev_kqfilter(int, struct knote *);

/* Fileops access routines */
static int	tap_fops_close(file_t *);
static int	tap_fops_read(file_t *, off_t *, struct uio *,
    kauth_cred_t, int);
static int	tap_fops_write(file_t *, off_t *, struct uio *,
    kauth_cred_t, int);
static int	tap_fops_ioctl(file_t *, u_long, void *);
static int	tap_fops_poll(file_t *, int);
static int	tap_fops_stat(file_t *, struct stat *);
static int	tap_fops_kqfilter(file_t *, struct knote *);

static const struct fileops tap_fileops = {
	.fo_read = tap_fops_read,
	.fo_write = tap_fops_write,
	.fo_ioctl = tap_fops_ioctl,
	.fo_fcntl = fnullop_fcntl,
	.fo_poll = tap_fops_poll,
	.fo_stat = tap_fops_stat,
	.fo_close = tap_fops_close,
	.fo_kqfilter = tap_fops_kqfilter,
	.fo_restart = fnullop_restart,
};

/* Helper for cloning open() */
static int	tap_dev_cloner(struct lwp *);

/* Character device routines */
static int	tap_cdev_open(dev_t, int, int, struct lwp *);
static int	tap_cdev_close(dev_t, int, int, struct lwp *);
static int	tap_cdev_read(dev_t, struct uio *, int);
static int	tap_cdev_write(dev_t, struct uio *, int);
static int	tap_cdev_ioctl(dev_t, u_long, void *, int, struct lwp *);
static int	tap_cdev_poll(dev_t, int, struct lwp *);
static int	tap_cdev_kqfilter(dev_t, struct knote *);

const struct cdevsw tap_cdevsw = {
	.d_open = tap_cdev_open,
	.d_close = tap_cdev_close,
	.d_read = tap_cdev_read,
	.d_write = tap_cdev_write,
	.d_ioctl = tap_cdev_ioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = tap_cdev_poll,
	.d_mmap = nommap,
	.d_kqfilter = tap_cdev_kqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER
};

#define TAP_CLONER	0xfffff		/* Maximal minor value */

/* kqueue-related routines */
static void	tap_kqdetach(struct knote *);
static int	tap_kqread(struct knote *, long);

/*
 * Those are needed by the if_media interface.
 */

static int	tap_mediachange(struct ifnet *);
static void	tap_mediastatus(struct ifnet *, struct ifmediareq *);

/*
 * Those are needed by the ifnet interface, and would typically be
 * there for any network interface driver.
 * Some other routines are optional: watchdog and drain.
 */

static void	tap_start(struct ifnet *);
static void	tap_stop(struct ifnet *, int);
static int	tap_init(struct ifnet *);
static int	tap_ioctl(struct ifnet *, u_long, void *);

/* Internal functions */
#if defined(COMPAT_40) || defined(MODULAR)
static int	tap_lifaddr(struct ifnet *, u_long, struct ifaliasreq *);
#endif
static void	tap_softintr(void *);

/*
 * tap is a clonable interface, although it is highly unrealistic for
 * an Ethernet device.
 *
 * Here are the bits needed for a clonable interface.
 */
static int	tap_clone_create(struct if_clone *, int);
static int	tap_clone_destroy(struct ifnet *);

struct if_clone tap_cloners = IF_CLONE_INITIALIZER("tap",
					tap_clone_create,
					tap_clone_destroy);

/* Helper functionis shared by the two cloning code paths */
static struct tap_softc *	tap_clone_creator(int);
int	tap_clone_destroyer(device_t);

void
tapattach(int n)
{
	int error;

	error = config_cfattach_attach(tap_cd.cd_name, &tap_ca);
	if (error) {
		aprint_error("%s: unable to register cfattach\n",
		    tap_cd.cd_name);
		(void)config_cfdriver_detach(&tap_cd);
		return;
	}

	if_clone_attach(&tap_cloners);
}

/* Pretty much useless for a pseudo-device */
static int
tap_match(device_t parent, cfdata_t cfdata, void *arg)
{

	return (1);
}

void
tap_attach(device_t parent, device_t self, void *aux)
{
	struct tap_softc *sc = device_private(self);
	struct ifnet *ifp;
#if defined(COMPAT_40) || defined(MODULAR)
	const struct sysctlnode *node;
	int error;
#endif
	uint8_t enaddr[ETHER_ADDR_LEN] =
	    { 0xf2, 0x0b, 0xa4, 0xff, 0xff, 0xff };
	char enaddrstr[3 * ETHER_ADDR_LEN];

	sc->sc_dev = self;
	sc->sc_sih = NULL;
	getnanotime(&sc->sc_btime);
	sc->sc_atime = sc->sc_mtime = sc->sc_btime;
	sc->sc_flags = 0;
	selinit(&sc->sc_rsel);

	/*
	 * Initialize the two locks for the device.
	 *
	 * We need a lock here because even though the tap device can be
	 * opened only once, the file descriptor might be passed to another
	 * process, say a fork(2)ed child.
	 *
	 * The Giant saves us from most of the hassle, but since the read
	 * operation can sleep, we don't want two processes to wake up at
	 * the same moment and both try and dequeue a single packet.
	 *
	 * The queue for event listeners (used by kqueue(9), see below) has
	 * to be protected too, so use a spin lock.
	 */
	mutex_init(&sc->sc_rdlock, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&sc->sc_kqlock, MUTEX_DEFAULT, IPL_VM);

	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");

	/*
	 * In order to obtain unique initial Ethernet address on a host,
	 * do some randomisation.  It's not meant for anything but avoiding
	 * hard-coding an address.
	 */
	cprng_fast(&enaddr[3], 3);

	aprint_verbose_dev(self, "Ethernet address %s\n",
	    ether_snprintf(enaddrstr, sizeof(enaddrstr), enaddr));

	/*
	 * Why 1000baseT? Why not? You can add more.
	 *
	 * Note that there are 3 steps: init, one or several additions to
	 * list of supported media, and in the end, the selection of one
	 * of them.
	 */
	ifmedia_init(&sc->sc_im, 0, tap_mediachange, tap_mediastatus);
	ifmedia_add(&sc->sc_im, IFM_ETHER|IFM_1000_T, 0, NULL);
	ifmedia_add(&sc->sc_im, IFM_ETHER|IFM_1000_T|IFM_FDX, 0, NULL);
	ifmedia_add(&sc->sc_im, IFM_ETHER|IFM_100_TX, 0, NULL);
	ifmedia_add(&sc->sc_im, IFM_ETHER|IFM_100_TX|IFM_FDX, 0, NULL);
	ifmedia_add(&sc->sc_im, IFM_ETHER|IFM_10_T, 0, NULL);
	ifmedia_add(&sc->sc_im, IFM_ETHER|IFM_10_T|IFM_FDX, 0, NULL);
	ifmedia_add(&sc->sc_im, IFM_ETHER|IFM_AUTO, 0, NULL);
	ifmedia_set(&sc->sc_im, IFM_ETHER|IFM_AUTO);

	/*
	 * One should note that an interface must do multicast in order
	 * to support IPv6.
	 */
	ifp = &sc->sc_ec.ec_if;
	strcpy(ifp->if_xname, device_xname(self));
	ifp->if_softc	= sc;
	ifp->if_flags	= IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl	= tap_ioctl;
	ifp->if_start	= tap_start;
	ifp->if_stop	= tap_stop;
	ifp->if_init	= tap_init;
	IFQ_SET_READY(&ifp->if_snd);

	sc->sc_ec.ec_capabilities = ETHERCAP_VLAN_MTU | ETHERCAP_JUMBO_MTU;

	/* Those steps are mandatory for an Ethernet driver. */
	if_initialize(ifp);
	ether_ifattach(ifp, enaddr);
	if_register(ifp);

#if defined(COMPAT_40) || defined(MODULAR)
	/*
	 * Add a sysctl node for that interface.
	 *
	 * The pointer transmitted is not a string, but instead a pointer to
	 * the softc structure, which we can use to build the string value on
	 * the fly in the helper function of the node.  See the comments for
	 * tap_sysctl_handler for details.
	 *
	 * Usually sysctl_createv is called with CTL_CREATE as the before-last
	 * component.  However, we can allocate a number ourselves, as we are
	 * the only consumer of the net.link.<iface> node.  In this case, the
	 * unit number is conveniently used to number the node.  CTL_CREATE
	 * would just work, too.
	 */
	if ((error = sysctl_createv(NULL, 0, NULL,
	    &node, CTLFLAG_READWRITE,
	    CTLTYPE_STRING, device_xname(self), NULL,
	    tap_sysctl_handler, 0, (void *)sc, 18,
	    CTL_NET, AF_LINK, tap_node, device_unit(sc->sc_dev),
	    CTL_EOL)) != 0)
		aprint_error_dev(self, "sysctl_createv returned %d, ignoring\n",
		    error);
#endif
}

/*
 * When detaching, we do the inverse of what is done in the attach
 * routine, in reversed order.
 */
static int
tap_detach(device_t self, int flags)
{
	struct tap_softc *sc = device_private(self);
	struct ifnet *ifp = &sc->sc_ec.ec_if;
#if defined(COMPAT_40) || defined(MODULAR)
	int error;
#endif
	int s;

	sc->sc_flags |= TAP_GOING;
	s = splnet();
	tap_stop(ifp, 1);
	if_down(ifp);
	splx(s);

	if (sc->sc_sih != NULL) {
		softint_disestablish(sc->sc_sih);
		sc->sc_sih = NULL;
	}

#if defined(COMPAT_40) || defined(MODULAR)
	/*
	 * Destroying a single leaf is a very straightforward operation using
	 * sysctl_destroyv.  One should be sure to always end the path with
	 * CTL_EOL.
	 */
	if ((error = sysctl_destroyv(NULL, CTL_NET, AF_LINK, tap_node,
	    device_unit(sc->sc_dev), CTL_EOL)) != 0)
		aprint_error_dev(self,
		    "sysctl_destroyv returned %d, ignoring\n", error);
#endif
	ether_ifdetach(ifp);
	if_detach(ifp);
	ifmedia_delete_instance(&sc->sc_im, IFM_INST_ANY);
	seldestroy(&sc->sc_rsel);
	mutex_destroy(&sc->sc_rdlock);
	mutex_destroy(&sc->sc_kqlock);

	pmf_device_deregister(self);

	return (0);
}

/*
 * This function is called by the ifmedia layer to notify the driver
 * that the user requested a media change.  A real driver would
 * reconfigure the hardware.
 */
static int
tap_mediachange(struct ifnet *ifp)
{
	return (0);
}

/*
 * Here the user asks for the currently used media.
 */
static void
tap_mediastatus(struct ifnet *ifp, struct ifmediareq *imr)
{
	struct tap_softc *sc = (struct tap_softc *)ifp->if_softc;
	imr->ifm_active = sc->sc_im.ifm_cur->ifm_media;
}

/*
 * This is the function where we SEND packets.
 *
 * There is no 'receive' equivalent.  A typical driver will get
 * interrupts from the hardware, and from there will inject new packets
 * into the network stack.
 *
 * Once handled, a packet must be freed.  A real driver might not be able
 * to fit all the pending packets into the hardware, and is allowed to
 * return before having sent all the packets.  It should then use the
 * if_flags flag IFF_OACTIVE to notify the upper layer.
 *
 * There are also other flags one should check, such as IFF_PAUSE.
 *
 * It is our duty to make packets available to BPF listeners.
 *
 * You should be aware that this function is called by the Ethernet layer
 * at splnet().
 *
 * When the device is opened, we have to pass the packet(s) to the
 * userland.  For that we stay in OACTIVE mode while the userland gets
 * the packets, and we send a signal to the processes waiting to read.
 *
 * wakeup(sc) is the counterpart to the tsleep call in
 * tap_dev_read, while selnotify() is used for kevent(2) and
 * poll(2) (which includes select(2)) listeners.
 */
static void
tap_start(struct ifnet *ifp)
{
	struct tap_softc *sc = (struct tap_softc *)ifp->if_softc;
	struct mbuf *m0;

	if ((sc->sc_flags & TAP_INUSE) == 0) {
		/* Simply drop packets */
		for(;;) {
			IFQ_DEQUEUE(&ifp->if_snd, m0);
			if (m0 == NULL)
				return;

			ifp->if_opackets++;
			bpf_mtap(ifp, m0);

			m_freem(m0);
		}
	} else if (!IFQ_IS_EMPTY(&ifp->if_snd)) {
		ifp->if_flags |= IFF_OACTIVE;
		wakeup(sc);
		selnotify(&sc->sc_rsel, 0, 1);
		if (sc->sc_flags & TAP_ASYNCIO)
			softint_schedule(sc->sc_sih);
	}
}

static void
tap_softintr(void *cookie)
{
	struct tap_softc *sc;
	struct ifnet *ifp;
	int a, b;

	sc = cookie;

	if (sc->sc_flags & TAP_ASYNCIO) {
		ifp = &sc->sc_ec.ec_if;
		if (ifp->if_flags & IFF_RUNNING) {
			a = POLL_IN;
			b = POLLIN|POLLRDNORM;
		} else {
			a = POLL_HUP;
			b = 0;
		}
		fownsignal(sc->sc_pgid, SIGIO, a, b, NULL);
	}
}

/*
 * A typical driver will only contain the following handlers for
 * ioctl calls, except SIOCSIFPHYADDR.
 * The latter is a hack I used to set the Ethernet address of the
 * faked device.
 *
 * Note that both ifmedia_ioctl() and ether_ioctl() have to be
 * called under splnet().
 */
static int
tap_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct tap_softc *sc = (struct tap_softc *)ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error;

	s = splnet();

	switch (cmd) {
#ifdef OSIOCSIFMEDIA
	case OSIOCSIFMEDIA:
#endif
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_im, cmd);
		break;
#if defined(COMPAT_40) || defined(MODULAR)
	case SIOCSIFPHYADDR:
		error = tap_lifaddr(ifp, cmd, (struct ifaliasreq *)data);
		break;
#endif
	default:
		error = ether_ioctl(ifp, cmd, data);
		if (error == ENETRESET)
			error = 0;
		break;
	}

	splx(s);

	return (error);
}

#if defined(COMPAT_40) || defined(MODULAR)
/*
 * Helper function to set Ethernet address.  This has been replaced by
 * the generic SIOCALIFADDR ioctl on a PF_LINK socket.
 */
static int
tap_lifaddr(struct ifnet *ifp, u_long cmd, struct ifaliasreq *ifra)
{
	const struct sockaddr *sa = &ifra->ifra_addr;

	if (sa->sa_family != AF_LINK)
		return (EINVAL);

	if_set_sadl(ifp, sa->sa_data, ETHER_ADDR_LEN, false);

	return (0);
}
#endif

/*
 * _init() would typically be called when an interface goes up,
 * meaning it should configure itself into the state in which it
 * can send packets.
 */
static int
tap_init(struct ifnet *ifp)
{
	ifp->if_flags |= IFF_RUNNING;

	tap_start(ifp);

	return (0);
}

/*
 * _stop() is called when an interface goes down.  It is our
 * responsability to validate that state by clearing the
 * IFF_RUNNING flag.
 *
 * We have to wake up all the sleeping processes to have the pending
 * read requests cancelled.
 */
static void
tap_stop(struct ifnet *ifp, int disable)
{
	struct tap_softc *sc = (struct tap_softc *)ifp->if_softc;

	ifp->if_flags &= ~IFF_RUNNING;
	wakeup(sc);
	selnotify(&sc->sc_rsel, 0, 1);
	if (sc->sc_flags & TAP_ASYNCIO)
		softint_schedule(sc->sc_sih);
}

/*
 * The 'create' command of ifconfig can be used to create
 * any numbered instance of a given device.  Thus we have to
 * make sure we have enough room in cd_devs to create the
 * user-specified instance.  config_attach_pseudo will do this
 * for us.
 */
static int
tap_clone_create(struct if_clone *ifc, int unit)
{
	if (tap_clone_creator(unit) == NULL) {
		aprint_error("%s%d: unable to attach an instance\n",
                    tap_cd.cd_name, unit);
		return (ENXIO);
	}

	return (0);
}

/*
 * tap(4) can be cloned by two ways:
 *   using 'ifconfig tap0 create', which will use the network
 *     interface cloning API, and call tap_clone_create above.
 *   opening the cloning device node, whose minor number is TAP_CLONER.
 *     See below for an explanation on how this part work.
 */
static struct tap_softc *
tap_clone_creator(int unit)
{
	struct cfdata *cf;

	cf = malloc(sizeof(*cf), M_DEVBUF, M_WAITOK);
	cf->cf_name = tap_cd.cd_name;
	cf->cf_atname = tap_ca.ca_name;
	if (unit == -1) {
		/* let autoconf find the first free one */
		cf->cf_unit = 0;
		cf->cf_fstate = FSTATE_STAR;
	} else {
		cf->cf_unit = unit;
		cf->cf_fstate = FSTATE_NOTFOUND;
	}

	return device_private(config_attach_pseudo(cf));
}

/*
 * The clean design of if_clone and autoconf(9) makes that part
 * really straightforward.  The second argument of config_detach
 * means neither QUIET nor FORCED.
 */
static int
tap_clone_destroy(struct ifnet *ifp)
{
	struct tap_softc *sc = ifp->if_softc;

	return tap_clone_destroyer(sc->sc_dev);
}

int
tap_clone_destroyer(device_t dev)
{
	cfdata_t cf = device_cfdata(dev);
	int error;

	if ((error = config_detach(dev, 0)) != 0)
		aprint_error_dev(dev, "unable to detach instance\n");
	free(cf, M_DEVBUF);

	return (error);
}

/*
 * tap(4) is a bit of an hybrid device.  It can be used in two different
 * ways:
 *  1. ifconfig tapN create, then use /dev/tapN to read/write off it.
 *  2. open /dev/tap, get a new interface created and read/write off it.
 *     That interface is destroyed when the process that had it created exits.
 *
 * The first way is managed by the cdevsw structure, and you access interfaces
 * through a (major, minor) mapping:  tap4 is obtained by the minor number
 * 4.  The entry points for the cdevsw interface are prefixed by tap_cdev_.
 *
 * The second way is the so-called "cloning" device.  It's a special minor
 * number (chosen as the maximal number, to allow as much tap devices as
 * possible).  The user first opens the cloner (e.g., /dev/tap), and that
 * call ends in tap_cdev_open.  The actual place where it is handled is
 * tap_dev_cloner.
 *
 * An tap device cannot be opened more than once at a time, so the cdevsw
 * part of open() does nothing but noting that the interface is being used and
 * hence ready to actually handle packets.
 */

static int
tap_cdev_open(dev_t dev, int flags, int fmt, struct lwp *l)
{
	struct tap_softc *sc;

	if (minor(dev) == TAP_CLONER)
		return tap_dev_cloner(l);

	sc = device_lookup_private(&tap_cd, minor(dev));
	if (sc == NULL)
		return (ENXIO);

	/* The device can only be opened once */
	if (sc->sc_flags & TAP_INUSE)
		return (EBUSY);
	sc->sc_flags |= TAP_INUSE;
	return (0);
}

/*
 * There are several kinds of cloning devices, and the most simple is the one
 * tap(4) uses.  What it does is change the file descriptor with a new one,
 * with its own fileops structure (which maps to the various read, write,
 * ioctl functions).  It starts allocating a new file descriptor with falloc,
 * then actually creates the new tap devices.
 *
 * Once those two steps are successful, we can re-wire the existing file
 * descriptor to its new self.  This is done with fdclone():  it fills the fp
 * structure as needed (notably f_devunit gets filled with the fifth parameter
 * passed, the unit of the tap device which will allows us identifying the
 * device later), and returns EMOVEFD.
 *
 * That magic value is interpreted by sys_open() which then replaces the
 * current file descriptor by the new one (through a magic member of struct
 * lwp, l_dupfd).
 *
 * The tap device is flagged as being busy since it otherwise could be
 * externally accessed through the corresponding device node with the cdevsw
 * interface.
 */

static int
tap_dev_cloner(struct lwp *l)
{
	struct tap_softc *sc;
	file_t *fp;
	int error, fd;

	if ((error = fd_allocfile(&fp, &fd)) != 0)
		return (error);

	if ((sc = tap_clone_creator(-1)) == NULL) {
		fd_abort(curproc, fp, fd);
		return (ENXIO);
	}

	sc->sc_flags |= TAP_INUSE;

	return fd_clone(fp, fd, FREAD|FWRITE, &tap_fileops,
	    (void *)(intptr_t)device_unit(sc->sc_dev));
}

/*
 * While all other operations (read, write, ioctl, poll and kqfilter) are
 * really the same whether we are in cdevsw or fileops mode, the close()
 * function is slightly different in the two cases.
 *
 * As for the other, the core of it is shared in tap_dev_close.  What
 * it does is sufficient for the cdevsw interface, but the cloning interface
 * needs another thing:  the interface is destroyed when the processes that
 * created it closes it.
 */
static int
tap_cdev_close(dev_t dev, int flags, int fmt,
    struct lwp *l)
{
	struct tap_softc *sc =
	    device_lookup_private(&tap_cd, minor(dev));

	if (sc == NULL)
		return (ENXIO);

	return tap_dev_close(sc);
}

/*
 * It might happen that the administrator used ifconfig to externally destroy
 * the interface.  In that case, tap_fops_close will be called while
 * tap_detach is already happening.  If we called it again from here, we
 * would dead lock.  TAP_GOING ensures that this situation doesn't happen.
 */
static int
tap_fops_close(file_t *fp)
{
	int unit = fp->f_devunit;
	struct tap_softc *sc;
	int error;

	sc = device_lookup_private(&tap_cd, unit);
	if (sc == NULL)
		return (ENXIO);

	/* tap_dev_close currently always succeeds, but it might not
	 * always be the case. */
	KERNEL_LOCK(1, NULL);
	if ((error = tap_dev_close(sc)) != 0) {
		KERNEL_UNLOCK_ONE(NULL);
		return (error);
	}

	/* Destroy the device now that it is no longer useful,
	 * unless it's already being destroyed. */
	if ((sc->sc_flags & TAP_GOING) != 0) {
		KERNEL_UNLOCK_ONE(NULL);
		return (0);
	}

	error = tap_clone_destroyer(sc->sc_dev);
	KERNEL_UNLOCK_ONE(NULL);
	return error;
}

static int
tap_dev_close(struct tap_softc *sc)
{
	struct ifnet *ifp;
	int s;

	s = splnet();
	/* Let tap_start handle packets again */
	ifp = &sc->sc_ec.ec_if;
	ifp->if_flags &= ~IFF_OACTIVE;

	/* Purge output queue */
	if (!(IFQ_IS_EMPTY(&ifp->if_snd))) {
		struct mbuf *m;

		for (;;) {
			IFQ_DEQUEUE(&ifp->if_snd, m);
			if (m == NULL)
				break;

			ifp->if_opackets++;
			bpf_mtap(ifp, m);
			m_freem(m);
		}
	}
	splx(s);

	if (sc->sc_sih != NULL) {
		softint_disestablish(sc->sc_sih);
		sc->sc_sih = NULL;
	}
	sc->sc_flags &= ~(TAP_INUSE | TAP_ASYNCIO);

	return (0);
}

static int
tap_cdev_read(dev_t dev, struct uio *uio, int flags)
{
	return tap_dev_read(minor(dev), uio, flags);
}

static int
tap_fops_read(file_t *fp, off_t *offp, struct uio *uio,
    kauth_cred_t cred, int flags)
{
	int error;

	KERNEL_LOCK(1, NULL);
	error = tap_dev_read(fp->f_devunit, uio, flags);
	KERNEL_UNLOCK_ONE(NULL);
	return error;
}

static int
tap_dev_read(int unit, struct uio *uio, int flags)
{
	struct tap_softc *sc = device_lookup_private(&tap_cd, unit);
	struct ifnet *ifp;
	struct mbuf *m, *n;
	int error = 0, s;

	if (sc == NULL)
		return (ENXIO);

	getnanotime(&sc->sc_atime);

	ifp = &sc->sc_ec.ec_if;
	if ((ifp->if_flags & IFF_UP) == 0)
		return (EHOSTDOWN);

	/*
	 * In the TAP_NBIO case, we have to make sure we won't be sleeping
	 */
	if ((sc->sc_flags & TAP_NBIO) != 0) {
		if (!mutex_tryenter(&sc->sc_rdlock))
			return (EWOULDBLOCK);
	} else {
		mutex_enter(&sc->sc_rdlock);
	}

	s = splnet();
	if (IFQ_IS_EMPTY(&ifp->if_snd)) {
		ifp->if_flags &= ~IFF_OACTIVE;
		/*
		 * We must release the lock before sleeping, and re-acquire it
		 * after.
		 */
		mutex_exit(&sc->sc_rdlock);
		if (sc->sc_flags & TAP_NBIO)
			error = EWOULDBLOCK;
		else
			error = tsleep(sc, PSOCK|PCATCH, "tap", 0);
		splx(s);

		if (error != 0)
			return (error);
		/* The device might have been downed */
		if ((ifp->if_flags & IFF_UP) == 0)
			return (EHOSTDOWN);
		if ((sc->sc_flags & TAP_NBIO)) {
			if (!mutex_tryenter(&sc->sc_rdlock))
				return (EWOULDBLOCK);
		} else {
			mutex_enter(&sc->sc_rdlock);
		}
		s = splnet();
	}

	IFQ_DEQUEUE(&ifp->if_snd, m);
	ifp->if_flags &= ~IFF_OACTIVE;
	splx(s);
	if (m == NULL) {
		error = 0;
		goto out;
	}

	ifp->if_opackets++;
	bpf_mtap(ifp, m);

	/*
	 * One read is one packet.
	 */
	do {
		error = uiomove(mtod(m, void *),
		    min(m->m_len, uio->uio_resid), uio);
		MFREE(m, n);
		m = n;
	} while (m != NULL && uio->uio_resid > 0 && error == 0);

	if (m != NULL)
		m_freem(m);

out:
	mutex_exit(&sc->sc_rdlock);
	return (error);
}

static int
tap_fops_stat(file_t *fp, struct stat *st)
{
	int error = 0;
	struct tap_softc *sc;
	int unit = fp->f_devunit;

	(void)memset(st, 0, sizeof(*st));

	KERNEL_LOCK(1, NULL);
	sc = device_lookup_private(&tap_cd, unit);
	if (sc == NULL) {
		error = ENXIO;
		goto out;
	}

	st->st_dev = makedev(cdevsw_lookup_major(&tap_cdevsw), unit);
	st->st_atimespec = sc->sc_atime;
	st->st_mtimespec = sc->sc_mtime;
	st->st_ctimespec = st->st_birthtimespec = sc->sc_btime;
	st->st_uid = kauth_cred_geteuid(fp->f_cred);
	st->st_gid = kauth_cred_getegid(fp->f_cred);
out:
	KERNEL_UNLOCK_ONE(NULL);
	return error;
}

static int
tap_cdev_write(dev_t dev, struct uio *uio, int flags)
{
	return tap_dev_write(minor(dev), uio, flags);
}

static int
tap_fops_write(file_t *fp, off_t *offp, struct uio *uio,
    kauth_cred_t cred, int flags)
{
	int error;

	KERNEL_LOCK(1, NULL);
	error = tap_dev_write(fp->f_devunit, uio, flags);
	KERNEL_UNLOCK_ONE(NULL);
	return error;
}

static int
tap_dev_write(int unit, struct uio *uio, int flags)
{
	struct tap_softc *sc =
	    device_lookup_private(&tap_cd, unit);
	struct ifnet *ifp;
	struct mbuf *m, **mp;
	int error = 0;
	int s;

	if (sc == NULL)
		return (ENXIO);

	getnanotime(&sc->sc_mtime);
	ifp = &sc->sc_ec.ec_if;

	/* One write, one packet, that's the rule */
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL) {
		ifp->if_ierrors++;
		return (ENOBUFS);
	}
	m->m_pkthdr.len = uio->uio_resid;

	mp = &m;
	while (error == 0 && uio->uio_resid > 0) {
		if (*mp != m) {
			MGET(*mp, M_DONTWAIT, MT_DATA);
			if (*mp == NULL) {
				error = ENOBUFS;
				break;
			}
		}
		(*mp)->m_len = min(MHLEN, uio->uio_resid);
		error = uiomove(mtod(*mp, void *), (*mp)->m_len, uio);
		mp = &(*mp)->m_next;
	}
	if (error) {
		ifp->if_ierrors++;
		m_freem(m);
		return (error);
	}

	ifp->if_ipackets++;
	m->m_pkthdr.rcvif = ifp;

	bpf_mtap(ifp, m);
	s = splnet();
	(*ifp->if_input)(ifp, m);
	splx(s);

	return (0);
}

static int
tap_cdev_ioctl(dev_t dev, u_long cmd, void *data, int flags,
    struct lwp *l)
{
	return tap_dev_ioctl(minor(dev), cmd, data, l);
}

static int
tap_fops_ioctl(file_t *fp, u_long cmd, void *data)
{
	return tap_dev_ioctl(fp->f_devunit, cmd, data, curlwp);
}

static int
tap_dev_ioctl(int unit, u_long cmd, void *data, struct lwp *l)
{
	struct tap_softc *sc = device_lookup_private(&tap_cd, unit);

	if (sc == NULL)
		return ENXIO;

	switch (cmd) {
	case FIONREAD:
		{
			struct ifnet *ifp = &sc->sc_ec.ec_if;
			struct mbuf *m;
			int s;

			s = splnet();
			IFQ_POLL(&ifp->if_snd, m);

			if (m == NULL)
				*(int *)data = 0;
			else
				*(int *)data = m->m_pkthdr.len;
			splx(s);
			return 0;
		} 
	case TIOCSPGRP:
	case FIOSETOWN:
		return fsetown(&sc->sc_pgid, cmd, data);
	case TIOCGPGRP:
	case FIOGETOWN:
		return fgetown(sc->sc_pgid, cmd, data);
	case FIOASYNC:
		if (*(int *)data) {
			if (sc->sc_sih == NULL) {
				sc->sc_sih = softint_establish(SOFTINT_CLOCK,
				    tap_softintr, sc);
				if (sc->sc_sih == NULL)
					return EBUSY; /* XXX */
			}
			sc->sc_flags |= TAP_ASYNCIO;
		} else {
			sc->sc_flags &= ~TAP_ASYNCIO;
			if (sc->sc_sih != NULL) {
				softint_disestablish(sc->sc_sih);
				sc->sc_sih = NULL;
			}
		}
		return 0;
	case FIONBIO:
		if (*(int *)data)
			sc->sc_flags |= TAP_NBIO;
		else
			sc->sc_flags &= ~TAP_NBIO;
		return 0;
#ifdef OTAPGIFNAME
	case OTAPGIFNAME:
#endif
	case TAPGIFNAME:
		{
			struct ifreq *ifr = (struct ifreq *)data;
			struct ifnet *ifp = &sc->sc_ec.ec_if;

			strlcpy(ifr->ifr_name, ifp->if_xname, IFNAMSIZ);
			return 0;
		}
	default:
		return ENOTTY;
	}
}

static int
tap_cdev_poll(dev_t dev, int events, struct lwp *l)
{
	return tap_dev_poll(minor(dev), events, l);
}

static int
tap_fops_poll(file_t *fp, int events)
{
	return tap_dev_poll(fp->f_devunit, events, curlwp);
}

static int
tap_dev_poll(int unit, int events, struct lwp *l)
{
	struct tap_softc *sc =
	    device_lookup_private(&tap_cd, unit);
	int revents = 0;

	if (sc == NULL)
		return POLLERR;

	if (events & (POLLIN|POLLRDNORM)) {
		struct ifnet *ifp = &sc->sc_ec.ec_if;
		struct mbuf *m;
		int s;

		s = splnet();
		IFQ_POLL(&ifp->if_snd, m);

		if (m != NULL)
			revents |= events & (POLLIN|POLLRDNORM);
		else {
			mutex_spin_enter(&sc->sc_kqlock);
			selrecord(l, &sc->sc_rsel);
			mutex_spin_exit(&sc->sc_kqlock);
		}
		splx(s);
	}
	revents |= events & (POLLOUT|POLLWRNORM);

	return (revents);
}

static struct filterops tap_read_filterops = { 1, NULL, tap_kqdetach,
	tap_kqread };
static struct filterops tap_seltrue_filterops = { 1, NULL, tap_kqdetach,
	filt_seltrue };

static int
tap_cdev_kqfilter(dev_t dev, struct knote *kn)
{
	return tap_dev_kqfilter(minor(dev), kn);
}

static int
tap_fops_kqfilter(file_t *fp, struct knote *kn)
{
	return tap_dev_kqfilter(fp->f_devunit, kn);
}

static int
tap_dev_kqfilter(int unit, struct knote *kn)
{
	struct tap_softc *sc =
	    device_lookup_private(&tap_cd, unit);

	if (sc == NULL)
		return (ENXIO);

	KERNEL_LOCK(1, NULL);
	switch(kn->kn_filter) {
	case EVFILT_READ:
		kn->kn_fop = &tap_read_filterops;
		break;
	case EVFILT_WRITE:
		kn->kn_fop = &tap_seltrue_filterops;
		break;
	default:
		KERNEL_UNLOCK_ONE(NULL);
		return (EINVAL);
	}

	kn->kn_hook = sc;
	mutex_spin_enter(&sc->sc_kqlock);
	SLIST_INSERT_HEAD(&sc->sc_rsel.sel_klist, kn, kn_selnext);
	mutex_spin_exit(&sc->sc_kqlock);
	KERNEL_UNLOCK_ONE(NULL);
	return (0);
}

static void
tap_kqdetach(struct knote *kn)
{
	struct tap_softc *sc = (struct tap_softc *)kn->kn_hook;

	KERNEL_LOCK(1, NULL);
	mutex_spin_enter(&sc->sc_kqlock);
	SLIST_REMOVE(&sc->sc_rsel.sel_klist, kn, knote, kn_selnext);
	mutex_spin_exit(&sc->sc_kqlock);
	KERNEL_UNLOCK_ONE(NULL);
}

static int
tap_kqread(struct knote *kn, long hint)
{
	struct tap_softc *sc = (struct tap_softc *)kn->kn_hook;
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	struct mbuf *m;
	int s, rv;

	KERNEL_LOCK(1, NULL);
	s = splnet();
	IFQ_POLL(&ifp->if_snd, m);

	if (m == NULL)
		kn->kn_data = 0;
	else
		kn->kn_data = m->m_pkthdr.len;
	splx(s);
	rv = (kn->kn_data != 0 ? 1 : 0);
	KERNEL_UNLOCK_ONE(NULL);
	return rv;
}

#if defined(COMPAT_40) || defined(MODULAR)
/*
 * sysctl management routines
 * You can set the address of an interface through:
 * net.link.tap.tap<number>
 *
 * Note the consistent use of tap_log in order to use
 * sysctl_teardown at unload time.
 *
 * In the kernel you will find a lot of SYSCTL_SETUP blocks.  Those
 * blocks register a function in a special section of the kernel
 * (called a link set) which is used at init_sysctl() time to cycle
 * through all those functions to create the kernel's sysctl tree.
 *
 * It is not possible to use link sets in a module, so the
 * easiest is to simply call our own setup routine at load time.
 *
 * In the SYSCTL_SETUP blocks you find in the kernel, nodes have the
 * CTLFLAG_PERMANENT flag, meaning they cannot be removed.  Once the
 * whole kernel sysctl tree is built, it is not possible to add any
 * permanent node.
 *
 * It should be noted that we're not saving the sysctlnode pointer
 * we are returned when creating the "tap" node.  That structure
 * cannot be trusted once out of the calling function, as it might
 * get reused.  So we just save the MIB number, and always give the
 * full path starting from the root for later calls to sysctl_createv
 * and sysctl_destroyv.
 */
SYSCTL_SETUP(sysctl_tap_setup, "sysctl net.link.tap subtree setup")
{
	const struct sysctlnode *node;
	int error = 0;

	if ((error = sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_PERMANENT,
	    CTLTYPE_NODE, "link", NULL,
	    NULL, 0, NULL, 0,
	    CTL_NET, AF_LINK, CTL_EOL)) != 0)
		return;

	/*
	 * The first four parameters of sysctl_createv are for management.
	 *
	 * The four that follows, here starting with a '0' for the flags,
	 * describe the node.
	 *
	 * The next series of four set its value, through various possible
	 * means.
	 *
	 * Last but not least, the path to the node is described.  That path
	 * is relative to the given root (third argument).  Here we're
	 * starting from the root.
	 */
	if ((error = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT,
	    CTLTYPE_NODE, "tap", NULL,
	    NULL, 0, NULL, 0,
	    CTL_NET, AF_LINK, CTL_CREATE, CTL_EOL)) != 0)
		return;
	tap_node = node->sysctl_num;
}

/*
 * The helper functions make Andrew Brown's interface really
 * shine.  It makes possible to create value on the fly whether
 * the sysctl value is read or written.
 *
 * As shown as an example in the man page, the first step is to
 * create a copy of the node to have sysctl_lookup work on it.
 *
 * Here, we have more work to do than just a copy, since we have
 * to create the string.  The first step is to collect the actual
 * value of the node, which is a convenient pointer to the softc
 * of the interface.  From there we create the string and use it
 * as the value, but only for the *copy* of the node.
 *
 * Then we let sysctl_lookup do the magic, which consists in
 * setting oldp and newp as required by the operation.  When the
 * value is read, that means that the string will be copied to
 * the user, and when it is written, the new value will be copied
 * over in the addr array.
 *
 * If newp is NULL, the user was reading the value, so we don't
 * have anything else to do.  If a new value was written, we
 * have to check it.
 *
 * If it is incorrect, we can return an error and leave 'node' as
 * it is:  since it is a copy of the actual node, the change will
 * be forgotten.
 *
 * Upon a correct input, we commit the change to the ifnet
 * structure of our interface.
 */
static int
tap_sysctl_handler(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	struct tap_softc *sc;
	struct ifnet *ifp;
	int error;
	size_t len;
	char addr[3 * ETHER_ADDR_LEN];
	uint8_t enaddr[ETHER_ADDR_LEN];

	node = *rnode;
	sc = node.sysctl_data;
	ifp = &sc->sc_ec.ec_if;
	(void)ether_snprintf(addr, sizeof(addr), CLLADDR(ifp->if_sadl));
	node.sysctl_data = addr;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return (error);

	len = strlen(addr);
	if (len < 11 || len > 17)
		return (EINVAL);

	/* Commit change */
	if (ether_aton_r(enaddr, sizeof(enaddr), addr) != 0)
		return (EINVAL);
	if_set_sadl(ifp, enaddr, ETHER_ADDR_LEN, false);
	return (error);
}
#endif
