#ifndef _FLASH_IO_H_
#define _FLASH_IO_H_

struct flash_io {
	device_t fio_dev;
	struct bintime fio_creation;
	struct bintime fio_last_write;
	struct bufq_state *fio_bufq;
	uint8_t *fio_data;
	daddr_t fio_block;
	kmutex_t fio_lock;
	bool fio_write_pending;
	struct lwp *fio_thread;
	kcondvar_t fio_cv;
	bool fio_exiting;
	struct flash_interface *fio_if;
};

int flash_io_submit(struct flash_io *, struct buf *);
void flash_sync_thread(void *);
int flash_sync_thread_init(struct flash_io *, device_t,
	struct flash_interface *);
void flash_sync_thread_destroy(struct flash_io *);

#endif
