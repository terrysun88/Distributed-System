#ifndef __FUNCTION_H__
#define __FUNCTION_H__


//Length Assumption
#define TYPE_LEN 18
#define ADDRESS_LEN 64
#define PORT_LEN 4
#define NAME_LEN 20

struct Function {
   char *name;
   int typesize;
   int *types;
   skeleton f;
};

struct serverInfo {
    std::string server_addr;
    int port;
    int fd;
    
    void create_info(std::string addr, int p, int s) {
        server_addr = addr;
        port = p;
        fd = s;
    }
};

struct cacheServerInfo {
    std::string server_addr;
    int port;
    
    void create_info(std::string addr, int p) {
        server_addr = addr;
        port = p;
    }
};

#endif
