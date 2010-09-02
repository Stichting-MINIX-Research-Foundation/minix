#include <minix/types.h>
#include <minix/ipc.h>

#define ACPI_REQ_GET_IRQ	1

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
	u32_t			dev;
	u32_t			pin;
	u32_t			__padding[5];
};

/* response from acpi to acpi_get_irq_req */
struct acpi_get_irq_resp {
	endpoint_t 	m_source; /* message header */
	i32_t		irq;
	u32_t		__padding[7];
};
