
#include "inc.h"

#include <dev/pci/pciio.h>

#include <minix/fb.h>
#include <minix/i2c.h>
#include <minix/keymap.h>
#include <minix/sound.h>

#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/kbdio.h>
#include <sys/termios.h>
#include <sys/time.h>

const char *
char_ioctl_name(unsigned long req)
{

	switch (req) {
	NAME(MINIX_I2C_IOCTL_EXEC);
	NAME(FBIOGET_VSCREENINFO);
	NAME(FBIOPUT_VSCREENINFO);
	NAME(FBIOGET_FSCREENINFO);	/* TODO: print argument */
	NAME(FBIOPAN_DISPLAY);
	NAME(DSPIORATE);
	NAME(DSPIOSTEREO);
	NAME(DSPIOSIZE);
	NAME(DSPIOBITS);
	NAME(DSPIOSIGN);
	NAME(DSPIOMAX);
	NAME(DSPIORESET);		/* no argument */
	NAME(DSPIOFREEBUF);
	NAME(DSPIOSAMPLESINBUF);
	NAME(DSPIOPAUSE);		/* no argument */
	NAME(DSPIORESUME);		/* no argument */
	NAME(MIXIOGETVOLUME);
	NAME(MIXIOGETINPUTLEFT);
	NAME(MIXIOGETINPUTRIGHT);
	NAME(MIXIOGETOUTPUT);
	NAME(MIXIOSETVOLUME);
	NAME(MIXIOSETINPUTLEFT);
	NAME(MIXIOSETINPUTRIGHT);
	NAME(MIXIOSETOUTPUT);
	NAME(TIOCEXCL);			/* no argument */
	NAME(TIOCNXCL);			/* no argument */
	NAME(TIOCFLUSH);
	NAME(TIOCGETA);
	NAME(TIOCSETA);
	NAME(TIOCSETAW);
	NAME(TIOCSETAF);
	NAME(TIOCGETD);
	NAME(TIOCSETD);
	NAME(TIOCGLINED);
	NAME(TIOCSLINED);
	NAME(TIOCSBRK);			/* no argument */
	NAME(TIOCCBRK);			/* no argument */
	NAME(TIOCSDTR);			/* no argument */
	NAME(TIOCCDTR);			/* no argument */
	NAME(TIOCGPGRP);
	NAME(TIOCSPGRP);
	NAME(TIOCOUTQ);
	NAME(TIOCSTI);
	NAME(TIOCNOTTY);		/* no argument */
	NAME(TIOCPKT);
	NAME(TIOCSTOP);			/* no argument */
	NAME(TIOCSTART);		/* no argument */
	NAME(TIOCMSET);			/* TODO: print argument */
	NAME(TIOCMBIS);			/* TODO: print argument */
	NAME(TIOCMBIC);			/* TODO: print argument */
	NAME(TIOCMGET);			/* TODO: print argument */
	NAME(TIOCREMOTE);
	NAME(TIOCGWINSZ);
	NAME(TIOCSWINSZ);
	NAME(TIOCUCNTL);
	NAME(TIOCSTAT);
	NAME(TIOCGSID);
	NAME(TIOCCONS);
	NAME(TIOCSCTTY);		/* no argument */
	NAME(TIOCEXT);
	NAME(TIOCSIG);			/* no argument */
	NAME(TIOCDRAIN);		/* no argument */
	NAME(TIOCGFLAGS);		/* TODO: print argument */
	NAME(TIOCSFLAGS);		/* TODO: print argument */
	NAME(TIOCDCDTIMESTAMP);		/* TODO: print argument */
	NAME(TIOCRCVFRAME);		/* TODO: print argument */
	NAME(TIOCXMTFRAME);		/* TODO: print argument */
	NAME(TIOCPTMGET);		/* TODO: print argument */
	NAME(TIOCGRANTPT);		/* no argument */
	NAME(TIOCPTSNAME);		/* TODO: print argument */
	NAME(TIOCSQSIZE);
	NAME(TIOCGQSIZE);
	NAME(TIOCSFON);			/* big IOCTL, not printing argument */
	NAME(KIOCBELL);
	NAME(KIOCSLEDS);
	NAME(KIOCSMAP);			/* not worth interpreting */
	NAME(PCI_IOC_CFGREAD);
	NAME(PCI_IOC_CFGWRITE);
	NAME(PCI_IOC_BDF_CFGREAD);
	NAME(PCI_IOC_BDF_CFGWRITE);
	NAME(PCI_IOC_BUSINFO);
	NAME(PCI_IOC_MAP);
	NAME(PCI_IOC_UNMAP);
	NAME(PCI_IOC_RESERVE);
	NAME(PCI_IOC_RELEASE);
	}

	return NULL;
}

static void
put_i2c_op(struct trace_proc * proc, const char *name, i2c_op_t op)
{
	const char *text = NULL;

	if (!valuesonly) {
		switch (op) {
		TEXT(I2C_OP_READ);
		TEXT(I2C_OP_READ_WITH_STOP);
		TEXT(I2C_OP_WRITE);
		TEXT(I2C_OP_WRITE_WITH_STOP);
		TEXT(I2C_OP_READ_BLOCK);
		TEXT(I2C_OP_WRITE_BLOCK);
		}
	}

	if (text != NULL)
		put_field(proc, name, text);
	else
		put_value(proc, name, "%d", op);
}

static void
put_sound_device(struct trace_proc * proc, const char * name, int device)
{
	const char *text = NULL;

	if (!valuesonly) {
		switch (device) {
		TEXT(Master);
		TEXT(Dac);
		TEXT(Fm);
		TEXT(Cd);
		TEXT(Line);
		TEXT(Mic);
		TEXT(Speaker);
		TEXT(Treble);
		TEXT(Bass);
		}
	}

	if (text != NULL)
		put_field(proc, name, text);
	else
		put_value(proc, name, "%d", device);
}

static void
put_sound_state(struct trace_proc * proc, const char * name, int state)
{

	if (!valuesonly && state == ON)
		put_field(proc, name, "ON");
	else if (!valuesonly && state == OFF)
		put_field(proc, name, "OFF");
	else
		put_value(proc, name, "%d", state);
}

static const struct flags flush_flags[] = {
	FLAG(FREAD),
	FLAG(FWRITE),
};

static const struct flags tc_iflags[] = {
	FLAG(IGNBRK),
	FLAG(BRKINT),
	FLAG(IGNPAR),
	FLAG(PARMRK),
	FLAG(INPCK),
	FLAG(ISTRIP),
	FLAG(INLCR),
	FLAG(IGNCR),
	FLAG(ICRNL),
	FLAG(IXON),
	FLAG(IXOFF),
	FLAG(IXANY),
	FLAG(IMAXBEL),
};

static const struct flags tc_oflags[] = {
	FLAG(OPOST),
	FLAG(ONLCR),
	FLAG(OXTABS),
	FLAG(ONOEOT),
	FLAG(OCRNL),
	FLAG(ONOCR),
	FLAG(ONLRET),
};

static const struct flags tc_cflags[] = {
	FLAG(CIGNORE),
	FLAG_MASK(CSIZE, CS5),
	FLAG_MASK(CSIZE, CS6),
	FLAG_MASK(CSIZE, CS7),
	FLAG_MASK(CSIZE, CS8),
	FLAG(CSTOPB),
	FLAG(CREAD),
	FLAG(PARENB),
	FLAG(PARODD),
	FLAG(HUPCL),
	FLAG(CLOCAL),
	FLAG(CRTSCTS),
	FLAG(CDTRCTS),
	FLAG(MDMBUF),
};

static const struct flags tc_lflags[] = {
	FLAG(ECHOKE),
	FLAG(ECHOE),
	FLAG(ECHOK),
	FLAG(ECHO),
	FLAG(ECHONL),
	FLAG(ECHOPRT),
	FLAG(ECHOCTL),
	FLAG(ISIG),
	FLAG(ICANON),
	FLAG(ALTWERASE),
	FLAG(IEXTEN),
	FLAG(EXTPROC),
	FLAG(TOSTOP),
	FLAG(FLUSHO),
	FLAG(NOKERNINFO),
	FLAG(PENDIN),
	FLAG(NOFLSH),
};

static void
put_tty_disc(struct trace_proc * proc, const char * name, int disc)
{
	const char *text = NULL;

	if (!valuesonly) {
		switch (disc) {
		TEXT(TTYDISC);
		TEXT(TABLDISC);
		TEXT(SLIPDISC);
		TEXT(PPPDISC);
		TEXT(STRIPDISC);
		TEXT(HDLCDISC);
		}
	}

	if (text != NULL)
		put_field(proc, name, text);
	else
		put_value(proc, name, "%d", disc);
}

static const struct flags kbd_leds[] = {
	FLAG(KBD_LEDS_NUM),
	FLAG(KBD_LEDS_CAPS),
	FLAG(KBD_LEDS_SCROLL),
};

int
char_ioctl_arg(struct trace_proc * proc, unsigned long req, void * ptr,
	int dir)
{
	minix_i2c_ioctl_exec_t *iie;
	struct fb_var_screeninfo *fbvs;
	struct volume_level *level;
	struct inout_ctrl *inout;
	struct termios *tc;
	struct ptmget *pm;
	struct winsize *ws;
	struct kio_bell *bell;
	struct kio_leds *leds;
	struct pciio_cfgreg *pci_cfgreg;
	struct pciio_bdf_cfgreg *pci_bdf_cfgreg;
	struct pciio_businfo *pci_businfo;
	struct pciio_map *pci_iomap;
	struct pciio_acl *pci_acl;

	switch (req) {
	case MINIX_I2C_IOCTL_EXEC:
		if ((iie = (minix_i2c_ioctl_exec_t *)ptr) == NULL)
			return IF_OUT; /* we print only the request for now */

		put_i2c_op(proc, "iie_op", iie->iie_op);
		put_value(proc, "iie_addr", "0x%04x", iie->iie_addr);
		return 0; /* TODO: print command/data/result */

	case FBIOGET_VSCREENINFO:
		if ((fbvs = (struct fb_var_screeninfo *)ptr) == NULL)
			return IF_IN;

		put_value(proc, "xres", "%"PRIu32, fbvs->xres);
		put_value(proc, "yres", "%"PRIu32, fbvs->yres);
		put_value(proc, "xres_virtual", "%"PRIu32, fbvs->xres_virtual);
		put_value(proc, "yres_virtual", "%"PRIu32, fbvs->yres_virtual);
		put_value(proc, "xoffset", "%"PRIu32, fbvs->xoffset);
		put_value(proc, "yoffset", "%"PRIu32, fbvs->yoffset);
		put_value(proc, "bits_per_pixel", "%"PRIu32,
		    fbvs->bits_per_pixel);
		return 0;

	case FBIOPUT_VSCREENINFO:
	case FBIOPAN_DISPLAY:
		if ((fbvs = (struct fb_var_screeninfo *)ptr) == NULL)
			return IF_OUT;

		put_value(proc, "xoffset", "%"PRIu32, fbvs->xoffset);
		put_value(proc, "yoffset", "%"PRIu32, fbvs->yoffset);
		return 0;

	case DSPIORATE:
	case DSPIOSTEREO:
	case DSPIOSIZE:
	case DSPIOBITS:
	case DSPIOSIGN:
	case DSPIOMAX:
	case DSPIOFREEBUF:
	case DSPIOSAMPLESINBUF:
		if (ptr == NULL)
			return dir;

		put_value(proc, NULL, "%u", *(unsigned int *)ptr);
		return IF_ALL;

	case MIXIOGETVOLUME:
		if ((level = (struct volume_level *)ptr) == NULL)
			return dir;

		if (dir == IF_OUT)
			put_sound_device(proc, "device", level->device);
		else {
			put_value(proc, "left", "%d", level->left);
			put_value(proc, "right", "%d", level->right);
		}
		return IF_ALL;

	case MIXIOSETVOLUME:
		/* Print the corrected volume levels only with verbosity on. */
		if ((level = (struct volume_level *)ptr) == NULL)
			return IF_OUT | ((verbose > 0) ? IF_IN : 0);

		if (dir == IF_OUT)
			put_sound_device(proc, "device", level->device);
		put_value(proc, "left", "%d", level->left);
		put_value(proc, "right", "%d", level->right);
		return IF_ALL;

	case MIXIOGETINPUTLEFT:
	case MIXIOGETINPUTRIGHT:
	case MIXIOGETOUTPUT:
		if ((inout = (struct inout_ctrl *)ptr) == NULL)
			return dir;

		if (dir == IF_OUT)
			put_sound_device(proc, "device", inout->device);
		else {
			put_sound_state(proc, "left", inout->left);
			put_sound_state(proc, "right", inout->right);
		}
		return IF_ALL;

	case MIXIOSETINPUTLEFT:
	case MIXIOSETINPUTRIGHT:
	case MIXIOSETOUTPUT:
		if ((inout = (struct inout_ctrl *)ptr) == NULL)
			return IF_OUT;

		put_sound_device(proc, "device", inout->device);
		put_sound_state(proc, "left", inout->left);
		put_sound_state(proc, "right", inout->right);
		return IF_ALL;

	case TIOCFLUSH:
		if (ptr == NULL)
			return IF_OUT;

		put_flags(proc, NULL, flush_flags, COUNT(flush_flags), "0x%x",
		    *(int *)ptr);
		return IF_ALL;

	case TIOCGETA:
	case TIOCSETA:
	case TIOCSETAW:
	case TIOCSETAF:
		if ((tc = (struct termios *)ptr) == NULL)
			return dir;

		/*
		 * These are fairly common IOCTLs, so printing everything by
		 * default would create a lot of noise.  By default we limit
		 * ourselves to printing the field that contains what I
		 * consider to be the most important flag: ICANON.
		 * TODO: see if we can come up with a decent format for
		 * selectively printing (relatively important) flags.
		 */
		if (verbose > 0) {
			put_flags(proc, "c_iflag", tc_iflags, COUNT(tc_iflags),
			    "0x%x", tc->c_iflag);
			put_flags(proc, "c_oflag", tc_oflags, COUNT(tc_oflags),
			    "0x%x", tc->c_oflag);
			put_flags(proc, "c_cflag", tc_cflags, COUNT(tc_cflags),
			    "0x%x", tc->c_cflag);
		}
		put_flags(proc, "c_lflag", tc_lflags, COUNT(tc_lflags), "0x%x",
			tc->c_lflag);
		if (verbose > 0) {
			put_value(proc, "c_ispeed", "%d", tc->c_ispeed);
			put_value(proc, "c_ospeed", "%d", tc->c_ospeed);
		}
		return 0; /* TODO: print the c_cc fields */

	case TIOCGETD:
	case TIOCSETD:
		if (ptr == NULL)
			return dir;

		put_tty_disc(proc, NULL, *(int *)ptr);
		return IF_ALL;

	case TIOCGLINED:
	case TIOCSLINED:
		if (ptr == NULL)
			return dir;

		put_buf(proc, NULL, PF_LOCADDR | PF_STRING, (vir_bytes)ptr,
		    sizeof(linedn_t));
		return IF_ALL;

	case TIOCGPGRP:
	case TIOCSPGRP:
	case TIOCOUTQ:
	case TIOCPKT:
	case TIOCREMOTE:
	case TIOCUCNTL:
	case TIOCSTAT:		/* argument seems unused? */
	case TIOCGSID:
	case TIOCCONS:		/* argument seems unused? */
	case TIOCEXT:
	case TIOCSQSIZE:
	case TIOCGQSIZE:
		/* Print a simple integer. */
		if (ptr == NULL)
			return dir;

		put_value(proc, NULL, "%d", *(int *)ptr);
		return IF_ALL;

	case TIOCPTSNAME:
		if ((pm = (struct ptmget *)ptr) == NULL)
			return IF_IN;

		put_buf(proc, "sn", PF_LOCADDR | PF_STRING, (vir_bytes)pm->sn,
		    sizeof(pm->sn));
		return IF_ALL;

	case TIOCSTI:
		if (ptr == NULL)
			return dir;

		if (!valuesonly)
			put_value(proc, NULL, "'%s'",
			    get_escape(*(char *)ptr));
		else
			put_value(proc, NULL, "%u", *(char *)ptr);
		return IF_ALL;

	case TIOCGWINSZ:
	case TIOCSWINSZ:
		if ((ws = (struct winsize *)ptr) == NULL)
			return dir;

		/* This is a stupid order, but we follow the struct layout. */
		put_value(proc, "ws_row", "%u", ws->ws_row);
		put_value(proc, "ws_col", "%u", ws->ws_col);
		if (verbose > 0) {
			put_value(proc, "ws_xpixel", "%u", ws->ws_xpixel);
			put_value(proc, "ws_ypixel", "%u", ws->ws_ypixel);
		}
		return (verbose > 0) ? IF_ALL : 0;

	case KIOCBELL:
		if ((bell = (struct kio_bell *)ptr) == NULL)
			return IF_OUT;

		put_value(proc, "kb_pitch", "%u", bell->kb_pitch);
		put_value(proc, "kb_volume", "%lu", bell->kb_volume);
		put_struct_timeval(proc, "kb_duration", PF_LOCADDR,
		    (vir_bytes)&bell->kb_duration);

		return IF_ALL;

	case KIOCSLEDS:
		if ((leds = (struct kio_leds *)ptr) == NULL)
			return IF_OUT;

		put_flags(proc, "kl_bits", kbd_leds, COUNT(kbd_leds), "0x%x",
		    leds->kl_bits);
		return IF_ALL;

	case PCI_IOC_CFGREAD:
		if ((pci_cfgreg = (struct pciio_cfgreg *)ptr) == NULL)
			return IF_IN;

		put_ptr(proc, "reg", (vir_bytes)pci_cfgreg->reg);
		put_value(proc, "val", "%08x", pci_cfgreg->val);
		return IF_ALL;

	case PCI_IOC_CFGWRITE:
		if ((pci_cfgreg = (struct pciio_cfgreg *)ptr) == NULL)
			return IF_OUT;

		put_ptr(proc, "reg", (vir_bytes)pci_cfgreg->reg);
		put_value(proc, "val", "%08x", pci_cfgreg->val);
		return IF_ALL;

	case PCI_IOC_BDF_CFGREAD:
		if ((pci_bdf_cfgreg = (struct pciio_bdf_cfgreg *)ptr) == NULL)
			return IF_IN;

		put_value(proc, "bus", "%u", pci_bdf_cfgreg->bus);
		put_value(proc, "device", "%u", pci_bdf_cfgreg->device);
		put_value(proc, "function", "%u", pci_bdf_cfgreg->function);
		put_ptr(proc, "cfgreg.reg", (vir_bytes)pci_bdf_cfgreg->cfgreg.reg);
		put_value(proc, "cfgreg.val", "%08x", pci_bdf_cfgreg->cfgreg.val);
		return IF_ALL;

	case PCI_IOC_BDF_CFGWRITE:
		if ((pci_bdf_cfgreg = (struct pciio_bdf_cfgreg *)ptr) == NULL)
			return IF_OUT;

		put_value(proc, "bus", "%u", pci_bdf_cfgreg->bus);
		put_value(proc, "device", "%u", pci_bdf_cfgreg->device);
		put_value(proc, "function", "%u", pci_bdf_cfgreg->function);
		put_ptr(proc, "cfgreg.reg", (vir_bytes)pci_bdf_cfgreg->cfgreg.reg);
		put_value(proc, "cfgreg.val", "%08x", pci_bdf_cfgreg->cfgreg.val);
		return IF_ALL;

	case PCI_IOC_BUSINFO:
		if ((pci_businfo = (struct pciio_businfo *)ptr) == NULL)
			return IF_IN;

		put_value(proc, "busno", "%u", pci_businfo->busno);
		put_value(proc, "maxdevs", "%u", pci_businfo->maxdevs);
		return IF_ALL;

	case PCI_IOC_MAP:
		if ((pci_iomap = (struct pciio_map *)ptr) == NULL)
			return IF_OUT|IF_IN;

		put_value(proc, "flags", "%x", pci_iomap->flags);
		put_value(proc, "phys_offset", "%08x", pci_iomap->phys_offset);
		put_value(proc, "size", "%zu", pci_iomap->size);
		put_value(proc, "readonly", "%x", pci_iomap->readonly);

		if (IF_IN == dir)
			put_ptr(proc, "vaddr_ret", (vir_bytes)pci_iomap->vaddr_ret);

		return IF_ALL;

	case PCI_IOC_UNMAP:
		if ((pci_iomap = (struct pciio_map *)ptr) == NULL)
			return IF_OUT;

		put_ptr(proc, "vaddr", (vir_bytes)pci_iomap->vaddr);

		return IF_ALL;

	case PCI_IOC_RESERVE:
		if ((pci_acl = (struct pciio_acl *)ptr) == NULL)
			return IF_OUT;

		put_value(proc, "domain", "%u", pci_acl->domain);
		put_value(proc, "bus", "%u", pci_acl->bus);
		put_value(proc, "device", "%u", pci_acl->device);
		put_value(proc, "function", "%u", pci_acl->function);

		return IF_ALL;
	case PCI_IOC_RELEASE:
		if ((pci_acl = (struct pciio_acl *)ptr) == NULL)
			return IF_OUT;

		put_value(proc, "domain", "%u", pci_acl->domain);
		put_value(proc, "bus", "%u", pci_acl->bus);
		put_value(proc, "device", "%u", pci_acl->device);
		put_value(proc, "function", "%u", pci_acl->function);

		return IF_ALL;

	default:
		return 0;
	}
}
