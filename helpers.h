#ifndef _HELPERS_H
#define _HELPERS_H 1

#include <stdio.h>
#include <stdlib.h>

#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);				\
		}									\
	} while(0)

#define BUFLEN		2000
#define MAX_CLIENTS	10
#define TOPIC_LEN 50
#define MESSAGE_UDP_LEN 1551
#define CONTENT_LEN 1500
#define ADDR_LEN 16
#define CLINID_LEN 10
#define COMMAND_LEN 20

#endif
