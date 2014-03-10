/*
 * server.c
 *
 *  Created on: Sep 14, 2013
 *      Author: Harish Mangalampalli
 */

#include "defns.h"

/* Fetch client connection details from the Server IP List using connection ids */
struct ServerIP * retrieveIPbyConnId(struct ServerIP *head, int conn_id);

/* Fetch client connection details from the Server IP List using the socket file descriptror */
struct ServerIP* retrieveIPbyFD(struct ServerIP *head, int rmt_fd);

/* Remove client connection details from the Server IP List using connection ids */
void removeIPbyConnId(struct ServerIP *head, struct ServerIP *node);

/* Format and send client connection details from the Server IP List to all connected clients */
char* createSendList(struct ServerIP *head);

/* Add registered clients to the Server IP List */
struct ServerIP *addIPToList(struct ServerIP *head, char* hostname, char* ip, int port, int listen_port, int remote_fd);

/* Determine server's IP */
char* whatsserversip();

int numConns = 0;
struct ServerIP *server_head = NULL;
char serverip[INET6_ADDRSTRLEN], serverhost[256] = { }, *serverport;

void createServer(char* port) {
	char *s;
	char buf[MAXDATASIZE], host[128], *rmt_hostname, service[20], remoteIP[INET6_ADDRSTRLEN], stdinbuf[MAXDATASIZE];
	serverport = port;
	fd_set master;
	fd_set read_fds;

	socklen_t addrlen;

	struct sockaddr_storage remoteaddr;
	struct addrinfo hints, *ai, *p;
	struct ServerIP * rmt_srv_ip;

	int fdmax;
	int listener;
	int newfd;
	int nbytes, remotePort, i, j, rv, selectValue;
	int yes = 1;
	int remote_listen_port;

	FD_ZERO(&master);
	FD_ZERO(&read_fds);
	whatsserversip(); // Determine server's IP
	gethostname(serverhost, sizeof serverhost);

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	if ((rv = getaddrinfo(NULL, port, &hints, &ai)) != 0) {
		fprintf(stderr, "\nSERVER: %s\n", gai_strerror(rv));
		exit(1);
	}

	for (p = ai; p != NULL; p = p->ai_next) {
		listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (listener < 0) {
			continue;
		}

		setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

		if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
			close(listener);
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "\nFailed to bind to listening port number: %ld\n",strtol(port, NULL, 10));
		exit(2);
	}

	freeaddrinfo(ai);

	if (listen(listener, 10) == -1) {
		perror("\nLISTEN");
		exit(3);
	}

	FD_SET(listener, &master);
	fdmax = listener;

	FD_SET(fileno(stdin), &master);
	if (fileno(stdin) > fdmax) {
		fdmax = fileno(stdin);
	}

	for (;;) {
		printf("proj1>>");
		fflush(stdout);
		read_fds = master;
		selectValue = select(fdmax + 1, &read_fds, NULL, NULL, NULL);
		if (selectValue > 0) {
			for (i = 0; i <= fdmax; i++) {
				if (FD_ISSET(i, &read_fds)) {
					if (i == listener) {
						addrlen = sizeof remoteaddr;
						newfd = accept(listener, (struct sockaddr *) &remoteaddr, &addrlen);

						if (newfd == -1) {
							perror("\nACCEPT");
						} else {
							numConns++;
							FD_SET(newfd, &master);
							if (newfd > fdmax) {
								fdmax = newfd;
							}
						}
					} else if (i == fileno(stdin)) {
						fgets(stdinbuf, MAXDATASIZE, stdin);
						int len = strlen(stdinbuf);
						if (stdinbuf[len - 1] == '\n')
							stdinbuf[len - 1] = '\0';
						s = stdinbuf;
						if (strcasecmp(s, "HELP") == 0) {
							printf("\n%s%s%s%s%s%s%s%s%s%s%s%s\n", HELPUSAGE, HELPCOMMANDS, HELPHELP, HELPMYIP, HELPMYPORT, HELPREGISTER, HELPCONNECT, HELPLIST, HELPTERMINATE, HELPEXIT, HELPDOWNLOAD, HELPCREATOR);
						} else if (strncasecmp(stdinbuf, "CONNECT", strlen("CONNECT") - 1) == 0){
							fprintf(stderr, "\nCONNECT cannot be called on server.\n");
						} else if (strncasecmp(stdinbuf, "REGISTER", strlen("REGISTER") - 1) == 0){
							fprintf(stderr, "\nREGISTER cannot be called on server.\n");
						}
						else if (strcasecmp(s, "CREATOR") == 0)
							printf("\n%s\n", CREATOR);
						else if (strcasecmp(s, "MYIP") == 0) {
							printf("\nMy IP is %s\n", serverip);
							//printf("\nMy hostname is %s\n", serverhost);
						} else if (strcasecmp(s, "MYPORT") == 0)
							printf("\nThe listening port is %s\n", port);
						else if (strcasecmp(s, "LIST") == 0) {
							int k;
							printf("\nList of connected peers\n");
							printf("id: %-35s\t%-15s\t%-6s\n", HOSTNAMESTRING, IPSTRING, PORTSTRING);
							printf("--------------------------------------------------------------------------\n");
							for (k = 1; k <= numConns; k++) {
								struct ServerIP *temp = retrieveIPbyConnId(server_head, k);
								if (temp != NULL) {
									printf("%d: ", k);
									printf("%-35s\t", temp->hostname);
									printf("%-15s\t", temp->remoteIP);
									printf("%-6d\n", temp->listen_port);
								}
							}
							printf("\n");
						}
						else if (strcasecmp(s, "EXIT") == 0) {
							if (numConns > 0) {
								int k = 0;
								memset(buf, 0, MAXDATASIZE);
								fflush(stdin);
								for(; k <= numConns; k++){
									rmt_srv_ip = retrieveIPbyConnId(server_head, k);
									close(i);
									FD_CLR(i, &master);
								}
								FD_ZERO(&master);
								FD_ZERO(&read_fds);
							}
							exit(0);
						}
						else if (strncasecmp(stdinbuf, "DOWNLOAD", strlen("DOWNLOAD") - 1) == 0){
							fprintf(stderr, "\nCannot download files onto server!\n");
						}
						else
							fprintf(stderr, "\nInvalid Command\n");
						memset(stdinbuf, 0, MAXDATASIZE);
						fflush(stdin);
					} else {
						memset(buf, 0, MAXDATASIZE);
						if ((nbytes = recv(i, buf, MAXDATASIZE - 1, 0)) <= 0) {
							// Fetch client connection details from the Server IP List using the socket file descriptror
							rmt_srv_ip = retrieveIPbyFD(server_head, i);
							printf("\nThe peer %s listening on %d and connected to socket %d terminated the connection remotely\n", rmt_srv_ip->hostname, rmt_srv_ip->listen_port, i);
							// Remove client connection details from the Server IP List using connection ids
							removeIPbyConnId(server_head, rmt_srv_ip);
							numConns--;
							close(i);
							FD_CLR(i, &master);
							if (numConns > 0) {
								// Format and send client connection details from the Server IP List to all connected clients
								char *list = createSendList(server_head);

								for (j = 0; j <= fdmax; j++) {
									if (FD_ISSET(j, &master)) {
										if (j != listener && j != fileno(stdin)) {
											if (send(j, list, strlen(list), 0) == -1) {
												perror("\nSEND");
											}
										}
									}
								}
							}
						} else {
							buf[nbytes] = '\0';
							if (strncasecmp(buf, "REGISTER", strlen("REGISTER") - 1) == 0){
								char* pch = strtok(buf, " ");
								char** toks = NULL;
								int tokitr;
								toks = malloc(sizeof(char*) * 5);
								for (tokitr = 0; pch != NULL; tokitr++) {
									toks[tokitr] = pch;
									pch = strtok(NULL, " ");
								}
								rmt_hostname = toks[1];
								strcpy(remoteIP, toks[2]);
								remote_listen_port = strtol(toks[3], NULL, 10);

								printf("\nConnected to peer: %s \t IP: %s \t listening on port: %d\n", rmt_hostname, remoteIP, remote_listen_port);
								if (numConns == 1) {
									// Add registered clients to the Server IP List
									server_head = addIPToList(NULL, rmt_hostname, remoteIP, remotePort, remote_listen_port, i);
								} else {
									// Add registered clients to the Server IP List
									addIPToList(server_head, rmt_hostname, remoteIP, remotePort, remote_listen_port, i);
								}
								// Format and send client connection details from the Server IP List to all connected clients
								char *list = createSendList(server_head);
								for (j = 0; j <= fdmax; j++) {
									if (FD_ISSET(j, &master)) {
										if (j != listener && j != fileno(stdin)) {
											if (send(j, list, strlen(list), 0) == -1) {
												perror("\nSEND");
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
	return;
}

/* Add registered clients to the Server IP List */
struct ServerIP *addIPToList(struct ServerIP *head, char* hostname, char* ip, int port, int listen_port, int remote_fd) {
	struct ServerIP *temp, *cur;
	cur = head;
	temp = (struct ServerIP *) malloc(sizeof(*temp));
	temp->hostname = (char*) malloc(strlen(hostname));
	sprintf(temp->hostname, "%s", hostname);
	temp->remoteIP = (char*) malloc(strlen(ip));
	sprintf(temp->remoteIP, "%s", ip);
	temp->remotePort = port;
	temp->listen_port = listen_port;
	temp->remoteFD = remote_fd;
	temp->num_chunks = 0;
	temp->next = NULL;

	if (cur == NULL) {
		cur = temp;
	} else {
		while (cur->next != NULL) {
			if (cur == NULL)
				break;
			cur = cur->next;
		}
		cur->next = temp;
	}
	return temp;
}

/* Fetch client connection details from the Server IP List using connection ids */
struct ServerIP* retrieveIPbyConnId(struct ServerIP *head, int conn_id) {
	struct ServerIP *cur;
	cur = head;
	int i;
	for (i = 1;; i++) {
		if (i == conn_id) {
			break;
		} else if (cur->next == NULL) {
			cur = NULL;
			break;
		} else
			cur = cur->next;
	}
	return cur;
}

/* Fetch client connection details from the Server IP List using the socket file descriptror */
struct ServerIP* retrieveIPbyFD(struct ServerIP *head, int rmt_fd) {
	struct ServerIP *cur;
	cur = head;
	int i;
	for (i = 0;; i++) {
		if (cur->remoteFD == rmt_fd) {
			break;
		} else if (cur->next == NULL) {
			cur = NULL;
			break;
		} else
			cur = cur->next;
	}
	return cur;
}

/* Remove client connection details from the Server IP List using connection ids */
void removeIPbyConnId(struct ServerIP *head, struct ServerIP *node) {
	struct ServerIP *temp, *cur;
	if (node != NULL) {
		temp = head->next;
		cur = head;
		if (node == cur) {
			free(cur);
			head = temp;
			server_head = head;
			return;
		}
		while (temp != NULL && cur != NULL) {
			if (temp == node) {
				cur->next = temp->next;
				free(temp);
				return;
			} else {
				cur = temp;
				temp = temp->next;
			}
		}
	}
}

/* Format and send client connection details from the Server IP List to all connected clients */
char* createSendList(struct ServerIP *head) {
	int i;
	char *list = malloc(200 * sizeof(char*));
	char target_string[7];
	strcat(list, "SERVER_HOSTNAME~");
	strcat(list, serverhost);
	strcat(list, "~");
	strcat(list, serverip);
	strcat(list, "~SERVER_IP_LIST");
	for (i = 1; i <= numConns; i++) {
		struct ServerIP *temp = retrieveIPbyConnId(head, i);
		if (temp != NULL) {
			strcat(list, ";");
			if (strcat(list, temp->hostname) == NULL) {
				list = realloc(list, 200 * sizeof(char*));
				strcat(list, temp->hostname);
			}
			strcat(list, "|");
			if (strcat(list, temp->remoteIP) == NULL) {
				list = realloc(list, 200 * sizeof(char*));
				strcat(list, temp->remoteIP);
			}
			strcat(list, "|");
			sprintf(target_string, "%d", temp->listen_port);
			if (strcat(list, target_string) == NULL) {
				list = realloc(list, 200 * sizeof(char*));
				strcat(list, target_string);
			}
		}
	}
	return list;
}

/* Determine server's IP */
char* whatsserversip() {
	int sockfd, port;
	struct addrinfo hints, *servinfo, *p;
	int rv, yes = 1;
	struct sockaddr_storage remoteaddr;

	socklen_t addrlen = sizeof(remoteaddr);

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;

	if ((rv = getaddrinfo("8.8.8.8", NULL, &hints, &servinfo)) != 0) {
		fprintf(stderr, "\nGETADDRINFO: %s\n", gai_strerror(rv));
		return NULL;
	}

	for (p = servinfo; p != NULL; p = p->ai_next) {
		if (p->ai_family == AF_INET) {
			if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
				perror("\nSOCKET");
				continue;
			}

			setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

			if ((connect(sockfd, p->ai_addr, p->ai_addrlen)) == -1) {
				close(sockfd);
				perror("\nCONNECT");
				continue;
			}

			break;
		} else
			continue;
	}

	if (p == NULL) {
		fprintf(stderr, "\nFailed to bind to socket\n");
		return NULL;
	}

	if ((getsockname(sockfd, (struct sockaddr *) &remoteaddr, &addrlen)) == -1)
		perror("GETSOCKNAME");
	else {
		if (remoteaddr.ss_family == AF_INET) {
			struct sockaddr_in *s = (struct sockaddr_in *) &remoteaddr;
			port = ntohs(s->sin_port);
			inet_ntop(AF_INET, &s->sin_addr, serverip, INET6_ADDRSTRLEN);
		} else {
			struct sockaddr_in6 *s = (struct sockaddr_in6 *) &remoteaddr;
			port = ntohs(s->sin6_port);
			inet_ntop(AF_INET6, &s->sin6_addr, serverip, INET6_ADDRSTRLEN);
		}
	}

	freeaddrinfo(servinfo);

	close(sockfd);
	return serverip;
}
