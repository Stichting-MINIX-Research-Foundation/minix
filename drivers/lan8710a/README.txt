--------------------------------------------------------------------------------
*                           INFORMATION:                                       *
--------------------------------------------------------------------------------
README file for the LAN8710A ethernet board driver for BeagleBone Rev. A6a

created July 2013, JPEmbedded (info@jpembedded.eu)

--------------------------------------------------------------------------------
*                           INSTALLATION:                                      *
--------------------------------------------------------------------------------
To install LAN8710A for BeagleBone under MINIX you have to edit /etc/inet.conf
by adding line:
eth0 lan8710a 0 { default; };
and changing:
psip0 { default; };
to:
psip1;
Restart the system and the driver should work.

--------------------------------------------------------------------------------
*                                 TESTS:                                       *
--------------------------------------------------------------------------------
Driver was tested using various tools, i. e.
* fetch - downloading file from the Internet and also local server. Every file
          downloaded well, but speed was about 50-200 kB/s.
* ftp - downloading and uploading 20 MB file completed.
* ping - checking connection between BeagleBone and computer passed using standard
         settings, when we set ping requests interval to 200 ms it also passed. But with
         20 ms and 2 ms driver dropped some packets (20 ms - about 20% loss, 2 ms - 50%
         loss).
* udpstat, hostaddr, dhcpd, ifconfig, arp gave proper results.
Tests passed, so driver meets the requirements of ethernet driver.

--------------------------------------------------------------------------------
*                                LIMITATION:                                   *
--------------------------------------------------------------------------------
Download speed:  50-200 kB/s
Low bandwidth is probably caused by memory copy functions. Standard Linux driver 
copies packets data directly to destination buffer using DMA. Minix driver needs
to do a safe copy (sys_safecopyfrom and sys_safecopyto) from local buffer to the 
system buffer. This operation slows down the whole driver.
