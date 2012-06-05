#include "common.h"
#include <ddekit/initcall.h> 
#include <ddekit/minix/msg_queue.h>
#include <ddekit/panic.h> 
#include <ddekit/pci.h> 
#include <ddekit/semaphore.h> 
#include <ddekit/timer.h> 
#include <signal.h>

#include "debug.h" 
#include "timer.h"  /* _ddekit_timer_interrupt()   */
#include "thread.h" /* _ddekit_thread_set_myprio() */
#include "irq.h"


static ddekit_sem_t *exit_sem;

unsigned long long jiffies;

void ddekit_pgtab_init(void);

static  ddekit_thread_t *dispatch_th = 0;


static void dispatcher_thread(void * unused);
static void ddekit_dispatcher_thread_init(void);

/****************************************************************************/
/*      dispatcher_thread                                                   */
/****************************************************************************/
static void dispatcher_thread(void *unused) {

	/*
	 * Gets all messages and dispatches them.
	 *
	 * NOTE: this thread runs only when no other ddekit is 
	 *       ready. So please take care that youre threads
	 *       leave some time for the others!
	 */
	message m;
	int r;
	int i;
	int ipc_status;

	_ddekit_thread_set_myprio(0);

	for( ; ; ) {

		/* Trigger a timer interrupt at each loop iteration */
		_ddekit_timer_update();

		/* Wait for messages */
		if ((r = sef_receive_status(ANY, &m, &ipc_status)) != 0) { 
				ddekit_panic("ddekit", "sef_receive failed", r);
		}


		_ddekit_timer_interrupt(); 

		_ddekit_thread_wakeup_sleeping();

		if (is_notify(m.m_type)) {
			switch (_ENDPOINT_P(m.m_source)) { 
				case HARDWARE:
					for	(i =0 ; i < 32 ; i++)
					{
						if(m.NOTIFY_ARG & (1 << i)) 
						{
							_ddekit_interrupt_trigger(i);
						}
					}
					break;
				case CLOCK:
					_ddekit_timer_pending = 0;
					break;
				default:
					ddekit_thread_schedule();
			}

		} else {

			/*
			 * I don't know how to handle this msg,
			 * but maybe we have a msg queue which can
			 * handle this msg.
			 */

			ddekit_minix_queue_msg(&m, ipc_status);
		}
	}
}

/****************************************************************************/
/*      ddekit_dispatcher_thread_init                                       */
/****************************************************************************/
static void ddekit_dispatcher_thread_init()
{

	dispatch_th = ddekit_thread_create(dispatcher_thread, NULL, "dispatch");

	ddekit_thread_schedule();
}

/****************************************************************************/
/*      ddekit_init                                                         */
/****************************************************************************/
void ddekit_init(void)
{
	sef_startup();

	ddekit_pgtab_init();

	ddekit_init_threads();

	ddekit_init_irqs();

	ddekit_init_timers();

	ddekit_dispatcher_thread_init();

	exit_sem = ddekit_sem_init(0);
}

/****************************************************************************/
/*      dispatcher_shutdown                                                 */
/****************************************************************************/
void ddekit_shutdown() 
{
	ddekit_sem_up(exit_sem);
}

/****************************************************************************/
/*  ddekit_minix_wait_exit                                                  */
/****************************************************************************/
void ddekit_minix_wait_exit(void)
{
	ddekit_sem_down(exit_sem);
}

