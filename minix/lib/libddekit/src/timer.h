#ifndef DDEKIT_TIMER_MINIX_H
#define DDEKIT_TIMER_MINIX_H 1 


extern unsigned long long jiffies;  
extern unsigned long HZ;  

void _ddekit_timer_interrupt(void);
extern int _ddekit_timer_pending;
void _ddekit_timer_update(void);
#endif
