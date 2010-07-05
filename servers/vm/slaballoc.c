
#define _SYSTEM 1

#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/config.h>
#include <minix/const.h>
#include <minix/ds.h>
#include <minix/endpoint.h>
#include <minix/keymap.h>
#include <minix/minlib.h>
#include <minix/type.h>
#include <minix/ipc.h>
#include <minix/sysutil.h>
#include <minix/syslib.h>
#include <minix/bitmap.h>
#include <minix/debug.h>

#include <assert.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <env.h>

#include <memory.h>

#include "glo.h"
#include "proto.h"
#include "util.h"
#include "sanitycheck.h"

#define SLABSIZES 60

#define ITEMSPERPAGE(bytes) (DATABYTES / (bytes))

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

#define MINSIZE 8
#define MAXSIZE (SLABSIZES-1+MINSIZE)
#define USEELEMENTS (1+(VM_PAGE_SIZE/MINSIZE/8))

PRIVATE int pages = 0;

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
	u8_t list;
	u16_t nused;	/* Number of data items used in this slab. */
	int freeguess;
	struct slabdata *next, *prev;
	elements_t usebits;
	phys_bytes phys;
#if SANITYCHECKS
	int writable;	/* data item number or WRITABLE_* */
	u32_t magic2;
#endif
};

#define DATABYTES	(VM_PAGE_SIZE-sizeof(struct sdh))

#define MAGIC1 0x1f5b842f
#define MAGIC2 0x8bb5a420
#define JUNK  0xdeadbeef
#define NOJUNK 0xc0ffee

#define LIST_UNUSED	0
#define LIST_FREE	1
#define LIST_USED	2
#define LIST_FULL	3
#define LIST_NUMBER	4

PRIVATE struct slabheader {
	struct slabdata {
		struct	sdh sdh;
		u8_t 	data[DATABYTES];
	} *list_head[LIST_NUMBER];
} slabs[SLABSIZES];

FORWARD _PROTOTYPE( int objstats, (void *, int, struct slabheader **, struct slabdata **, int *)); 

#define GETSLAB(b, s) {			\
	int i;				\
	assert((b) >= MINSIZE);	\
	i = (b) - MINSIZE;		\
	assert((i) < SLABSIZES);	\
	assert((i) >= 0);		\
	s = &slabs[i];			\
}

#define LH(sl, l) (sl)->list_head[l]

/* move head of list l1 to list of l2 in slabheader sl. */
#define MOVEHEAD(sl, l1, l2) {		\
	struct slabdata *t;		\
	assert(LH(sl,l1));		\
	REMOVEHEAD(sl, l1, t);		\
	ADDHEAD(t, sl, l2);		\
}

/* remove head of list 'list' in sl, assign it unlinked to 'to'. */
#define REMOVEHEAD(sl, list, to) {	\
	struct slabdata *dat;		\
	dat = (to) = LH(sl, list);	\
	assert(dat);			\
	LH(sl, list) = dat->sdh.next;	\
	UNLINKNODE(dat);		\
}

/* move slabdata nw to slabheader sl under list number l. */
#define ADDHEAD(nw, sl, l) {			\
	SLABDATAUSE(nw,				\
		(nw)->sdh.next = LH(sl, l);	\
		(nw)->sdh.prev = NULL;		\
		(nw)->sdh.list = l;);		\
	LH(sl, l) = (nw);			\
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

struct slabdata *newslabdata(int list)
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
	n->sdh.list = list;

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
PRIVATE int checklist(char *file, int line,
	struct slabheader *s, int l, int bytes)
{
	struct slabdata *n = s->list_head[l];
	int ch = 0;

	while(n) {
		int count = 0, i;
#if SANITYCHECKS
		MYASSERT(n->sdh.magic1 == MAGIC1);
		MYASSERT(n->sdh.magic2 == MAGIC2);
#endif
		MYASSERT(n->sdh.list == l);
		MYASSERT(usedpages_add(n->sdh.phys, VM_PAGE_SIZE) == OK);
		if(n->sdh.prev)
			MYASSERT(n->sdh.prev->sdh.next == n);
		else
			MYASSERT(s->list_head[l] == n);
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
PUBLIC void slab_sanitycheck(char *file, int line)
{
	int s;
	for(s = 0; s < SLABSIZES; s++) {
		int l;
		for(l = 0; l < LIST_NUMBER; l++) {
			checklist(file, line, &slabs[s], l, s + MINSIZE);
		}
	}
}

/*===========================================================================*
 *				int slabsane				     *
 *===========================================================================*/
PUBLIC int slabsane_f(char *file, int line, void *mem, int bytes)
{
	struct slabheader *s;
	struct slabdata *f;
	int i;

	return (objstats(mem, bytes, &s, &f, &i) == OK);
}
#endif

static int nojunkwarning = 0;

/*===========================================================================*
 *				void *slaballoc				     *
 *===========================================================================*/
PUBLIC void *slaballoc(int bytes)
{
	int i;
	int count = 0;
	struct slabheader *s;
	struct slabdata *firstused;

	SLABSANITYCHECK(SCL_FUNCTIONS);

	/* Retrieve entry in slabs[]. */
	GETSLAB(bytes, s);
	assert(s);

	/* To make the common case more common, make space in the 'used'
	 * queue first.
	 */
	if(!LH(s, LIST_USED)) {
		/* Make sure there is something on the freelist. */
	SLABSANITYCHECK(SCL_DETAIL);
		if(!LH(s, LIST_FREE)) {
			struct slabdata *nd = newslabdata(LIST_FREE);
	SLABSANITYCHECK(SCL_DETAIL);
			if(!nd) return NULL;
			ADDHEAD(nd, s, LIST_FREE);
	SLABSANITYCHECK(SCL_DETAIL);
		}


	SLABSANITYCHECK(SCL_DETAIL);
		MOVEHEAD(s, LIST_FREE, LIST_USED);
	SLABSANITYCHECK(SCL_DETAIL);

	}
	SLABSANITYCHECK(SCL_DETAIL);

	assert(s);
	firstused = LH(s, LIST_USED);
	assert(firstused);
#if SANITYCHECKS
	assert(firstused->sdh.magic1 == MAGIC1);
	assert(firstused->sdh.magic2 == MAGIC2);
#endif
	assert(firstused->sdh.nused < ITEMSPERPAGE(bytes));

	for(i = firstused->sdh.freeguess;
		count < ITEMSPERPAGE(bytes); count++, i++) {
	SLABSANITYCHECK(SCL_DETAIL);
		i = i % ITEMSPERPAGE(bytes);

		if(!GETBIT(firstused, i)) {
			struct slabdata *f;
			char *ret;
			SETBIT(firstused, i);
	SLABSANITYCHECK(SCL_DETAIL);
			if(firstused->sdh.nused == ITEMSPERPAGE(bytes)) {
	SLABSANITYCHECK(SCL_DETAIL);
				MOVEHEAD(s, LIST_USED, LIST_FULL);
	SLABSANITYCHECK(SCL_DETAIL);
			}
	SLABSANITYCHECK(SCL_DETAIL);
			ret = ((char *) firstused->data) + i*bytes;

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

			SLABSANITYCHECK(SCL_FUNCTIONS);
			SLABDATAUSE(firstused, firstused->sdh.freeguess = i+1;);

#if SANITYCHECKS
	if(bytes >= SLABSIZES+MINSIZE) {
		printf("slaballoc: odd, bytes %d?\n", bytes);
	}
			if(!slabsane_f(__FILE__, __LINE__, ret, bytes))
				panic("slaballoc: slabsane failed");
#endif

			return ret;
		}

	SLABSANITYCHECK(SCL_DETAIL);

	}
	SLABSANITYCHECK(SCL_FUNCTIONS);

	panic("slaballoc: no space in 'used' slabdata");

	/* Not reached. */
	return NULL;
}

/*===========================================================================*
 *				int objstats				     *
 *===========================================================================*/
PRIVATE int objstats(void *mem, int bytes,
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
	OBJSTATSCHECK(f->sdh.list == LIST_USED || f->sdh.list == LIST_FULL);

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
PUBLIC void slabfree(void *mem, int bytes)
{
	int i;
	struct slabheader *s;
	struct slabdata *f;

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
		/* Now become FREE; must've been USED */
		assert(f->sdh.list == LIST_USED);
		UNLINKNODE(f);
		if(f == LH(s, LIST_USED))
			LH(s, LIST_USED) = f->sdh.next;
		ADDHEAD(f, s, LIST_FREE);
		SLABSANITYCHECK(SCL_DETAIL);
	} else if(f->sdh.nused == ITEMSPERPAGE(bytes)-1) {
		/* Now become USED; must've been FULL */
		assert(f->sdh.list == LIST_FULL);
		UNLINKNODE(f);
		if(f == LH(s, LIST_FULL))
			LH(s, LIST_FULL) = f->sdh.next;
		ADDHEAD(f, s, LIST_USED);
		SLABSANITYCHECK(SCL_DETAIL);
	} else {
		/* Stay USED */
		assert(f->sdh.list == LIST_USED);
	}

	SLABSANITYCHECK(SCL_FUNCTIONS);

	return;
}

/*===========================================================================*
 *				void *slablock				     *
 *===========================================================================*/
PUBLIC void slablock(void *mem, int bytes)
{
	int i;
	struct slabheader *s;
	struct slabdata *f;

	if(objstats(mem, bytes, &s, &f, &i) != OK)
		panic("slablock objstats failed");

	SLABDATAUNWRITABLE(f);

	return;
}

/*===========================================================================*
 *				void *slabunlock			     *
 *===========================================================================*/
PUBLIC void slabunlock(void *mem, int bytes)
{
	int i;
	struct slabheader *s;
	struct slabdata *f;

	if(objstats(mem, bytes, &s, &f, &i) != OK)
		panic("slabunlock objstats failed");

	SLABDATAWRITABLE(f, i);

	return;
}

#if SANITYCHECKS
/*===========================================================================*
 *				void slabstats				     *
 *===========================================================================*/
PUBLIC void slabstats(void)
{
	int s, total = 0, totalbytes = 0;
	static int n;
	n++;
	if(n%1000) return;
	for(s = 0; s < SLABSIZES; s++) {
		int l;
		for(l = 0; l < LIST_NUMBER; l++) {
			int b, t;
			b = s + MINSIZE;
			t = checklist(__FILE__, __LINE__, &slabs[s], l, b);

			if(t > 0) {
				int bytes = t * b;
				printf("VMSTATS: %2d slabs: %d (%dkB)\n", b, t, bytes/1024);
				totalbytes += bytes;
			}
		}
	}

	if(pages > 0) {
		printf("VMSTATS: %dK net used in slab objects in %d pages (%dkB): %d%% utilization\n",
			totalbytes/1024, pages, pages*VM_PAGE_SIZE/1024,
				100 * totalbytes / (pages*VM_PAGE_SIZE));
	}
}
#endif
