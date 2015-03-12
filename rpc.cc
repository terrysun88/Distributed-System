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
#include <sstream>
#include <vector>
#include <signal.h>
#include "function.h"

using namespace std;

//Declare Global Variables
uint16_t portnum;
char hostname[50];
int volatile server_binder_s,server_client_s;
int volatile *pool = new int[20];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
vector<Function*> record;
int volatile threads=0;

//return size of each type
int argsizebytes(int argtype) {
   switch (argtype) {
   case 1:
      return sizeof(char);
   case 2:
      return sizeof(short);
   case 3:
      return sizeof(int);
   case 4:
      return sizeof(long);
   case 5:
      return sizeof(double);
   case 6:
      return sizeof(float);
   }
}

int function(Function* fn,char *name1,int typesize1,int *types1, skeleton f1) {
   fn->name = new char[20];
   fn->types = new int[typesize1];
   strcpy(fn->name,name1);
   for (int i = 0; i < typesize1; i++) fn->types[i]=types1[i];
   fn->f=f1;
   return 0;
}

int deletefn() {
   for (int i = 0; i < record.size(); i++) {
      delete [] record[i]->name;
      delete [] record[i]->types;
      delete record[i];
   }
   return 0;
}

skeleton findfunc(char *name,int type) {
   for (int i = 0; i < record.size(); i++) {
      if (strcmp(record[i]->name,name) == 0) return record[i]->f;
   }
   return NULL;
}

/*###################################################################
int rpcInit();
#####################################################################*/

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
   if (status == -1) {
      cout << "connection to Binder failed: " << errno << endl;
      return -2; //for now, need to change later
   }
   
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
   cout << "SERVER_PORT " << portnum << endl;
   freeaddrinfo(servinfo);
   return 0;
}

/*###################################################################
int rpcRegister(char* name, int* argTypes, skeleton f)
#####################################################################*/

int rpcRegister(char* name, int* argTypes, skeleton f) {
   int bytes_sent;
   string tmp;
   int status,s,length;
   s=server_binder_s;
   //prepare the message
   char type[TYPE_LEN];
   int i =0;
   while (argTypes[i] != 0) i++;
   //Assume length: type= 18; server address=40;port=4;name=20
   length=4+TYPE_LEN+ADDRESS_LEN+PORT_LEN+NAME_LEN+i*sizeof(int);
   //compose the message
   /*Message format: 
      length: int
      type: char[]
      address: char[]
      port: int
      name: char[]
      argTypes: int[]
   */
   Function *func = new Function;
   function(func,name,i,argTypes,f);
   record.push_back(func);
   strcpy(type,"REGISTER");
   int offset = 0 ;
   char msg[length];
   memset(msg,0,sizeof(msg));
   memcpy(&msg[offset],&length,4);
   offset+=4;
   memcpy(&msg[offset],type,TYPE_LEN);
   offset+=TYPE_LEN;
   memcpy(&msg[offset],hostname,ADDRESS_LEN);
   offset+=ADDRESS_LEN;
   memcpy(&msg[offset],&portnum,PORT_LEN);
   offset+=PORT_LEN;
   memcpy(&msg[offset],name,NAME_LEN);
   offset+=NAME_LEN;
   memcpy(&msg[offset],argTypes,i*sizeof(int));
   offset+=i*sizeof(int);
   //send the message to binder
   bytes_sent = send(s, msg, length, 0);
   if (bytes_sent == -1) {
      cout << "Registration Failed: Unable to send" << endl;
      return -2; //for now, need to change later
   } 
   //check the repley from binder
   char buf[26];
   int errorcode;
   memset(buf,0,26);
   status = recv(s, buf, 26, 0);
   memset(type,0,TYPE_LEN);
   memcpy(type,&buf[4],TYPE_LEN);
   memcpy(&errorcode,&buf[22],4);
   if (strcmp(type,"REGISTER_SUCCESS") == 0) {
      cout << "REGISTER_SUCCESS" << endl;
      return errorcode;
   } else {
      cout << "REGISTER_FAILURE" << endl;
      return errorcode; //for now, need to change later
   }
}

/*###################################################################
int rpcExecute()
#####################################################################*/

void* Execute (void *val) {
   int s=*(int*)val;
   int status,bytes_sent,length,argsnum;
   char name[NAME_LEN];
   char type[TYPE_LEN];
   //recv from client
   char temp[4];
   status = recv(s, &length, 4, 0);
   if (status == -1) {
      cout << "Server from Client failed: " << errno << endl;
      errno=-2; //for now, need to change later
   }
   char fncall[length-4];
   memset(fncall,0,length-4);
   status = recv(s, fncall, length-4, 0);
   if (status == -1) {
      cout << "Server from Client failed: " << errno << endl;
      errno=-2; //for now, need to change later
   }
   memcpy(type,fncall,TYPE_LEN);
   if (strcmp(type,"EXECUTE") == 0) {
      int offset=TYPE_LEN;
      memcpy(name,&fncall[offset],NAME_LEN);
      offset+=NAME_LEN;
      memcpy(&argsnum,&fncall[offset],4);
      offset+=4;
      int argTypes[argsnum];
      void *args[argsnum];
      int argsize[argsnum];
      memcpy(argTypes,&fncall[offset],argsnum*sizeof(int));
      offset+=argsnum*sizeof(int);
      for (int i =0; i < argsnum; i++) {
         int size = argTypes[i] & 65535;
         int argtype = (argTypes[i]>>16) & 255;
         if (size < 1) size=1;
         int bytesize = size*argsizebytes(argtype);
         argsize[i]=bytesize;
         char *temp = new char[bytesize];
         memcpy(temp,&fncall[offset],bytesize);
         offset+=bytesize;
         args[i]=(void*)temp;
      }
      //invoke the function
      int retval=1; 
      skeleton f = findfunc(name,0);
      if (f != NULL) retval = f(argTypes,args);
      if (retval == 0) {
         //cout << retval << endl;
         //cout << "Evaluate: " << *((int *)(args[0])) << endl;
         //send result back
         offset = 0;
         char result[length];
         memcpy(result,&length,4);
         offset+=4;
         strcpy(type,"EXECUTE_SUCCESS");
         memcpy(&result[offset],type,TYPE_LEN);
         offset+=TYPE_LEN;
         memcpy(&result[offset],name,NAME_LEN);
         offset+=NAME_LEN;
         memcpy(&result[offset],&argsnum,4);
         offset+=4;
         memcpy(&result[offset],argTypes,argsnum*sizeof(int));
         offset+=argsnum*sizeof(int);
         for (int i = 0; i < argsnum; i++) {
            memcpy(&result[offset],args[i],argsize[i]);
            offset+=argsize[i];
         }
         bytes_sent = send(s, result, length, 0);
         if (bytes_sent == -1) {
            cout << "Server to Client: Unable to send" << endl;
            errno = -2; //for now, need to change later
         }
      } else if (retval < 0) {
         char result[length];
         strcpy(type,"EXECUTE_FAILURE");
         //cout << type << endl;
         int errorcode = -2; //for now
         length=4+TYPE_LEN+4;
         offset = 0;
         memcpy(result,&length,4);
         offset+=4;
         memcpy(&result[offset],type,TYPE_LEN);
         offset+=TYPE_LEN;
         memcpy(&result[offset],&errorcode,TYPE_LEN);
         offset+=4;
         bytes_sent = send(s, result, length, 0);
         if (bytes_sent == -1) {
            cout << "Server to Client: Unable to send" << endl;
            errno = -2; //for now, need to change later
         }
      }
   }
   //clean up
   sleep(10); //testing server wait for all runing threads
   pthread_mutex_lock(&mutex);
   int i = 0;
   while (pool[i] != s) i++;
   pool[i] = NULL;
   threads--;
   pthread_cond_signal(&cv);
   pthread_mutex_unlock(&mutex);
   close(s);
}


void* acceptnew (void *flag) {
   struct sockaddr_storage their_addr;
   socklen_t addr_size;
   int status,bytes_sent;
   //support up to 20 clients
   listen(server_client_s,20);
   while (*(bool*)flag) {
      int i = 0;
      int s_new = accept(server_client_s, (struct sockaddr *)&their_addr, &addr_size);
      if (s_new == -1) cout << "Server Accept faileds: " << errno << endl;
      pthread_t thread2;
      pthread_mutex_lock(&mutex);
      while (pool[i] != NULL) i++;
      pool[i]=s_new;
      int ret2 = pthread_create(&thread2, NULL, Execute, (void *)&pool[i]);
      if(ret2) {
        fprintf(stderr,"Error - pthread_create() return code: %d\n",ret2);
        exit(EXIT_FAILURE);
      }
      threads++;
      pthread_mutex_unlock(&mutex);
   }
}

int rpcExecute() {
   pthread_t thread1;
   int ret1;
   bool flag = true;
   ret1 = pthread_create(&thread1, NULL, acceptnew, (void*)&flag);
   if(ret1) {
      fprintf(stderr,"Error - pthread_create() return code: %d\n",ret1);
      exit(EXIT_FAILURE);
   }
   int status,length;
   int s=server_binder_s;
   char temp[4];
   status = recv(s, temp, 4, 0);
   memcpy(&length,temp,4);
   char buf[length-4];
   status = recv(s, buf, length-4, 0);
   if (strcmp(buf,"TERMINATE") == 0) {
      close(server_binder_s);
      bool f=false;
      memcpy(&flag,&f,sizeof(bool));
      pthread_mutex_lock( &mutex);
      while (threads != 0) pthread_cond_wait(&cv, &mutex);
      pthread_cancel(thread1);
      cout << "closing" << endl;
      close(server_client_s);
   } else
      cerr << "Binder sending crazy msg to Server Poi~" << endl;
   deletefn();   
   delete [] pool;
   /*
   char call[20];
   status = recv(s_new, call, 20, 0);
   cout << "message from client: " << call << endl;
   strcpy(call,"Damn it!");
   bytes_sent = send(s_new, call, 20, 0);*/
}

/*###################################################################
int rpcCall(char* name, int* argTypes, void** args)
#####################################################################*/

int rpcCall(char* name, int* argTypes, void** args) {
//open a connection to the binder
   int status,s,byte_sent;
   char *addr= getenv("BINDER_ADDRESS");
   char *port= getenv("BINDER_PORT");
   char server_addr[ADDRESS_LEN];
   int server_port;
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
   if (status == -1) {
      cout << "connection to Binder failed: " << errno << endl;
      return -2; //for now, need to change later
   }
//send LOC_REQUEST to binder
   //prepare the message
   char type[TYPE_LEN];
   strcpy(type,"LOC_REQUEST");
   int i =0;
   while (argTypes[i] != 0) i++;
   //Assume length: type= 18; server address=40;port=4;name=20
   int length=4+TYPE_LEN+NAME_LEN+i*sizeof(int);
   //compose the message
   /*Message format: 
      length: int
      type: char[]
      name: char[]
      argTypes: int[]
   */
   int offset = 0 ;
   char msg[length];
   memset(msg,0,sizeof(msg));
   memcpy(&msg[offset],&length,4);
   offset+=4;
   memcpy(&msg[offset],type,TYPE_LEN);
   offset+=TYPE_LEN;
   memcpy(&msg[offset],name,NAME_LEN);
   offset+=NAME_LEN;
   memcpy(&msg[offset],argTypes,i*sizeof(int));
   offset+=i*sizeof(int);
   //send the message to binder
   int bytes_sent = send(s, msg, length, 0);
   if (bytes_sent == -1) {
      cout << "Client to Binder Failed: Unable to send" << endl;
      return -2; //for now, need to change later
   } 
   //check the repley from binder
   length = 4+TYPE_LEN+ADDRESS_LEN+PORT_LEN;
   char buf[length];
   status = recv(s, buf, length, 0);
   char temp[TYPE_LEN];
   memcpy(temp,&buf[4],TYPE_LEN);
   if (strcmp(temp,"LOC_SUCCESS") == 0) {
      memcpy(server_addr,&buf[4+TYPE_LEN],ADDRESS_LEN);
      memcpy(&server_port,&buf[4+TYPE_LEN+ADDRESS_LEN],4);
   } else {
      int errorcode;
      memcpy(&errorcode,&buf[4+TYPE_LEN],4);
      return errorcode; //for now, need to change later
   }
   //close client's connection to binder
   close(s);
//connect to the server
   //cout << "Address: " << server_addr << endl << "Port: " << server_port << endl;
   //convert int port to string
   char server_portnum[4];
   string tmp;
   ostringstream convert;
   convert << server_port;
   tmp = convert.str();
   strcpy(server_portnum,tmp.c_str());
   memset(&hints, 0, sizeof hints); // make sure the struct is empty
   hints.ai_family = AF_UNSPEC; // don't care IPv4 or IPv6
   hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
   // get ready to connect
   status = getaddrinfo(server_addr, server_portnum, &hints, &servinfo);
   // servinfo now points to a linked list of 1 or more struct addrinfos
   //create socket and connect
   s = socket(servinfo->ai_family, servinfo->ai_socktype, 
                     servinfo->ai_protocol);
   status = connect(s, servinfo->ai_addr, servinfo->ai_addrlen);
   if (status == -1) {
      cout << "connection to Server failed: " << errno << endl;
      return -2; //for now, need to change later
   }
//send request to server
   /*
   char call[]="Hello World";
   bytes_sent = send(s, call, 20, 0);
   status = recv(s, call, 20, 0);
   cout << "message from server: " << call << endl;*/
   //prepare the message
   //calculate the length of message
   strcpy(type,"EXECUTE");
   length = 0;
   int argnum = 0;
   while (argTypes[argnum] != 0) argnum++;
   //msg len+type len+ name len + argsnum;
   length=4+TYPE_LEN+NAME_LEN+4;
   //add size of argTypes
   length+=argnum*sizeof(int);
   //add size of args
   int argsize[argnum];
   for (int i = 0; i < argnum; i++) {
      int size = argTypes[i] & 65535;
      int argtype = (argTypes[i]>>16) & 255;
      if (size < 1) size=1;
      argsize[i]=size*argsizebytes(argtype);
      length+=argsize[i];
   }
   char fncall[length];
   memset(fncall,0,sizeof(fncall));
   //compose the message
   offset=0;
   memcpy(&fncall[offset],&length,4);
   offset+=4;
   memcpy(&fncall[offset],type,TYPE_LEN);
   offset+=TYPE_LEN;
   memcpy(&fncall[offset],name,sizeof(name)+1);
   offset+=NAME_LEN;
   memcpy(&fncall[offset],&argnum,sizeof(int));
   offset+=4;
   memcpy(&fncall[offset],argTypes,argnum*sizeof(int));
   offset+=argnum*sizeof(int);
   for (int i = 0; i < argnum; i++) {
      memcpy(&fncall[offset],args[i],argsize[i]);
      offset+=argsize[i];
   }
   
   //send the message
   bytes_sent = send(s, fncall, length, 0);
   if (bytes_sent == -1) {
      cout << "Client to Server Failed: Unable to send" << endl;
      return -2; //for now, need to change later
   } 
   //waiting for the repley
   memset(fncall,0,length);
   status = recv(s, fncall, length, 0);
   memcpy(type,&fncall[4],TYPE_LEN);
   if (strcmp(type,"EXECUTE_SUCCESS") == 0) {
      cout << "EXECUTE_SUCCESS" << endl;
      offset=4+TYPE_LEN+NAME_LEN+4+argnum*sizeof(int);
      for (int i = 0; i < argnum; i++) {
         if (((argTypes[i]>>ARG_OUTPUT) & 1) == 1) {
            memcpy(args[i],&fncall[offset],argsize[i]);
            break;
         }
         offset+=argsize[i];
      }
   } else {
      cout << "EXECUTE_FAILURE" << endl;
      int errorcode;
      memcpy(&errorcode,&fncall[4+TYPE_LEN],4);
      return errorcode;
   }
   close(s);
   return 0;
}

/*###################################################################
int rpcTerminate()
#####################################################################*/

int rpcTerminate() {
   //open a connection to the binder
   int status,s,byte_sent;
   char *addr= getenv("BINDER_ADDRESS");
   char *port= getenv("BINDER_PORT");
   char server_addr[ADDRESS_LEN];
   int server_port;
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
   if (status == -1) {
      cout << "connection to Binder failed: " << errno << endl;
      return -2; //for now, need to change later
   }
//send terminate to binder
   int length=4+TYPE_LEN;
   char msg[length];
   char type[TYPE_LEN]="TERMINATE";
   memset(msg,0,sizeof(msg));
   memcpy(&msg,&length,4);
   memcpy(&msg[4],type,TYPE_LEN);
   int bytes_sent = send(s, msg, length, 0);
   close(s);
   return 0;
}







