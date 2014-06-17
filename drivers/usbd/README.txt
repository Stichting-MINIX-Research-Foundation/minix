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
- Driver assumes that there is no preemption for DDEKit threading
- URBs are enqueued in DDEKit but not in USBD itself
- DDEKit way of handling interface numbers is not explicitly defined, bitmask
  formatting for it, is therefore hardcoded into USBD
- Waiting for USB0 clock to leave IDLEST.Disable state, by nanosleep, was
  removed, as this should be implemented for all clocks in clkconf_set
- Control transfers can only be performed with EP0
