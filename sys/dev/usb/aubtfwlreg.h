
#define AR3K_FIRMWARE_HEADER_SIZE 20

#define AR3K_SEND_FIRMWARE	1
#define AR3K_GET_STATE		5
#define AR3K_SET_NORMAL_MODE	7
#define AR3K_GET_VERSION	9
#define AR3K_SWITCH_VID_PID	10

#define AR3K_STATE_MODE_MASK	0x3f
#define AR3K_STATE_MODE_NORMAL	14
#define AR3K_STATE_IS_SYSCFGED	0x40
#define AR3K_STATE_IS_PATCHED	0x80

struct ar3k_version {
	uint32_t rom;
	uint32_t build;
	uint32_t ram;
	uint8_t clock;
#define AR3K_CLOCK_26M		0
#define AR3K_CLOCK_40M		1
#define AR3K_CLOCK_19M		2
	uint8_t pad[7];
};
