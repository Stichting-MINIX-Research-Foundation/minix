/*
 * Test name: test06_cli.c
 *
 * Objective: Test a simple TCP client
 *
 * Description: Implements a simple echo client using the TCP protocol. First
 * it waits until it is possible to write (which is always).
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/select.h>
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

#define PORT 6060L

int tcp_connect(char *host, long port)
{
  /* creates a tcp connection with specified host and port */
  char *tcp_device;
  struct hostent *hp;
  int netfd;
  nwio_tcpcl_t tcpcopt;
  nwio_tcpconf_t tcpconf;
  ipaddr_t dirhost;
  int tries;
  int result;
  
  /* get host address */
  if ((hp = gethostbyname(host)) == (struct hostent*) NULL) 
  {
    fprintf(stderr,"Unknown host\n");
    return(-1);
  }
  memcpy((char *)&dirhost, (char *)hp->h_addr, hp->h_length);
  
  /* Get default TCP device */
  if (( tcp_device = getenv("TCP_DEVICE") ) == NULL) 
    tcp_device = TCP_DEVICE;
  
  /* Establish TCP connection */ 
  if ((netfd = open(tcp_device, O_RDWR)) < 0) 
  {
    fprintf(stderr,"Error opening TCP device\n");
    return -1;
  }
  
  /* Configure TCP connection */
  tcpconf.nwtc_flags=NWTC_LP_SEL | NWTC_SET_RA | NWTC_SET_RP;
  tcpconf.nwtc_remaddr = dirhost;
  tcpconf.nwtc_remport = (tcpport_t) htons(port);
  
  if ((result = ioctl(netfd, NWIOSTCPCONF, &tcpconf) ) <0) 
  {
    fprintf(stderr, "Error establishing communication\n");
    printf("Error:  %d\n",result);
    close(netfd);
    return -1;
  }
  
  /* Get configuration for TCP comm */ 
  if ((result = ioctl(netfd, NWIOGTCPCONF, &tcpconf) ) < 0) 
  {
    fprintf(stderr,"Error getting configuration\n");
    printf("Error: %d\n", result);
    close(netfd);
    return -1;
  }

  /* Establish connection */
  tcpcopt.nwtcl_flags = 0;
  tries = 0; 
  while (tries < 10) {
  	if ( (result = ioctl(netfd, NWIOTCPCONN, &tcpcopt)) < 0 ) {
  		if (errno != EAGAIN)
  		{
  			fprintf(stderr, "Server is not listening\n");
  			close(netfd);
  			return(-1);
  		}
  		fprintf(stderr, "Unable to connect\n");
  		sleep(1);
  		tries++;
  	}
  	else
  		break;	/* Connection */
 }
 /* Check result value */
 if (result < 0) {
 	fprintf(stderr, "Error connecting\n");
 	fprintf(stderr, "Error: %d\n", result);
 	printf("Number of tries: %d\n", tries);
 	printf("Error: %d\n", errno);
 	close(netfd);
 	return -1;
 }
 return netfd;
}

int main(int argc,char *argv[]) {
  int fd;
  ssize_t data_read;
  char send_buf[1024];
  char recv_buf[1024];
  fd_set fds_write;
  int ret;

  /* Check parameters */
  if (argc !=2) {
    fprintf(stderr,"Usage: %s host\n", argv[0]);
    exit(-1);
  }

  if ((fd = tcp_connect(argv[1], PORT) ) < 0) 
    exit(-1);	
  printf("Connected to server\n");
  /* init fd_set */
  FD_ZERO(&fds_write);
  FD_SET(fd, &fds_write);
  while (1) 
  {
    /* Wait until it is possible to write with select */
    ret = select(4, NULL, &fds_write, NULL, NULL);
    if (ret < 0) {
    	fprintf(stderr, "Error on select waiting for write: %d\n", errno);
    	exit(-1);
    }
    if (!FD_ISSET(fd, &fds_write)) {
    	fprintf(stderr, "Error: The net connection is not ready for writing (?)\n"); 
    	exit(-1);
    }
    
    /* Get a string and send it */
    printf("Ready to write...\n");
    printf("Send data: ");
    gets(send_buf);
    write(fd, &send_buf, strlen(send_buf)+1);
  
    /* If data sent is exit then break */
    if (!strcmp(send_buf,"exit")) 
      break;
  		
    /* Get server response */
    data_read = read(fd, &recv_buf, 1024);
    printf("Received: %s\n\n", recv_buf);
  }
  
  /* Close TCP communication */
  close(fd);
}
