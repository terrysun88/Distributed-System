#include <iostream>
#include <string>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/utsname.h>

using namespace std;
int MAX_SIZE = 2147483;
char server_addr[40];
int server_port;
   
int recv_repley(int fd) {
   char *buf = new char[MAX_SIZE];
   int status,bytes_sent;
   memset(buf,0,sizeof(buf));
   char temp[4];
   status = recv(fd, temp, 4, 0);
   if (status == -1) {
      //cout << "recv_repley: " << errno << endl;
      return -1;
   } else if (status == 0) return -1;
   int counter = 0;
   int len;
   memcpy(&len,temp,4);
   cout << "length: " << len << endl;
   cout << "actual: " << status << endl;
   status = recv(fd, buf, len-4, 0);
   char type[18];
   memcpy(type,buf,18);
   printf("type: %s\n",type);
   if (strcmp(type,"REGISTER") == 0) {
      strcpy(type,"REGISTER_SUCCESS");
      memcpy(server_addr,&buf[18],64);
      memcpy(&server_port,&buf[82],4);
      int length=26;
      char msg[length];
      memset(msg,0,sizeof(msg));
      memcpy(msg,&length,4);
      memcpy(&msg[4],type,18);
      int code=100;
      memcpy(&msg[22],&code,4);
      bytes_sent = send(fd, msg, length, 0);
   } else if (strcmp(type,"LOC_REQUEST") == 0) {
      strcpy(type,"LOC_SUCCESS");
      int length=4+18+64+4;
      char msg[length];
      memset(msg,0,sizeof(msg));
      memcpy(msg,&length,4);
      memcpy(&msg[4],type,18);
      memcpy(&msg[22],server_addr,64);
      memcpy(&msg[86],&server_port,4);
      bytes_sent = send(fd, msg, length, 0);
   }
}

int main() {
   //char *addr= getenv("SERVER_ADDRESS");
   char *port= getenv("SERVER_PORT");
   //code partly from http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html
   int status;
   int s,new_sfd;
   fd_set active_set, read_set;
   struct sockaddr_storage their_addr;
   socklen_t addr_size;
   struct addrinfo hints;
   struct addrinfo *servinfo; // will point to the results
   memset(&hints, 0, sizeof hints); // make sure the struct is empty
   hints.ai_family = AF_UNSPEC; // don't care IPv4 or IPv6
   hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
   hints.ai_flags = AI_PASSIVE; // fill in my IP for me
   if ((status = getaddrinfo(INADDR_ANY, "0", &hints, &servinfo)) != 0) {
      fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
      exit(1);
   }
   //create socket and bind to a available port
   s = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
   status = bind(s, servinfo->ai_addr, servinfo->ai_addrlen);
   //print hostname and port number
   struct utsname name;
   uname(&name);
   const char* hostname=name.nodename;
   //printf("SERVER_ADDRESS %s\n",hostname);
   struct sockaddr_in sin;
   socklen_t leng = sizeof(sin);
   struct hostent* retval=gethostbyname(hostname);
   printf("SERVER_ADDRESS %s\n",retval->h_name);
   if (getsockname(s, (struct sockaddr *)&sin, &leng) == -1) 
      perror("getsockname");
   else
      printf("SERVER_PORT %d\n", ntohs(sin.sin_port));
   //max 5 clients
   listen(s,5);
   freeaddrinfo(servinfo);
   //init select()
   FD_ZERO(&active_set);
   FD_SET(s, &active_set);
   for (;;) {
      //assign backup to ready queue
      read_set = active_set;
      //modify read_set drop socket without new message
      status = select (FD_SETSIZE, &read_set, NULL, NULL, NULL);
      if (status < 0) {
         perror ("select() failed");
         exit(1);
      }
      for (int i = 0; i < FD_SETSIZE; i++)
         if (FD_ISSET(i, &read_set)) {
            if (i == s) {
                // New Connection
               addr_size = sizeof their_addr;
               new_sfd = accept(s, (struct sockaddr *)&their_addr, &addr_size);
               if (new_sfd < 0) {
                  perror("accept() failed");
                  exit(1);
               }
               FD_SET(new_sfd, &active_set);
            } else {
                // Data arriving on connected socket.
               status = recv_repley(i);
               if (status < 0) {
                  close(i);
                  FD_CLR(i, &active_set);
               }
            }
         }
   }
   //destructor
   return 0;
}
