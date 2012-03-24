#ifndef _DDEKIT_THREAD_H
#define _DDEKIT_THREAD_H

/** \defgroup DDEKit_threads */
#include <ddekit/ddekit.h>
#include <ddekit/lock.h>

struct ddekit_thread;
typedef struct ddekit_thread ddekit_thread_t;

/** Create thread
 *
 * \ingroup DDEKit_threads
 *
 * Create a new thread running the specified thread function with the specified 
 * arguments. The thread is assigned the given internal name. 
 *
 * Additionally, DDEKit threads possess a thread-local storage area where they
 * may store arbitrary data.
 *
 * \param fun     thread function
 * \param arg     optional argument to thread function, set to NULL if not needed
 * \param name    internal thread name
 */
ddekit_thread_t *ddekit_thread_create(void (*fun)(void *), void *arg,
	const char *name);

/** Reference to own DDEKit thread id. 
 *
 * \ingroup DDEKit_threads
 */
ddekit_thread_t *ddekit_thread_myself(void);

/** Initialize thread with given name. 
 *
 * \ingroup DDEKit_threads
 *
 * This function may be used by threads that were not created using
 * \ref ddekit_thread_create. This enables such threads to be handled as if they
 * were DDEKit threads.
 */
ddekit_thread_t *ddekit_thread_setup_myself(const char *name);

/** Get TLS data for a specific thread.
 *
 * \ingroup DDEKit_threads
 *
 * \return Pointer to TLS data of this thread.
 */
void *ddekit_thread_get_data(ddekit_thread_t *thread);

/** Get TLS data for current thread.
 *
 * \ingroup DDEKit_threads
 *
 * Same as calling \ref ddekit_thread_get_data with \ref ddekit_thread_myself
 * as parameter.
 *
 * \return Pointer to TLS data of current thread.
 */
void *ddekit_thread_get_my_data(void);

/** Set TLS data for specific thread.
 *
 * \ingroup DDEKit_threads
 *
 * \param thread     DDEKit thread
 * \param data       pointer to thread data
 */
void ddekit_thread_set_data(ddekit_thread_t *thread, void *data);

/** Set TLS data for current thread.
 *
 * \ingroup DDEKit_threads
 *
 * \param data       pointer to thread data
 */
void ddekit_thread_set_my_data(void *data);

/** Sleep for some miliseconds.
 *
 * \ingroup DDEKit_threads
 *
 * \param msecs      time to sleep in ms.
 */
void ddekit_thread_msleep(unsigned long msecs);

/** Sleep for some microseconds.
 *
 * \ingroup DDEKit_threads
 *
 * \param usecs      time to sleep in µs.
 */
void ddekit_thread_usleep(unsigned long usecs);

/** Sleep for some nanoseconds.
 *
 * \ingroup DDEKit_threads
 *
 * \param usecs      time to sleep in ns.
 */
void ddekit_thread_nsleep(unsigned long nsecs);

/** Sleep until a lock becomes unlocked.
 *
 * \ingroup DDEKit_threads
 */
void ddekit_thread_sleep(ddekit_lock_t *lock);

/** Wakeup a waiting thread. 
 *
 * \ingroup DDEKit_threads
 */
void ddekit_thread_wakeup(ddekit_thread_t *thread);

/** Terminate a thread 
 *
 * \ingroup DDEKit_threads
 */
void ddekit_thread_exit(void) __attribute__((noreturn));

/** Terminate a thread 
 *
 * \ingroup DDEKit_threads
 */
void ddekit_thread_terminate(ddekit_thread_t *thread);

/** Get the name, a thread registered with DDEKit. 
 *
 * \ingroup DDEKit_threads
 */
const char *ddekit_thread_get_name(ddekit_thread_t *thread);

/** Get unique ID of a DDEKit thread.
 *
 * \ingroup DDEKit_threads
 *
 *  DDEKit does not allow direct access to the thread data
 *  structure, since this struct contains L4-specific data types.
 *  However, applications might want to get some kind of ID related
 *  to a ddekit_thread, for instance to use it as a Linux-like PID.
 */
int ddekit_thread_get_id(ddekit_thread_t *thread);

/** Hint that this thread is done and may be scheduled somehow. 
 *
 * \ingroup DDEKit_threads
 */
void ddekit_thread_schedule(void);

/** Hint that this thread is done and may be scheduled somehow. 
 *
 * \ingroup DDEKit_threads
 */
void ddekit_yield(void);

/** Initialize DDEKit thread subsystem. 
 *
 * \ingroup DDEKit_threads
 */
void ddekit_init_threads(void);

#endif
