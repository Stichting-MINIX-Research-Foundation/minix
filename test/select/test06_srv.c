/*
 * Test name: test06_srv.c
 *
 * Objective: Test a simple TCP server
 *
 * Description: Implements a simple echo server using the TCP protocol. Instead
 * of blocking on read(), it performs a select call first blocking there 
 * until there is data to be read
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <net/netlib.h>
#include <net/gen/netdb.h>
#include <net/gen/in.h>
#include <net/gen/tcp.h>
#include <net/gen/tcp_io.h>
#include <net/hton.h>
#include <net/gen/inet.h>

#define PORT 6060L

int listen(long port) {

  char *tcp_device;
  int netfd;
  nwio_tcpconf_t tcpconf;
  nwio_tcpcl_t tcplistenopt;
                            
  /* Get default UDP device */
  if ((tcp_device = getenv("TCP_DEVICE")) == NULL) 
    tcp_device = TCP_DEVICE;
                                
  /* Open TCP connection */ 
  if ((netfd = open(tcp_device, O_RDWR)) < 0) 
  {
    fprintf(stderr,"Error opening TCP connection\n");
    return -1;
  }               
                                
  /* Configure TCP connection */ 
  tcpconf.nwtc_flags = NWTC_LP_SET | NWTC_UNSET_RA | NWTC_UNSET_RP;
  tcpconf.nwtc_locport = (tcpport_t) htons(port);
	                        
  if ((ioctl(netfd, NWIOSTCPCONF, &tcpconf))<0) 
  {
    fprintf(stderr,"Error configuring the connection\n");
    close(netfd);
    return -1;
  }               

  /* Get communication options */
  if ((ioctl(netfd, NWIOGTCPCONF, &tcpconf)) < 0) {
    fprintf(stderr, "Error getting configuration\n");
    close(netfd);
    return -1;
   }                
	                        
  /* Set conf options */
  tcplistenopt.nwtcl_flags = 0;
  printf("Waiting for connections...\n");
  while ((ioctl(netfd, NWIOTCPLISTEN, &tcplistenopt)) == -1) 
  {
    if (errno != EAGAIN)
    {
    	fprintf(stderr,"Unable to listen for connections\n");
    	close(netfd);
    }
    sleep(-1);
  }               
  return netfd;
}

int main(int argc,char *argv[]) {
  int fd;
  ssize_t data_read;
  char buffer[1024];
  int ret;
  fd_set fds_read;

  if ((fd = listen(PORT)) < 0) {
    	exit(-1);	
  }
  printf("Waiting for messages on port: %ld\n", PORT);
  fflush(stdout);
  /* Initialize fd_set */
  FD_ZERO(&fds_read);
  FD_SET(fd, &fds_read);

  while (1) 
  {
    /* Wait for data available to be read (no timeout) */
   
    ret = select(4, &fds_read, NULL, NULL, NULL);
    if (ret < 0) {
    	fprintf(stderr, "Error on select: %d\n", errno);
    	exit(-1);
    }
    if (!FD_ISSET(fd, &fds_read)) {
    	printf("Error: network fd is not ready (?)\n");
    	exit(-1);
    }
    
    printf("Ready to receive...\n");
    /* Read received data */
    data_read = read(fd, &buffer, 1024);
    printf("Received data: %s\n", buffer);
                                
    /* Can exit if the received string == exit */
    if (!strcmp(buffer,"exit")) 
      break; 

    /* Write the same back  */
    write(fd, &buffer, data_read);             
  }               
  printf("Connection finished\n");
  close(fd);
}                       
