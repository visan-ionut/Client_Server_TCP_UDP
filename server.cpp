#include <stdio.h>
#include <map>
#include <vector>
#include <queue>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "helpers.h"
#include <algorithm>

void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_port\n", file);
	exit(0);
}

struct cmp_str
{
   bool operator()(char const *a, char const *b) const
   {
      return strcmp(a, b) < 0;
   }
};

int main(int argc, char *argv[]){
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);
	// map with key topic and value vector of sockets
	std::map<char*, std::vector<int>, cmp_str> subscribers;
	// map with key client id and value queue of messages
	std::map<char*, std::queue<char *>, cmp_str> store_forw;
	// map with key client id and value socket
	std::map<char*, int, cmp_str> cli_sock;
	// map with key socket and value client id
	std::map<int, char*> sock_cli;
	/* 
	 map with key client id and value bool
	 that decide if client is connected
	*/
	std::map<char *, bool, cmp_str> disconnected;
	
	int newsockfd, n, i, ret, fdmax;
	
	char buffer[BUFLEN], NEW_CLI = 'n', SUBS = 's', EXIT[5] = "exit";
	
	struct sockaddr_in serv_addr, cli_addr;
	
	socklen_t clilen;
	
	fd_set read_fds;
	fd_set tmp_fds;

	if (argc < 2) {
		usage(argv[0]);
	}

	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	int tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0);
	int udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);

	DIE(tcp_sockfd < 0, "socket");
	DIE(udp_sockfd < 0, "socket");

	int portno = atoi(argv[1]);
	DIE(portno == 0, "atoi");

	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	serv_addr.sin_addr.s_addr = INADDR_ANY;

	ret = bind(tcp_sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
	DIE(ret < 0, "bind");

	ret = bind(udp_sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
	DIE(ret < 0, "bind");

	ret = listen(tcp_sockfd, MAX_CLIENTS);
	DIE(ret < 0, "listen");

	FD_SET(tcp_sockfd, &read_fds);
	FD_SET(udp_sockfd, &read_fds);
	FD_SET(STDIN_FILENO, &read_fds);
	
	fdmax = tcp_sockfd > udp_sockfd ? tcp_sockfd : udp_sockfd;
	
	clilen = sizeof(cli_addr);
	
	while (1) {
		tmp_fds = read_fds; 
		
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");

		for (i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &tmp_fds)) {
				if (i == tcp_sockfd) { // accept and create connection
					newsockfd = accept(tcp_sockfd, (struct sockaddr *) &cli_addr, &clilen);
					DIE(newsockfd < 0, "accept");
					
					FD_SET(newsockfd, &read_fds);
					
					if (newsockfd > fdmax) {
						fdmax = newsockfd;
					}
					
				} else if (i == udp_sockfd) { // receive message from udp client
					int fd = recvfrom(udp_sockfd, &buffer, BUFLEN, 0, (struct sockaddr *) &cli_addr, &clilen);

					char *topic = (char *) calloc(TOPIC_LEN, sizeof(char));

					short port = cli_addr.sin_port;

					memcpy(topic, buffer, TOPIC_LEN);
					// add port and address to the message
					memcpy(buffer + MESSAGE_UDP_LEN, &port, sizeof(short));

					memcpy(buffer + MESSAGE_UDP_LEN + sizeof(short), inet_ntoa(cli_addr.sin_addr), ADDR_LEN);
					/* 
					 forward the message or save it
					 if the client tcp is disconnected
					*/
					for (auto sockfd : subscribers[topic]) {
						if (disconnected.count(sock_cli[sockfd]) != 0 
							&& disconnected[sock_cli[sockfd]] 
							&& store_forw.count(sock_cli[sockfd]) != 0) {

							char *new_buffer = (char *)calloc(BUFLEN, sizeof(char));
						
							memcpy(new_buffer, buffer, BUFLEN);
						
							store_forw[sock_cli[sockfd]].push(new_buffer);
						
						} else {
						
							send(sockfd, buffer, BUFLEN, 0);
						
						}
					}
				} else if (i == STDIN_FILENO) { // read from terminal
					
					char command[COMMAND_LEN];
					scanf("%s", command);
					// check if the command is right
					if (memcmp(command, EXIT, 4) == 0) {
						
						for (auto sock : cli_sock) {
							printf("Disconnecting client %s from socket %d.\n", sock.first, sock.second);
							send(sock.second, "exit", 5, 0);
							close(sock.second);
						}
						
						close(udp_sockfd);
						close(tcp_sockfd);
						
						exit(0);
					} else {
						printf("Usage: exit");
					}
				} else { // receive message from tcp client
					memset(buffer, 0, BUFLEN);
					
					n = recv(i, buffer, sizeof(buffer), 0);
					DIE(n < 0, "recv");

					if (n == 0) { // check if the connection is closing
						printf("Client %s disconnected.\n", sock_cli[i]);

						close(i);

						disconnected[sock_cli[i]] = true;

						FD_CLR(i, &read_fds);
					} else { // parse the command from tcp client
						char decision;

						char *client_id = (char *) calloc(CLINID_LEN, sizeof(char));					
						
						char *topic = (char *) calloc(TOPIC_LEN, sizeof(char));

						memcpy(client_id, buffer, CLINID_LEN);

						memcpy(&decision, buffer + CLINID_LEN, sizeof(char));
						// announce which tcp client connected
						if (memcmp(&decision, &NEW_CLI, sizeof(char)) == 0) {
														
							if (cli_sock.count(client_id) != 0 
								&& !disconnected[client_id]) { // check if other client_id is connected on other socket
								
								printf("Client %s already connected.\n", client_id);
								
								send(cli_sock[client_id], EXIT, 5, 0);

								close(cli_sock[client_id]);
								
								FD_CLR(cli_sock[client_id], &read_fds);
								
								cli_sock.erase(client_id);
							}
							else {
								printf("New client %s connected from %s:%d.\n",
								client_id, inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
							}
							/*
							 if a tcp client subscribed with SF = 1
							 when its reconnected send all the
							 messages unreceived
							*/
							if (store_forw.count(client_id) != 0) {
								while (!store_forw[client_id].empty()) {
									send(i, store_forw[client_id].front(), BUFLEN, 0);
									free(store_forw[client_id].front());
									store_forw[client_id].pop();
								}
							}

							sock_cli[i] = client_id;
							cli_sock[client_id] = i;

						} else if (memcmp(&decision, &SUBS, sizeof(char)) == 0) { // check for subscribe command
							int sf;

							memcpy(&sf, buffer + CLINID_LEN + sizeof(char), sizeof(int));
							
							memcpy(topic, buffer + CLINID_LEN + sizeof(int) + sizeof(char), 50);
							
							if (subscribers.count(topic) == 0) {
								subscribers[topic] = std::vector<int>();
							}
							
							subscribers[topic].push_back(i);
							
							if (sf == 1) {
								store_forw[client_id] = std::queue<char *>();
							}
							disconnected[client_id] = false;
						} else { // check for unsubscribe command				
							memcpy(topic, buffer + CLINID_LEN + sizeof(char), TOPIC_LEN);
							
							std::vector<int>::iterator pos = find(subscribers[topic].begin(), subscribers[topic].end(), i);
							store_forw.erase(client_id);
							if(pos != subscribers[topic].end()) {
								subscribers[topic].erase(pos);
							}

						}
					}
				}
			}
		}
	}

	close(udp_sockfd);

	close(tcp_sockfd);

	return 0;
}
