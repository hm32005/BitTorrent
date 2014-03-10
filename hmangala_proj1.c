/*
 * proj1.c
 *
 *  Created on: Sep 14, 2013
 *      Author: Harish Mangalampalli
 */

#include "defns.h"

void *get_in_addr(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*) sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*) sa)->sin6_addr);
}
int main(int argc, char *argv[]) {
	char* port;

	if (argc == 3) {
		port = argv[2];
		if (strcasecmp(argv[1], "s") == 0 || strcasecmp(argv[1], "c") == 0) {
			if (strcasecmp(argv[1], "s") == 0) {
				createServer(port);
			}
			if (strcasecmp(argv[1], "c") == 0)
				createClient(port);
		} else {
			fprintf(stderr, "\n%s\n", HELPUSAGE);
			exit(0);
		}
	} else {
		fprintf(stderr, "\n%s\n", HELPUSAGE);
		exit(0);
	}
	return 0;
}

