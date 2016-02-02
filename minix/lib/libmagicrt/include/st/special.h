#ifndef ST_SPECIAL_H
#define ST_SPECIAL_H

/* Public functions for special types and regions. */
PUBLIC void st_register_typename_key(const char *key);
PUBLIC void st_register_typename_keys(const char **keys);
PUBLIC int st_add_special_mmapped_region(void *address, size_t size,
	const char* name);
PUBLIC int st_del_special_mmapped_region_by_addr(void *address);

#endif /* ST_SPECIAL_H */
