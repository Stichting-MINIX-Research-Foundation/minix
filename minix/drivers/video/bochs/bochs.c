/*
 * bochs - "DRM-lite" driver for the Bochs/QEMU standard VGA (PCI 1234:1111,
 * QEMU "-vga std").  It exposes /dev/drm0 with the interface described in
 * <sys/ioc_mdrm.h>: dumb buffers in video memory that clients mmap(2),
 * mode setting through the Bochs VBE "DISPI" registers, and page flipping
 * by moving the display start offset, with flip completion events emulated
 * at 60 Hz from the clock.
 *
 * Every open of minor 0 creates a client (cloned minor 1..NR_CLIENTS).
 * Clients own their buffers.  The client that sets a mode is the master
 * until it closes the device, which turns the display off again.  The
 * first BOCHS_RESERVED bytes of video memory are never handed out: they hold
 * the VGA text console (and its font), which therefore survives a session in
 * graphics mode.  The VGA registers that enabling VBE overwrites are saved
 * and restored on the way back to text mode.
 *
 * Limitation: a client's mmap() of a buffer outlives destroying the buffer
 * or closing the device, and the memory may then be given to another
 * buffer; /dev/drm0 is therefore root/operator-only (as Linux's primary DRM
 * nodes effectively are for modesetting).
 */

#include <minix/drivers.h>
#include <minix/chardriver.h>
#include <minix/sysutil.h>
#include <machine/pci.h>
#include <machine/vmparam.h>
#include <sys/mman.h>
#include <sys/ioc_mdrm.h>
#include <sys/ioc_file.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

/* Bochs VBE "DISPI" interface. */
#define VBE_DISPI_IOPORT_INDEX	0x01CE
/* 0x1CF is the classic data port, but MINIX only permits aligned word I/O
 * (kernel do_devio); QEMU and Bochs also decode the data register at 0x1D0. */
#define VBE_DISPI_IOPORT_DATA	0x01D0

#define VBE_DISPI_INDEX_ID		0x0
#define VBE_DISPI_INDEX_XRES		0x1
#define VBE_DISPI_INDEX_YRES		0x2
#define VBE_DISPI_INDEX_BPP		0x3
#define VBE_DISPI_INDEX_ENABLE		0x4
#define VBE_DISPI_INDEX_BANK		0x5
#define VBE_DISPI_INDEX_VIRT_WIDTH	0x6
#define VBE_DISPI_INDEX_VIRT_HEIGHT	0x7
#define VBE_DISPI_INDEX_X_OFFSET	0x8
#define VBE_DISPI_INDEX_Y_OFFSET	0x9

#define VBE_DISPI_ID2			0xB0C2	/* 32 bpp support */
#define VBE_DISPI_ID5			0xB0C5

#define VBE_DISPI_DISABLED		0x00
#define VBE_DISPI_ENABLED		0x01
#define VBE_DISPI_GETCAPS		0x02
#define VBE_DISPI_LFB_ENABLED		0x40
#define VBE_DISPI_NOCLEARMEM		0x80

/* VGA registers (color mode) that enabling VBE clobbers. */
#define VGA_SEQ_INDEX	0x3C4
#define VGA_SEQ_DATA	0x3C5
#define VGA_GFX_INDEX	0x3CE
#define VGA_GFX_DATA	0x3CF
#define VGA_CRTC_INDEX	0x3D4
#define VGA_CRTC_DATA	0x3D5

static const u8_t crtc_saved_regs[] = { 0x01, 0x07, 0x09, 0x11, 0x12, 0x13,
	0x17, 0x18 };
static const u8_t gfx_saved_regs[] = { 0x05, 0x06 };
static const u8_t seq_saved_regs[] = { 0x01, 0x02, 0x04 };

#define BOCHS_VID	0x1234
#define BOCHS_DID	0x1111

#define BOCHS_RESERVED	(256 * 1024)	/* VGA planes: text console + font */

#define NR_CLIENTS	16		/* minors 1..NR_CLIENTS */
#define NR_BUFS		64
#define NR_EVENTS	8		/* queued events per client */
#define BPP		32

struct buf {
	int		b_owner;	/* client minor, 0 = free slot */
	int		b_zombie;	/* destroyed while scanned out */
	u32_t		b_handle;
	u32_t		b_width, b_height, b_pitch;
	u32_t		b_offset;	/* in video memory */
	u32_t		b_size;		/* page multiple */
};

struct client {
	int		c_open;
	/* Completion events waiting to be read. */
	struct mdrm_event_vblank c_ev[NR_EVENTS];
	int		c_nev;
	/* Suspended read, if any. */
	endpoint_t	c_rd_endpt;	/* NONE if none */
	cp_grant_id_t	c_rd_grant;
	size_t		c_rd_size;
	cdev_id_t	c_rd_id;
	/* Select. */
	int		c_selected;	/* CDEV_OP_RD if someone waits */
	endpoint_t	c_sel_endpt;
};

static struct client clients[NR_CLIENTS + 1];	/* [0] unused */
static struct buf bufs[NR_BUFS];
static u32_t next_handle = 1;

static phys_bytes vram_phys;
static u32_t vram_size;
static u8_t *vram;			/* mapped into this driver */
static u16_t dispi_id;
static u16_t max_xres, max_yres;
static int preferred_w = 1024, preferred_h = 768;

/* Display state. */
static int master;			/* client minor, 0 = none */
static u16_t cur_w, cur_h;		/* 0 = display off */
static struct buf *cur_fb;
static int vga_saved;
static u8_t crtc_save[sizeof(crtc_saved_regs)];
static u8_t gfx_save[sizeof(gfx_saved_regs)];
static u8_t seq_save[sizeof(seq_saved_regs)];

/* Emulated vblank. */
static clock_t hz;
static u32_t vblank_seq;
static int flip_pending;		/* a flip event is due next tick */
static int flip_client;
static u64_t flip_user_data;
static int alarm_set;

static const struct mdrm_mode std_modes[] = {
	{ 640, 480, 60, 0 },	{ 800, 600, 60, 0 },	{ 1024, 768, 60, 0 },
	{ 1280, 720, 60, 0 },	{ 1280, 800, 60, 0 },	{ 1280, 1024, 60, 0 },
	{ 1366, 768, 60, 0 },	{ 1440, 900, 60, 0 },	{ 1600, 900, 60, 0 },
	{ 1680, 1050, 60, 0 },	{ 1920, 1080, 60, 0 },	{ 1920, 1200, 60, 0 },
};

static int bochs_open(devminor_t minor, int access, endpoint_t user_endpt);
static int bochs_close(devminor_t minor);
static ssize_t bochs_read(devminor_t minor, u64_t position, endpoint_t endpt,
	cp_grant_id_t grant, size_t size, int flags, cdev_id_t id);
static int bochs_ioctl(devminor_t minor, unsigned long request,
	endpoint_t endpt, cp_grant_id_t grant, int flags,
	endpoint_t user_endpt, cdev_id_t id);
static int bochs_cancel(devminor_t minor, endpoint_t endpt, cdev_id_t id);
static int bochs_select(devminor_t minor, unsigned int ops, endpoint_t endpt);
static void bochs_alarm(clock_t stamp);

static struct chardriver bochs_tab = {
	.cdr_open	= bochs_open,
	.cdr_close	= bochs_close,
	.cdr_read	= bochs_read,
	.cdr_ioctl	= bochs_ioctl,
	.cdr_cancel	= bochs_cancel,
	.cdr_select	= bochs_select,
	.cdr_alarm	= bochs_alarm,
};

/*===========================================================================*
 *				Hardware access				     *
 *===========================================================================*/
static void dispi_write(u16_t index, u16_t value)
{
	int r;

	if ((r = sys_outw(VBE_DISPI_IOPORT_INDEX, index)) != OK ||
	    (r = sys_outw(VBE_DISPI_IOPORT_DATA, value)) != OK)
		panic("bochs: sys_outw failed: %d", r);
}

static u16_t dispi_read(u16_t index)
{
	u32_t value;
	int r;

	if ((r = sys_outw(VBE_DISPI_IOPORT_INDEX, index)) != OK ||
	    (r = sys_inw(VBE_DISPI_IOPORT_DATA, &value)) != OK)
		panic("bochs: sys_inw failed: %d", r);
	return (u16_t)value;
}

static u8_t vga_read(int index_port, u8_t index)
{
	u32_t value;

	if (sys_outb(index_port, index) != OK ||
	    sys_inb(index_port + 1, &value) != OK)
		panic("bochs: VGA register read failed");
	return (u8_t)value;
}

static void vga_write(int index_port, u8_t index, u8_t value)
{
	if (sys_outb(index_port, index) != OK ||
	    sys_outb(index_port + 1, value) != OK)
		panic("bochs: VGA register write failed");
}

static void vga_save(void)
{
	unsigned int i;

	for (i = 0; i < sizeof(crtc_saved_regs); i++)
		crtc_save[i] = vga_read(VGA_CRTC_INDEX, crtc_saved_regs[i]);
	for (i = 0; i < sizeof(gfx_saved_regs); i++)
		gfx_save[i] = vga_read(VGA_GFX_INDEX, gfx_saved_regs[i]);
	for (i = 0; i < sizeof(seq_saved_regs); i++)
		seq_save[i] = vga_read(VGA_SEQ_INDEX, seq_saved_regs[i]);
	vga_saved = TRUE;
}

static void vga_restore(void)
{
	unsigned int i;
	u8_t cr11;

	if (!vga_saved)
		return;

	/* Unlock CRTC registers 0-7 (CR11 bit 7) while restoring. */
	cr11 = vga_read(VGA_CRTC_INDEX, 0x11);
	vga_write(VGA_CRTC_INDEX, 0x11, cr11 & ~0x80);
	for (i = 0; i < sizeof(crtc_saved_regs); i++)
		if (crtc_saved_regs[i] != 0x11)
			vga_write(VGA_CRTC_INDEX, crtc_saved_regs[i],
			    crtc_save[i]);
	for (i = 0; i < sizeof(crtc_saved_regs); i++)
		if (crtc_saved_regs[i] == 0x11)
			vga_write(VGA_CRTC_INDEX, 0x11, crtc_save[i]);
	for (i = 0; i < sizeof(gfx_saved_regs); i++)
		vga_write(VGA_GFX_INDEX, gfx_saved_regs[i], gfx_save[i]);
	for (i = 0; i < sizeof(seq_saved_regs); i++)
		vga_write(VGA_SEQ_INDEX, seq_saved_regs[i], seq_save[i]);
}

/* Program a mode scanning out 'b'.  Return OK or an error. */
static int hw_setmode(u16_t w, u16_t h, struct buf *b)
{
	if (cur_w == 0 && !vga_saved)
		vga_save();

	dispi_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
	dispi_write(VBE_DISPI_INDEX_XRES, w);
	dispi_write(VBE_DISPI_INDEX_YRES, h);
	dispi_write(VBE_DISPI_INDEX_BPP, BPP);
	dispi_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED |
	    VBE_DISPI_LFB_ENABLED | VBE_DISPI_NOCLEARMEM);
	dispi_write(VBE_DISPI_INDEX_VIRT_WIDTH, b->b_pitch / (BPP / 8));
	dispi_write(VBE_DISPI_INDEX_X_OFFSET, 0);
	dispi_write(VBE_DISPI_INDEX_Y_OFFSET, b->b_offset / b->b_pitch);

	if (dispi_read(VBE_DISPI_INDEX_XRES) != w ||
	    dispi_read(VBE_DISPI_INDEX_YRES) != h ||
	    dispi_read(VBE_DISPI_INDEX_BPP) != BPP ||
	    dispi_read(VBE_DISPI_INDEX_Y_OFFSET) != b->b_offset / b->b_pitch) {
		printf("bochs: mode %ux%u not accepted by the device\n", w, h);
		dispi_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
		vga_restore();
		cur_w = cur_h = 0;
		return EINVAL;
	}

	cur_w = w;
	cur_h = h;
	return OK;
}

static void hw_flip(struct buf *b)
{
	dispi_write(VBE_DISPI_INDEX_Y_OFFSET, b->b_offset / b->b_pitch);
}

static void hw_off(void)
{
	if (cur_w == 0)
		return;
	dispi_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
	vga_restore();
	cur_w = cur_h = 0;
}

/*===========================================================================*
 *				Buffers					     *
 *===========================================================================*/
static struct buf *buf_lookup(int minor, u32_t handle)
{
	int i;

	for (i = 0; i < NR_BUFS; i++)
		if (bufs[i].b_owner == minor && !bufs[i].b_zombie &&
		    bufs[i].b_handle == handle)
			return &bufs[i];
	return NULL;
}

static u32_t gcd(u32_t a, u32_t b)
{
	while (b != 0) {
		u32_t t = a % b;
		a = b;
		b = t;
	}
	return a;
}

/* Find 'size' bytes of free video memory aligned to 'align'.  First fit. */
static int vram_alloc(u32_t size, u32_t align, u32_t *offp)
{
	u32_t off = roundup(BOCHS_RESERVED, align);
	int i, moved;

	do {
		moved = FALSE;
		if (off + size > vram_size || off + size < off)
			return ENOSPC;
		for (i = 0; i < NR_BUFS; i++) {
			struct buf *b = &bufs[i];
			if (b->b_owner == 0) continue;
			if (off < b->b_offset + b->b_size &&
			    b->b_offset < off + size) {
				off = roundup(b->b_offset + b->b_size, align);
				moved = TRUE;
			}
		}
	} while (moved);

	*offp = off;
	return OK;
}

static void buf_free(struct buf *b)
{
	if (b == cur_fb) {
		/* Keep scanning it out until the mode changes. */
		b->b_zombie = TRUE;
		return;
	}
	memset(b, 0, sizeof(*b));
}

static int do_create_dumb(int minor, struct mdrm_create_dumb *req)
{
	struct buf *b = NULL;
	u32_t pitch, size, align, off;
	int i, r;

	if (req->bpp != BPP || req->flags != 0 || req->width == 0 ||
	    req->height == 0 || req->width > 8192 || req->height > 8192)
		return EINVAL;

	pitch = req->width * (BPP / 8);
	size = roundup(pitch * req->height, PAGE_SIZE);
	/* Scanout needs the buffer to start on a whole line. */
	align = PAGE_SIZE / gcd(PAGE_SIZE, pitch) * pitch;

	for (i = 0; i < NR_BUFS; i++)
		if (bufs[i].b_owner == 0) { b = &bufs[i]; break; }
	if (b == NULL)
		return ENOSPC;

	if ((r = vram_alloc(size, align, &off)) != OK)
		return ENOMEM;

	b->b_owner = minor;
	b->b_zombie = FALSE;
	b->b_handle = next_handle++;
	if (next_handle == 0) next_handle = 1;
	b->b_width = req->width;
	b->b_height = req->height;
	b->b_pitch = pitch;
	b->b_offset = off;
	b->b_size = size;

	/* Do not leak the previous contents of this memory. */
	memset(vram + off, 0, size);

	req->handle = b->b_handle;
	req->pitch = pitch;
	req->size = size;
	return OK;
}

/*===========================================================================*
 *				Events					     *
 *===========================================================================*/
static void client_wakeup(int minor)
{
	struct client *c = &clients[minor];
	size_t done;
	int r = OK;

	if (c->c_nev == 0)
		return;

	if (c->c_rd_endpt != NONE) {
		/* Deliver as many whole events as fit. */
		for (done = 0; c->c_nev > 0 &&
		    done + sizeof(c->c_ev[0]) <= c->c_rd_size;
		    done += sizeof(c->c_ev[0])) {
			r = sys_safecopyto(c->c_rd_endpt, c->c_rd_grant, done,
			    (vir_bytes)&c->c_ev[0], sizeof(c->c_ev[0]));
			if (r != OK) break;
			memmove(&c->c_ev[0], &c->c_ev[1],
			    --c->c_nev * sizeof(c->c_ev[0]));
		}
		chardriver_reply_task(c->c_rd_endpt, c->c_rd_id,
		    (done > 0 || r == OK) ? (int)done : r);
		c->c_rd_endpt = NONE;
	}

	if (c->c_nev > 0 && (c->c_selected & CDEV_OP_RD)) {
		chardriver_reply_select(c->c_sel_endpt, minor, CDEV_OP_RD);
		c->c_selected &= ~CDEV_OP_RD;
	}
}

static void queue_event(int minor, u64_t user_data)
{
	struct client *c = &clients[minor];
	struct mdrm_event_vblank *ev;
	clock_t ticks;

	if (!c->c_open)
		return;
	if (c->c_nev == NR_EVENTS) {
		/* Client is not reading; drop the oldest. */
		memmove(&c->c_ev[0], &c->c_ev[1],
		    (NR_EVENTS - 1) * sizeof(c->c_ev[0]));
		c->c_nev--;
	}

	ticks = getticks();
	ev = &c->c_ev[c->c_nev++];
	memset(ev, 0, sizeof(*ev));
	ev->type = MDRM_EVENT_FLIP_COMPLETE;
	ev->length = sizeof(*ev);
	ev->user_data = user_data;
	ev->tv_sec = ticks / hz;
	ev->tv_usec = (u32_t)((u64_t)(ticks % hz) * 1000000 / hz);
	ev->sequence = vblank_seq;
	ev->crtc_id = 0;

	client_wakeup(minor);
}

static void arm_alarm(void)
{
	int r;

	if (alarm_set)
		return;
	/* Next tick ~ next vblank at the default 60 Hz system clock. */
	if ((r = sys_setalarm(MAX(1, hz / 60), 0)) != OK)
		panic("bochs: sys_setalarm failed: %d", r);
	alarm_set = TRUE;
}

static void bochs_alarm(clock_t UNUSED(stamp))
{
	alarm_set = FALSE;
	vblank_seq++;

	if (flip_pending) {
		flip_pending = FALSE;
		queue_event(flip_client, flip_user_data);
	}
}

/*===========================================================================*
 *				Mode setting				     *
 *===========================================================================*/
static void release_master(void)
{
	int i;

	hw_off();
	cur_fb = NULL;
	master = 0;
	flip_pending = FALSE;
	/* Scanout no longer holds any destroyed buffers. */
	for (i = 0; i < NR_BUFS; i++)
		if (bufs[i].b_owner != 0 && bufs[i].b_zombie)
			memset(&bufs[i], 0, sizeof(bufs[i]));
}

static void set_scanout(struct buf *b)
{
	struct buf *old = cur_fb;

	cur_fb = b;
	if (old != NULL && old != b && old->b_zombie)
		memset(old, 0, sizeof(*old));
}

static int do_setcrtc(int minor, struct mdrm_setcrtc *req)
{
	struct buf *b;
	int r;

	if (master != 0 && master != minor)
		return EBUSY;

	if (req->fb == 0) {
		if (master == minor)
			release_master();
		return OK;
	}

	if ((b = buf_lookup(minor, req->fb)) == NULL)
		return ENOENT;
	if (req->width == 0 || req->height == 0 || req->width > max_xres ||
	    req->height > max_yres || b->b_width < req->width ||
	    b->b_height < req->height || req->width % 8 != 0)
		return EINVAL;

	if ((r = hw_setmode(req->width, req->height, b)) != OK) {
		if (master == minor) release_master();
		return r;
	}

	master = minor;
	set_scanout(b);
	return OK;
}

static int do_page_flip(int minor, struct mdrm_page_flip *req)
{
	struct buf *b;

	if (master != minor || cur_w == 0)
		return EBUSY;
	if (flip_pending)
		return EBUSY;		/* as Linux: one flip per vblank */
	if ((b = buf_lookup(minor, req->fb)) == NULL)
		return ENOENT;
	if (b->b_pitch != cur_fb->b_pitch || b->b_width < cur_w ||
	    b->b_height < cur_h)
		return EINVAL;

	/* Switch now; report completion at the next emulated vblank, which
	 * also paces the client to the refresh rate.
	 */
	hw_flip(b);
	set_scanout(b);

	if (req->flags & MDRM_FLIP_EVENT) {
		flip_pending = TRUE;
		flip_client = minor;
		flip_user_data = req->user_data;
		arm_alarm();
	}
	return OK;
}

static void do_getinfo(int minor, struct mdrm_info *info)
{
	unsigned int i;

	memset(info, 0, sizeof(*info));
	memcpy(info->name, "bochs", sizeof("bochs"));
	info->version = MDRM_VERSION;
	info->vram_size = vram_size;
	info->max_width = max_xres;
	info->max_height = max_yres;

	for (i = 0; i < __arraycount(std_modes) &&
	    info->nmodes < MDRM_MAX_MODES; i++) {
		const struct mdrm_mode *m = &std_modes[i];
		/* Only modes that fit in memory double buffered. */
		if (m->width > max_xres || m->height > max_yres ||
		    2 * (u64_t)m->width * m->height * (BPP / 8) >
		    vram_size - BOCHS_RESERVED)
			continue;
		info->modes[info->nmodes] = *m;
		if (m->width == preferred_w && m->height == preferred_h)
			info->modes[info->nmodes].flags |= MDRM_MODE_PREFERRED;
		info->nmodes++;
	}

	info->cur_width = cur_w;
	info->cur_height = cur_h;
	info->cur_fb = (cur_fb != NULL && cur_fb->b_owner == minor &&
	    !cur_fb->b_zombie) ? cur_fb->b_handle : 0;
	info->vblank_seq = vblank_seq;
	info->is_master = (master == minor);
}

/* FIOMMAP from VFS: a client mmap()s [offset, offset+len) of its fd. */
static int do_mmap(int minor, struct devmmap *req)
{
	int i;

	for (i = 0; i < NR_BUFS; i++) {
		struct buf *b = &bufs[i];
		if (b->b_owner != minor || b->b_zombie)
			continue;
		if (req->dm_offset >= b->b_offset &&
		    req->dm_offset + req->dm_len <= b->b_offset + b->b_size) {
			req->dm_phys = vram_phys + req->dm_offset;
			return OK;
		}
	}
	return EINVAL;
}

/*===========================================================================*
 *				Character driver callbacks		     *
 *===========================================================================*/
static int bochs_open(devminor_t minor, int UNUSED(access),
	endpoint_t UNUSED(user_endpt))
{
	int i;

	if (minor != 0)
		return ENXIO;	/* clients only come into being by cloning */

	for (i = 1; i <= NR_CLIENTS; i++) {
		if (!clients[i].c_open) {
			memset(&clients[i], 0, sizeof(clients[i]));
			clients[i].c_open = TRUE;
			clients[i].c_rd_endpt = NONE;
			return CDEV_CLONED | i;
		}
	}
	return ENFILE;
}

static int bochs_close(devminor_t minor)
{
	int i;

	if (minor < 1 || minor > NR_CLIENTS || !clients[minor].c_open)
		return ENXIO;

	if (master == minor)
		release_master();
	for (i = 0; i < NR_BUFS; i++)
		if (bufs[i].b_owner == minor)
			buf_free(&bufs[i]);
	if (flip_client == minor)
		flip_pending = FALSE;
	clients[minor].c_open = FALSE;
	return OK;
}

static ssize_t bochs_read(devminor_t minor, u64_t UNUSED(position),
	endpoint_t endpt, cp_grant_id_t grant, size_t size, int flags,
	cdev_id_t id)
{
	struct client *c;
	size_t done;
	int r;

	if (minor < 1 || minor > NR_CLIENTS || !clients[minor].c_open)
		return ENXIO;
	c = &clients[minor];

	if (size < sizeof(struct mdrm_event_vblank))
		return EINVAL;
	if (c->c_rd_endpt != NONE)
		return EIO;	/* one reader at a time */

	if (c->c_nev == 0) {
		if (flags & CDEV_NONBLOCK)
			return EAGAIN;
		c->c_rd_endpt = endpt;
		c->c_rd_grant = grant;
		c->c_rd_size = size;
		c->c_rd_id = id;
		return EDONTREPLY;
	}

	for (done = 0; c->c_nev > 0 && done + sizeof(c->c_ev[0]) <= size;
	    done += sizeof(c->c_ev[0])) {
		r = sys_safecopyto(endpt, grant, done, (vir_bytes)&c->c_ev[0],
		    sizeof(c->c_ev[0]));
		if (r != OK)
			return done > 0 ? (ssize_t)done : r;
		memmove(&c->c_ev[0], &c->c_ev[1],
		    --c->c_nev * sizeof(c->c_ev[0]));
	}
	return done;
}

static int bochs_ioctl(devminor_t minor, unsigned long request,
	endpoint_t endpt, cp_grant_id_t grant, int UNUSED(flags),
	endpoint_t UNUSED(user_endpt), cdev_id_t UNUSED(id))
{
	union {
		struct mdrm_info info;
		struct mdrm_create_dumb create;
		struct mdrm_map_dumb map;
		struct mdrm_destroy_dumb destroy;
		struct mdrm_setcrtc setcrtc;
		struct mdrm_page_flip flip;
		struct devmmap mmap;
	} u;
	struct buf *b;
	size_t size;
	int r;

	if (minor < 1 || minor > NR_CLIENTS || !clients[minor].c_open)
		return ENXIO;

	switch (request) {
	case MDRM_GETINFO:	size = sizeof(u.info);		break;
	case MDRM_CREATE_DUMB:	size = sizeof(u.create);	break;
	case MDRM_MAP_DUMB:	size = sizeof(u.map);		break;
	case MDRM_DESTROY_DUMB:	size = sizeof(u.destroy);	break;
	case MDRM_SETCRTC:	size = sizeof(u.setcrtc);	break;
	case MDRM_PAGE_FLIP:	size = sizeof(u.flip);		break;
	case FIOMMAP:		size = sizeof(u.mmap);		break;
	default:
		return ENOTTY;
	}

	if (request != MDRM_GETINFO &&
	    (r = sys_safecopyfrom(endpt, grant, 0, (vir_bytes)&u, size)) != OK)
		return r;

	switch (request) {
	case MDRM_GETINFO:
		do_getinfo(minor, &u.info);
		r = OK;
		break;
	case MDRM_CREATE_DUMB:
		r = do_create_dumb(minor, &u.create);
		break;
	case MDRM_MAP_DUMB:
		if ((b = buf_lookup(minor, u.map.handle)) == NULL) {
			r = ENOENT;
		} else {
			u.map.offset = b->b_offset;
			r = OK;
		}
		break;
	case MDRM_DESTROY_DUMB:
		if ((b = buf_lookup(minor, u.destroy.handle)) == NULL) {
			r = ENOENT;
		} else {
			buf_free(b);
			r = OK;
		}
		return r;
	case MDRM_SETCRTC:
		return do_setcrtc(minor, &u.setcrtc);
	case MDRM_PAGE_FLIP:
		return do_page_flip(minor, &u.flip);
	case FIOMMAP:
		r = do_mmap(minor, &u.mmap);
		break;
	default:
		return ENOTTY;
	}

	if (r == OK)
		r = sys_safecopyto(endpt, grant, 0, (vir_bytes)&u, size);
	return r;
}

static int bochs_cancel(devminor_t minor, endpoint_t endpt, cdev_id_t id)
{
	struct client *c;

	if (minor < 1 || minor > NR_CLIENTS)
		return EDONTREPLY;
	c = &clients[minor];
	if (c->c_rd_endpt != endpt || c->c_rd_id != id)
		return EDONTREPLY;	/* stale */
	c->c_rd_endpt = NONE;
	return EINTR;
}

static int bochs_select(devminor_t minor, unsigned int ops, endpoint_t endpt)
{
	struct client *c;
	unsigned int ready = 0, want;

	if (minor < 1 || minor > NR_CLIENTS || !clients[minor].c_open)
		return ENXIO;
	c = &clients[minor];

	want = ops & (CDEV_OP_RD | CDEV_OP_WR | CDEV_OP_ERR);
	if ((want & CDEV_OP_RD) && c->c_nev > 0)
		ready |= CDEV_OP_RD;

	want &= ~ready & CDEV_OP_RD;
	if ((ops & CDEV_NOTIFY) && want) {
		c->c_selected |= want;
		c->c_sel_endpt = endpt;
	}
	return ready;
}

/*===========================================================================*
 *				Initialization				     *
 *===========================================================================*/
static int bochs_probe(void)
{
	int devind, ioflag, r;
	u16_t vid, did;
	u32_t base, size;
	char val[32];

	pci_init();
	r = pci_first_dev(&devind, &vid, &did);
	while (r > 0 && !(vid == BOCHS_VID && did == BOCHS_DID))
		r = pci_next_dev(&devind, &vid, &did);
	if (r <= 0) {
		printf("bochs: no Bochs/QEMU standard VGA found\n");
		return ENODEV;
	}

	pci_reserve(devind);

	if ((r = pci_get_bar(devind, PCI_BAR, &base, &size, &ioflag)) != OK ||
	    ioflag) {
		printf("bochs: no video memory BAR (%d)\n", r);
		return EIO;
	}
	vram_phys = base;
	vram_size = size;

	dispi_id = dispi_read(VBE_DISPI_INDEX_ID);
	if (dispi_id < VBE_DISPI_ID2 || dispi_id > VBE_DISPI_ID5 + 0x10) {
		printf("bochs: unsupported DISPI id 0x%x\n", dispi_id);
		return ENODEV;
	}

	/* Maximum resolution: ask the device. */
	dispi_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_GETCAPS);
	max_xres = dispi_read(VBE_DISPI_INDEX_XRES);
	max_yres = dispi_read(VBE_DISPI_INDEX_YRES);
	dispi_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
	if (max_xres == 0 || max_yres == 0) {
		max_xres = 1920;
		max_yres = 1200;
	}

	if ((vram = vm_map_phys(SELF, (void *)vram_phys, vram_size)) ==
	    MAP_FAILED) {
		printf("bochs: cannot map video memory\n");
		return ENOMEM;
	}

	/* Optional preferred mode: -args "mode=1280x800". */
	if (env_get_param("mode", val, sizeof(val)) == OK) {
		char *end;
		unsigned long w, h;
		w = strtoul(val, &end, 10);
		if (*end == 'x') {
			h = strtoul(end + 1, &end, 10);
			if (*end == '\0' && w > 0 && h > 0 && w <= 8192 &&
			    h <= 8192) {
				preferred_w = (int)w;
				preferred_h = (int)h;
			}
		}
	}

	printf("bochs: %s, DISPI 0x%x, %u KB video memory at 0x%lx, "
	    "max %ux%u\n", pci_slot_name(devind), dispi_id, vram_size / 1024,
	    (unsigned long)vram_phys, max_xres, max_yres);
	return OK;
}

static int sef_cb_init_fresh(int UNUSED(type), sef_init_info_t *UNUSED(info))
{
	int r;

	if ((hz = sys_hz()) <= 0)
		hz = DEFAULT_HZ;

	if ((r = bochs_probe()) != OK)
		return r;

	chardriver_announce();
	return OK;
}

static void sef_cb_signal_handler(int signo)
{
	if (signo != SIGTERM)
		return;
	/* Give the console back. */
	hw_off();
	chardriver_terminate();
}

int main(void)
{
	sef_setcb_init_fresh(sef_cb_init_fresh);
	sef_setcb_init_restart(sef_cb_init_fresh);
	sef_setcb_signal_handler(sef_cb_signal_handler);
	sef_startup();

	chardriver_task(&bochs_tab);
	return 0;
}
