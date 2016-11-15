/* $NetBSD: hd44780_subr.c,v 1.21 2010/11/13 13:52:01 uebayasi Exp $ */

/*
 * Copyright (c) 2002 Dennis I. Chernoivanov
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Subroutines for Hitachi HD44870 style displays
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: hd44780_subr.c,v 1.21 2010/11/13 13:52:01 uebayasi Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/types.h>
#include <sys/ioccom.h>

#include <machine/autoconf.h>
#include <sys/intr.h>
#include <sys/bus.h>

#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wscons_callbacks.h>

#include <dev/ic/hd44780reg.h>
#include <dev/ic/hd44780var.h>

#define COORD_TO_IDX(x, y)	((y) * sc->sc_cols + (x))
#define COORD_TO_DADDR(x, y)	((y) * HD_ROW2_ADDR + (x))
#define IDX_TO_ROW(idx)		((idx) / sc->sc_cols)
#define IDX_TO_COL(idx)		((idx) % sc->sc_cols)
#define IDX_TO_DADDR(idx)	(IDX_TO_ROW((idx)) * HD_ROW2_ADDR + \
				IDX_TO_COL((idx)))
#define DADDR_TO_ROW(daddr)	((daddr) / HD_ROW2_ADDR)
#define DADDR_TO_COL(daddr)	((daddr) % HD_ROW2_ADDR)
#define DADDR_TO_CHIPDADDR(daddr)	((daddr) % (HD_ROW2_ADDR * 2))
#define DADDR_TO_CHIPNO(daddr)	((daddr) / (HD_ROW2_ADDR * 2))

static void	hlcd_cursor(void *, int, int, int);
static int	hlcd_mapchar(void *, int, unsigned int *);
static void	hlcd_putchar(void *, int, int, u_int, long);
static void	hlcd_copycols(void *, int, int, int,int);
static void	hlcd_erasecols(void *, int, int, int, long);
static void	hlcd_copyrows(void *, int, int, int);
static void	hlcd_eraserows(void *, int, int, long);
static int	hlcd_allocattr(void *, int, int, int, long *);
static void	hlcd_updatechar(struct hd44780_chip *, int, int);
static void	hlcd_redraw(void *);

const struct wsdisplay_emulops hlcd_emulops = {
	hlcd_cursor,
	hlcd_mapchar,
	hlcd_putchar,
	hlcd_copycols,
	hlcd_erasecols,
	hlcd_copyrows,
	hlcd_eraserows,
	hlcd_allocattr
};

static int	hlcd_ioctl(void *, void *, u_long, void *, int, struct lwp *);
static paddr_t	hlcd_mmap(void *, void *, off_t, int);
static int	hlcd_alloc_screen(void *, const struct wsscreen_descr *,
		    void **, int *, int *, long *);
static void	hlcd_free_screen(void *, void *);
static int	hlcd_show_screen(void *, void *, int,
		    void (*) (void *, int, int), void *);

const struct wsdisplay_accessops hlcd_accessops = {
	hlcd_ioctl,
	hlcd_mmap,
	hlcd_alloc_screen,
	hlcd_free_screen,
	hlcd_show_screen,
	0 /* load_font */
};

static void
hlcd_cursor(void *id, int on, int row, int col)
{
	struct hlcd_screen *hdscr = id;

	hdscr->hlcd_curon = on;
	hdscr->hlcd_curx = col;
	hdscr->hlcd_cury = row;
}

static int
hlcd_mapchar(void *id, int uni, unsigned int *index)
{

	if (uni < 256) {
		*index = uni;
		return 5;
	}
	*index = ' ';
	return 0;
}

static void
hlcd_putchar(void *id, int row, int col, u_int c, long attr)
{
	struct hlcd_screen *hdscr = id;

	c &= 0xff;
	if (row > 0 && (hdscr->hlcd_sc->sc_flags & (HD_MULTILINE|HD_MULTICHIP)))
		hdscr->image[hdscr->hlcd_sc->sc_cols * row + col] = c;
	else
		hdscr->image[col] = c;
}

/*
 * copies columns inside a row.
 */
static void
hlcd_copycols(void *id, int row, int srccol, int dstcol, int ncols)
{
	struct hlcd_screen *hdscr = id;

	if ((dstcol + ncols - 1) > hdscr->hlcd_sc->sc_cols)
		ncols = hdscr->hlcd_sc->sc_cols - srccol;

	if (row > 0 && (hdscr->hlcd_sc->sc_flags & (HD_MULTILINE|HD_MULTICHIP)))
		memmove(&hdscr->image[hdscr->hlcd_sc->sc_cols * row + dstcol],
		    &hdscr->image[hdscr->hlcd_sc->sc_cols * row + srccol],
		    ncols);
	else
		memmove(&hdscr->image[dstcol], &hdscr->image[srccol], ncols);
}


/*
 * Erases a bunch of chars inside one row.
 */
static void
hlcd_erasecols(void *id, int row, int startcol, int ncols, long fillattr)
{
	struct hlcd_screen *hdscr = id;

	if ((startcol + ncols) > hdscr->hlcd_sc->sc_cols)
		ncols = hdscr->hlcd_sc->sc_cols - startcol;

	if (row > 0 && (hdscr->hlcd_sc->sc_flags & (HD_MULTILINE|HD_MULTICHIP)))
		memset(&hdscr->image[hdscr->hlcd_sc->sc_cols * row + startcol],
		    ' ', ncols);
	else
		memset(&hdscr->image[startcol], ' ', ncols);
}


static void
hlcd_copyrows(void *id, int srcrow, int dstrow, int nrows)
{
	struct hlcd_screen *hdscr = id;
	int ncols = hdscr->hlcd_sc->sc_cols;

	if (!(hdscr->hlcd_sc->sc_flags & (HD_MULTILINE|HD_MULTICHIP)))
		return;
	memmove(&hdscr->image[dstrow * ncols], &hdscr->image[srcrow * ncols],
	    nrows * ncols);
}

static void
hlcd_eraserows(void *id, int startrow, int nrows, long fillattr)
{
	struct hlcd_screen *hdscr = id;
	int ncols = hdscr->hlcd_sc->sc_cols;

	memset(&hdscr->image[startrow * ncols], ' ', ncols * nrows);
}


static int
hlcd_allocattr(void *id, int fg, int bg, int flags, long *attrp)
{

        *attrp = flags;
        return 0;
}

static int
hlcd_ioctl(void *v, void *vs, u_long cmd, void *data, int flag, struct lwp *l)
{

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_HDLCD;
		break;

	case WSDISPLAYIO_SVIDEO:
		break;

	case WSDISPLAYIO_GVIDEO:
		*(u_int *)data = WSDISPLAYIO_VIDEO_ON;
		break;

	default:
		return EPASSTHROUGH;
	}
	return 0;
}

static paddr_t
hlcd_mmap(void *v, void *vs, off_t offset, int prot)
{

	return -1;
}

static int
hlcd_alloc_screen(void *v, const struct wsscreen_descr *type, void **cookiep,
    int *curxp, int *curyp, long *defattrp)
{
	struct hlcd_screen *hdscr = v, *new;

	new = *cookiep = malloc(sizeof(struct hlcd_screen),
				M_DEVBUF, M_WAITOK|M_ZERO);
	new->hlcd_sc = hdscr->hlcd_sc;
	new->image = malloc(PAGE_SIZE, M_DEVBUF, M_WAITOK);
	memset(new->image, ' ', PAGE_SIZE);
	*curxp = *curyp = *defattrp = 0;
	return 0;
}

static void
hlcd_free_screen(void *v, void *cookie)
{
}

static int
hlcd_show_screen(void *v, void *cookie, int waitok,
    void (*cb)(void *, int, int), void *cbarg)
{
	struct hlcd_screen *hdscr = v;

	hdscr->hlcd_sc->sc_curscr = cookie;
	callout_schedule(&hdscr->hlcd_sc->redraw, 1);
	return 0;
}

static void
hlcd_updatechar(struct hd44780_chip *sc, int daddr, int c)
{
	int curdaddr, en, chipdaddr;

	curdaddr = COORD_TO_DADDR(sc->sc_screen.hlcd_curx,
	    sc->sc_screen.hlcd_cury);
	en = DADDR_TO_CHIPNO(daddr);
	chipdaddr = DADDR_TO_CHIPDADDR(daddr);
	if (daddr != curdaddr)
		hd44780_ir_write(sc, en, cmd_ddramset(chipdaddr));

	hd44780_dr_write(sc, en, c);

	daddr++;
	sc->sc_screen.hlcd_curx = DADDR_TO_COL(daddr);
	sc->sc_screen.hlcd_cury = DADDR_TO_ROW(daddr);
}

static void
hlcd_redraw(void *arg)
{
	struct hd44780_chip *sc = arg;
	int len, crsridx, startidx, x, y;
	int old_en, new_en;
	uint8_t *img, *curimg;

	if (sc->sc_curscr == NULL)
		return;

	if (sc->sc_flags & HD_MULTILINE)
		len = 2 * sc->sc_cols;
	else
		len = sc->sc_cols;

	if (sc->sc_flags & HD_MULTICHIP)
		len = len * 2;

	x = sc->sc_screen.hlcd_curx;
	y = sc->sc_screen.hlcd_cury;
	old_en = DADDR_TO_CHIPNO(COORD_TO_DADDR(x, y));

	img = sc->sc_screen.image;
	curimg = sc->sc_curscr->image;
	startidx = crsridx =
	    COORD_TO_IDX(sc->sc_screen.hlcd_curx, sc->sc_screen.hlcd_cury);
	do {
		if (img[crsridx] != curimg[crsridx]) {
			hlcd_updatechar(sc, IDX_TO_DADDR(crsridx),
			    curimg[crsridx]);
			img[crsridx] = curimg[crsridx];
		}
		crsridx++;
		if (crsridx == len)
			crsridx = 0;
	} while (crsridx != startidx);

	x = sc->sc_curscr->hlcd_curx;
	y = sc->sc_curscr->hlcd_cury;
	new_en = DADDR_TO_CHIPNO(COORD_TO_DADDR(x, y));

	if (sc->sc_screen.hlcd_curx != sc->sc_curscr->hlcd_curx ||
	    sc->sc_screen.hlcd_cury != sc->sc_curscr->hlcd_cury) {

		x = sc->sc_screen.hlcd_curx = sc->sc_curscr->hlcd_curx;
		y = sc->sc_screen.hlcd_cury = sc->sc_curscr->hlcd_cury;

		hd44780_ir_write(sc, new_en, cmd_ddramset(
		    DADDR_TO_CHIPDADDR(COORD_TO_DADDR(x, y))));

	}

	/* visible cursor switched to other chip */
	if (old_en != new_en && sc->sc_screen.hlcd_curon) {
		hd44780_ir_write(sc, old_en, cmd_dispctl(1, 0, 0));
		hd44780_ir_write(sc, new_en, cmd_dispctl(1, 1, 1));
	}

	if (sc->sc_screen.hlcd_curon != sc->sc_curscr->hlcd_curon) {
		sc->sc_screen.hlcd_curon = sc->sc_curscr->hlcd_curon;
		if (sc->sc_screen.hlcd_curon)
			hd44780_ir_write(sc, new_en, cmd_dispctl(1, 1, 1));
		else
			hd44780_ir_write(sc, new_en, cmd_dispctl(1, 0, 0));
	}

	callout_schedule(&sc->redraw, 1);
}


/*
 * Finish device attach. sc_writereg, sc_readreg and sc_flags must be properly
 * initialized prior to this call.
 */
void
hd44780_attach_subr(struct hd44780_chip *sc)
{
	int err = 0;

	/* Putc/getc are supposed to be set by platform-dependent code. */
	if ((sc->sc_writereg == NULL) || (sc->sc_readreg == NULL))
		sc->sc_dev_ok = 0;

	/* Make sure that HD_MAX_CHARS is enough. */
	if ((sc->sc_flags & HD_MULTILINE) && (2 * sc->sc_cols > HD_MAX_CHARS))
		sc->sc_dev_ok = 0;
	else if (sc->sc_cols > HD_MAX_CHARS)
		sc->sc_dev_ok = 0;

	if (sc->sc_dev_ok) {
		if ((sc->sc_flags & HD_UP) == 0)
			err = hd44780_init(sc);
		if (err != 0)
			aprint_error_dev(sc->sc_dev,
			    "LCD not responding or unconnected\n");
	}

	sc->sc_screen.hlcd_sc = sc;

	sc->sc_screen.image = malloc(PAGE_SIZE, M_DEVBUF, M_WAITOK);
	memset(sc->sc_screen.image, ' ', PAGE_SIZE);
	sc->sc_curscr = NULL;
	sc->sc_curchip = 0;
	callout_init(&sc->redraw, 0);
	callout_setfunc(&sc->redraw, hlcd_redraw, sc);
}

int hd44780_init(struct hd44780_chip *sc)
{
	int ret;

	ret = hd44780_chipinit(sc, 0);
	if (ret != 0 || !(sc->sc_flags & HD_MULTICHIP))
		return ret;
	else
		return hd44780_chipinit(sc, 1);
}

/*
 * Initialize 4-bit or 8-bit connected device.
 */
int
hd44780_chipinit(struct hd44780_chip *sc, uint32_t en)
{
	uint8_t cmd, dat;

	sc->sc_flags &= ~(HD_TIMEDOUT|HD_UP);
	sc->sc_dev_ok = 1;

	cmd = cmd_init(sc->sc_flags & HD_8BIT);
	hd44780_ir_write(sc, en, cmd);
	delay(HD_TIMEOUT_LONG);
	hd44780_ir_write(sc, en, cmd);
	hd44780_ir_write(sc, en, cmd);

	cmd = cmd_funcset(
			sc->sc_flags & HD_8BIT,
			sc->sc_flags & HD_MULTILINE,
			sc->sc_flags & HD_BIGFONT);

	if ((sc->sc_flags & HD_8BIT) == 0)
		hd44780_ir_write(sc, en, cmd);

	sc->sc_flags |= HD_UP;

	hd44780_ir_write(sc, en, cmd);
	hd44780_ir_write(sc, en, cmd_dispctl(0, 0, 0));
	hd44780_ir_write(sc, en, cmd_clear());
	hd44780_ir_write(sc, en, cmd_modset(1, 0));

	if (sc->sc_flags & HD_TIMEDOUT) {
		sc->sc_flags &= ~HD_UP;
		return EIO;
	}

	/* Turn display on and clear it. */
	hd44780_ir_write(sc, en, cmd_clear());
	hd44780_ir_write(sc, en, cmd_dispctl(1, 0, 0));

	/* Attempt a simple probe for presence */
	hd44780_ir_write(sc, en, cmd_ddramset(0x5));
	hd44780_ir_write(sc, en, cmd_shift(0, 1));
	hd44780_busy_wait(sc, en);
	if ((dat = hd44780_ir_read(sc, en) & 0x7f) != 0x6) {
		sc->sc_dev_ok = 0;
		sc->sc_flags &= ~HD_UP;
		return EIO;
	}
	hd44780_ir_write(sc, en, cmd_ddramset(0));

	return 0;
}

/*
 * Standard hd44780 ioctl() functions.
 */
int
hd44780_ioctl_subr(struct hd44780_chip *sc, u_long cmd, void *data)
{
	uint8_t tmp;
	int error = 0;
	uint32_t en = sc->sc_curchip;

#define hd44780_io()	((struct hd44780_io *)data)
#define hd44780_info()	((struct hd44780_info *)data)
#define hd44780_ctrl()	((struct hd44780_dispctl *)data)

	switch (cmd) {
	case HLCD_CLEAR:
		/* Clear the LCD. */
		hd44780_ir_write(sc, en, cmd_clear());
		break;

	case HLCD_CURSOR_LEFT:
		/* Move the cursor one position to the left. */
		hd44780_ir_write(sc, en, cmd_shift(0, 0));
			break;

	case HLCD_CURSOR_RIGHT:
		/* Move the cursor one position to the right. */
		hd44780_ir_write(sc, en, cmd_shift(0, 1));
		break;

	case HLCD_DISPCTL:
		/* Control the LCD. */
		hd44780_ir_write(sc, en, cmd_dispctl(
					hd44780_ctrl()->display_on,
					hd44780_ctrl()->cursor_on,
					hd44780_ctrl()->blink_on));
		break;

	case HLCD_GET_INFO:
		/* Get LCD configuration. */
		hd44780_info()->lines
			= (sc->sc_flags & HD_MULTILINE) ? 2 : 1;
		if (sc->sc_flags & HD_MULTICHIP)
			hd44780_info()->lines *= 2;
		hd44780_info()->phys_rows = sc->sc_cols;
		hd44780_info()->virt_rows = sc->sc_vcols;
		hd44780_info()->is_wide = sc->sc_flags & HD_8BIT;
		hd44780_info()->is_bigfont = sc->sc_flags & HD_BIGFONT;
		hd44780_info()->kp_present = sc->sc_flags & HD_KEYPAD;
		break;


	case HLCD_RESET:
		/* Reset the LCD. */
		error = hd44780_init(sc);
		break;

	case HLCD_GET_CURSOR_POS:
		/* Get the current cursor position. */
		hd44780_io()->dat = (hd44780_ir_read(sc, en) & 0x7f);
		break;

	case HLCD_SET_CURSOR_POS:
		/* Set the cursor position. */
		hd44780_ir_write(sc, en, cmd_ddramset(hd44780_io()->dat));
		break;

	case HLCD_GETC:
		/* Get the value at the current cursor position. */
		tmp = (hd44780_ir_read(sc, en) & 0x7f);
		hd44780_ir_write(sc, en, cmd_ddramset(tmp));
		hd44780_io()->dat = hd44780_dr_read(sc, en);
		break;

	case HLCD_PUTC:
		/* Set the character at the cursor position + advance cursor. */
		hd44780_dr_write(sc, en, hd44780_io()->dat);
		break;

	case HLCD_SHIFT_LEFT:
		/* Shift display left. */
		hd44780_ir_write(sc, en, cmd_shift(1, 0));
		break;

	case HLCD_SHIFT_RIGHT:
		/* Shift display right. */
		hd44780_ir_write(sc, en, cmd_shift(1, 1));
		break;

	case HLCD_HOME:
		/* Return home. */
		hd44780_ir_write(sc, en, cmd_rethome());
		break;

	case HLCD_WRITE:
		/* Write a string to the LCD virtual area. */
		error = hd44780_ddram_io(sc, en, hd44780_io(), HD_DDRAM_WRITE);
		break;

	case HLCD_READ:
		/* Read LCD virtual area. */
		error = hd44780_ddram_io(sc, en, hd44780_io(), HD_DDRAM_READ);
		break;

	case HLCD_REDRAW:
		/* Write to the LCD visible area. */
		hd44780_ddram_redraw(sc, en, hd44780_io());
		break;

	case HLCD_WRITE_INST:
		/* Write raw instruction. */
		hd44780_ir_write(sc, en, hd44780_io()->dat);
		break;

	case HLCD_WRITE_DATA:
		/* Write raw data. */
		hd44780_dr_write(sc, en, hd44780_io()->dat);
		break;

	case HLCD_GET_CHIPNO:
		/* Get current chip 0 or 1 (top or bottom) */
		*(uint8_t *)data = sc->sc_curchip;
		break;

	case HLCD_SET_CHIPNO:
		/* Set current chip 0 or 1 (top or bottom) */
		sc->sc_curchip = *(uint8_t *)data;
		break;

	default:
		error = EINVAL;
	}

	if (sc->sc_flags & HD_TIMEDOUT)
		error = EIO;

	return error;
}

/*
 * Read/write particular area of the LCD screen.
 */
int
hd44780_ddram_io(struct hd44780_chip *sc, uint32_t en, struct hd44780_io *io,
    uint8_t dir)
{
	uint8_t hi;
	uint8_t addr;
	int error = 0;
	uint8_t i = 0;

	if (io->dat < sc->sc_vcols) {
		hi = HD_ROW1_ADDR + sc->sc_vcols;
		addr = HD_ROW1_ADDR + io->dat;
		for (; (addr < hi) && (i < io->len); addr++, i++) {
			hd44780_ir_write(sc, en, cmd_ddramset(addr));
			if (dir == HD_DDRAM_READ)
				io->buf[i] = hd44780_dr_read(sc, en);
			else
				hd44780_dr_write(sc, en, io->buf[i]);
		}
	}
	if (io->dat < 2 * sc->sc_vcols) {
		hi = HD_ROW2_ADDR + sc->sc_vcols;
		if (io->dat >= sc->sc_vcols)
			addr = HD_ROW2_ADDR + io->dat - sc->sc_vcols;
		else
			addr = HD_ROW2_ADDR;
		for (; (addr < hi) && (i < io->len); addr++, i++) {
			hd44780_ir_write(sc, en, cmd_ddramset(addr));
			if (dir == HD_DDRAM_READ)
				io->buf[i] = hd44780_dr_read(sc, en);
			else
				hd44780_dr_write(sc, en, io->buf[i]);
		}
		if (i < io->len)
			io->len = i;
	} else {
		error = EINVAL;
	}
	return error;
}

/*
 * Write to the visible area of the display.
 */
void
hd44780_ddram_redraw(struct hd44780_chip *sc, uint32_t en,
    struct hd44780_io *io)
{
	uint8_t i;

	hd44780_ir_write(sc, en, cmd_clear());
	hd44780_ir_write(sc, en, cmd_rethome());
	hd44780_ir_write(sc, en, cmd_ddramset(HD_ROW1_ADDR));
	for (i = 0; (i < io->len) && (i < sc->sc_cols); i++) {
		hd44780_dr_write(sc, en, io->buf[i]);
	}
	hd44780_ir_write(sc, en, cmd_ddramset(HD_ROW2_ADDR));
	for (; (i < io->len); i++)
		hd44780_dr_write(sc, en, io->buf[i]);
}

void
hd44780_busy_wait(struct hd44780_chip *sc, uint32_t en)
{
	int nloops = 100;

	if (sc->sc_flags & HD_TIMEDOUT)
		return;

	while (nloops-- && (hd44780_ir_read(sc, en) & BUSY_FLAG) == BUSY_FLAG)
		continue;

	if (nloops == 0) {
		sc->sc_flags |= HD_TIMEDOUT;
		sc->sc_dev_ok = 0;
	}
}

#if defined(HD44780_STD_WIDE)
/*
 * Standard 8-bit version of 'sc_writereg' (8-bit port, 8-bit access)
 */
void
hd44780_writereg(struct hd44780_chip *sc, uint32_t en, uint32_t reg,
    uint8_t cmd)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh;

	if (sc->sc_dev_ok == 0)
		return;

	if (reg == 0)
		ioh = sc->sc_ioir;
	else
		ioh = sc->sc_iodr;

	bus_space_write_1(iot, ioh, 0x00, cmd);
	delay(HD_TIMEOUT_NORMAL);
}

/*
 * Standard 8-bit version of 'sc_readreg' (8-bit port, 8-bit access)
 */
uint8_t
hd44780_readreg(struct hd44780_chip *sc, uint32_t en, uint32_t reg)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh;

	if (sc->sc_dev_ok == 0)
		return;

	if (reg == 0)
		ioh = sc->sc_ioir;
	else
		ioh = sc->sc_iodr;

	delay(HD_TIMEOUT_NORMAL);
	return bus_space_read_1(iot, ioh, 0x00);
}
#elif defined(HD44780_STD_SHORT)
/*
 * Standard 4-bit version of 'sc_writereg' (4-bit port, 8-bit access)
 */
void
hd44780_writereg(struct hd44780_chip *sc, uint32_t en, uint32_t reg,
    uint8_t cmd)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh;

	if (sc->sc_dev_ok == 0)
		return;

	if (reg == 0)
		ioh = sc->sc_ioir;
	else
		ioh = sc->sc_iodr;

	bus_space_write_1(iot, ioh, 0x00, hi_bits(cmd));
	if (sc->sc_flags & HD_UP)
		bus_space_write_1(iot, ioh, 0x00, lo_bits(cmd));
	delay(HD_TIMEOUT_NORMAL);
}

/*
 * Standard 4-bit version of 'sc_readreg' (4-bit port, 8-bit access)
 */
uint8_t
hd44780_readreg(struct hd44780_chip *sc, uint32_t en, uint32_t reg)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh;
	uint8_t rd, dat;

	if (sc->sc_dev_ok == 0)
		return;

	if (reg == 0)
		ioh = sc->sc_ioir;
	else
		ioh = sc->sc_iodr;

	rd = bus_space_read_1(iot, ioh, 0x00);
	dat = (rd & 0x0f) << 4;
	rd = bus_space_read_1(iot, ioh, 0x00);
	return (dat | (rd & 0x0f));
}
#endif
