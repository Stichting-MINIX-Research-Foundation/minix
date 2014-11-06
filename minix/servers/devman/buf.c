/* buf.c - by Alen Stojanov and David van Moolenbroek, taken from procfs */

#include "devman.h"
#include "proto.h"
#include <stdarg.h>
#include <assert.h>

static char *buf;
static size_t left, used;
static off_t skip;

/*===========================================================================*
 *				buf_init				     *
 *===========================================================================*/
void buf_init(char *ptr, size_t len, off_t start)
{
	/* Initialize the buffer for fresh use. The output is to be stored into
	 * 'ptr' which is BUF_SIZE bytes in size, since that is the size we
	 * requested. Due to the way vsnprintf works, we cannot use the last
	 * byte of this buffer. The first 'start' bytes of the produced output
	 * are to be skipped. After that, a total of 'len' bytes are requested.
	 */

	buf = ptr;
	skip = start;
	left = MIN(len, BUF_SIZE - 1);
	used = 0;
}

/*===========================================================================*
 *				buf_printf				     *
 *===========================================================================*/
void buf_printf(char *fmt, ...)
{
	/* Add formatted text to the end of the buffer.
	 */
	va_list args;
	ssize_t len, max;

	if (left == 0)
		return;

	/* There is no way to estimate how much space the result will take, so
	 * we need to produce the string even when skipping part of the start.
	 * The null terminating character is not part of the result, so room
	 * must be given for it to be stored after completely filling up the
	 * requested part of the buffer.
	 */
	max = MIN(skip + left + 1, BUF_SIZE);

	va_start(args, fmt);
	len = vsnprintf(&buf[used], max, fmt, args);
	va_end(args);

	/* The snprintf family returns the number of bytes that would be stored
	 * if the buffer were large enough, excluding the null terminator.
	 */
	if (len >= BUF_SIZE)
		len = BUF_SIZE - 1;

	if (skip > 0) {
		assert(used == 0);

		if (skip >= len) {
			skip -= len;

			return;
		}

		memmove(buf, &buf[skip], len - skip);

		len -= skip;
		skip = 0;
	}

	assert(skip == 0);
	assert(len >= 0);
	assert((ssize_t) left >= 0);

	if (len > (ssize_t) left)
		len = left;

	used += len;
	left -= len;
}

/*===========================================================================*
 *				buf_append				     *
 *===========================================================================*/
void buf_append(char *data, size_t len)
{
	/* Add arbitrary data to the end of the buffer.
	 */

	if (left == 0)
		return;

	if (skip > 0) {
		if (skip >= (ssize_t) len) {
			skip -= len;

			return;
		}

		data += skip;
		len -= skip;
		skip = 0;
	}

	if (len > left)
		len = left;

	memcpy(&buf[used], data, len);

	used += len;
	left -= len;
}

/*===========================================================================*
 *				buf_result				     *
 *===========================================================================*/
ssize_t buf_result(void)
{
	/* Return the resulting number of bytes produced, not counting the
	 * trailing null character in the buffer.
	 */

	return used;
}
