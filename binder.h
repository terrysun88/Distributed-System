//
//  binder.h
//
//  Created by Terry sun on 2015-03-13.
//  Copyright (c) 2015 Terry sun. All rights reserved.
//

#ifndef __BINDER_H__
#define __BINDER_H__

bool terminal_signal = false;
int register_error_no = 0;
int reasonCode = 0;
int request_error_no = 0;
fd_set *sets;

// database of service and server
std::map <std::pair<std::string, std::vector<int> >, std::vector<struct serverInfo> > procedure_db;
std::vector<struct serverInfo> roundRobin_list;


std::pair<std::string, std::vector<int> > create_key(std::string name, std::vector<int> args);
void error(std::string s);
std::pair<std::string, std::vector<int> > service_search(std::pair<std::string, std::vector<int> > key);
bool server_check(std::vector<struct serverInfo> service_list, struct serverInfo loc2);
struct serverInfo loc_search(std::vector<struct serverInfo> locs);
int addServerService(std::string hostname, int server_port, std::string funcName, std::vector<int> args, int fd);
void clean_service(struct serverInfo loc);
void remove_crash_server();


//handle different type request
int handleAllRequest(int fd);
void server_register(int len, int fd);
void handleLocRequest(int len, int fd);
void terminateRequest();



#endif
