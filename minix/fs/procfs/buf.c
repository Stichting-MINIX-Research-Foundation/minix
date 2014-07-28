/* ProcFS - buf.c - by Alen Stojanov and David van Moolenbroek */

#include "inc.h"
#include <stdarg.h>

#define BUF_SIZE 4096

static char buf[BUF_SIZE + 1];
static size_t off, left, used;
static off_t skip;

/*===========================================================================*
 *				buf_init				     *
 *===========================================================================*/
void buf_init(off_t start, size_t len)
{
	/* Initialize the buffer for fresh use. The first 'start' bytes of the
	 * produced output are to be skipped. After that, up to a total of
	 * 'len' bytes are requested.
	 */

	skip = start;
	left = MIN(len, BUF_SIZE);
	off = 0;
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
	 * If part of the result is to be skipped, do not memcpy; instead, save
	 * the offset of where the result starts within the buffer.
	 *
	 * The null terminating character is not part of the result, so room
	 * must be given for it to be stored after completely filling up the
	 * requested part of the buffer.
	 */
	max = MIN(skip + left, BUF_SIZE);

	va_start(args, fmt);
	len = vsnprintf(&buf[off + used], max + 1, fmt, args);
	va_end(args);

	if (skip > 0) {
		assert(off == 0);
		assert(used == 0);

		if (skip >= len) {
			skip -= len;

			return;
		}

		off = skip;
		if (left > BUF_SIZE - off)
			left = BUF_SIZE - off;
		len -= off;
		skip = 0;
	}

	assert(skip == 0);
	assert(len >= 0);
	assert((long) left >= 0);

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

	memcpy(&buf[off + used], data, len);

	used += len;
	left -= len;
}

/*===========================================================================*
 *				buf_get					     *
 *===========================================================================*/
size_t buf_get(char **ptr)
{
	/* Return the buffer's starting address and the length of the used
	 * part, not counting the trailing null character for the latter.
	 */

	*ptr = &buf[off];

	return used;
}
