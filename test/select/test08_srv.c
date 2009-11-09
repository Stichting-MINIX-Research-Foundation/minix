/*
 * Test name: test08_srv.c
 *
 * Objective: Test a simple TCP server waiting for urgent data.
 *
 * Description: Implements a echo TCP server as in test06_srv but waits
 * for urgent data, using select on exception.
 * 
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
  nwio_tcpcl_t tcpcl;
  nwio_tcpopt_t tcpopt;                     
                            
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

  /* Get communication conf*/
  if ((ioctl(netfd, NWIOGTCPCONF, &tcpconf)) < 0) {
    fprintf(stderr, "Error getting configuration\n");
    close(netfd);
    return -1;
   }                

   /* Set comm options */
  tcpopt.nwto_flags = NWTO_RCV_URG;              

  if ((ioctl(netfd, NWIOSTCPOPT, &tcpopt))<0) 
  {
    fprintf(stderr,"Error configuring the connection\n");
    close(netfd);
    return -1;
  } 
	                        
  /* Get communication opt*/
  if ((ioctl(netfd, NWIOGTCPOPT, &tcpopt)) < 0) {
    fprintf(stderr, "Error getting options\n");
    close(netfd);
    return -1;
   }                

  /* Set conn options */
  tcpcl.nwtcl_flags = 0;
  printf("Waiting for connections...\n");
  while ((ioctl(netfd, NWIOTCPLISTEN, &tcpcl)) == -1) 
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
  fd_set fds_excep;

  if ((fd = listen(PORT)) < 0) {
    	exit(-1);	
  }
  printf("Waiting for messages on port: %ld\n", PORT);
  fflush(stdout);
  /* Initialize fd_set */
  FD_ZERO(&fds_excep);
  FD_SET(fd, &fds_excep);

  while (1) 
  {
    /* Wait for data available to be read (no timeout) */
  	 
    ret = select(4, NULL, NULL, &fds_excep, NULL);
    if (ret < 0) {
    	fprintf(stderr, "Error on select: %d\n", errno);
    	exit(-1);
    }
    if (!FD_ISSET(fd, &fds_excep)) {
    	printf("Error: no URG data received (?)\n");
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
