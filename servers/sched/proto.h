/* Function prototypes. */

struct schedproc;
#include <timers.h>

/* main.c */
_PROTOTYPE( int main, (void)						);
_PROTOTYPE( void setreply, (int proc_nr, int result)			);

/* schedule.c */
_PROTOTYPE( int do_noquantum, (message *m_ptr)				);
_PROTOTYPE( int do_start_scheduling, (message *m_ptr)			);
_PROTOTYPE( int do_stop_scheduling, (message *m_ptr)			);
_PROTOTYPE( int do_nice, (message *m_ptr)				);
/*_PROTOTYPE( void balance_queues, (struct timer *tp)			);*/
_PROTOTYPE( void init_scheduling, (void)				);

/* utility.c */
_PROTOTYPE( int no_sys, (int who_e, int call_nr)			);
_PROTOTYPE( int sched_isokendpt, (int ep, int *proc)			);
_PROTOTYPE( int sched_isemtyendpt, (int ep, int *proc)			);
_PROTOTYPE( int accept_message, (message *m_ptr)			);
