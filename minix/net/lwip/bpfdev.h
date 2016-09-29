#ifndef MINIX_NET_LWIP_BPFDEV_H
#define MINIX_NET_LWIP_BPFDEV_H

/*
 * BPF link structure, used to abstract away the details of the BPF structure
 * from other modules.
 */
struct bpfdev_link {
	TAILQ_ENTRY(bpfdev_link) bpfl_next;
};

void bpfdev_init(void);
void bpfdev_process(message * m_ptr, int ipc_status);
void bpfdev_detach(struct bpfdev_link * bpf);
void bpfdev_input(struct bpfdev_link * bpf, const struct pbuf * pbuf);
void bpfdev_output(struct bpfdev_link * bpf, const struct pbuf * pbuf);

#endif /* !MINIX_NET_LWIP_BPFDEV_H */
