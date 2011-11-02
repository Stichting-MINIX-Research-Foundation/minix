#ifndef _DRIVER_EVENT_H
#define _DRIVER_EVENT_H

typedef struct {
  mthread_mutex_t mutex;
  mthread_cond_t cond;
} event_t;

_PROTOTYPE( void driver_event_init, (event_t *event) );
_PROTOTYPE( void driver_event_destroy, (event_t *event) );
_PROTOTYPE( void driver_event_wait, (event_t *event) );
_PROTOTYPE( void driver_event_fire, (event_t *event) );

#endif /* _DRIVER_EVENT_H */
