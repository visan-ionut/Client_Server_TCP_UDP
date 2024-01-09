CFLAGS = -Wall -g

all: server subscriber

server: server.cpp
	g++ -g server.cpp -o server

subscriber: client.cpp
	g++ -g client.cpp -o subscriber
	
.PHONY: clean run_server run_client

clean:
	rm -f server subscriber
