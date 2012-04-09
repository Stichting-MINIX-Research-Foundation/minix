#ifndef _VBOX_VMMDEV_H
#define _VBOX_VMMDEV_H

#define VMMDEV_PCI_VID 0x80ee
#define VMMDEV_PCI_DID 0xcafe

#define VMMDEV_REQ_HOSTTIME		10
#define VMMDEV_REQ_ACKNOWLEDGEEVENTS	41
#define VMMDEV_REQ_REPORTGUESTINFO	50
#define VMMDEV_REQ_HGCMCONNECT		60
#define VMMDEV_REQ_HGCMDISCONNECT	61
#define VMMDEV_REQ_HGCMCALL		62
#define VMMDEV_REQ_HGCMCANCEL		64

#define VMMDEV_ERR_OK			0		/* success */
#define VMMDEV_ERR_GENERIC		(-1)		/* general failure */
#define VMMDEV_ERR_HGCM_NOT_FOUND	(-2900)		/* service not found */
#define VMMDEV_ERR_HGCM_DENIED		2901		/* client rejected */
#define VMMDEV_ERR_HGCM_INVALID_ADDR	(-2902)		/* invalid address */
#define VMMDEV_ERR_HGCM_ASYNC_EXEC	2903		/* call in progress */
#define VMMDEV_ERR_HGCM_INTERNAL	(-2904)		/* internal error */
#define VMMDEV_ERR_HGCM_INVALID_ID	(-2905)		/* invalid client ID */

#define VMMDEV_MAKEWORD(m,n)	(((m) << 16) | (n))

#define VMMDEV_BACKDOOR_VERSION	VMMDEV_MAKEWORD(1, 1)
#define VMMDEV_GUEST_VERSION	VMMDEV_MAKEWORD(1, 4)
#define VMMDEV_GUEST_OS_OTHER	0x90000		/* this is L4 - close enough */

struct VMMDevRequestHeader {
	u32_t size;
	u32_t version;
	u32_t type;
	i32_t result;
	u32_t reserved[2];
};

struct VMMDevReportGuestInfo {
	struct VMMDevRequestHeader header;
	u32_t add_version;
	u32_t os_type;
};

struct VMMDevReqHostTime {
	struct VMMDevRequestHeader header;
	u64_t time;
};

#define VMMDEV_EVENT_HGCM	(1 << 1)

struct VMMDevEvents {
	struct VMMDevRequestHeader header;
	u32_t events;
};

#define VMMDEV_HGCM_REQ_DONE	(1 << 0)

struct VMMDevHGCMHeader {
	struct VMMDevRequestHeader header;
	u32_t flags;
	i32_t result;
};

#define VMMDEV_HGCM_SVCLOC_LOCALHOST_EXISTING 2

#define VMMDEV_HGCM_NAME_SIZE 128

struct VMMDevHGCMConnect {
	struct VMMDevHGCMHeader header;
	u32_t type;
	char name[VMMDEV_HGCM_NAME_SIZE];
	u32_t client_id;
};

struct VMMDevHGCMDisconnect {
	struct VMMDevHGCMHeader header;
	u32_t client_id;
};

#define VMMDEV_HGCM_FLAG_TO_HOST	0x01
#define VMMDEV_HGCM_FLAG_FROM_HOST	0x02

struct VMMDevHGCMPageList {
	u32_t flags;
	u16_t offset;
	u16_t count;
	u64_t addr[1];
};

#define VMMDEV_HGCM_PARAM_U32		1
#define VMMDEV_HGCM_PARAM_U64		2
#define VMMDEV_HGCM_PARAM_PAGELIST	10

struct VMMDevHGCMParam {
	u32_t type;
	union {
		u32_t u32;
		u64_t u64;
		struct {
			u32_t size;
			union {
				u32_t phys;
				void *vir;
			} addr;
		} ptr;
		struct {
			u32_t size;
			u32_t offset;
		} pagelist;
	};
};

struct VMMDevHGCMCall {
	struct VMMDevHGCMHeader header;
	u32_t client_id;
	u32_t function;
	u32_t count;
};

struct VMMDevHGCMCancel {
	struct VMMDevHGCMHeader header;
};

#define VMMDEV_BUF_SIZE		4096		/* just one page */

#endif /* _VBOX_VMMDEV_H */
