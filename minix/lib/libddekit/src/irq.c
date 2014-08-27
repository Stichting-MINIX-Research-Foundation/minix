#include "common.h"
#include <ddekit/interrupt.h>
#include <ddekit/memory.h>
#include <ddekit/panic.h>
#include <ddekit/semaphore.h>
#include <ddekit/thread.h>

#ifdef DDEBUG_LEVEL_IRQ
#undef DDEBUG
#define DDEBUG DDEBUG_LEVEL_IRQ
#endif

#include "debug.h"

struct ddekit_irq_s {
	int irq;
	int notify_id;
	int irq_hook;
	int shared;
	void(*thread_init)(void *);
	void(*handler)(void *);
	void *priv;
	int enabled;
	ddekit_thread_t *th;
	ddekit_sem_t *sem;
	struct ddekit_irq_s *next;
};

static struct ddekit_irq_s *irqs = 0;
static ddekit_lock_t lock;
static int next_notify_id = 0; /* TODO: This is only incremented and after
				* enough interrupt attachments/detachments
				* we can run out of legal IDs (this is however
				* very atypical use case) */

/******************************************************************************
 *    Local helpers                                                           *
 *****************************************************************************/


static void ddekit_irq_lock(void);
static void ddekit_irq_unlock(void);
static struct ddekit_irq_s* find_by_irq(int irq);
static struct ddekit_irq_s* find_by_irq_id(int irq_id);
static void ddekit_irq_remove(struct ddekit_irq_s *irq_s);
static void ddekit_irq_thread(void *data);

/******************************************************************************
 *       ddekit_irq_lock                                                      *
 *****************************************************************************/
static void  ddekit_irq_lock(void)
{  
	ddekit_lock_lock(&lock);
}

/******************************************************************************
 *       ddekit_irq_unlock                                                    *
 *****************************************************************************/
static void ddekit_irq_unlock(void)
{  
	ddekit_lock_unlock(&lock);
}

/******************************************************************************
 *       find_by_irq                                                          *
 *****************************************************************************/
static struct ddekit_irq_s * find_by_irq(int irq)
{ 
	struct ddekit_irq_s * irq_s;
	ddekit_irq_lock();
	if (!irqs) {
		return 0;
	}
	
	irq_s = irqs;

	while(irq_s) {
		if(irq_s->irq==irq)
			break;
		irq_s = irq_s->next;
	}

	ddekit_irq_unlock();
	return irq_s;
}

/******************************************************************************
 *       find_by_irq_id                                                       *
 *****************************************************************************/
static struct ddekit_irq_s * find_by_irq_id(int irq_id)
{
	struct ddekit_irq_s * irq_s;
	ddekit_irq_lock();
	if (!irqs) {
		return 0;
	}

	irq_s = irqs;

	while(irq_s) {
		if(irq_s->notify_id==irq_id)
			break;
		irq_s = irq_s->next;
	}

	ddekit_irq_unlock();
	return irq_s;
}

/******************************************************************************
 *       ddekit_irq_remove                                                    *
 *****************************************************************************/
static void ddekit_irq_remove(struct ddekit_irq_s *irq_s)
{ 
	struct ddekit_irq_s *i;

	ddekit_irq_lock();

	if(!irqs) {
		ddekit_irq_unlock();
		return;	
	}
	
	if(irqs==irq_s) {
		irqs=irq_s->next;
		ddekit_irq_unlock();
		return;
	}

	i = irqs;
	
	while(i) {
		if (i->next == irq_s)  { 
			i->next = irq_s->next;
			ddekit_irq_unlock();
			return;
		}
		i = i->next; 
	}	

	ddekit_irq_unlock();
}

/******************************************************************************
 *       ddekit_irq_thread                                                    *
 *****************************************************************************/
static void ddekit_irq_thread(void *data) 
{
	/* For each IRQ line an own thread is started */
	
	struct ddekit_irq_s *irq_s = (struct ddekit_irq_s *) data;
	
	/* call IRQ thread init function */
	irq_s->thread_init(irq_s->priv);

	while(1) {
		
		/* Wait for IRQs */
		DDEBUG_MSG_VERBOSE("wating for IRQ %d to occur", irq_s->irq);	
		ddekit_sem_down(irq_s->sem);
		DDEBUG_MSG_VERBOSE("executing handler for IRQ %d", irq_s->irq);	
		irq_s->handler(irq_s->priv);
		
	}
}


/******************************************************************************
 *   DDEKIT public API (include/dde/ddekit)                                   *
 *****************************************************************************/

/******************************************************************************
 *       ddekit_interrupt_attach                                              *
 *****************************************************************************/
ddekit_thread_t *ddekit_interrupt_attach(int irq,
                                         int shared,
                                         void(*thread_init)(void *),
                                         void(*handler)(void *),
                                         void *priv)
{
	struct ddekit_irq_s *irq_s;
	int err_code;
	char name[32];
	irq_s = (struct ddekit_irq_s *)
	                ddekit_simple_malloc(sizeof(struct ddekit_irq_s));

	irq_s->sem         = ddekit_sem_init(0);
	irq_s->irq         = irq;		/* represents real IRQ number */
	ddekit_irq_lock();
	irq_s->notify_id   = next_notify_id;	/* represents kernel's IRQ ID */
	irq_s->irq_hook    = next_notify_id;	/* after given ID is passed to
						 * kernel, this field will be
						 * set to real irq_hook */
	next_notify_id++;			/* next time, assign different
						 * ID so we can distinguish
						 * interrupts */
	ddekit_irq_unlock();
	irq_s->shared      = shared;
	irq_s->thread_init = thread_init;
	irq_s->handler     = handler;
	irq_s->priv        = priv;
	irq_s->next        = 0;
	irq_s->enabled     = 1;

	/* create interrupt thread */
	snprintf(name, 32, "ddekit_irq_%d",irq);
	irq_s->th = ddekit_thread_create(ddekit_irq_thread, irq_s, name);

	/* try attaching to IRQ */
	/* do not automatically re-enable interupts */
	if (0 != (err_code = sys_irqsetpolicy(irq, 0, &irq_s->irq_hook)))
		ddekit_panic("Failed to attach interrupt (ERROR %d)", err_code);

	/* add to IRQ list */
	ddekit_irq_lock();
	irq_s->next = irqs;
	irqs=irq_s;
	ddekit_irq_unlock();

	DDEBUG_MSG_INFO("Attached to irq %d (hook: %d)", irq, irq_s->irq_hook);

	return irq_s->th;	
}

/******************************************************************************
 *       ddekit_interrupt_detach                                              *
 *****************************************************************************/
void ddekit_interrupt_detach(int irq)
{
	struct ddekit_irq_s *irq_s;
	int err_code;

	irq_s = find_by_irq(irq);

	if (0 != (err_code = sys_irqrmpolicy(&irq_s->irq_hook)))
		ddekit_panic("Failed to detach interrupt (ERROR %d)", err_code);

	ddekit_thread_terminate(irq_s->th);
	ddekit_irq_remove(irq_s);
	ddekit_simple_free(irq_s);
	DDEBUG_MSG_VERBOSE(" IRQ %d", irq);
}

/******************************************************************************
 *       ddekit_interrupt_disable                                             *
 *****************************************************************************/
void ddekit_interrupt_disable(int irq)
{
	struct ddekit_irq_s *irq_s;
	irq_s = find_by_irq(irq);
	irq_s->enabled=0;
	//sys_irqdisable(&irq_s->irq_hook);
	DDEBUG_MSG_VERBOSE(" IRQ %d", irq);
}

/******************************************************************************
 *       ddekit_interrupt_enable                                              *
 *****************************************************************************/
void ddekit_interrupt_enable(int irq)
{
	struct ddekit_irq_s *irq_s;
	irq_s = find_by_irq(irq);
	irq_s->enabled=1;
	//sys_irqenable(&irq_s->irq_hook);
	DDEBUG_MSG_VERBOSE(" IRQ %d", irq);
}

/******************************************************************************
 *       ddekit_init_irqs                                                     *
 *****************************************************************************/
void ddekit_init_irqs()
{  
	ddekit_lock_init(&lock);
} 

/******************************************************************************
 *   DDEKIT internals (src/irq.h)                                             *
 *****************************************************************************/

/******************************************************************************
 *       _ddekit_interrupt_trigger                                            *
 *****************************************************************************/
void _ddekit_interrupt_trigger(int irq_id)
{
	struct ddekit_irq_s *irq_s;
	int err_code;

	irq_s = find_by_irq_id(irq_id);

	if (irq_s)	{
		DDEBUG_MSG_VERBOSE("Triggering IRQ %d", irq_s->irq);
		ddekit_sem_up(irq_s->sem);
		if (0 != (err_code = sys_irqenable(&irq_s->irq_hook)))
			ddekit_panic("Failed to enable interrupt "
					"(ERROR %d)", err_code);
	} else {
		DDEBUG_MSG_WARN("no handler for IRQ %d", irq_s->irq);
	}
}


