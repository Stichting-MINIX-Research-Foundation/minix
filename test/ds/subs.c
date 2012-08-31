#include "inc.h"

char *key_u32 = "test_u32";

/* SEF functions and variables. */
static void sef_local_startup(void);

/*===========================================================================*
 *				main					     *
 *===========================================================================*/
int main(void)
{
	int r;
	message mess;
	char key[DS_MAX_KEYLEN];
	int type;
	u32_t num;
	char string[17];
	char buf[1000];
	size_t length = 1000;

	/* SEF local startup. */
	sef_local_startup();

	/* Subscribe. */
	r = ds_subscribe(key_u32, DSF_INITIAL);
	if(r != OK && r != EEXIST) {
		printf("SUBSCRIBER: error in ds_subscribe: %d\n", r);
		return -1;
	}

	while(1) {
		/* Wait for a message. */
		r = sef_receive(ANY, &mess);
		if(r != OK) {
			printf("SUBSCRIBER: sef_receive failed.\n");
			return 1;
		}
		/* Only handle notifications from DS. */
		if(mess.m_source != DS_PROC_NR)
			continue;

		/* Check which one was changed. */
		r = ds_check(key, &type, NULL);
		if(r == ENOENT) {
			printf("SUBSCRIBER: the key %s was deleted.\n",
				key);
			continue;
		}
		if(r != OK) {
			printf("SUBSCRIBER: error in ds_check.\n");
			continue;
		}

		/* Retrieve the entry. */
		printf("SUBSCRIBER: key: %s, ", key);
		switch(type) {
		case DSF_TYPE_U32:
			r = ds_retrieve_u32(key, &num);
			if(r != OK)
				printf("error in ds_retrieve_u32.\n");
			printf("U32: %d\n", num);
			break;
		case DSF_TYPE_STR:
			r = ds_retrieve_str(key, string, sizeof(string)-1);
			if(r != OK)
				printf("error in ds_retrieve_str.\n");
			printf("STR: %s\n", string);
			break;
		case DSF_TYPE_MEM:
			r = ds_retrieve_mem(key, buf, &length);
			if(r != OK)
				printf("error in ds_retrieve_mem.\n");
			break;
		case DSF_TYPE_MAP:
			break;
		default:
			printf("error in type! %d\n", type);
		}
	}

	return 0;
}

/*===========================================================================*
 *			       sef_local_startup			     *
 *===========================================================================*/
static void sef_local_startup()
{
  /* Let SEF perform startup. */
  sef_startup();
}

