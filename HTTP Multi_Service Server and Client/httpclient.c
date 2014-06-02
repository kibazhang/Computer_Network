#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>

#define BUFFSIZE 1024

int open_clientfd(char * hostname, int port);

int main (int argc, char **argv)
{
  int clientfd, port;
  char *host, buf[BUFFSIZE], *path;
  host = argv[1];
  port = atoi(argv[2]);
  path = argv[3];
  clientfd = open_clientfd(host,port);
  strcpy(buf,"GET ");
  strcat(buf, path);
  strcat(buf, " HTTP/1.0\r\n\r\n");
  write(clientfd, buf, strlen(buf));
  int rs = 0;
  do
    {
      bzero(buf,BUFFSIZE);
      rs = read(clientfd,buf,BUFFSIZE);
      fputs(buf,stdout);
    } while (rs > 0);
  close(clientfd);
  exit(0);
}

int open_clientfd(char *hostname, int port) 
{ 
  int clientfd; 
  struct hostent *hp; 
  struct sockaddr_in serveraddr; 
  if ((clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    return -1; /* check errno for cause of error */ 
  /* Fill in the server's IP address and port */ 
  if ((hp = gethostbyname(hostname)) == NULL) 
    return -2; /* check h_errno for cause of error */ 
  bzero((char *) &serveraddr, sizeof(serveraddr)); 
  serveraddr.sin_family = AF_INET; 
  bcopy((char *)hp->h_addr, 
	(char *)&serveraddr.sin_addr.s_addr, hp->h_length); 
  serveraddr.sin_port = htons(port); 
  /* Establish a connection with the server */ 
  if (connect(clientfd,(struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0) 
    return -1; 
  return clientfd; 
}       

