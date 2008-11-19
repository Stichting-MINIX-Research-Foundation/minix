
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

#include <errno.h>
#include <string.h>
#include <env.h>

#include <memory.h>

#include "glo.h"
#include "proto.h"
#include "util.h"
#include "sanitycheck.h"

#define SLABSIZES 60

#define ITEMSPERPAGE(s, bytes) (DATABYTES / (bytes))

#define ELBITS		(sizeof(element_t)*8)
#define BITPAT(b)	(1UL << ((b) %  ELBITS))
#define BITEL(f, b)	(f)->sdh.usebits[(b)/ELBITS]


#define OFF(f, b) vm_assert(!GETBIT(f, b))
#define ON(f, b)  vm_assert(GETBIT(f, b))

#define GETBIT(f, b)	  (BITEL(f,b) &   BITPAT(b))
#define SETBIT(f, b)   {OFF(f,b); (BITEL(f,b)|= BITPAT(b)); (f)->sdh.nused++; }
#define CLEARBIT(f, b) {ON(f, b); (BITEL(f,b)&=~BITPAT(b)); (f)->sdh.nused--; (f)->sdh.freeguess = (b); }

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
#if SANITYCHECKS
#define SLABSANITYCHECK(l) if((l) <= vm_sanitychecklevel) { \
	slab_sanitycheck(__FILE__, __LINE__); }
#else
#define SLABSANITYCHECK(l)
#endif

struct sdh {
	u8_t list;
	u16_t nused;	/* Number of data items used in this slab. */
#if SANITYCHECKS
	u32_t magic;
#endif
	int freeguess;
	struct slabdata *next, *prev;
	elements_t usebits;
	phys_bytes phys;
};

#define DATABYTES	(VM_PAGE_SIZE-sizeof(struct sdh))

#define MAGIC 0x1f5b842f
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

#define GETSLAB(b, s) {			\
	int i;				\
	vm_assert((b) >= MINSIZE);	\
	i = (b) - MINSIZE;		\
	vm_assert((i) < SLABSIZES);	\
	vm_assert((i) >= 0);		\
	s = &slabs[i];			\
}

#define LH(sl, l) (sl)->list_head[l]

#define MOVEHEAD(sl, l1, l2) {		\
	struct slabdata *t;		\
	vm_assert(LH(sl,l1));		\
	REMOVEHEAD(sl, l1, t);		\
	ADDHEAD(t, sl, l2);		\
}

#define REMOVEHEAD(sl, list, to) {	\
	(to) = LH(sl, list);		\
	vm_assert(to);			\
	LH(sl, list) = (to)->sdh.next;	\
	if(LH(sl, list)) LH(sl, list) = NULL; \
	vm_assert((to)->sdh.magic == MAGIC);\
	vm_assert(!(to)->sdh.prev);		\
}

#define ADDHEAD(nw, sl, l) {			\
	vm_assert((nw)->sdh.magic == MAGIC);	\
	(nw)->sdh.next = LH(sl, l);			\
	(nw)->sdh.prev = NULL;			\
	(nw)->sdh.list = l;				\
	LH(sl, l) = (nw);			\
	if((nw)->sdh.next) (nw)->sdh.next->sdh.prev = (nw);	\
}

#define UNLINKNODE(n)	{				\
	if((f)->sdh.prev) (f)->sdh.prev->sdh.next = (f)->sdh.next;	\
	if((f)->sdh.next) (f)->sdh.next->sdh.prev = (f)->sdh.prev;	\
	}

struct slabdata *newslabdata(int list)
{
	struct slabdata *n;
	phys_bytes p;

	vm_assert(sizeof(*n) == VM_PAGE_SIZE);

	if(!(n = vm_allocpages(&p, 1, VMP_SLAB)))
		return NULL;
	memset(n->sdh.usebits, 0, sizeof(n->sdh.usebits));
	pages++;

	n->sdh.phys = p;
#if SANITYCHECKS
	n->sdh.magic = MAGIC;
#endif
	n->sdh.nused = 0;
	n->sdh.freeguess = 0;
	n->sdh.list = list;

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
		MYASSERT(n->sdh.list == l);
		MYASSERT(n->sdh.magic == MAGIC);
		if(n->sdh.prev)
			MYASSERT(n->sdh.prev->sdh.next == n);
		else
			MYASSERT(s->list_head[l] == n);
		if(n->sdh.next) MYASSERT(n->sdh.next->sdh.prev == n);
		for(i = 0; i < USEELEMENTS*8; i++)
			if(i >= ITEMSPERPAGE(s, bytes))
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

#endif

/*===========================================================================*
 *				void *slaballoc				     *
 *===========================================================================*/
PUBLIC void *slaballoc(int bytes)
{
	int i, n = 0;
	struct slabheader *s;
	struct slabdata *firstused;

	SLABSANITYCHECK(SCL_FUNCTIONS);

	/* Retrieve entry in slabs[]. */
	GETSLAB(bytes, s);
	vm_assert(s);

	/* To make the common case more common, make space in the 'used'
	 * queue first.
	 */
	if(!LH(s, LIST_USED)) {
		/* Make sure there is something on the freelist. */
	SLABSANITYCHECK(SCL_DETAIL);
		if(!LH(s, LIST_FREE)) {
			struct slabdata *n = newslabdata(LIST_FREE);
	SLABSANITYCHECK(SCL_DETAIL);
			if(!n) return NULL;
			ADDHEAD(n, s, LIST_FREE);
	SLABSANITYCHECK(SCL_DETAIL);
		}


	SLABSANITYCHECK(SCL_DETAIL);
		MOVEHEAD(s, LIST_FREE, LIST_USED);
	SLABSANITYCHECK(SCL_DETAIL);

	}
	SLABSANITYCHECK(SCL_DETAIL);

	vm_assert(s);
	firstused = LH(s, LIST_USED);
	vm_assert(firstused);
	vm_assert(firstused->sdh.magic == MAGIC);

	for(i = firstused->sdh.freeguess; n < ITEMSPERPAGE(s, bytes); n++, i++) {
	SLABSANITYCHECK(SCL_DETAIL);
		i = i % ITEMSPERPAGE(s, bytes);

		if(!GETBIT(firstused, i)) {
			struct slabdata *f;
			char *ret;
			SETBIT(firstused, i);
	SLABSANITYCHECK(SCL_DETAIL);
			if(firstused->sdh.nused == ITEMSPERPAGE(s, bytes)) {
	SLABSANITYCHECK(SCL_DETAIL);
				MOVEHEAD(s, LIST_USED, LIST_FULL);
	SLABSANITYCHECK(SCL_DETAIL);
			}
	SLABSANITYCHECK(SCL_DETAIL);
			ret = ((char *) firstused->data) + i*bytes;

#if SANITYCHECKS
			f = (struct slabdata *) ((char *) ret - (vir_bytes) ret % VM_PAGE_SIZE);
			if(f->sdh.magic != MAGIC) {
				printf("slaballoc bogus pointer 0x%lx, "
					"rounded 0x%lx, bad magic 0x%lx\n",
					ret, f, f->sdh.magic);
				vm_panic("slaballoc check failed", NO_NUM);
			}
			*(u32_t *) ret = NOJUNK;
#endif
			SLABSANITYCHECK(SCL_FUNCTIONS);
			firstused->sdh.freeguess = i+1;
			return ret;
		}

	SLABSANITYCHECK(SCL_DETAIL);

	}
	SLABSANITYCHECK(SCL_FUNCTIONS);

	vm_panic("slaballoc: no space in 'used' slabdata", NO_NUM);

	/* Not reached. */
	return NULL;
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

#if SANITYCHECKS
	if(*(u32_t *) mem == JUNK) {
		printf("VM: WARNING: likely double free, JUNK seen\n");
	}
#endif

	/* Retrieve entry in slabs[]. */
	GETSLAB(bytes, s);

	/* Round address down to VM_PAGE_SIZE boundary to get header. */
	f = (struct slabdata *) ((char *) mem - (vir_bytes) mem % VM_PAGE_SIZE);

	vm_assert(f->sdh.magic == MAGIC);
	vm_assert(f->sdh.list == LIST_USED || f->sdh.list == LIST_FULL);

	/* Make sure it's in range. */
	vm_assert((char *) mem >= (char *) f->data);
	vm_assert((char *) mem < (char *) f->data + sizeof(f->data));

	/* Get position. */
	i = (char *) mem - (char *) f->data;
	vm_assert(!(i % bytes));
	i = i / bytes;

	/* Make sure it _was_ allocated. */
	vm_assert(GETBIT(f, i));

	/* Free this data. */
	CLEARBIT(f, i);

#if SANITYCHECKS
	*(u32_t *) mem = JUNK;
#endif

	/* Check if this slab changes lists. */
	if(f->sdh.nused == 0) {
		/* Now become FREE; must've been USED */
		vm_assert(f->sdh.list == LIST_USED);
		UNLINKNODE(f);
		if(f == LH(s, LIST_USED))
			LH(s, LIST_USED) = f->sdh.next;
		ADDHEAD(f, s, LIST_FREE);
		SLABSANITYCHECK(SCL_DETAIL);
	} else if(f->sdh.nused == ITEMSPERPAGE(s, bytes)-1) {
		/* Now become USED; must've been FULL */
		vm_assert(f->sdh.list == LIST_FULL);
		UNLINKNODE(f);
		if(f == LH(s, LIST_FULL))
			LH(s, LIST_FULL) = f->sdh.next;
		ADDHEAD(f, s, LIST_USED);
		SLABSANITYCHECK(SCL_DETAIL);
	} else {
		/* Stay USED */
		vm_assert(f->sdh.list == LIST_USED);
	}

	SLABSANITYCHECK(SCL_FUNCTIONS);

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
