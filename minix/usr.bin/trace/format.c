
#include "inc.h"

#include <stdarg.h>

/*
 * The size of the formatting buffer, which in particular limits the maximum
 * size of the output from the variadic functions.  All printer functions which
 * are dealing with potentially large or even unbounded output, should be able
 * to generate their output in smaller chunks.  In the end, nothing that is
 * being printed as a unit should even come close to reaching this limit.
 */
#define FORMAT_BUFSZ	4096

/*
 * The buffer which is used for all intermediate copying and/or formatting.
 * Care must be taken that only one function uses this buffer at any time.
 */
static char formatbuf[FORMAT_BUFSZ];

/*
 * Reset the line formatting for the given process.
 */
void
format_reset(struct trace_proc * proc)
{

	proc->next_sep = NULL;
	proc->depth = -1;
}

/*
 * Set the next separator for the given process.  The given separator may be
 * NULL.
 */
void
format_set_sep(struct trace_proc * proc, const char * sep)
{

	proc->next_sep = sep;
}

/*
 * Print and clear the next separator for the process, if any.
 */
void
format_push_sep(struct trace_proc * proc)
{

	if (proc->next_sep != NULL) {
		put_text(proc, proc->next_sep);

		proc->next_sep = NULL;
	}
}

/*
 * Print a field, e.g. a parameter or a field from a structure, separated from
 * other fields at the same nesting depth as appropriate.  If the given field
 * name is not NULL, it may or may not be printed.  The given text is what will
 * be printed for this field so far, but the caller is allowed to continue
 * printing text for the same field with e.g. put_text().  As such, the given
 * text may even be an empty string.
 */
void
put_field(struct trace_proc * proc, const char * name, const char * text)
{

	/*
	 * At depth -1 (the basic line level), names are not used.  A name
	 * should not be supplied by the caller in that case, but, it happens.
	 */
	if (proc->depth < 0)
		name = NULL;

	format_push_sep(proc);

	if (name != NULL && (proc->depths[proc->depth].name || allnames)) {
		put_text(proc, name);
		put_text(proc, "=");
	}

	put_text(proc, text);

	format_set_sep(proc, proc->depths[proc->depth].sep);
}

/*
 * Increase the nesting depth with a new block of fields, enclosed within
 * parentheses, brackets, etcetera.  The given name, which may be NULL, is the
 * name of the entire nested block.  In the flags field, PF_NONAME indicates
 * that the fields within the block should have their names printed or not,
 * although this may be overridden by setting the allnames variable.  The given
 * string is the block opening string (e.g., an opening parenthesis).  The
 * given separator is used to separate the fields within the nested block, and
 * should generally be ", " to maintain output consistency.
 */
void
put_open(struct trace_proc * proc, const char * name, int flags,
	const char * string, const char * sep)
{

	put_field(proc, name, string);

	proc->depth++;

	assert(proc->depth < MAX_DEPTH);

	proc->depths[proc->depth].sep = sep;
	proc->depths[proc->depth].name = !(flags & PF_NONAME);

	format_set_sep(proc, NULL);
}

/*
 * Decrease the nesting depth by ending a nested block of fields.  The given
 * string is the closing parenthesis, bracket, etcetera.
 */
void
put_close(struct trace_proc * proc, const char * string)
{

	assert(proc->depth >= 0);

	put_text(proc, string);

	proc->depth--;

	if (proc->depth >= 0)
		format_set_sep(proc, proc->depths[proc->depth].sep);
	else
		format_set_sep(proc, NULL);
}

/*
 * Version of put_text with variadic arguments.  The given process may be NULL.
 */
void
put_fmt(struct trace_proc * proc, const char * fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	(void)vsnprintf(formatbuf, sizeof(formatbuf), fmt, ap);
	va_end(ap);

	put_text(proc, formatbuf);
}

/*
 * Version of put_field with variadic arguments.
 */
void
put_value(struct trace_proc * proc, const char * name, const char * fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	(void)vsnprintf(formatbuf, sizeof(formatbuf), fmt, ap);
	va_end(ap);

	put_field(proc, name, formatbuf);
}

/*
 * Start printing a structure.  In general, the function copies the contents of
 * the structure of size 'size' from the traced process at 'addr' into the
 * local 'ptr' structure, opens a nested block with name 'name' (which may
 * be NULL) using an opening bracket, and returns TRUE to indicate that the
 * caller should print fields from the structure.  However, if 'flags' contains
 * PF_FAILED, the structure will be printed as a pointer, no copy will be made,
 * and the call will return FALSE.  Similarly, if the remote copy fails, a
 * pointer will be printed and the call will return FALSE.  If PF_LOCADDR is
 * given, 'addr' is a local address, and an intraprocess copy will be made.
 */
int
put_open_struct(struct trace_proc * proc, const char * name, int flags,
	vir_bytes addr, void * ptr, size_t size)
{

	if ((flags & PF_FAILED) || valuesonly > 1 || addr == 0) {
		if (flags & PF_LOCADDR)
			put_field(proc, name, "&..");
		else
			put_ptr(proc, name, addr);

		return FALSE;
	}

	if (!(flags & PF_LOCADDR)) {
		if (mem_get_data(proc->pid, addr, ptr, size) < 0) {
			put_ptr(proc, name, addr);

			return FALSE;
		}
	} else
		memcpy(ptr, (void *) addr, size);

	put_open(proc, name, flags, "{", ", ");

	return TRUE;
}

/*
 * End printing a structure.  This must be called only to match a successful
 * call to put_open_struct.  The given 'all' flag indicates whether all fields
 * of the structure have been printed; if not, a ".." continuation text is
 * printed to show the user that some structure fields have not been printed.
 */
void
put_close_struct(struct trace_proc * proc, int all)
{

	if (!all)
		put_field(proc, NULL, "..");

	put_close(proc, "}");
}

/*
 * Print a pointer.  NULL is treated as a special case.
 */
void
put_ptr(struct trace_proc * proc, const char * name, vir_bytes addr)
{

	if (addr == 0 && !valuesonly)
		put_field(proc, name, "NULL");
	else
		put_value(proc, name, "&0x%lx", addr);
}

/*
 * Print the contents of a buffer, at remote address 'addr' and of 'bytes'
 * size, as a field using name 'name' (which may be NULL).  If the PF_FAILED
 * flag is given, the buffer address is printed instead, since it is assumed
 * that the actual buffer contains garbage.  If the PF_LOCADDR flag is given,
 * the given address is a local address and no intraprocess copies are
 * performed.  If the PF_STRING flag is given, the buffer is expected to
 * contain a null terminator within its size, and the string will be printed
 * only up to there.  Normally, the string is cut off beyond a number of bytes
 * which depends on the verbosity level; if the PF_FULL flag is given, the full
 * string will be printed no matter its size (used mainly for path names, which
 * typically become useless once cut off).
 */
void
put_buf(struct trace_proc * proc, const char * name, int flags, vir_bytes addr,
	ssize_t size)
{
	const char *escaped;
	size_t len, off, max, chunk;
	unsigned int i;
	int cutoff;
	char *p;

	if ((flags & PF_FAILED) || valuesonly || addr == 0 || size < 0) {
		if (flags & PF_LOCADDR)
			put_field(proc, name, "&..");
		else
			put_ptr(proc, name, addr);

		return;
	}

	if (size == 0) {
		put_field(proc, name, "\"\"");

		return;
	}

	/*
	 * TODO: the maximum says nothing about the size of the printed text.
	 * Escaped-character printing can make the output much longer.  Does it
	 * make more sense to apply a limit after the escape transformation?
	 */
	if (verbose == 0) max = 32;
	else if (verbose == 1) max = 256;
	else max = SIZE_MAX;

	/*
	 * If the output is cut off, we put two dots after the closing quote.
	 * For non-string buffers, the output is cut off if the size exceeds
	 * our limit or we run into a copying error somewhere in the middle.
	 * For strings, the output is cut off unless we find a null terminator.
	 */
	cutoff = !!(flags & PF_STRING);
	len = (size_t)size;
	if (!(flags & PF_FULL) && len > max) {
		len = max;
		cutoff = TRUE;
	}

	for (off = 0; off < len; off += chunk) {
		chunk = len - off;
		if (chunk > sizeof(formatbuf) - 1)
			chunk = sizeof(formatbuf) - 1;

		if (!(flags & PF_LOCADDR)) {
			if (mem_get_data(proc->pid, addr + off, formatbuf,
			    chunk) < 0) {
				if (off == 0) {
					put_ptr(proc, name, addr);

					return;
				}

				cutoff = TRUE;
				break;
			}
		} else
			memcpy(formatbuf, (void *)addr, chunk);

		if (off == 0)
			put_field(proc, name, "\"");

		/* In strings, look for the terminating null character. */
		if ((flags & PF_STRING) &&
		    (p = memchr(formatbuf, '\0', chunk)) != NULL) {
			chunk = (size_t)(p - formatbuf);
			cutoff = FALSE;
		}

		/* Print the buffer contents using escaped characters. */
		for (i = 0; i < chunk; i++) {
			escaped = get_escape(formatbuf[i]);

			put_text(proc, escaped);
		}

		/* Stop if we found the end of the string. */
		if ((flags & PF_STRING) && !cutoff)
			break;
	}

	if (cutoff)
		put_text(proc, "\"..");
	else
		put_text(proc, "\"");
}

/*
 * Print a flags field, using known flag names.  The name of the whole field is
 * given as 'name' and may be NULL.  The caller must supply an array of known
 * flags as 'fp' (with 'num' entries).  Each entry in the array has a mask, a
 * value, and a name.  If the given flags 'value', bitwise-ANDed with the mask
 * of an entry, yields the value of that entry, then the name is printed.  This
 * means that certain zero bits may also be printed as actual flags, and that
 * by supplying an all-bits-set mask can print a flag name for a zero value,
 * for example F_OK for access().  See the FLAG macros and their usage for
 * examples.  All matching flag names are printed with a "|" separator, and if
 * after evaluating all 'num' entries in 'fp' there are still bits in 'value'
 * for which nothing has been printed, the remaining bits will be printed with
 * the 'fmt' format string for an integer (generally "%d" should be used).
 */
void
put_flags(struct trace_proc * proc, const char * name, const struct flags * fp,
	unsigned int num, const char * fmt, unsigned int value)
{
	unsigned int left;
	int first;

	if (valuesonly) {
		put_value(proc, name, fmt, value);

		return;
	}

	put_field(proc, name, "");

	for (first = TRUE, left = value; num > 0; fp++, num--) {
		if ((value & fp->mask) == fp->value) {
			if (first)
				first = FALSE;
			else
				put_text(proc, "|");
			put_text(proc, fp->name);

			left -= fp->value;
		}
	}

	if (left != 0) {
		if (first)
			first = FALSE;
		else
			put_text(proc, "|");

		put_fmt(proc, fmt, left);
	}

	/*
	 * If nothing has been printed so far, simply print a zero.  Ignoring
	 * the given format in this case is intentional: a simple 0 looks
	 * better than 0x0 or 00 etc.
	 */
	if (first)
		put_text(proc, "0");
}

/*
 * Print a tail field at the end of an array.  The given 'count' value is the
 * total number of elements in the array, or 0 to indicate that an error
 * occurred.  The given 'printed' value is the number of fields printed so far.
 * If some fields have been printed already, the number of fields not printed
 * will be shown as "..(+N)".  If no fields have been printed already, the
 * (total) number of fields not printed will be shown as "..(N)".  An error
 * will print "..(?)".
 *
 * The rules for printing an array are as follows.  In principle, arrays should
 * be enclosed in "[]".  However, if a copy error occurs immediately, a pointer
 * to the array should be printed instead.  An empty array should be printed as
 * "[]" (not "[..(0)]").  If a copy error occurs in the middle of the array,
 * put_tail should be used with count == 0.  Only if not all fields in the
 * array are printed, put_tail should be used with count > 0.  The value of
 * 'printed' is typically the result of an arbitrary limit set based on the
 * verbosity level.
 */
void
put_tail(struct trace_proc * proc, unsigned int count, unsigned int printed)
{

	if (count == 0)
		put_field(proc, NULL, "..(?)");
	else
		put_value(proc, NULL, "..(%s%u)",
		    (printed > 0) ? "+" : "", count - printed);
}
