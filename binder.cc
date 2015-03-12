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
#include "function.h"


using namespace std;


// terminal signal
bool terminal_signal = false;
int register_error_no = 0;
int reasonCode = 0;

struct serverInfo {
    string server_addr;
    int port;
    int fd;
    
    void create_info(string addr, int p, int s) {
        server_addr = addr;
        port = p;
        fd = s;
    }
};

map <pair<string, vector<int> >, vector<struct serverInfo> > procedure_db;
vector<struct serverInfo> roundRobin_list;

pair<string, vector<int> > create_key(string name, vector<int> args) {
    pair<string, vector<int> > key;
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
pair<string, vector<int> > service_search(pair<string, vector<int> > key) {
    map <pair<string, vector<int> >, vector<struct serverInfo> >::iterator it;
    for (it = procedure_db.begin(); it != procedure_db.end(); ++it) {
        pair<string, vector<int> > tmp = it->first;
        if (key.first == tmp.first && key.second.size() == tmp.second.size()) {
            bool same = true;
            for (int i = 0; i < key.second.size(); i++) {
                if (key.second[i] >> 16 != tmp.second[i] >> 16) {
                    same = false;
                    break;
                }
                else {
                    short len1 = key.second[i];
                    short len2 = tmp.second[i];
                    if ((len1 == 0 && len2 > 0) || (len1 > 0 && len2 == 0)) {
                        same = false;
                        break;
                    }

                }
            }
            if (same) {
                return tmp;
            }
        }
    }
    pair<string, vector<int> > not_exist;
    not_exist.first = "not_exist";
    return not_exist;
}

bool server_check(vector<struct serverInfo> service_list, struct serverInfo loc2) {
    bool exist = false;
    for (int i = 0; i < service_list.size(); i++) {
        struct serverInfo loc1 = service_list[i];
        if (loc1.server_addr == loc2.server_addr && loc1.port == loc2.port) {
            exist = true;
            break;
        }
    }
    return exist;
}

int addServerService(string hostname, int server_port, string funcName, vector<int> args, int fd) {
    try {
        // creating key which is formed by  function name and args type
        pair<string, vector<int> > key = create_key(funcName, args);
    
        // create service location to correspounding hostname and port number
        struct serverInfo location;
        location.create_info(hostname, server_port, fd);

        // check if server location exist in the roundroubin; if not, store location into roundrobine list
        if (!server_check(roundRobin_list, location)) {
            roundRobin_list.push_back(location);
        }
        
        // no any server exist for the request service
        pair<string, vector<int> > exist_key = service_search(key);
        if (exist_key.first == "not_exist") {
            // store location into procedure database
            procedure_db[key].push_back(location);
        
            register_error_no = 0;
            return 0;
        
        }
    
        else {
            vector<struct serverInfo> service_list;
            service_list = procedure_db[exist_key];
            if (!server_check(service_list, location)) {
                procedure_db[key].push_back(location);
                register_error_no = 0;
                return 0;
            }
            else {
                register_error_no = 1;
                return 0;
            }
        }
    } catch (exception e) {
        register_error_no = -1;
        return -1;
    }
    
}

struct serverInfo loc_search(vector<struct serverInfo> locs) {
    struct serverInfo server_location;
    for (int i = 0; i < roundRobin_list.size(); i++){
        struct serverInfo next_server = roundRobin_list[i];
        for (int j = 0; j < locs.size(); j++) {
            if(locs[i].server_addr == next_server.server_addr && locs[i].port == next_server.port) {
                server_location = next_server;
                break;
            }
        }
        roundRobin_list.erase(roundRobin_list.begin() + i + 1);
        roundRobin_list.push_back(next_server);
    }
    return server_location;
}

void server_register(int len, int fd) {
    register_error_no = -4; // other errors
    bool register_success = true;
    try {
        char msg_buff[len]; // receive msg from server request
        int res = (int) recv(fd, msg_buff, sizeof(msg_buff), 0);
        if (res < 0) {
            register_success = false;
            register_error_no = -2;
            cerr << "FAILED to receive msg from server request" << endl;
        }
    
        char hostname[ADDRESS_LEN], funcName[NAME_LEN];
        int server_port, offset = 0;
        // get hostname from msg
        memcpy(hostname, msg_buff, sizeof(hostname));
        offset += ADDRESS_LEN;
        // get port number from msg
        memcpy(&server_port, &msg_buff[offset], sizeof(server_port));
        offset += PORT_LEN;
        // get function name from msg
        memcpy(funcName, &msg_buff[offset], sizeof(funcName));
        offset += NAME_LEN;
    
        // compute the number of arguments
        int args_size = len - ADDRESS_LEN - PORT_LEN - NAME_LEN;
        int args_num = args_size/4;
    
        int args[args_num];
        memcpy(args, &msg_buff[offset], args_size);
        vector<int> args_list;
        for (int i = 0; i < args_num; i++) {
            if (args[i] < 1 || args[i] > 6) {
                register_error_no = -3; //invalid args type
                throw exception();
            }
            args_list.push_back(args[i]);
        }
        res = addServerService(hostname, server_port, funcName, args_list, fd);

        if (res < 0) {
        register_success = false;
        }
    } catch (exception e) {
        register_success = false;
    }
    
    if (register_success) {
        int length = 4 + 18 + 4;
        char success[18] = "REGISTER_SUCCESS";
        char msg[length];
        memcpy(msg, &length, sizeof(length));
        memcpy(&msg[4], success, sizeof(success));
        memcpy(&msg[22], &register_error_no, sizeof(register_error_no));
        int status = send(fd, msg, sizeof(msg), 0);
        if (status < 0) {
            cerr << "send back register response failed" << endl;
        }
    }
    
    else {
        int length = 4 + TYPE_LEN + 4;
        char success[TYPE_LEN] = "REGISTER_FAILURE";
        char msg[length];
        memcpy(msg, &length, sizeof(length));
        memcpy(&msg[4], success, sizeof(success));
        memcpy(&msg[22], &register_error_no, sizeof(register_error_no));
        int status = send(fd, msg, sizeof(msg), 0);
        if (status < 0) {
            cerr << "send back register response failed" << endl;
        }
    }
}

void handleLocRequest(int len, int fd) {
    bool loc_success = true;
    try {
        char msgBuff[len];
        int args_size = len - 20;
        int args_num = args_size / 4;
        int res = recv(fd, msgBuff, sizeof(msgBuff), 0);
        
        if (res < 0) {
            loc_success = false;
            // reason Code
            cerr << "Failed to receive message from client" << endl;
        }
        // collect client function name from message
        char funcName[NAME_LEN];
        memcpy(funcName, msgBuff, sizeof(funcName));
        
        // collect args type from message
        int args[args_num];
        memcpy(args, &msgBuff[20], args_size);
        vector<int> args_list;
        for (int i = 0; i < args_num; i++) {
            args_list.push_back(args[i]);
        }
        // create requesting key by client
        pair<string, vector<int> > key = create_key(funcName, args_list);
        
        // figure out if the key exist in procedure_db
        pair<string, vector<int> > db_key = service_search(key);
        
        if (db_key.first == "not_exist") {
            loc_success = false;
            reasonCode = -1; // no server register required service
        }
        
        else {
            vector<struct serverInfo> server_loc = procedure_db[db_key];
            struct serverInfo location = loc_search(server_loc);
            string hostname = location.server_addr;
            char hostIP[ADDRESS_LEN];
            strcpy(hostIP, hostname.c_str());
            int port = location.port;
            char success_msg[TYPE_LEN] = "LOC_SUCCESS";
            
            int length = 4 + TYPE_LEN + ADDRESS_LEN + PORT_LEN;
            char msg[length];
            memset(msg, 0, sizeof(msg));
            memcpy(msg, &length, 4);
            int offset = 4;
            memcpy(&msg[offset], success_msg, TYPE_LEN);
            offset += TYPE_LEN;
            memcpy(&msg[offset], hostIP, ADDRESS_LEN);
            offset += ADDRESS_LEN;
            memcpy(&msg[offset], &port, PORT_LEN);
            int status = send(fd, msg, sizeof(msg), 0);
            if (status < 0) {
                loc_success = false;
                cerr << "failed to send response to client" << endl;
                reasonCode = -3; // error occurs while send response to client
            }
            
        }
        
        
    } catch (exception e) {
        loc_success = false;
        cerr << "error ocurs while handle local request" << endl;
        reasonCode = -2; // error occurs while handle local request
    }
    
    if (!loc_success) {
        char failure_msg[TYPE_LEN] = "LOC_FAILURE";
        int length = 4 + TYPE_LEN + 4;
        char msg[length];
        memset(msg, 0, sizeof(msg));
        memcpy(msg, &length, 4);
        memcpy(&msg[4], failure_msg, TYPE_LEN);
        memcpy(&msg[22], &reasonCode, 4);
        int status = send(fd, msg, sizeof(msg), 0);
        if (status < 0) {
            cerr << "Fail to send failure message" << endl;
        }
    }
}

void remove_crashed_server(string hostname, int port, int fd, fd_set *activitys) {
    
}

void terminateRequest() {
    int length = 4 + TYPE_LEN;
    char terminate_msg[TYPE_LEN] = "TERMINATE";
    char msg[length];
    memset(msg, 0, sizeof(msg));
    memcpy(msg, &length, 4);
    memcpy(&msg[4], terminate_msg, TYPE_LEN);

    // send terminate message to all registered server
    for (int i = 0; i < roundRobin_list.size(); i++) {
        int fd = roundRobin_list[i].fd;
        int status = send(fd, msg, sizeof(msg), 0);
        if (status < 0) {
            cerr << "Failed to send terminate message to Server: ";
            cerr << roundRobin_list[i].server_addr << " Port: ";
            cerr << roundRobin_list[i].port << endl;
        }
    }
    terminal_signal = true;
    
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
    len -= 22; // msg size that include hostname, portNum, funcName and argsType
    memcpy(MSG_TYPE, &buff[4], 18);
    if (strcmp(MSG_TYPE, "REGISTER") == 0)
        server_register(len, fd);
    else if (strcmp(MSG_TYPE, "LOC_REQUEST") == 0)
        handleLocRequest(len, fd);
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
    listen(s,20);
    freeaddrinfo(servinfo);
    //init select()
    FD_ZERO(&active_set);
    FD_SET(s, &active_set);
    while (!terminal_signal) {
        int nread;
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
                    ioctl(i, FIONREAD, &nread);
                    
                    if (nread == 0) {
                        close(i);
                        FD_CLR(i, &active_set);
                    }
                    else {
                        handleAllRequest(i);
                    }

                }
            }
    }
    close(s);
    exit(0);
}



