/*
 * @author: Dirk Vogt
 * @date 2010-02-10 
 *
 * This file implements a local pagetable, to prevent IPC on physical 
 * address lookups. For now it's implement in a signle linked list.
 *
 * As soon as the DDE will use a premeptive thread mechanism access to 
 * the page table has to be sznchronized.
 */
#include "common.h"

#include <ddekit/pgtab.h>
#include <ddekit/memory.h>
#include <ddekit/lock.h>

#ifdef DDEBUG_LEVEL_PGTAB
#undef DDEBUG
#define DDEBUG DDEBUG_LEVEL_PGTAB
#endif

#include "util.h"
#include "debug.h" 


static void lock_pgtab(void);
static void unlock_pgtab(void);
static struct dde_pgtab_region * allocate_region(void);
static void free_region(struct dde_pgtab_region *r);
static void add_region(struct dde_pgtab_region *r);
static void rm_region(struct dde_pgtab_region *r);
static struct dde_pgtab_region * find_region_virt(ddekit_addr_t va);
static struct dde_pgtab_region * find_region_phys(ddekit_addr_t pa);

struct dde_pgtab_region {  
	ddekit_addr_t vm_start; 
	ddekit_addr_t phy_start;
	unsigned size; 
	unsigned type; /* do we really have to keep track of the type here? */ 
	struct dde_pgtab_region *next;
	struct dde_pgtab_region *prev;
};

static struct dde_pgtab_region  head = {0,0,0,0,&head,&head}; 
static ddekit_lock_t lock;

/*
 * INTERNAL HELPERS
 */

/****************************************************************************/
/*      lock_pgtab                                                          */
/****************************************************************************/
static void lock_pgtab()
{
	ddekit_lock_lock(&lock);
}


/****************************************************************************/
/*      unlock_pgtab                                                        */
/****************************************************************************/
static void unlock_pgtab()
{
	ddekit_lock_unlock(&lock);
}

/****************************************************************************/
/*      dde_pgtab_region                                                    */
/****************************************************************************/
static struct dde_pgtab_region * allocate_region()
{ 
	struct dde_pgtab_region * res;

	res = (struct dde_pgtab_region *) 
	          ddekit_simple_malloc(sizeof(struct dde_pgtab_region));
	if (!res)
	{ 
		DDEBUG_MSG_ERR("Could not allocate region");
	}
	return res;
}

/****************************************************************************/
/*      free_region                                                         */
/****************************************************************************/
static void free_region(struct dde_pgtab_region *r)
{  
	ddekit_simple_free(r);
}

/****************************************************************************/
/*      add_region                                                          */
/****************************************************************************/
static void add_region (struct dde_pgtab_region *r)
{ 
	r->next    = head.next;
	head.next  = r; 
	r->prev    = &head;

	if (r->next) {
	
		r->next->prev = r;

	}
} 

/****************************************************************************/
/*      rm_region                                                           */
/****************************************************************************/
static void rm_region(struct dde_pgtab_region *r)
{ 
	if (r->next) {
		r->next->prev = r->prev; 
	}
	if (r->prev) {
		r->prev->next = r->next;
	}
	r->next = 0;
	r->prev = 0;
}


/****************************************************************************/
/*      find_region_virt                                                    */
/****************************************************************************/
static struct dde_pgtab_region * find_region_virt(ddekit_addr_t va)
{
	struct dde_pgtab_region * r;
	
	for( r = head.next; r != &head ; r = r->next ) {   
		
		if ( (r->vm_start <= va)  && (va < (r->vm_start + r->size) ) )  { 
			break; 
		}
	}
	
	if (r == &head) {
		DDEBUG_MSG_VERBOSE("No virt->phys mapping found for %x", va);
		r = 0;
	}
	
	return r;
}

/****************************************************************************/
/*      find_region_phys                                                    */
/****************************************************************************/
static struct dde_pgtab_region * find_region_phys(ddekit_addr_t pa)
{  
	struct dde_pgtab_region * r;
	
	for( r = head.next; r != &head ; r = r->next ) {		
		if ( (r->phy_start <= pa)  && (pa < (r->phy_start + r->size) ) )
			break;   
	}
	
	if (r == &head)	{
		r=0;
		DDEBUG_MSG_VERBOSE("No phys->virt mapping found for %x", pa);  
	}
	
	return r;
} 

/****************************************************************************/
/*      ddekit_pgtab_do_fo_each_region                                      */
/****************************************************************************/
void ddekit_pgtab_do_fo_each_region(void (*func) (unsigned, unsigned)) {
	struct dde_pgtab_region * r;
	
	for( r = head.next; r != &head ; r = r->next ) {   
		ddekit_printf("%p",r->vm_start);
		func(r->vm_start, r->size);
	}
}

/*
 * Interface implementation
 */

/****************************************************************************/
/*      ddekit_pgtab_set_region                                             */
/****************************************************************************/
void ddekit_pgtab_set_region(void *virt, ddekit_addr_t phys, int pages, int type)
{ 
	ddekit_pgtab_set_region_with_size(virt, phys, (4096)*pages, type);
}

/****************************************************************************/
/*      ddekit_pgtab_set_region_with_size                                   */
/****************************************************************************/
void ddekit_pgtab_set_region_with_size(void *virt, ddekit_addr_t phys, int size, int type)
{
	struct dde_pgtab_region * r;
	
	lock_pgtab();

	r = allocate_region();	

	r->vm_start  = (ddekit_addr_t) virt;
	r->phy_start = phys;  
	r->size      = size; 
	r->type      = type;

	add_region(r);

	unlock_pgtab();
}


/****************************************************************************/
/*      ddekit_pgtab_clear_region                                           */
/****************************************************************************/
void ddekit_pgtab_clear_region(void *virt, int type) {
	
	struct dde_pgtab_region *r;

	lock_pgtab();

	r = find_region_virt((ddekit_addr_t)virt);

	if (r)
	{ 
		rm_region(r);
		free_region(r);
	}

	unlock_pgtab();
	
}


/****************************************************************************/
/*      ddekit_pgtab_get_physaddr                                           */
/****************************************************************************/
ddekit_addr_t ddekit_pgtab_get_physaddr(const void *virt)
{
	struct dde_pgtab_region *r;
	ddekit_addr_t ret = 0;
	lock_pgtab();
    r = find_region_virt((ddekit_addr_t)virt);
	unlock_pgtab();
	if (r != NULL) { 

		ret =  ((ddekit_addr_t) virt - r->vm_start) + r->phy_start;
		DDEBUG_MSG_VERBOSE("pa: %p -> %p\n", virt, ret);
	}

	return ret; 
}

/****************************************************************************/
/*      ddekit_pgtab_get_virtaddr                                           */
/****************************************************************************/
ddekit_addr_t ddekit_pgtab_get_virtaddr(const ddekit_addr_t physical)
{
	struct dde_pgtab_region *r;
	lock_pgtab();
    r = find_region_phys((ddekit_addr_t)physical);
	unlock_pgtab();
	if (r != NULL)
	{ 
		return ((ddekit_addr_t) physical - r->phy_start) + r->vm_start;
	}  

	return 0;
}

/****************************************************************************/
/*      ddekit_pgtab_get_size                                               */
/****************************************************************************/
int ddekit_pgtab_get_type(const void *virt)
{ 
	/*
	 * needed for dde fbsd
	 */
	struct dde_pgtab_region *r;

	lock_pgtab();
	r = find_region_virt((ddekit_addr_t)virt);
	unlock_pgtab();
	return r->type;
}


/****************************************************************************/
/*      ddekit_pgtab_get_size                                               */
/****************************************************************************/
int ddekit_pgtab_get_size(const void *virt) 
{ 
	/*
	 * needed for fbsd
	 */
	struct dde_pgtab_region *r;

	lock_pgtab();
	r = find_region_virt((ddekit_addr_t)virt);
	unlock_pgtab();
	if(r) 
		return r->size;
	else
		return 0;
}

/****************************************************************************/
/*      ddekit_pgtab_init                                                   */
/****************************************************************************/
void ddekit_pgtab_init() {
	/* called by ddekit_init() */ 
	ddekit_lock_init(&lock);
}

