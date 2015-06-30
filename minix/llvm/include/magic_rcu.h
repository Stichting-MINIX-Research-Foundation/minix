#ifndef _MAGIC_RCU_H
#define _MAGIC_RCU_H

void magic_rcu_quiescent_begin();
void magic_rcu_quiescent_end();
void magic_rcu_quiescent_state();
void magic_rcu_quiescent_state_start();
void magic_rcu_quiescent_state_end();

void magic_rcu_init();
void magic_synchronize_rcu();

void magic_rcu_read_lock();
void magic_rcu_read_unlock();

#define magic_rcu_has_atomic_quiescent_state() magic_rcu_has_atomic_qs
extern int magic_rcu_has_atomic_qs;

#endif /* _MAGIC_RCU_H */
