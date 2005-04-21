#ifndef __mtools_scsi_h
#define __mtools_scsi_h


#define SCSI_READ 0x8
#define SCSI_WRITE 0xA
#define SCSI_IOMEGA 0xC
#define SCSI_INQUIRY 0x12
#define SCSI_MODE_SENSE 0x1a
#define SCSI_START_STOP 0x1b
#define SCSI_ALLOW_MEDIUM_REMOVAL 0x1e
#define SCSI_GROUP1 0x20
#define SCSI_READ_CAPACITY 0x25


typedef enum { SCSI_IO_READ, SCSI_IO_WRITE } scsi_io_mode_t;
int scsi_max_length(void);
int scsi_cmd(int fd, unsigned char cdb[6], int clen, scsi_io_mode_t mode,
	     void *data, size_t len, void *extra_data);
int scsi_open(const char *name, int flags, int mode, void **extra_data);

#endif /* __mtools_scsi_h */
