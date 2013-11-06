#include "inc.h"

char *key_u32 = "test_u32";
char *key_str = "test_str";
char *key_mem = "test_mem";
char *key_label = "test_label";

/*===========================================================================*
 *				test_u32				     *
 *===========================================================================*/
void test_u32(void)
{
	int r;
	u32_t value;

	/* Publish and retrieve. */
	r = ds_publish_u32(key_u32, 1234, 0);
	assert(r == OK);
	r = ds_retrieve_u32(key_u32, &value);
	assert(r == OK && value == 1234);

	/* If dstest deletes 'test_u32' immediately after publishing it,
	 * subs will catch the event, but it can't check it immediately.
	 * So dstest will sleep 2 seconds to wait for subs to complete. */
	sleep(2);

	/* Publish again without DSF_OVERWRITE. */
	r = ds_publish_u32(key_u32, 4321, 0);
	assert(r == EEXIST);

	/* Publish again with DSF_OVERWRITE to overwrite it. */
	r = ds_publish_u32(key_u32, 4321, DSF_OVERWRITE);
	assert(r == OK);
	r = ds_retrieve_u32(key_u32, &value);
	assert(r == OK && value == 4321);

	/* Delete. */
	r = ds_delete_u32(key_u32);
	assert(r == OK);
	r = ds_retrieve_u32(key_u32, &value);
	assert(r == ESRCH);

	printf("DSTEST: U32 test successful!\n");
}

/*===========================================================================*
 *				test_str				     *
 *===========================================================================*/
void test_str(void)
{
	int r;
	char *string = "little";
	char *longstring = "verylooooooongstring";
	char buf[17];

	r = ds_publish_str(key_str, string, 0);
	assert(r == OK);

	r = ds_retrieve_str(key_str, buf, sizeof(buf)-1);
	assert(r == OK && strcmp(string, buf) == 0);

	r = ds_delete_str(key_str);
	assert(r == OK);

	/* Publish a long string. */
	r = ds_publish_str(key_str, longstring, 0);
	assert(r == OK);

	r = ds_retrieve_str(key_str, buf, sizeof(buf)-1);
	assert(r == OK && strcmp(string, buf) != 0
		&& strncmp(longstring, buf, sizeof(buf)-1) == 0);

	r = ds_delete_str(key_str);
	assert(r == OK);

	printf("DSTEST: STRING test successful!\n");
}

/*===========================================================================*
 *				test_mem				     *
 *===========================================================================*/
void test_mem(void)
{
	char *string1 = "ok, this is a string";
	char *string2 = "ok, this is a very looooong string";
	size_t len1 = strlen(string1) + 1;
	size_t len2 = strlen(string2) + 1;
	char buf[100];
	size_t get_len;
	int r;

	/* Publish and retrieve. */
	r = ds_publish_mem(key_mem, string1, len1, 0);
	assert(r == OK);
	get_len = 100;
	r = ds_retrieve_mem(key_mem, buf, &get_len);
	assert(r == OK && strcmp(string1, buf) == 0);
	assert(get_len == len1);

	/* Let get_len=8, which is less than strlen(string1). */
	get_len = 8;
	r = ds_retrieve_mem(key_mem, buf, &get_len);
	assert(r == OK && get_len == 8);

	/* Publish again to overwrite with a bigger memory range. */
	r = ds_publish_mem(key_mem, string2, len2, DSF_OVERWRITE);
	assert(r == OK);

	get_len = 100;
	r = ds_retrieve_mem(key_mem, buf, &get_len);
	assert(r == OK && strcmp(string2, buf) == 0);
	assert(get_len == len2);

	r = ds_delete_mem(key_mem);
	assert(r == OK);

	printf("DSTEST: MEM test successful!\n");
}

/*===========================================================================*
 *				test_label				     *
 *===========================================================================*/
void test_label(void)
{
	int r;
	char label[DS_MAX_KEYLEN];
	endpoint_t endpoint;

	/* Retrieve own label and endpoint. */
	r = ds_retrieve_label_name(label, sef_self());
	assert(r == OK);
	r = ds_retrieve_label_endpt(label, &endpoint);
	assert(r == OK && endpoint == sef_self());

	/* Publish and delete. */
	r = ds_publish_label(label, endpoint, 0);
	assert(r == EPERM);
	r = ds_delete_label(label);
	assert(r == EPERM);

	printf("DSTEST: LABEL test successful!\n");
}

/*===========================================================================*
 *			       sef_cb_init_fresh			     *
 *===========================================================================*/
static int sef_cb_init_fresh(int UNUSED(type), sef_init_info_t *UNUSED(info))
{
	/* Run all the tests. */
	test_u32();
	test_str();
	test_mem();
	test_label();

	return OK;
}

/*===========================================================================*
 *			       sef_local_startup			     *
 *===========================================================================*/
static void sef_local_startup(void)
{
	/* Let SEF perform startup. */
	sef_setcb_init_fresh(sef_cb_init_fresh);

	sef_startup();
}

/*===========================================================================*
 *				main					     *
 *===========================================================================*/
int main(void)
{
	/* SEF local startup. */
	sef_local_startup();

	return 0;
}
