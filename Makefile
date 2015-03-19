FLAGS= -g -pthread
all: librpc.a binder 
librpc.a: rpc.o 
	ar rcs librpc.a rpc.o
rpc.o: rpc.cc rpc.h function.h
	g++ $(FLAGS) -c rpc.cc -o rpc.o
binder: binder.h binder.cc librpc.a
	g++ $(FLAGS) -L. binder.cc -lrpc -o binder
clean:
	rm *.o *.a 
