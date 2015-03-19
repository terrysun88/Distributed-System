Group Member:
Name         UserID         StudentID
Zhenyu Bai   z3bai           20489611
Lipeng Sun   lpsun           20339554

How to Compile:
1.make
2.gcc -c server_function_skels.c server_functions.c server.c client1.c 
3.g++ -L. -pthread client1.o -lrpc -o client
4.g++ -L. -pthread server_functions.o server_function_skels.o server.o -lrpc -o server


