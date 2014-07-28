/* libbdev - asynchronous call structure management */

#include <minix/drivers.h>
#include <minix/bdev.h>
#include <assert.h>

#include "const.h"
#include "type.h"
#include "proto.h"

static bdev_call_t *calls[NR_CALLS];

bdev_call_t *bdev_call_alloc(int count)
{
/* Allocate a call structure.
 */
  bdev_call_t *call;
  bdev_id_t id;

  for (id = 0; id < NR_CALLS; id++)
	if (calls[id] == NULL)
		break;

  if (id == NR_CALLS)
	return NULL;

  call = malloc(sizeof(bdev_call_t) +
	sizeof(call->gvec[0]) * (count - 1) +
	sizeof(call->vec[0]) * count);

  if (call == NULL)
	return NULL;

  call->id = id;
  call->vec = (iovec_t *) &call->gvec[count];

  calls[id] = call;

  return call;
}

void bdev_call_free(bdev_call_t *call)
{
/* Free a call structure.
 */

  assert(calls[call->id] == call);

  calls[call->id] = NULL;

  free(call);
}

bdev_call_t *bdev_call_get(bdev_id_t id)
{
/* Retrieve a call structure by request number.
 */

  if (id < 0 || id >= NR_CALLS)
	return NULL;

  return calls[id];
}

bdev_call_t *bdev_call_find(dev_t dev)
{
/* Find the first asynchronous request for the given device, if any.
 */
  bdev_id_t id;

  for (id = 0; id < NR_CALLS; id++)
	if (calls[id] != NULL && calls[id]->dev == dev)
		return calls[id];

  return NULL;
}

bdev_call_t *bdev_call_iter_maj(dev_t dev, bdev_call_t *call,
	bdev_call_t **next)
{
/* Iterate over all asynchronous requests for a major device. This function
 * must be safe even if the returned call structure is freed.
 */
  bdev_id_t id;
  int major;

  major = major(dev);

  /* If this is the first invocation, find the first match. Otherwise, take the
   * call we found to be next in the last invocation, which may be NULL.
   */
  if (call == NULL) {
	for (id = 0; id < NR_CALLS; id++)
		if (calls[id] != NULL && major(calls[id]->dev) == major)
			break;

	if (id == NR_CALLS)
		return NULL;

	call = calls[id];
  } else {
	if ((call = *next) == NULL)
		return NULL;
  }

  /* Look for the next match, if any. */
  *next = NULL;

  for (id = call->id + 1; id < NR_CALLS; id++) {
	if (calls[id] != NULL && major(calls[id]->dev) == major) {
		*next = calls[id];

		break;
	}
  }

  return call;
}
