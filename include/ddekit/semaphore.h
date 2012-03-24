#ifndef _DDEKIT_SEMAPHORE_H
#define _DDEKIT_SEMAPHORE_H

#include <ddekit/ddekit.h>


/** \defgroup DDEKit_synchronization */

struct ddekit_sem;
typedef struct ddekit_sem ddekit_sem_t;

/** Initialize DDEKit semaphore.
 *
 * \ingroup DDEKit_synchronization
 *
 * \param value  initial semaphore counter
 */
ddekit_sem_t *ddekit_sem_init(int value);

/** Uninitialize semaphore.
 *
 * \ingroup DDEKit_synchronization
 */
void ddekit_sem_deinit(ddekit_sem_t *sem);

/** Semaphore down method. */
void ddekit_sem_down(ddekit_sem_t *sem);

/** Semaphore down method, non-blocking.
 *
 * \ingroup DDEKit_synchronization
 *
 * \return 0   success
 * \return !=0 would block
 */
int ddekit_sem_down_try(ddekit_sem_t *sem);

/** Semaphore down with timeout.
 *
 * \ingroup DDEKit_synchronization
 *
 * \return 0   success
 * \return !=0 would block
 */
int ddekit_sem_down_timed(ddekit_sem_t *sem, int timo);

/** Semaphore up method. 
 *
 * \ingroup DDEKit_synchronization
 */
void ddekit_sem_up(ddekit_sem_t *sem);

#endif
