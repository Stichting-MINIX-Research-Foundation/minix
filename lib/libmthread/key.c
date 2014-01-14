#include <minix/mthread.h>
#include <string.h>
#include "global.h"
#include "proto.h"

static int keys_used = 0;
static struct {
  int used;
  int nvalues;
  void *mvalue;
  void **value;
  void (*destr)(void *);
} keys[MTHREAD_KEYS_MAX];

/*===========================================================================*
 *				mthread_init_keys			     *
 *===========================================================================*/
void mthread_init_keys(void)
{
/* Initialize the table of key entries.
 */
  mthread_key_t k;

  for (k = 0; k < MTHREAD_KEYS_MAX; k++)
	keys[k].used = FALSE;
}

/*===========================================================================*
 *				mthread_key_create			     *
 *===========================================================================*/
int mthread_key_create(mthread_key_t *key, void (*destructor)(void *))
{
/* Allocate a key.
 */
  mthread_key_t k;

  keys_used = 1;

  /* We do not yet allocate storage space for the values here, because we can
   * not estimate how many threads will be created in the common case that the
   * application creates keys before spawning threads.
   */
  for (k = 0; k < MTHREAD_KEYS_MAX; k++) {
	if (!keys[k].used) {
		keys[k].used = TRUE;
		keys[k].nvalues = 0;
		keys[k].mvalue = NULL;
		keys[k].value = NULL;
		keys[k].destr = destructor;
		*key = k;

		return(0);
	}
  }

  return(EAGAIN);
}

/*===========================================================================*
 *				mthread_key_delete			     *
 *===========================================================================*/
int mthread_key_delete(mthread_key_t key)
{
/* Free up a key, as well as any associated storage space.
 */

  if (key < 0 || key >= MTHREAD_KEYS_MAX || !keys[key].used)
	return(EINVAL);

  free(keys[key].value);

  keys[key].used = FALSE;

  return(0);
}

/*===========================================================================*
 *				mthread_getspecific			     *
 *===========================================================================*/
void *mthread_getspecific(mthread_key_t key)
{
/* Get this thread's local value for the given key. The default is NULL.
 */

  if (key < 0 || key >= MTHREAD_KEYS_MAX || !keys[key].used)
	return(NULL);

  if (current_thread == MAIN_THREAD)
	return keys[key].mvalue;

  if (current_thread < keys[key].nvalues)
	return(keys[key].value[current_thread]);

  return(NULL);
}

/*===========================================================================*
 *				mthread_setspecific			     *
 *===========================================================================*/
int mthread_setspecific(mthread_key_t key, void *value)
{
/* Set this thread's value for the given key. Allocate more resources as
 * necessary.
 */
  void **p;

  if (key < 0 || key >= MTHREAD_KEYS_MAX || !keys[key].used)
	return(EINVAL);

  if (current_thread == MAIN_THREAD) {
	keys[key].mvalue = value;

	return(0);
  }

  if (current_thread >= keys[key].nvalues) {
	if (current_thread >= no_threads)
		mthread_panic("Library state corrupt");

	if ((p = (void **) realloc(keys[key].value,
			sizeof(void*) * no_threads)) == NULL)
		return(ENOMEM);

	memset(&p[keys[key].nvalues], 0,
		sizeof(void*) * (no_threads - keys[key].nvalues));

	keys[key].nvalues = no_threads;
	keys[key].value = p;
  }

  keys[key].value[current_thread] = value;

  return(0);
}

/*===========================================================================*
 *				mthread_cleanup_values			     *
 *===========================================================================*/
void mthread_cleanup_values(void)
{
/* Clean up all the values associated with an exiting thread, calling keys'
 * destruction procedures as appropriate.
 */
  mthread_key_t k;
  void *value;
  int found;

  if (!keys_used) return;	/* Only clean up if we used any keys at all */

  /* Any of the destructors may set a new value on any key, so we may have to
   * loop over the table of keys multiple times. This implementation has no
   * protection against infinite loops in this case.
   */
  do {
	found = FALSE;

	for (k = 0; k < MTHREAD_KEYS_MAX; k++) {
		if (!keys[k].used) continue;
		if (keys[k].destr == NULL) continue;

		if (current_thread == MAIN_THREAD) {
			value = keys[k].mvalue;

			keys[k].mvalue = NULL;
		} else {
			if (current_thread >= keys[k].nvalues) continue;

			value = keys[k].value[current_thread];

			keys[k].value[current_thread] = NULL;
		}

		if (value != NULL) {
			/* Note: calling mthread_exit() from a destructor
			 * causes undefined behavior.
			 */
			keys[k].destr(value);

			found = TRUE;
		}
	}
  } while (found);
}

/* pthread compatibility layer. */
__weak_alias(pthread_key_create, mthread_key_create)
__weak_alias(pthread_key_delete, mthread_key_delete)
__weak_alias(pthread_getspecific, mthread_getspecific)
__weak_alias(pthread_setspecific, mthread_setspecific)

