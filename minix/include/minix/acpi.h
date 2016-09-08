#include <sys/types.h>
#include <minix/ipc.h>

#define ACPI_REQ_GET_IRQ			1
#define ACPI_REQ_MAP_BRIDGE			2

struct acpi_request_hdr {
	endpoint_t 	m_source; /* message header */
	u32_t		request;
};

/*
 * Message to request dev/pin translation to IRQ by acpi using the acpi routing
 * tables
 */
struct acpi_get_irq_req {
	struct acpi_request_hdr	hdr;
	u32_t			bus;
	u32_t			dev;
	u32_t			pin;
	u32_t			__padding[4];
};

/* response from acpi to acpi_get_irq_req */
struct acpi_get_irq_resp {
	endpoint_t 	m_source; /* message header */
	i32_t		irq;
	u32_t		__padding[7];
};

/* message format for pci bridge mappings to acpi */
struct acpi_map_bridge_req {
	struct acpi_request_hdr	hdr;
	u32_t	primary_bus;
	u32_t	secondary_bus;
	u32_t	device;
	u32_t	__padding[4];
};

struct acpi_map_bridge_resp {
	endpoint_t 	m_source; /* message header */
	int		err;
	u32_t		__padding[7];
};

int acpi_init(void);
int acpi_get_irq(unsigned bus, unsigned dev, unsigned pin);
void acpi_map_bridge(unsigned int pbnr, unsigned int dev, unsigned int sbnr);
