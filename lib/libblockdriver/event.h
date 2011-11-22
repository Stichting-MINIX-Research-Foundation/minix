#ifndef _BLOCKDRIVER_EVENT_H
#define _BLOCKDRIVER_EVENT_H

typedef struct {
  mthread_mutex_t mutex;
  mthread_cond_t cond;
} event_t;

_PROTOTYPE( void event_init, (event_t *event) );
_PROTOTYPE( void event_destroy, (event_t *event) );
_PROTOTYPE( void event_wait, (event_t *event) );
_PROTOTYPE( void event_fire, (event_t *event) );

#endif /* _BLOCKDRIVER_EVENT_H */
