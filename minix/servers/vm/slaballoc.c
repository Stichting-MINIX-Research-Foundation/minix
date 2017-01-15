
#define _SYSTEM 1

#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/config.h>
#include <minix/const.h>
#include <minix/ds.h>
#include <minix/endpoint.h>
#include <minix/minlib.h>
#include <minix/type.h>
#include <minix/ipc.h>
#include <minix/sysutil.h>
#include <minix/syslib.h>
#include <minix/bitmap.h>
#include <minix/debug.h>

#include <assert.h>
#include <errno.h>
#include <string.h>

#include <sys/param.h>

#include "glo.h"
#include "proto.h"
#include "util.h"
#include "sanitycheck.h"

#define SLABSIZES 200

#define ITEMSPERPAGE(bytes) (int)(DATABYTES / (bytes))

#define ELBITS		(sizeof(element_t)*8)
#define BITPAT(b)	(1UL << ((b) %  ELBITS))
#define BITEL(f, b)	(f)->sdh.usebits[(b)/ELBITS]


#define OFF(f, b) assert(!GETBIT(f, b))
#define ON(f, b)  assert(GETBIT(f, b))

#if MEMPROTECT
#define SLABDATAWRITABLE(data, wr) do {			\
	assert(data->sdh.writable == WRITABLE_NONE);	\
	assert(wr != WRITABLE_NONE);			\
	vm_pagelock(data, 0);				\
	data->sdh.writable = wr;			\
} while(0)

#define SLABDATAUNWRITABLE(data) do {			\
	assert(data->sdh.writable != WRITABLE_NONE);	\
	data->sdh.writable = WRITABLE_NONE;		\
	vm_pagelock(data, 1);				\
} while(0)

#define SLABDATAUSE(data, code) do {			\
	SLABDATAWRITABLE(data, WRITABLE_HEADER);	\
	code						\
	SLABDATAUNWRITABLE(data);			\
} while(0)

#else

#define SLABDATAWRITABLE(data, wr)
#define SLABDATAUNWRITABLE(data)
#define SLABDATAUSE(data, code) do { code } while(0)

#endif

#define GETBIT(f, b)	  (BITEL(f,b) &   BITPAT(b))
#define SETBIT(f, b)   {OFF(f,b); SLABDATAUSE(f, BITEL(f,b)|= BITPAT(b); (f)->sdh.nused++;); }
#define CLEARBIT(f, b) {ON(f, b); SLABDATAUSE(f, BITEL(f,b)&=~BITPAT(b); (f)->sdh.nused--; (f)->sdh.freeguess = (b);); }

#define OBJALIGN	8

#define MINSIZE 8
#define MAXSIZE (SLABSIZES-1+MINSIZE)
#define USEELEMENTS (1+(VM_PAGE_SIZE/MINSIZE/8))

static int pages = 0;

typedef u8_t element_t;
#define BITS_FULL (~(element_t)0)
typedef element_t elements_t[USEELEMENTS];

/* This file is too low-level to have global SANITYCHECKs everywhere,
 * as the (other) data structures are often necessarily in an
 * inconsistent state during a slaballoc() / slabfree(). So only do
 * our own sanity checks here, with SLABSANITYCHECK.
 */


/* Special writable values. */
#define WRITABLE_NONE	-2
#define WRITABLE_HEADER	-1

struct sdh {
#if SANITYCHECKS
	u32_t magic1;
#endif
	int freeguess;
	struct slabdata *next, *prev;
	elements_t usebits;
	phys_bytes phys;
#if SANITYCHECKS
	int writable;	/* data item number or WRITABLE_* */
	u32_t magic2;
#endif
	u16_t nused;	/* Number of data items used in this slab. */
};

#define DATABYTES	(VM_PAGE_SIZE-sizeof(struct sdh))

#define MAGIC1 0x1f5b842f
#define MAGIC2 0x8bb5a420
#define JUNK  0xdeadbeef
#define NOJUNK 0xc0ffee

struct slabdata {
	u8_t 	data[DATABYTES];
	struct	sdh sdh;
};

static struct slabheader {
	struct slabdata *list_head;
} slabs[SLABSIZES];

static int objstats(void *, int, struct slabheader **, struct slabdata
	**, int *);

#define GETSLAB(b, s) {			\
	int _gsi;				\
	assert((b) >= MINSIZE);	\
	_gsi = (b) - MINSIZE;		\
	assert((_gsi) < SLABSIZES);	\
	assert((_gsi) >= 0);		\
	s = &slabs[_gsi];			\
}

/* move slabdata nw to slabheader sl under list number l. */
#define ADDHEAD(nw, sl) {			\
	SLABDATAUSE(nw,				\
		(nw)->sdh.next = sl->list_head;	\
		(nw)->sdh.prev = NULL;);	\
	sl->list_head = nw;			\
	if((nw)->sdh.next) {			\
		SLABDATAUSE((nw)->sdh.next, \
			(nw)->sdh.next->sdh.prev = (nw););	\
	} \
}

#define UNLINKNODE(node)	{				\
	struct slabdata *next, *prev;				\
	prev = (node)->sdh.prev;				\
	next = (node)->sdh.next;				\
	if(prev) { SLABDATAUSE(prev, prev->sdh.next = next;); }	\
	if(next) { SLABDATAUSE(next, next->sdh.prev = prev;); }	\
}

static struct slabdata *newslabdata(void)
{
	struct slabdata *n;
	phys_bytes p;

	assert(sizeof(*n) == VM_PAGE_SIZE);

	if(!(n = vm_allocpage(&p, VMP_SLAB))) {
		printf("newslabdata: vm_allocpage failed\n");
		return NULL;
	}
	memset(n->sdh.usebits, 0, sizeof(n->sdh.usebits));
	pages++;

	n->sdh.phys = p;
#if SANITYCHECKS
	n->sdh.magic1 = MAGIC1;
	n->sdh.magic2 = MAGIC2;
#endif
	n->sdh.nused = 0;
	n->sdh.freeguess = 0;

#if SANITYCHECKS
	n->sdh.writable = WRITABLE_HEADER;
	SLABDATAUNWRITABLE(n);
#endif

	return n;
}

#if SANITYCHECKS

/*===========================================================================*
 *				checklist				     *
 *===========================================================================*/
static int checklist(const char *file, int line,
	struct slabheader *s, int bytes)
{
	struct slabdata *n = s->list_head;
	int ch = 0;

	while(n) {
		int count = 0, i;
#if SANITYCHECKS
		MYASSERT(n->sdh.magic1 == MAGIC1);
		MYASSERT(n->sdh.magic2 == MAGIC2);
#endif
		MYASSERT(usedpages_add(n->sdh.phys, VM_PAGE_SIZE) == OK);
		if(n->sdh.prev)
			MYASSERT(n->sdh.prev->sdh.next == n);
		else
			MYASSERT(s->list_head == n);
		if(n->sdh.next) MYASSERT(n->sdh.next->sdh.prev == n);
		for(i = 0; i < USEELEMENTS*8; i++)
			if(i >= ITEMSPERPAGE(bytes))
				MYASSERT(!GETBIT(n, i));
			else
				if(GETBIT(n,i))
					count++;
		MYASSERT(count == n->sdh.nused);
		ch += count;
		n = n->sdh.next;
	}

	return ch;
}

/*===========================================================================*
 *				void slab_sanitycheck			     *
 *===========================================================================*/
void slab_sanitycheck(const char *file, int line)
{
	int s;
	for(s = 0; s < SLABSIZES; s++) {
		checklist(file, line, &slabs[s], s + MINSIZE);
	}
}

/*===========================================================================*
 *				int slabsane				     *
 *===========================================================================*/
int slabsane_f(const char *file, int line, void *mem, int bytes)
{
	struct slabheader *s;
	struct slabdata *f;
	int i;

	bytes = roundup(bytes, OBJALIGN);

	return (objstats(mem, bytes, &s, &f, &i) == OK);
}
#endif

#if SANITYCHECKS
static int nojunkwarning = 0;
#endif

/*===========================================================================*
 *				void *slaballoc				     *
 *===========================================================================*/
void *slaballoc(int bytes)
{
	int i;
	int count = 0;
	struct slabheader *s;
	struct slabdata *newslab;
	char *ret;

	bytes = roundup(bytes, OBJALIGN);

	SLABSANITYCHECK(SCL_FUNCTIONS);

	/* Retrieve entry in slabs[]. */
	GETSLAB(bytes, s);
	assert(s);

	if(!(newslab = s->list_head)) {
		/* Make sure there is something on the freelist. */
		newslab = newslabdata();
		if(!newslab) return NULL;
		ADDHEAD(newslab, s);
		assert(newslab->sdh.nused == 0);
	} else	assert(newslab->sdh.nused > 0);
	assert(newslab->sdh.nused < ITEMSPERPAGE(bytes));

	SLABSANITYCHECK(SCL_DETAIL);

#if SANITYCHECKS
	assert(newslab->sdh.magic1 == MAGIC1);
	assert(newslab->sdh.magic2 == MAGIC2);
#endif

	for(i = newslab->sdh.freeguess;
		count < ITEMSPERPAGE(bytes); count++, i++) {
		i = i % ITEMSPERPAGE(bytes);

		if(!GETBIT(newslab, i)) 
			break;
	}

	SLABSANITYCHECK(SCL_FUNCTIONS);

	assert(count < ITEMSPERPAGE(bytes));
	assert(i >= 0 && i < ITEMSPERPAGE(bytes));

	SETBIT(newslab, i);
	if(newslab->sdh.nused == ITEMSPERPAGE(bytes)) {
		UNLINKNODE(newslab);
		s->list_head = newslab->sdh.next;
	}

	ret = ((char *) newslab) + i*bytes;

#if SANITYCHECKS
#if MEMPROTECT
	nojunkwarning++;
	slabunlock(ret, bytes);
	nojunkwarning--;
	assert(!nojunkwarning);
#endif
	*(u32_t *) ret = NOJUNK;
#if MEMPROTECT
	slablock(ret, bytes);
#endif
#endif

	SLABDATAUSE(newslab, newslab->sdh.freeguess = i+1;);

#if SANITYCHECKS
	if(bytes >= SLABSIZES+MINSIZE) {
		printf("slaballoc: odd, bytes %d?\n", bytes);
	}

	if(!slabsane_f(__FILE__, __LINE__, ret, bytes))
		panic("slaballoc: slabsane failed");
#endif

	assert(!((vir_bytes) ret % OBJALIGN));

	return ret;
}

/*===========================================================================*
 *				int objstats				     *
 *===========================================================================*/
static inline int objstats(void *mem, int bytes,
	struct slabheader **sp, struct slabdata **fp, int *ip)
{
#if SANITYCHECKS
#define OBJSTATSCHECK(cond) \
	if(!(cond)) { \
		printf("VM: objstats: %s failed for ptr %p, %d bytes\n", \
			#cond, mem, bytes); \
		return EINVAL; \
	}
#else
#define OBJSTATSCHECK(cond)
#endif

	struct slabheader *s;
	struct slabdata *f;
	int i;

	assert(!(bytes % OBJALIGN));

	OBJSTATSCHECK((char *) mem >= (char *) VM_PAGE_SIZE);

#if SANITYCHECKS
	if(*(u32_t *) mem == JUNK && !nojunkwarning) {
		util_stacktrace();
		printf("VM: WARNING: JUNK seen in slab object, likely freed\n");
	}
#endif
	/* Retrieve entry in slabs[]. */
	GETSLAB(bytes, s);

	/* Round address down to VM_PAGE_SIZE boundary to get header. */
	f = (struct slabdata *) ((char *) mem - (vir_bytes) mem % VM_PAGE_SIZE);

#if SANITYCHECKS
	OBJSTATSCHECK(f->sdh.magic1 == MAGIC1);
	OBJSTATSCHECK(f->sdh.magic2 == MAGIC2);
#endif

	/* Make sure it's in range. */
	OBJSTATSCHECK((char *) mem >= (char *) f->data);
	OBJSTATSCHECK((char *) mem < (char *) f->data + sizeof(f->data));

	/* Get position. */
	i = (char *) mem - (char *) f->data;
	OBJSTATSCHECK(!(i % bytes));
	i = i / bytes;

	/* Make sure it is marked as allocated. */
	OBJSTATSCHECK(GETBIT(f, i));

	/* return values */
	*ip = i;
	*fp = f;
	*sp = s;

	return OK;
}

/*===========================================================================*
 *				void *slabfree				     *
 *===========================================================================*/
void slabfree(void *mem, int bytes)
{
	int i;
	struct slabheader *s;
	struct slabdata *f;

	bytes = roundup(bytes, OBJALIGN);

	SLABSANITYCHECK(SCL_FUNCTIONS);

	if(objstats(mem, bytes, &s, &f, &i) != OK) {
		panic("slabfree objstats failed");
	}

#if SANITYCHECKS
	if(*(u32_t *) mem == JUNK) {
		printf("VM: WARNING: likely double free, JUNK seen\n");
	}
#endif

#if SANITYCHECKS
#if MEMPROTECT
	slabunlock(mem, bytes);
#endif
#if JUNKFREE
	memset(mem, 0xa6, bytes);
#endif
	*(u32_t *) mem = JUNK;
	nojunkwarning++;
#if MEMPROTECT
	slablock(mem, bytes);
#endif
	nojunkwarning--;
	assert(!nojunkwarning);
#endif

	/* Free this data. */
	CLEARBIT(f, i);

	/* Check if this slab changes lists. */
	if(f->sdh.nused == 0) {
		UNLINKNODE(f);
		if(f == s->list_head) s->list_head = f->sdh.next;
		vm_freepages((vir_bytes) f, 1);
		SLABSANITYCHECK(SCL_DETAIL);
	} else if(f->sdh.nused == ITEMSPERPAGE(bytes)-1) {
		ADDHEAD(f, s);
	}

	SLABSANITYCHECK(SCL_FUNCTIONS);

	return;
}

#if MEMPROTECT
/*===========================================================================*
 *				void *slablock				     *
 *===========================================================================*/
void slablock(void *mem, int bytes)
{
	int i;
	struct slabheader *s;
	struct slabdata *f;

	bytes = roundup(bytes, OBJALIGN);

	if(objstats(mem, bytes, &s, &f, &i) != OK)
		panic("slablock objstats failed");

	SLABDATAUNWRITABLE(f);

	return;
}

/*===========================================================================*
 *				void *slabunlock			     *
 *===========================================================================*/
void slabunlock(void *mem, int bytes)
{
	int i;
	struct slabheader *s;
	struct slabdata *f;

	bytes = roundup(bytes, OBJALIGN);

	if(objstats(mem, bytes, &s, &f, &i) != OK)
		panic("slabunlock objstats failed");

	SLABDATAWRITABLE(f, i);

	return;
}
#endif

#if SANITYCHECKS
/*===========================================================================*
 *				void slabstats				     *
 *===========================================================================*/
void slabstats(void)
{
	int s, totalbytes = 0;
	static int n;
	n++;
	if(n%1000) return;
	for(s = 0; s < SLABSIZES; s++) {
		int b, t;
		b = s + MINSIZE;
		t = checklist(__FILE__, __LINE__, &slabs[s], b);

		if(t > 0) {
			int bytes = t * b;
			printf("VMSTATS: %2d slabs: %d (%dkB)\n", b, t, bytes/1024);
			totalbytes += bytes;
		}
	}

	if(pages > 0) {
		printf("VMSTATS: %dK net used in slab objects in %d pages (%dkB): %d%% utilization\n",
			totalbytes/1024, pages, pages*VM_PAGE_SIZE/1024,
				100 * totalbytes / (pages*VM_PAGE_SIZE));
	}
}
#endif
