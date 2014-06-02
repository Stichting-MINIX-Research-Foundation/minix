-------------------------------------------------------------------------------
*                           INFORMATION:                                      *
-------------------------------------------------------------------------------
README file for "USBD" USB host controller driver.

created march-june 2014, JPEmbedded (info@jpembedded.eu)

-------------------------------------------------------------------------------
*                           KNOWN LIMITATIONS:                                *
-------------------------------------------------------------------------------
- Only first configuration can be selected for attached device
- Only one device can be handled at a time, no hub functionality
- DDEKit does not implement resource deallocation for corresponding thread
  creation (see ddekit_thread_terminate, ddekit_thread_create) thus resources
  are spilled
- Driver assumes that there is no preemption for DDEKit threading
- URBs are enqueued in DDEKit but not in USBD itself
- DDEKit way of handling interface numbers is not explicitly defined, bitmask
  formatting for it, is therefore hardcoded into USBD
