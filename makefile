#CC=g++
#CFLAGS= -std=c++11 
#LDFLAGS= -pthread
#OBJECTS=main.o libevent_server.o worker_thread.o

#lib_event: $(OBJECTS)
#	g++ -std=c++11 -pthread  -o libevent_server $(OBJECTS) -levent
#main.o: libevent_server.h
#	g++ -std=c++11 -c -o main.o main.cpp 
#libevent_server.o: worker_thread.h libevent_server.h
#	g++ -std=c++11 -c -o libevent_server.o  libevent_server.cpp
#worker_thread.o: worker_thread.h
#	g++ -std=c++11 -c -o worker_thread.o  worker_thread.cpp
#clean:
#	rm -f *.o
#
CC=g++
CFLAGS= -std=c++11
objects = worker_thread.o data_handle.o libevent_server.o main.o

all:$(objects)
	g++   -o libevent_server $(objects) -Wl,-dn -std=c++11 -levent -lnettle -lz -luuid -Wl,-dy -lpthread
$(objects): %.o: %.cpp
	$(CC) -c -g $(CFLAGS) $< -o $@
clean:
	rm -f *.o libevent_server
