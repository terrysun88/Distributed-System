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
#include "rpc.h"


using namespace std;

uint16_t portnum;
char hostname[50];
int server_binder_s,server_client_s;


void encode(char *dest, char *srs) {
   size_t len = strlen(srs)+1;
   memcpy(dest,&len,sizeof(int));
   memcpy(&dest[4],srs, len-1);
   //strcpy(&dest[4],srs);
   dest[len+3]='\0';
}

int decode(char *dest, char *srs) {
   size_t len = 0;
   memcpy(&len,srs,4);
   memcpy(dest,&srs[4],len);
   return len;
}


int rpcInit() {
//open a connection to the binder
   int status;
   char *addr= getenv("BINDER_ADDRESS");
   char *port= getenv("BINDER_PORT");
   struct addrinfo hints;
   struct addrinfo *servinfo; // will point to the results
   memset(&hints, 0, sizeof hints); // make sure the struct is empty
   hints.ai_family = AF_UNSPEC; // don't care IPv4 or IPv6
   hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
   // get ready to connect
   status = getaddrinfo(addr, port, &hints, &servinfo);
   // servinfo now points to a linked list of 1 or more struct addrinfos
   //create socket and connect
   server_binder_s = socket(servinfo->ai_family, servinfo->ai_socktype, 
                     servinfo->ai_protocol);
   status = connect(server_binder_s, servinfo->ai_addr, servinfo->ai_addrlen);
   if (status == -1) cout << "connection to Binder failed: " << errno << endl;
   
//create a connection socket to be used for accepting connections from clients.
   memset(&hints, 0, sizeof hints); // make sure the struct is empty
   memset(&servinfo, 0, sizeof servinfo);
   hints.ai_family = AF_UNSPEC; // don't care IPv4 or IPv6
   hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
   hints.ai_flags = AI_PASSIVE; // fill in my IP for me
   if ((status = getaddrinfo(INADDR_ANY, "0", &hints, &servinfo)) != 0) {
      fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
      exit(1);
   }
   //create socket and bind to a available port
   server_client_s = socket(servinfo->ai_family, servinfo->ai_socktype, 
                            servinfo->ai_protocol);
   status = bind(server_client_s, servinfo->ai_addr, servinfo->ai_addrlen);
   //print hostname and port number
   struct utsname name;
   uname(&name);
   const char* hostname1=name.nodename;
   struct sockaddr_in sin;
   socklen_t leng = sizeof(sin);
   struct hostent* retval=gethostbyname(hostname1);
   printf("SERVER_ADDRESS %s\n",retval->h_name);
   strcpy(hostname,retval->h_name);
   if (getsockname(server_client_s, (struct sockaddr *)&sin, &leng) == -1) 
      perror("getsockname");
   else
      //printf("SERVER_PORT %d\n", ntohs(sin.sin_port));
      portnum=ntohs(sin.sin_port);
   //max 100 clients
   listen(server_client_s,100);
   freeaddrinfo(servinfo);
   return 0;
}

int rpcRegister(char* name, int* argTypes, skeleton f) {
   int bytes_sent;
   string tmp;
   int status,s;
   s=server_binder_s;
   //prepare the message
   
}

int rpcExecute(){
}

int rpcCall(char* name, int* argTypes, void** args) {
//open a connection to the binder
   int status,s;
   char *addr= getenv("BINDER_ADDRESS");
   char *port= getenv("BINDER_PORT");
   struct addrinfo hints;
   struct addrinfo *servinfo; // will point to the results
   memset(&hints, 0, sizeof hints); // make sure the struct is empty
   hints.ai_family = AF_UNSPEC; // don't care IPv4 or IPv6
   hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
   // get ready to connect
   status = getaddrinfo(addr, port, &hints, &servinfo);
   // servinfo now points to a linked list of 1 or more struct addrinfos
   //create socket and connect
   s = socket(servinfo->ai_family, servinfo->ai_socktype, 
                     servinfo->ai_protocol);
   status = connect(s, servinfo->ai_addr, servinfo->ai_addrlen);
   if (status == -1) cout << "connection to Binder failed: " << errno << endl;
   

}

int rpcTerminate(){
}






