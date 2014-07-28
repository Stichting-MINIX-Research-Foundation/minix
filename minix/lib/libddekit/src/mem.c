#include "common.h"

#include <ddekit/lock.h>
#include <ddekit/memory.h> 
#include <ddekit/panic.h>
#include <ddekit/pgtab.h> 
#include <ddekit/inline.h> 
#include <ddekit/types.h> 

#ifdef DDEKIT_DEBUG_MEM 
#undef DDEBUG
#define DDEBUG DDEKIT_DEBUG_MEM
#endif
#include "debug.h"
#include "util.h"

#define SLAB_SIZE (4096*4)

struct ddekit_slab; 

struct ddekit_slab_slab {
	struct ddekit_slab * cache; 
	unsigned long free; 
	void *objects;
	void *mem;
	struct ddekit_slab_slab *next;
	struct ddekit_slab_slab *prev;
};

struct ddekit_slab { 
	ddekit_lock_t lock;
	void * data;    /* user pointer */
	int contiguous; /* is it coniguous mem*/
	unsigned size;  /* the size of he objects */
	unsigned number; /* the number of objects stored per slab */
	struct ddekit_slab_slab full;  
	struct ddekit_slab_slab partial;
	struct ddekit_slab_slab empty;
};

static void ddekit_slab_lock(struct ddekit_slab * sc);
static void ddekit_slab_unlock(struct ddekit_slab * sc);
static struct ddekit_slab_slab * ddekit_slab_find_slab(struct
	ddekit_slab * sc, void * obj);
static void ddekit_slab_slab_insert(struct ddekit_slab_slab *list,
	struct ddekit_slab_slab *s);
static void ddekit_slab_slab_remove(struct ddekit_slab_slab *s);
static void ddekit_slab_grow(struct ddekit_slab * sc);
static void *ddekit_slab_getobj(struct ddekit_slab_slab *s);
static void ddekit_slab_free_slab(struct ddekit_slab_slab * sl, int
	cont);

/******************************************************************************
 *       ddekit_simple_malloc                                                 *
 *****************************************************************************/
void *ddekit_simple_malloc(unsigned size)
{  
	/* Simple memory allocation... malloc and free should be ok... */
	void * r = malloc(size);
	if (!r) {
		ddekit_panic("out of mem?");
	}
	DDEBUG_MSG_VERBOSE("%p", r);
	return r;
}

/******************************************************************************
 *       ddekit_simple_free                                                   *
 *****************************************************************************/
void ddekit_simple_free(void *p)
{ 
	DDEBUG_MSG_VERBOSE("%p", p);
	free(p);
}

/******************************************************************************
 *       ddekit_large_malloc                                                  *
 *****************************************************************************/
void *ddekit_large_malloc(int size)
{  
	ddekit_addr_t phys;	
	/* allocate a piece of coniguous memory */
	void * r = alloc_contig(size, AC_ALIGN4K, &phys);
	if (!r) {
		ddekit_panic("out of mem?");
	}
	ddekit_pgtab_set_region_with_size(r, phys, size, PTE_TYPE_LARGE);
	DDEBUG_MSG_VERBOSE("%p, phys: %p, size: %p.",r, phys, size);
	DDEBUG_MSG_VERBOSE("%p", r);
	return r; 
}

/******************************************************************************
 *       ddekit_large_free                                                    *
 *****************************************************************************/
void ddekit_large_free(void *p)
{  
	unsigned len;
	DDEBUG_MSG_VERBOSE("get size of region %x", p); 
	len= ddekit_pgtab_get_size(p);
	DDEBUG_MSG_VERBOSE("freeing %x, len %d...", p , len);
	ddekit_pgtab_clear_region(p, 0); /* type is not used here... */
	DDEBUG_MSG_VERBOSE("cleared region", p , len);
	free_contig(p, len);
	DDEBUG_MSG_VERBOSE("freed mem", p , len);
	DDEBUG_MSG_VERBOSE("%p", p);
}

/******************************************************************************
 *       ddekit_contig_malloc                                                 *
 *****************************************************************************/
void *ddekit_contig_malloc(unsigned long size, unsigned long low, 
                                  unsigned long high, unsigned long aligment,  
                                  unsigned long boundary)
{ 
	WARN_UNIMPL;
	return 0;
}

/******************************************************************************
 *       ddekit_slab_lock                                                     *
 *****************************************************************************/
static DDEKIT_INLINE void ddekit_slab_lock(struct ddekit_slab * sc) {
	ddekit_lock_lock(&sc->lock);
}

/******************************************************************************
 *       ddekit_slab_unlock                                                   *
 *****************************************************************************/
static DDEKIT_INLINE void ddekit_slab_unlock(struct ddekit_slab * sc) {
	ddekit_lock_unlock(&sc->lock);
}

/******************************************************************************
 *       ddekit_slab_find_slab                                                *
 *****************************************************************************/
static struct ddekit_slab_slab * 
ddekit_slab_find_slab(struct ddekit_slab * sc, void * obj) 
{  

	struct ddekit_slab_slab *s;

	for( s = sc->full.next; s!=&sc->full; s = s->next )
	{
		if (s->mem <= obj && obj < s->mem+(SLAB_SIZE))
		{
				return s;
		}
	} 

	for( s = sc->partial.next; s!=&sc->partial; s = s->next )
	{
		if (s->mem <= obj && obj < s->mem+(SLAB_SIZE))
		{
				return s;
		}
	}

	return 0; 
}

/******************************************************************************
 *       ddekit_slab_slab_insert                                              *
 *****************************************************************************/
static void  ddekit_slab_slab_insert(struct ddekit_slab_slab *list,
                                      struct ddekit_slab_slab *s) 
{  
	s->prev          = list; 
	s->next          = list->next; 
	list->next->prev = s;
	list->next       = s;
} 

/******************************************************************************
 *       ddekit_slab_slab_remove                                              *
 *****************************************************************************/
static void ddekit_slab_slab_remove(struct ddekit_slab_slab *s) 
{  
	s->next->prev     = s->prev;
	s->prev->next     = s->next; 
	s->next = s->prev = 0;
}


/******************************************************************************
 *       ddekit_slab_grow                                                     *
 *****************************************************************************/
static void ddekit_slab_grow(struct ddekit_slab *sc) 
{ 
	/*
	 * NOTE:
	 * As it doesn't seem to make problems ddekit_slabs are disregarding 
	 * alignment.  However this should be revisited, maybe this leads to
	 * performance degregation somewhere.
	 * Further the ddekit_slab doesn't have to be real slab, as the entries are
	 * initialized in the personalized DDEs. (slab is simple the wrong name.)
	 */
	int i; 
	char *p;
	void **p1;
	struct ddekit_slab_slab *s;
	
	/* allocate slab control structure */
	
	s = (struct ddekit_slab_slab *) 
	    ddekit_simple_malloc(sizeof(struct ddekit_slab_slab)); 
	
	s->cache = sc;

	if(sc->contiguous)	
		s->mem = ddekit_large_malloc(SLAB_SIZE);
	else 
		s->mem = ddekit_simple_malloc(SLAB_SIZE);

	/* setup the object list */

	s->free = sc->number; 

	/* put obj into list */
	p1 = s->mem; 
	*p1 = s->mem; 
	s->objects =  p1;

	DDEBUG_MSG_VERBOSE("obj size: %d, memory at: %p , first obj: %p, %p ", 
	                    sc->size, s->mem, s->objects);	
	
	for (i = 0; i < s->free; i++)
	{
		p  = *p1;
		p1 = (void **) (p + sc->size);
		
		if ( i != s->free-1 ) 
		{
			*p1 = p1+1;
			DDEBUG_MSG_VERBOSE("%p, %p -> %p", p, p1, *p1);
		}
		else
		{ 
			*p1 = 0;	
			DDEBUG_MSG_VERBOSE("%p, %p -> %p", p, p1, *p1);
		}
	} 

	/* add new slab to free list */
	ddekit_slab_slab_insert(&sc->empty, s);
} 


/******************************************************************************
 *       ddekit_slab_getobj                                                   *
 *****************************************************************************/
static void *ddekit_slab_getobj(struct ddekit_slab_slab *s)
{ 
	struct ddekit_slab *sc; 
	void *ret = 0; 

	sc = s->cache; 
	ret  = s->objects;
	
	/* get pointer to next object */

	s->objects = *(void **)((char *) ret + sc->size); 
	s->free--;

	DDEBUG_MSG_VERBOSE("old: %p new: %p", ret, s->objects);

	/* if no more objects move to full */

	if (!s->free)
	{
		ddekit_slab_slab_remove(s);
		ddekit_slab_slab_insert(&sc->full,s);
	} 
	
	if (s->free == sc->number-1)
	{ 
		ddekit_slab_slab_remove(s); 
		ddekit_slab_slab_insert(&sc->partial,s); 
	}

	return ret;
}

/******************************************************************************
 *       ddekit_slab_alloc                                                    *
 *****************************************************************************/
void *ddekit_slab_alloc(struct ddekit_slab * sc)
{ 
	struct ddekit_slab_slab *s=0; 

	ddekit_slab_lock(sc);

	DDEBUG_MSG_VERBOSE("from slab %p", sc);

	/* first try from partial */  
	if (sc->partial.next != &sc->partial) { 
		DDEBUG_MSG_VERBOSE("from slab %p partial (next=%p)", sc,sc->partial.next);
		s = sc->partial.next; 
	}

	/* must grow? */
	if (!s && (sc->empty.next == &sc->empty )){
		DDEBUG_MSG_VERBOSE("slab %p has to grow", sc);
		ddekit_slab_grow(sc);
	}

	/* take from free? */
	if (!s) { 
		DDEBUG_MSG_VERBOSE("from slab %p empty", sc);
		s = sc->empty.next;
	}
	
	ddekit_slab_unlock(sc);

	return ddekit_slab_getobj(s);
}

/******************************************************************************
 *       ddekit_slab_free                                                     *
 *****************************************************************************/
void ddekit_slab_free(struct ddekit_slab *sc, void* obj)
{   
	void **p;	

	struct ddekit_slab_slab *s = 0; 

	ddekit_slab_lock(sc);	
	/* first find slab the obj came from */ 

	s = ddekit_slab_find_slab(sc, obj);
	
	p = (void **)((char *) obj + sc->size);
	
	*p= s->objects;
	s->objects=obj;

	DDEBUG_MSG_VERBOSE("old: %p, new: %p",*p,s->objects );

	s->free++; 
	 
	if (s->free == sc->number) {
		ddekit_slab_slab_remove(s); 
		ddekit_slab_slab_insert(&sc->empty, s);
	}
	
	if (s->free == 1) { 
		ddekit_slab_slab_remove(s); 
		ddekit_slab_slab_insert(&sc->partial, s); 
	}

	ddekit_slab_unlock(sc);
}

/******************************************************************************
 *       ddekit_slab_set_data                                                 *
 *****************************************************************************/
void ddekit_slab_set_data(struct ddekit_slab * sc, void *data)
{   
	ddekit_slab_lock(sc);
	sc->data = data;
	ddekit_slab_unlock(sc);
}

/******************************************************************************
 *       ddekit_slab_get_data                                                 *
 *****************************************************************************/
void *ddekit_slab_get_data (struct ddekit_slab *sc)
{  
	void *ret;
	ddekit_slab_lock(sc);
	ret=sc->data;
	ddekit_slab_unlock(sc);
	return ret;
}


/******************************************************************************
 *       ddekit_slab_init                                                     *
 *****************************************************************************/
struct ddekit_slab * ddekit_slab_init(unsigned size, int contiguous)
{

	struct ddekit_slab * sc = 0;

	sc = (struct ddekit_slab *)
	    ddekit_simple_malloc(sizeof(struct ddekit_slab));

	sc->data       = 0;
	sc->contiguous = contiguous;
	sc->size       = size;
	sc->number     = SLAB_SIZE/(size+sizeof(void*));
	
	if (sc->number == 0) {
		ddekit_panic("objects too big!");
	}

	sc->empty.next   = sc->empty.prev   = &sc->empty;
	sc->partial.next = sc->partial.prev = &sc->partial;  
	sc->full.next    = sc->full.prev    = &sc->full; 

	ddekit_lock_init(&sc->lock);

	DDEBUG_MSG_VERBOSE("initialzed slab cache %p: size %x, number %d ",
	    sc, sc->size, sc->number);

	DDEBUG_MSG_VERBOSE("partial %p next %p", &sc->partial, sc->partial.next); 
	return sc ;
 
}


/******************************************************************************
 *       ddekit_slab_free_slab                                                *
 *****************************************************************************/
static void ddekit_slab_free_slab(struct ddekit_slab_slab * sl, int cont) 
{

	struct ddekit_slab_slab *s,*t;
	
	if (!sl) {
		ddekit_panic("no slab to free!");
	}

	for ( s = sl->next; s != sl; ) 
	{
		DDEBUG_MSG_VERBOSE("cont: %d, %p, s->mem", cont, s->mem);
		if(cont) 
		{
			ddekit_large_free(s->mem);
		} 
		else 
		{
			ddekit_simple_free(s->mem);
		}
		t = s;
		s = s->next;
		ddekit_simple_free(t);
	}

}

/******************************************************************************
 *       ddekit_slab_destroy                                                  *
 *****************************************************************************/
void ddekit_slab_destroy(struct ddekit_slab *sc) 
{
	DDEBUG_MSG_VERBOSE("%p full", sc);
	ddekit_slab_free_slab(&sc->full,sc->contiguous);
	
	DDEBUG_MSG_VERBOSE("%p empty", sc);
	ddekit_slab_free_slab(&sc->empty,sc->contiguous);
	
	DDEBUG_MSG_VERBOSE("%p partial", sc);
	ddekit_slab_free_slab(&sc->partial,sc->contiguous);
	
	ddekit_lock_deinit(&sc->lock);

	ddekit_simple_free(sc);
}
