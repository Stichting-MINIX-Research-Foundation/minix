
#include "fsdriver.h"

/*
 * Copy data from the caller into the local address space.
 */
int
fsdriver_copyin(const struct fsdriver_data * data, size_t off, void * ptr,
	size_t len)
{

	/* Do nothing for peek requests. */
	if (data == NULL)
		return OK;

	/* The data size field is used only for this integrity check. */
	if (off + len > data->size)
		panic("fsdriver: copy-in buffer overflow");

	if (data->endpt == SELF) {
		memcpy(ptr, &data->ptr[off], len);

		return OK;
	}

	return sys_safecopyfrom(data->endpt, data->grant, off, (vir_bytes)ptr,
	    (phys_bytes)len);
}

/*
 * Copy data from the local address space to the caller.
 */
int
fsdriver_copyout(const struct fsdriver_data * data, size_t off,
	const void * ptr, size_t len)
{

	/* Do nothing for peek requests. */
	if (data == NULL)
		return OK;

	/* The data size field is used only for this integrity check. */
	if (off + len > data->size)
		panic("fsdriver: copy-out buffer overflow");

	if (data->endpt == SELF) {
		memcpy(&data->ptr[off], ptr, len);

		return OK;
	}

	return sys_safecopyto(data->endpt, data->grant, off, (vir_bytes)ptr,
	    (phys_bytes)len);
}

/*
 * Zero out a data region in the caller.
 */
int
fsdriver_zero(const struct fsdriver_data * data, size_t off, size_t len)
{

	/* Do nothing for peek requests. */
	if (data == NULL)
		return OK;

	/* The data size field is used only for this integrity check. */
	if (off + len > data->size)
		panic("fsdriver: copy-out buffer overflow");

	if (data->endpt == SELF) {
		memset(&data->ptr[off], 0, len);

		return OK;
	}

	return sys_safememset(data->endpt, data->grant, off, 0, len);
}

/*
 * Copy in a null-terminated name, and perform sanity checks.
 */
int
fsdriver_getname(endpoint_t endpt, cp_grant_id_t grant, size_t len,
	char * name, size_t size, int not_empty)
{
	int r;

	/* The given length includes the null terminator. */
	if (len == 0 || (not_empty && len == 1))
		return EINVAL;
	if (len > size)
		return ENAMETOOLONG;

	if ((r = sys_safecopyfrom(endpt, grant, 0, (vir_bytes)name,
	    (phys_bytes)len)) != OK)
		return r;

	if (name[len - 1] != 0) {
		printf("fsdriver: name not null-terminated\n");
		return EINVAL;
	}

	return OK;
}
