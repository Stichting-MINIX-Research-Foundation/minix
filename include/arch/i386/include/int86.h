/*	int86.h - 8086 interrupt types			Author: Kees J. Bot
 *								3 May 2000
 */

/* Registers used in an PC real mode call for BIOS or DOS services.  A
 * driver is called through the vector if the interrupt number is zero.
 */
union reg86 {
    struct l {
	u32_t	ef;			/* 32 bit flags (output only) */
	u32_t	vec;			/* Driver vector (input only) */
	u32_t	_ds_es[1];
	u32_t	eax;			/* 32 bit general registers */
	u32_t	ebx;
	u32_t	ecx;
	u32_t	edx;
	u32_t	esi;
	u32_t	edi;
	u32_t	ebp;
    } l;
    struct w {
	u16_t	f, _ef[1];		/* 16 bit flags (output only) */
	u16_t	off, seg;		/* Driver vector (input only) */
	u16_t	ds, es;			/* DS and ES real mode segment regs */
	u16_t	ax, _eax[1];		/* 16 bit general registers */
	u16_t	bx, _ebx[1];
	u16_t	cx, _ecx[1];
	u16_t	dx, _edx[1];
	u16_t	si, _esi[1];
	u16_t	di, _edi[1];
	u16_t	bp, _ebp[1];
    } w;
    struct b {
	u8_t	intno, _intno[3];	/* Interrupt number (input only) */
	u8_t	_vec[4];
	u8_t	_ds_es[4];
	u8_t	al, ah, _eax[2];	/* 8 bit general registers */
	u8_t	bl, bh, _ebx[2];
	u8_t	cl, ch, _ecx[2];
	u8_t	dl, dh, _edx[2];
	u8_t	_esi[4];
	u8_t	_edi[4];
	u8_t	_ebp[4];
    } b;
};

struct reg86u { union reg86 u; };	/* Better for forward declarations */

/* Parameters passed on ioctls to the memory task. */

struct mio_int86 {		/* MIOCINT86 */
	union reg86 reg86;		/* x86 registers as above */
	u16_t	off, seg;		/* Address of kernel buffer */
	void	*buf;			/* User data buffer */
	size_t	len;			/* Size of user buffer */
};

struct mio_ldt86 {		/* MIOCGLDT86, MIOCSLDT86 */
	size_t	idx;			/* Index in process' LDT */
	u16_t	entry[4];		/* One LDT entry to get or set. */
};
