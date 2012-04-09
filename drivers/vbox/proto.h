#ifndef _VBOX_PROTO_H
#define _VBOX_PROTO_H

/* err.c */
extern int convert_err(int code);

/* hgcm.c */
extern void hgcm_message(message *m_ptr, int ipc_status);
extern void hgcm_intr(void);

/* vbox.c */
extern int vbox_request(struct VMMDevRequestHeader *header, phys_bytes addr,
	int type, size_t size);

#endif /* _VBOX_PROTO_H */
