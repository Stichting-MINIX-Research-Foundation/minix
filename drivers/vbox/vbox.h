#ifndef _VBOX_H
#define _VBOX_H

#define VBOX_PCI_VID 0x80ee
#define VBOX_PCI_DID 0xcafe

struct VMMDevRequestHeader {
	u32_t size;
	u32_t version;
	u32_t type;
	i32_t rc;
	u32_t reserved[2];
};

struct VBoxGuestInfo {
	u32_t add_version;
	u32_t os_type;
};

struct VMMDevReportGuestInfo {
	struct VMMDevRequestHeader header;
	struct VBoxGuestInfo guest_info;
};

struct VMMDevReqHostTime {
	struct VMMDevRequestHeader header;
	u64_t time;
};

#define VMMDEV_MAKEWORD(m,n)	(((m) << 16) | (n))

#define VMMDEV_BACKDOOR_VERSION	VMMDEV_MAKEWORD(1, 1)
#define VMMDEV_GUEST_VERSION	VMMDEV_MAKEWORD(1, 4)
#define VMMDEV_GUEST_OS_OTHER	0x90000		/* this is L4 - close enough */

#define VMMDEV_REQ_REPORTGUESTINFO	50
#define VMMDEV_REQ_HOSTTIME		10

#define VMMDEV_ERR_OK		0
#define VMMDEV_ERR_PERM		(-10)

#define VMMDEV_BUF_SIZE		4096		/* just one page */

#endif /* _VBOX_H */
