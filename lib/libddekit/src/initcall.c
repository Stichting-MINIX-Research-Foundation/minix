#include "common.h" 

#include <ddekit/initcall.h>


#ifdef DDEKIT_DEBUG_INITCALL
#undef DDEBUG
#define DDEBUG DDEKIT_DEBUG_INITCALL
#endif

#include "debug.h"
 
static struct __ddekit_initcall_s head = {0,0,0};

/****************************************************************************/
/*        __ddekit_add_initcall                                             */
/****************************************************************************/
void __attribute__((used)) 
__ddekit_add_initcall(struct __ddekit_initcall_s * ic) {

	/* This function is required for the DDEKIT_INITCALL makro */
	 
	struct __ddekit_initcall_s *i = 0; 
  	
	DDEBUG_MSG_VERBOSE("adding initcall (%p) to %p with prio %d head at %p",
	ic, ic->func, ic->prio, &head);  

	for (i = &head; i; i=i->next) 
	{
		if (!i->next) { 
			i->next = ic;
			return;
		}
		if (i->next->prio > ic->prio) {
			ic->next = i->next;
			i->next = ic;
			return;
		}
	} 
}

/****************************************************************************/
/*        ddekit_do_initcalls                                               */
/****************************************************************************/
void ddekit_do_initcalls()
{ 
	struct __ddekit_initcall_s *i = 0; 

	DDEBUG_MSG_VERBOSE("exectuing initcalls (head at %p, head->next = %p)",
		&head, head.next);
	
	for (i = head.next; i; i=i->next) { 
		DDEBUG_MSG_VERBOSE("executing initcall: %p with prio %d",
		   i->func, i->prio); 
		i->func();
	}
}
