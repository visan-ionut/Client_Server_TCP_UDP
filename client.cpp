#include <iostream>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "helpers.h"

void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_address server_port\n", file);
	exit(0);
}

int main(int argc, char *argv[])
{
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);
	int sockfd, n, ret, fdmax, i;
	struct sockaddr_in serv_addr;
	char buffer[BUFLEN], SUBSCRIBE[10] = "subscribe",
		UNSUBSCRIBE[12] = "unsubscribe", EXIT[5] = "exit";
	char client_id[11];
	
	client_id[0] = '\0';
	strncpy(client_id, argv[1], strlen(argv[1])+1);

	fd_set read_fds;
	fd_set tmp_fds;

	if (argc < 4) {
		usage(argv[0]);
	}

	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[3]));
	ret = inet_aton(argv[2], &serv_addr.sin_addr);
	DIE(ret == 0, "inet_aton");

	ret = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect");
	/* 
	 after the client make the connection 
	 send a message with the client id
	*/
	memcpy(buffer, &client_id, CLINID_LEN);
	memcpy(buffer + CLINID_LEN, "n", sizeof(char));
	send(sockfd, buffer, BUFLEN, 0);

	FD_SET(sockfd, &read_fds);
	FD_SET(STDIN_FILENO, &read_fds);
	
	fdmax = sockfd;

	while (1) {
		
		tmp_fds = read_fds;

		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");
		
		for (i = 0; i <= fdmax; ++i) {
			if (FD_ISSET(i, &tmp_fds)) {
				if (i == sockfd) { // message from server
					memset(buffer, 0, BUFLEN);
					
					n = recv(sockfd, buffer, BUFLEN, 0);
					
					DIE(n < 0, "recv");
					// check if the server closed the connection
					if (memcmp(buffer, EXIT, 5) == 0) {
						close(sockfd);
						exit(0);
					}
					// parse the message from server and print
					char addr[ADDR_LEN]; 
					short port;
					
					memcpy(&port, buffer + MESSAGE_UDP_LEN, sizeof(short));
					memcpy(addr ,buffer + MESSAGE_UDP_LEN + sizeof(short), ADDR_LEN);
					
					char topic[TOPIC_LEN];
					char content[CONTENT_LEN];
					
					uint8_t data_type;
					memcpy(topic, buffer, TOPIC_LEN);
					
					printf("%s:%d - %s - ", addr, ntohs(port), topic);
					
					memcpy(&data_type, buffer + TOPIC_LEN, 1);
					memcpy(content, buffer + TOPIC_LEN + 1, CONTENT_LEN);
					
					if (data_type == 0) {
						int number;
						char sign;
						
						sign = content[0];
						number = *(unsigned int *) &content[1];
						number = ntohl(number);

						printf("INT - %d\n", sign > 0 ? -number : number);
					} else if (data_type == 1) {
						unsigned short number;
						
						memcpy(&number, content, sizeof(short));
							
						number = ntohs(number);
						
						printf("SHORT_REAL - %.02f\n", (float)number / 100.0);
					} else if (data_type == 2) {
						int number;
						char sign;
						
						uint8_t exp;
						
						memcpy(&sign, content, sizeof(char));
						memcpy(&number, content + sizeof(char), sizeof(int));
						memcpy(&exp, content + sizeof(char) + sizeof(int), sizeof(uint8_t));
						
						number = ntohl(number);
						
						float fl_num = number;
						
						while (exp > 0) {
							fl_num = fl_num / 10;
							exp--;
						}
						
						printf("FLOAT - %f\n", sign > 0 ? -fl_num : fl_num);
					} else {
						printf("STRING - %s\n", content);
					}
				} else { // read from terminal
					memset(buffer, 0, BUFLEN);
					
					int store_for;
					
					char command[COMMAND_LEN], topic[TOPIC_LEN];
					
					scanf("%s", command);
					
					if (memcmp(command, SUBSCRIBE, 9) == 0) { // check for subscribe command
						scanf("%s", topic);

						scanf("%d", &store_for);
						
						memcpy(buffer, &client_id, CLINID_LEN);
						
						memcpy(buffer + CLINID_LEN, "s", sizeof(char));
						
						memcpy(buffer + CLINID_LEN + sizeof(char), &store_for, sizeof(int));
						
						memcpy(buffer + CLINID_LEN + sizeof(int) + sizeof(char), topic, TOPIC_LEN);
						
						printf("Subscribed to topic.\n");
					} else if (memcmp(command, UNSUBSCRIBE, 11) == 0) { // check for unsubscribe command
						scanf("%s", topic);
						
						memcpy(buffer, &client_id, CLINID_LEN);
						
						memcpy(buffer + CLINID_LEN, "u", sizeof(char));
						
						memcpy(buffer + CLINID_LEN + sizeof(char), topic, TOPIC_LEN);
						
						printf("Unsubscribed from topic.\n");
					} else if (memcmp(command, EXIT, 4) == 0) { // check for exit command
						close(sockfd);
						exit(0);
					} else { // usage
						printf("Usage:\nSUBSCRIBE: subscribe <topic> <SF>\nUNSUBSCRIBE: unsubscribe <topic>\n EXIT: exit");
					}								
					// send the command				
					send(sockfd, buffer, BUFLEN, 0);
				}
			}
		}
	}

	close(sockfd);

	return 0;
}
