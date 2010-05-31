/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
#include "obj.h"

extern long		lseek();

/*
 * Parts of the output file.
 */
#undef PARTEMIT
#undef PARTRELO
#undef PARTNAME
#undef PARTCHAR
#undef PARTDBUG
#undef NPARTS

#define	PARTEMIT	0
#define	PARTRELO	1
#define	PARTNAME	2
#define	PARTCHAR	3
#ifdef SYMDBUG
#define PARTDBUG	4
#else
#define PARTDBUG	3
#endif
#define	NPARTS		(PARTDBUG + 1)

static long		offset[MAXSECT];

static int		outfile;
static long		outseek[NPARTS];
static long		currpos;
static long		rd_base;
#define OUTSECT(i) \
	(outseek[PARTEMIT] = offset[i])
#define BEGINSEEK(p, o) \
	(outseek[(p)] = (o))

static int sectionnr;

static void
OUTREAD(p, b, n)
	char *b;
	long n;
{
	register long l = outseek[p];

	if (currpos != l) {
		lseek(outfile, l, 0);
	}
	rd_bytes(outfile, b, n);
	l += n;
	currpos = l;
	outseek[p] = l;
}

/*
 * Open the output file according to the chosen strategy.
 */
int
rd_open(f)
	char *f;
{

	if ((outfile = open(f, 0)) < 0)
		return 0;
	return rd_fdopen(outfile);
}

static int offcnt;

int
rd_fdopen(fd)
{
	register int i;

	for (i = 0; i < NPARTS; i++) outseek[i] = 0;
	offcnt = 0;
	rd_base = lseek(fd, 0L, 1);
	if (rd_base < 0) {
		return 0;
	}
	currpos = rd_base;
	outseek[PARTEMIT] = currpos;
	outfile = fd;
	sectionnr = 0;
	return 1;
}

void
rd_close()
{

	close(outfile);
	outfile = -1;
}

int
rd_fd()
{
	return outfile;
}

void
rd_ohead(head)
	register struct outhead	*head;
{
	register long off;

	OUTREAD(PARTEMIT, (char *) head, (long) SZ_HEAD);
#if BYTE_ORDER == 0x0123
	if (sizeof(struct outhead) != SZ_HEAD)
#endif
	{
		register char *c = (char *) head + (SZ_HEAD-4);
		
		head->oh_nchar = get4(c);
		c -= 4; head->oh_nemit = get4(c);
		c -= 2; head->oh_nname = uget2(c);
		c -= 2; head->oh_nrelo = uget2(c);
		c -= 2; head->oh_nsect = uget2(c);
		c -= 2; head->oh_flags = uget2(c);
		c -= 2; head->oh_stamp = uget2(c);
		c -= 2; head->oh_magic = uget2(c);
	}
	off = OFF_RELO(*head) + rd_base;
	BEGINSEEK(PARTRELO, off);
	off += (long) head->oh_nrelo * SZ_RELO;
	BEGINSEEK(PARTNAME, off);
	off += (long) head->oh_nname * SZ_NAME;
	BEGINSEEK(PARTCHAR, off);
#ifdef SYMDBUG
	off += head->oh_nchar;
	BEGINSEEK(PARTDBUG, off);
#endif
}

void
rd_rew_relos(head)
	register struct outhead *head;
{
	register long off = OFF_RELO(*head) + rd_base;

	BEGINSEEK(PARTRELO, off);
}

void
rd_sect(sect, cnt)
	register struct outsect	*sect;
	register unsigned int	cnt;
{
	register char *c = (char *) sect + cnt * SZ_SECT;

	OUTREAD(PARTEMIT, (char *) sect, (long)cnt * SZ_SECT);
	sect += cnt;
	offcnt += cnt;
	while (cnt--) {
		sect--;
#if BYTE_ORDER == 0x0123
		if (sizeof(struct outsect) != SZ_SECT)
#endif
		{
			c -= 4; sect->os_lign = get4(c);
			c -= 4; sect->os_flen = get4(c);
			c -= 4; sect->os_foff = get4(c);
			c -= 4; sect->os_size = get4(c);
			c -= 4; sect->os_base = get4(c);
		}
		offset[--offcnt] = sect->os_foff + rd_base;
	}
}

void
rd_outsect(s)
{
	OUTSECT(s);
	sectionnr = s;
}

/*
 * We don't have to worry about byte order here.
 */
void
rd_emit(emit, cnt)
	char		*emit;
	long		cnt;
{
	OUTREAD(PARTEMIT, emit, cnt);
	offset[sectionnr] += cnt;
}

void
rd_relo(relo, cnt)
	register struct outrelo	*relo;
	register unsigned int cnt;
{

	OUTREAD(PARTRELO, (char *) relo, (long) cnt * SZ_RELO);
#if BYTE_ORDER == 0x0123
	if (sizeof(struct outrelo) != SZ_RELO)
#endif
	{
		register char *c = (char *) relo + (long) cnt * SZ_RELO;

		relo += cnt;
		while (cnt--) {
			relo--;
			c -= 4; relo->or_addr = get4(c);
			c -= 2; relo->or_nami = uget2(c);
			relo->or_sect = *--c;
			relo->or_type = *--c;
		}
	}
}

void
rd_name(name, cnt)
	register struct outname	*name;
	register unsigned int cnt;
{

	OUTREAD(PARTNAME, (char *) name, (long) cnt * SZ_NAME);
#if BYTE_ORDER == 0x0123
	if (sizeof(struct outname) != SZ_NAME)
#endif
	{
		register char *c = (char *) name + (long) cnt * SZ_NAME;

		name += cnt;
		while (cnt--) {
			name--;
			c -= 4; name->on_valu = get4(c);
			c -= 2; name->on_desc = uget2(c);
			c -= 2; name->on_type = uget2(c);
			c -= 4; name->on_foff = get4(c);
		}
	}
}

void
rd_string(addr, len)
	char *addr;
	long len;
{
	
	OUTREAD(PARTCHAR, addr, len);
}

#ifdef SYMDBUG
void
rd_dbug(buf, size)
	char		*buf;
	long		size;
{
	OUTREAD(PARTDBUG, buf, size);
}
#endif
