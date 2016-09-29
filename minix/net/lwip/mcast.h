#ifndef MINIX_NET_LWIP_MCAST_H
#define MINIX_NET_LWIP_MCAST_H

struct mcast_member;

struct mcast_head {
	LIST_HEAD(, mcast_member) mh_list;
};

#define mcast_isempty(mcast_head) (LIST_EMPTY(&(mcast_head)->mh_list))

void mcast_init(void);
void mcast_reset(struct mcast_head * mcast_head);
int mcast_join(struct mcast_head * mcast_head, const ip_addr_t * group,
	struct ifdev * ifdev);
int mcast_leave(struct mcast_head * mcast_head, const ip_addr_t * group,
	struct ifdev * ifdev);
void mcast_leave_all(struct mcast_head * mcast_head);
void mcast_clear(struct ifdev * ifdev);

#endif /* !MINIX_NET_LWIP_MCAST_H */
