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


using namespace std;

// terminal signal
bool terminal_signal = false;

void error(string s) {
    cerr << s << endl;
    exit(1);
}

void server_register() {
    
}

void handleLocRequest() {
    
}

void terminateRequest() {
    
}

void handleAllRequest(int fd) {
    char buff[22];
    memset(&buff, 0, sizeof(buff));
    int res = (int)recv(fd, buff, sizeof(buff), 0);
    if (res < 0) {
        error("FAILED to receive len & MSG_TYPE");
    }
    
    int len;
    string MSG_TYPE;
    memcpy(&len, &buff, 4);
    memcpy(&MSG_TYPE, &buff[4], 18);
    
    if (MSG_TYPE == "REGISTER")
        server_register();
    else if (MSG_TYPE == "local")
        handleLocRequest();
    else if (MSG_TYPE == "")
        error(""); //need to modified
    else if (MSG_TYPE == "TERMINATE")
        terminateRequest();
    else {
        error("receiving incorrect request!");
    }
}

int main() {
    int localSocket, port, res;
    struct sockaddr_in binder_addr, cur_addr;
    char binder_address[256];
    fd_set workfds, readfds;
    
    localSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (localSocket < 0) {
        error("Opening socket Failed!");
    }
    
    bzero(&binder_addr, sizeof(binder_addr));
    
    binder_addr.sin_family = AF_INET;
    binder_addr.sin_addr.s_addr = INADDR_ANY;
    binder_addr.sin_port = 0;
    
    if (bind(localSocket, (struct sockaddr *)&binder_addr, sizeof(binder_addr)) < 0) {
        error("Failed to bind server");
    }

    socklen_t port_len = sizeof(cur_addr);
    
    if (getsockname(localSocket, (struct sockaddr *)&cur_addr, &port_len) < 0) {
        error("Failed to get server address");
    }
    
    gethostname(binder_address, 1023);
    port = ntohs(cur_addr.sin_port);
    printf("BINDER_ADDRESS %s\n", binder_address);
    printf("BINDER_PORT %d\n", port);
    
    listen(localSocket, 5);
    
    int fd_size = localSocket;
    FD_ZERO(&readfds);
    FD_SET(localSocket, &readfds);
    
    while (!terminal_signal) {
        int nread;
        memcpy(&workfds, &readfds, sizeof(readfds));
        
        res = select(fd_size + 1, &workfds, NULL, NULL, NULL);
        if (res < 0) {
            error("selection Failed");
        }
        
        for (int fd = 0; fd < fd_size; fd++) {
            if (FD_ISSET(localSocket, &workfds)) {
                if (fd == localSocket) {
                    socklen_t client_size;
                    int connectionSocket;
                    struct sockaddr_in client_addr;
                    client_size = sizeof(client_addr);
                    connectionSocket = accept(localSocket, (struct sockaddr *)&client_addr, &client_size);
                    if (connectionSocket < 0) {
                        error("Accepting request failed");
                    }
                    
                    FD_SET(connectionSocket, &readfds);
                    fd_size += 1;
                }
                else {
                    ioctl(nread, FIONREAD, &readfds);
                    if (nread == 0) {
                        close(fd);
                        FD_CLR(fd, &readfds);
                    }
                    else {
                        handleAllRequest(fd);
                    }
                }
            }
        }
        
    }
    
    close(localSocket);
    exit(0);
}



