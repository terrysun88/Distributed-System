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
#include <unistd.h>
#include <sys/ioctl.h>
#include <vector>
#include <map>
#include <sys/utsname.h>


using namespace std;


// terminal signal
bool terminal_signal = false;


struct serverInfo {
    string server_addr;
    int port;
    
    void create_info(string addr, int p) {
        server_addr = addr;
        port = p;
    }
};

static map <pair<string, int*>, vector<struct serverInfo> > procedure_db;
static vector<struct serverInfo> roundRobin_list;

pair<string, int*> create_key(string name, int* args) {
    pair<string, int*> key;
    key.first = name;
    key.second = args;
    return key;
}

void error(string s) {
    cerr << s << endl;
    exit(1);
}


// checking existing service, if current service exist in the database, return the service.
// if not return a "not_exist" service
pair<string, int*> service_search(pair<string, int*> key) {
    map <pair<string, int*>, vector<struct serverInfo> >::iterator it;
    for (it = procedure_db.begin(); it != procedure_db.end(); ++it) {
        pair<string, int*> tmp = it->first;
        if (sizeof(key.second) == sizeof(tmp.second)) {
            bool same = true;
            for (int i = 0; i < sizeof(key.second); i++) {
                if (key.second[i] != tmp.second[i]) {
                    same = false;
                }
            }
            if (same) {
                return tmp;
            }
        }
    }
    pair<string, int*> not_exist;
    not_exist.first = "not_exist";
    return not_exist;
}

bool server_check(vector<struct serverInfo> service_list, struct serverInfo loc2) {
    bool exist = false;
    for (int i = 0; i < sizeof(service_list); i++) {
        struct serverInfo loc1 = service_list[i];
        if (loc1.server_addr == loc2.server_addr && loc1.port == loc2.port) {
            exist = true;
            break;
        }
    }
    return exist;
}

int addServerService(string hostname, int server_port, string funcName, int* args) {
    // creating key which is formed by  function name and args type
    pair<string, int*> key = create_key(funcName, args);
    
    // create service location to correspounding hostname and port number
    struct serverInfo location;
    location.create_info(hostname, server_port);

    // no any server exist for the request service
    pair<string, int*> exist_key = service_search(key);
    if (exist_key.first == "not_exist") {
        // store location into procedure database
        procedure_db[key].push_back(location);
        
        // store location into roundrobine list
        roundRobin_list.push_back(location);
        
    }
    
    else {
        vector<struct serverInfo> service_list;
        service_list = procedure_db[key];
        if (!server_check(service_list, location)) {
            procedure_db[key].push_back(location);
        }
    }
    
    return 0;
}

void server_register(int len, int fd) {

    bool resigter_success = true;
    char msg_buff[len]; // receive msg from server request
    memset(msg_buff, 0, sizeof(msg_buff));

    int res = (int) recv(fd, msg_buff, sizeof(msg_buff), 0);
    if (res < 0) {
        resigter_success = false;
        cerr << "FAILED to receive msg from server request" << endl;
    }
    
    char hostname[40], funcName[20];
    int server_port, offset = 0;
    // get hostname from msg
    memcpy(hostname, msg_buff, 40);
    offset += 40;
    // get port number from msg
    memcpy(&server_port, &msg_buff[offset], 4);
    offset += 4;
    // get function name from msg
    memcpy(funcName, &msg_buff[offset], 20);
    offset += 20;
    
    // compute the number of arguments
    int args_size = len - 64;
    int args_num = args_size/4;
    
    int args[args_num];
    memcpy(args, &msg_buff[offset], args_size);
    
    res = addServerService(hostname, server_port, funcName, args);

    if (res < 0) {
        resigter_success = false;
    }
    
    if (resigter_success) {
        // send REGISTER_SUCCESS
    }
    
    else {
        // send REGISTER_FAILURE
    }
}

void handleLocRequest() {
    
}

void terminateRequest() {
    
}

int handleAllRequest(int fd) {
    int res = 0;
    char buff[22];
    res = recv(fd, buff, 22, 0);
    if (res < 0) {
        error("FAILED to receive len & MSG_TYPE");
    }
    
    int len;
    char MSG_TYPE[18];
    memcpy(&len, buff, 4);
    cout << len << endl;
    len -= 22; // msg size that include hostname, portNum, funcName and argsType
    memcpy(MSG_TYPE, &buff[4], 18);
    cout << MSG_TYPE << endl;
    if (strcmp(MSG_TYPE, "REGISTER") == 0)
        server_register(len, fd);
    else if (strcmp(MSG_TYPE, "LOC_REQUEST") == 0)
        handleLocRequest();
    else if (strcmp(MSG_TYPE, "TERMINATE") == 0)
        terminateRequest();
    else {
        error("receiving incorrect request!");
    }
    
    return 0;
}

int main() {


    //char *addr= getenv("SERVER_ADDRESS");
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
    printf("BINDER_ADDRESS %s\n",retval->h_name);
    if (getsockname(s, (struct sockaddr *)&sin, &leng) == -1)
        perror("getsockname");
    else
        printf("BINDER_PORT %d\n", ntohs(sin.sin_port));
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
                    status = handleAllRequest(i);
                    if (status < 0) {
                        close(i);
                        FD_CLR(i, &active_set);
                    }
                }
            }
    }

    exit(0);
}



