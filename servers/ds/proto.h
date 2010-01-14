/* Function prototypes. */

/* main.c */
_PROTOTYPE(int main, (int argc, char **argv));

/* store.c */
_PROTOTYPE(int do_publish, (message *m_ptr));
_PROTOTYPE(int do_retrieve, (message *m_ptr));
_PROTOTYPE(int do_retrieve_label, (message *m_ptr));
_PROTOTYPE(int do_subscribe, (message *m_ptr));
_PROTOTYPE(int do_check, (message *m_ptr));
_PROTOTYPE(int do_delete, (message *m_ptr));
_PROTOTYPE(int do_snapshot, (message *m_ptr));
_PROTOTYPE(int do_getsysinfo, (message *m_ptr));
_PROTOTYPE(int sef_cb_init_fresh, (int type, sef_init_info_t *info));
_PROTOTYPE(int map_service, (struct rprocpub *rpub));

