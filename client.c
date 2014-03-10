/*
 * server.c
 *
 *  Created on: Sep 14, 2013
 *      Author: Harish Mangalampalli
 */

#include "defns.h"

/* Tokenize the message received from server and display the available peers */
void tokenize_server_list(char* buf);

/* Validate if the host requested for connection is present in the server list and absent from client connection list */
int validateHostPort(struct ServerIP *head, char* host, char* port);

/* Determine client's IP */
char* whatsclientsip();

/* Fetch client connection details from the Client connection List using connection ids */
struct ServerIP *retrieveIPbyConnId(struct ServerIP *head, int conn_id);

/* Add registered clients to the Client connection List */
struct ServerIP *addIPToList(struct ServerIP *head, char* hostname, char* ip, int port, int listen_port, int remote_fd);

/* Custom send method to ensure entire data chunk is sent */
int sendall(int s, void *buf, int *len);

int num_conn_peers = 0, chunk_size = 0;
char clientip[INET6_ADDRSTRLEN], clienthost[256] = { }, *clientport;
struct ServerIP *client_head = NULL;
struct ServerIP *client_server_head = NULL;

/* Create a client */
void createClient(char* port) {
	char* s;
	char buf[MAXDATASIZE], host[128] = { }, *rmt_hostname, service[20], remoteIP[INET6_ADDRSTRLEN], stdinbuf[MAXDATASIZE];
	char remote_client_IP[INET6_ADDRSTRLEN];
	clientport = port;
	fd_set master;
	fd_set read_fds;

	int fdmax;
	int numbytes, i, rv, remotePort;
	int yes = 1;
	int nbytes;
	int listener_fd = -1, server_fd = -1, client_fd;
	int new_conn_fd;

	socklen_t addrlen, lenaddr;

	struct addrinfo sa_src, sa_dest, *ai_dest, *ai_src, *p;
	struct sockaddr_storage remote_acpt_addr, remote_peer_addr;
	struct ServerIP * rmt_srv_ip;
	
	time_t begin,end;

	FD_ZERO(&master);
	FD_ZERO(&read_fds);

	fdmax = fileno(stdin);
	whatsclientsip(); // Determine client's IP
	gethostname(clienthost, sizeof clienthost);
	for (;;) {
		printf("proj1>>");
		fflush(stdout);
		FD_SET(fileno(stdin), &master);
		read_fds = master;
		select(fdmax + 1, &read_fds, NULL, NULL, NULL);

		for (i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &read_fds)) {
				if (i == listener_fd) {
					addrlen = sizeof(remote_acpt_addr);
					lenaddr = sizeof(remote_peer_addr);

					new_conn_fd = accept(listener_fd, (struct sockaddr *) &remote_acpt_addr, &addrlen);

					if (new_conn_fd == -1) {
						perror("\nACCEPT ");
					} else {
						num_conn_peers++;
						FD_SET(new_conn_fd, &master);
						if (new_conn_fd > fdmax) {
							fdmax = new_conn_fd;
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
					} else if (strncasecmp(s, "CONNECT", strlen("CONNECT") - 1) == 0) {
						char* pch = strtok(s, " ");
						char** toks = NULL;
						int l;
						toks = (char **)malloc(sizeof(char*) * 5);
						for (l = 0; pch != NULL; l++) {
							toks[l] = pch;
							pch = strtok(NULL, " ");
						}
						if (toks[1] == NULL || toks[2] == NULL) {
							fprintf(stderr, "\nUsage: CONNECT <destination IP> <port_no>\n");
						}
						// Validate if the host requested for connection is present in the server list
						else if (validateHostPort(client_server_head, toks[1], toks[2]) == 0) {
							fprintf(stderr, "\nCould not connect. Remote IP/Port not found in the server list.\n");
						}
						// Validate if the host requested for connection is absent from client connection list
						else if (validateHostPort(client_head, toks[1], toks[2]) == 1) {
							fprintf(stderr, "\nConnection already established!\n");
						}
						else if (((strcmp(clienthost, toks[1]) == 0) || (strcmp(clientip, toks[1]) == 0)) && strtol(clientport, NULL, 10) == strtol(toks[2], NULL, 10)) {
							fprintf(stderr, "\nCannot connect to self!\n");
						}
						else if (num_conn_peers == 0) {
							fprintf(stderr, "\nRegister with server first!\n");
						}
						else {
							memset(&sa_dest, 0, sizeof(struct addrinfo));
							sa_dest.ai_family = AF_UNSPEC;
							sa_dest.ai_socktype = SOCK_STREAM;
							int rmt_listen_port = atoi(toks[2]);
							if ((rv = getaddrinfo(toks[1], toks[2], &sa_dest, &ai_dest)) != 0) {
								fprintf(stderr, "\nGETADDRINFO: %s\n", gai_strerror(rv));
							}

							for (p = ai_dest; p != NULL; p = p->ai_next) {
								if ((client_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
									perror("\nSOCKET ");
									continue;
								}
								setsockopt(client_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

								if ((connect(client_fd, p->ai_addr, p->ai_addrlen)) == -1) {
									close(client_fd);
									perror("\nCONNECT ");
									continue;
								}
								break;
							}

							if (p == NULL) {
								fprintf(stderr, "\nFailed to connect\n");
								close(client_fd);
								FD_CLR(client_fd, &master);
								continue;
							}

							FD_SET(client_fd, &master);
							if (client_fd > fdmax) {
								fdmax = client_fd;
							}

							freeaddrinfo(ai_dest);

							if ((getsockname(client_fd, (struct sockaddr *) &remote_acpt_addr, &addrlen)) == -1)
								perror("\nGETSOCKNAME ");
							else {
								if (remote_acpt_addr.ss_family == AF_INET) {
									struct sockaddr_in *s = (struct sockaddr_in *) &remote_acpt_addr;
									remotePort = ntohs(s->sin_port);
									inet_ntop(AF_INET, &s->sin_addr, remoteIP, INET6_ADDRSTRLEN);
								} else {
									struct sockaddr_in6 *s = (struct sockaddr_in6 *) &remote_acpt_addr;
									remotePort = ntohs(s->sin6_port);
									inet_ntop(AF_INET6, &s->sin6_addr, remoteIP, INET6_ADDRSTRLEN);
								}
							}
							char ip_port[256];
							strcpy(ip_port, "CONNECT~");
							strcat(ip_port, clienthost);
							strcat(ip_port, "~");
							strcat(ip_port, clientip);
							strcat(ip_port, "~");
							strcat(ip_port, clientport);
							if (send(client_fd, ip_port, strlen(ip_port), 0) == -1) {
								perror("\nSEND");
							}
							memset(buf, 0, MAXDATASIZE);
							if ((numbytes = recv(client_fd, buf, MAXDATASIZE - 1, 0)) <= 0) {
								perror("\nRECEIVE");
							}

							buf[numbytes] = '\0';

							if (strncasecmp(buf, "CONNECT", strlen("CONNECT") - 1) == 0){
								char* pch = strtok(buf, "~");
								char** toks = NULL;
								int l;
								toks = (char **)malloc(sizeof(char*) * 5);
								for (l = 0; pch != NULL; l++) {
									toks[l] = pch;
									pch = strtok(NULL, "~");
								}
								rmt_hostname = toks[1];
								strcpy(remoteIP, toks[2]);
							}

							printf("\nConnected to %s \t IP: %s \t on remote port: %d\n", rmt_hostname, remoteIP, remotePort);
							num_conn_peers++;
							if (client_fd != server_fd)
								// Add registered clients to the Client connection List
								addIPToList(client_head, rmt_hostname, remoteIP, remotePort, rmt_listen_port, client_fd);
							free(toks);
						}
					} else if (strcasecmp(s, "CREATOR") == 0)
						printf("\n%s\n", CREATOR);
					else if (strcasecmp(s, "MYIP") == 0) {
						printf("\nMy IP is %s\n", clientip);
						//printf("\nMy hostname is %s\n", clienthost);
					} else if (strcasecmp(s, "MYPORT") == 0)
						printf("\nThe listening port is %s\n", port);
					else if (strcasecmp(s, "LIST") == 0) {
						int k;
						printf("\nLIST OF CONNECTED PEERS\n\n");
						printf("id: %-35s\t%-15s\t%-6s\n", HOSTNAMESTRING, IPSTRING, PORTSTRING);
						printf("--------------------------------------------------------------------------\n");
						for (k = 1; k <= num_conn_peers; k++) {
							struct ServerIP *temp = retrieveIPbyConnId(client_head, k);
							if (temp != NULL) {
								printf("%d: ", k);
								printf("%-35s\t", temp->hostname);
								printf("%-15s\t", temp->remoteIP);
								printf("%-6d\n", temp->listen_port);
							}
						}
						printf("\n");
					} else if (strncasecmp(s, "REGISTER", strlen("REGISTER") - 1) == 0) {
						char* pch = strtok(s, " ");
						char** toks = NULL;
						int l;
						toks = (char **)malloc(sizeof(char*) * 5);
						for (l = 0; pch != NULL; l++) {
							toks[l] = pch;
							pch = strtok(NULL, " ");
						}
						char *server_listen_port;

						if (toks[1] == NULL || toks[2] == NULL) {
							fprintf(stderr, "\nUsage: REGISTER <server IP> <port_no>\n");
						} else if (server_fd >= 0) {
							fprintf(stderr, "\nAlready registered to the server!\n");
						} else {
							server_listen_port = toks[2];
							memset(&sa_dest, 0, sizeof(struct addrinfo));
							sa_dest.ai_family = AF_INET;
							sa_dest.ai_socktype = SOCK_STREAM;

							if ((rv = getaddrinfo(toks[1], toks[2], &sa_dest, &ai_dest)) != 0) {
								fprintf(stderr, "\nGETADDRINFO: %s\n", gai_strerror(rv));
								continue;
							}

							for (p = ai_dest; p != NULL; p = p->ai_next) {
								if (p->ai_family == AF_INET) {
									if ((server_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
										perror("\nSOCKET ");
										continue;
									}
									setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

									if ((connect(server_fd, p->ai_addr, p->ai_addrlen)) == -1) {
										close(server_fd);
										perror("\nCONNECT ");
										FD_CLR(server_fd, &master);
										memset(buf, 0, MAXDATASIZE);
										fflush(stdin);
										server_fd = -1;
										continue;
									}

									break;
								} else
									continue;
							}

							if (p == NULL) {
								fprintf(stderr, "\nFailed to connect\n");
								close(server_fd);
								FD_CLR(server_fd, &master);
								memset(buf, 0, MAXDATASIZE);
								fflush(stdin);
								server_fd = -1;
								continue;
							}

							FD_SET(server_fd, &master);
							if (server_fd > fdmax) {
								fdmax = server_fd;
							}

							freeaddrinfo(ai_dest);

							if ((getsockname(server_fd, (struct sockaddr *) &remote_acpt_addr, &addrlen)) == -1){
								perror("\nGETSOCKNAME ");
							}
							else {
								if (remote_acpt_addr.ss_family == AF_INET) {
									struct sockaddr_in *s = (struct sockaddr_in *) &remote_acpt_addr;
									remotePort = ntohs(s->sin_port);
								} else {
									struct sockaddr_in6 *s = (struct sockaddr_in6 *) &remote_acpt_addr;
									remotePort = ntohs(s->sin6_port);
								}
							}

							char ip_port[1024];
							strcpy(ip_port, "REGISTER");
							strcat(ip_port, " ");
							strcat(ip_port, clienthost);
							strcat(ip_port, " ");
							strcat(ip_port, clientip);
							strcat(ip_port, " ");
							strcat(ip_port, port);
							if (send(server_fd, ip_port, strlen(ip_port), 0) == -1) {
								perror("\nSEND ");
								close(server_fd);
								FD_CLR(server_fd, &master);
								memset(buf, 0, MAXDATASIZE);
								fflush(stdin);
								server_fd = -1;
								continue;
							}

							memset(buf, 0, MAXDATASIZE);
							if ((numbytes = recv(server_fd, buf, MAXDATASIZE - 1, 0)) <= 0) {
								fprintf(stderr, "\nConnection closed\n");
								close(server_fd);
								FD_CLR(server_fd, &master);
								memset(buf, 0, MAXDATASIZE);
								fflush(stdin);
								server_fd = -1;
								continue;
							}

							buf[numbytes] = '\0';

							if (strncasecmp(buf, "SERVER_HOSTNAME", strlen("SERVER_HOSTNAME") - 1) == 0){
								pch = strtok(buf, "~");
								toks = NULL;
								l;
								toks = (char **)malloc(sizeof(char*) * 5);
								for (l = 0; pch != NULL; l++) {
									toks[l] = pch;
									pch = strtok(NULL, "~");
								}
								rmt_hostname = toks[1];
								strcpy(remoteIP, toks[2]);
								printf("\nRegistered to %s \t IP: %s \t with client port: %d\n", rmt_hostname, remoteIP, remotePort);
								// Add server to the Client connection List
								tokenize_server_list(toks[3]); // Tokenize the message received from server and display the available peers
								num_conn_peers++;
								client_head = addIPToList(NULL, rmt_hostname, remoteIP, remotePort, atoi(server_listen_port), server_fd);

								free(toks);

								memset(&sa_src, 0, sizeof(struct addrinfo));
								sa_src.ai_family = AF_UNSPEC;
								sa_src.ai_socktype = SOCK_STREAM;
								sa_src.ai_flags = AI_PASSIVE;

								if ((rv = getaddrinfo(NULL, port, &sa_src, &ai_src)) != 0) {
									fprintf(stderr, "\nGETADDRINFO: %s\n", gai_strerror(rv));
									exit(1);
								}

								// START LISTENING

								for (p = ai_src; p != NULL; p = p->ai_next) {
									listener_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
									if (listener_fd < 0) {
										continue;
									}

									setsockopt(listener_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

									if (bind(listener_fd, p->ai_addr, p->ai_addrlen) < 0) {
										close(listener_fd);
										continue;
									}

									break;
								}

								if (p == NULL) {
									fprintf(stderr, "\nFailed to bind to port number: %ld\n", strtol(port, NULL, 10));
								}

								freeaddrinfo(ai_src);

								if (listen(listener_fd, 5) == -1) {
									perror("\nLISTEN");
								}

								FD_SET(listener_fd, &master);

								if (listener_fd > fdmax) {
									fdmax = listener_fd;
								}
							}
							else{
								fprintf(stderr, "\nCannot register to a client!\n");
								close(server_fd);
								FD_CLR(server_fd, &master);
								server_fd = -1;
							}
						}
					} else if (strncasecmp(s, "TERMINATE", strlen("TERMINATE") - 1) == 0) {
						char* pch = strtok(s, " ");
						char** toks = NULL;
						int l, rmt_conn_id = -1;
						toks = (char **)malloc(sizeof(char*) * 5);
						for (l = 0; pch != NULL; l++) {
							toks[l] = pch;
							pch = strtok(NULL, " ");
						}

						if (toks[1] == NULL) {
							fprintf(stderr, "\nUsage: TERMINATE <connection_id>\n");
						}
						rmt_conn_id = strtol(toks[1], NULL, 10);
						if(rmt_conn_id == 1){
							fprintf(stderr, "\nCannot TERMINATE connection with server.\n");
						}
						else if ((rmt_srv_ip = retrieveIPbyConnId(client_head, rmt_conn_id)) != NULL) {
							int rmt_fd = rmt_srv_ip->remoteFD;
							removeIPbyConnId(client_head, rmt_srv_ip);
							num_conn_peers--;
							close(rmt_fd);
							printf("\nThe connection with %-35s IP: %s listening on port number: %d connected to port number: %d has been terminated\n", rmt_srv_ip->hostname, rmt_srv_ip->remoteIP, rmt_srv_ip->listen_port, rmt_srv_ip->remotePort);
							FD_CLR(rmt_fd, &master);
							memset(buf, 0, MAXDATASIZE);
							fflush(stdin);
						} else {
							fprintf(stderr, "\nCould not close connection #%d. Check if the connection id entered is correct\n", rmt_conn_id);
						}
					}
					else if (strcasecmp(s, "EXIT") == 0) {
						if ((rmt_srv_ip = retrieveIPbyConnId(client_head, 1)) != NULL) {
							int rmt_fd = rmt_srv_ip->remoteFD, k = 1;
							for(; k <= num_conn_peers; k++){
								rmt_srv_ip = retrieveIPbyConnId(client_head, k);
								rmt_fd = rmt_srv_ip->remoteFD;
								close(rmt_fd);
								FD_CLR(rmt_fd, &master);
							}
							FD_ZERO(&master);
							FD_ZERO(&read_fds);
							memset(buf, 0, MAXDATASIZE);
							fflush(stdin);
							exit(0);
						}
						exit(1);
					}
					else if (strncasecmp(s, "DOWNLOAD", strlen("DOWNLOAD") - 1) == 0) {
						char* pch = strtok(s, " ");
						char** toks = NULL;
						int l;

						toks = (char**) malloc(sizeof(char*) * 5);
						for (l = 0; pch != NULL; l++) {
							toks[l] = pch;
							pch = strtok(NULL, " ");
						}
						if (toks[1] == NULL || toks[2] == NULL) {
							fprintf(stderr, "\nUsage: DOWNLOAD <file_name> <file_chunk_size_in_bytes>\n");
						}
						else{
							if(num_conn_peers <= 1){
								fprintf(stderr, "\nNot connected to any peer!\n");
							}
							else{
								chunk_size = strtol(toks[2], NULL, 10);
								int break_outer_loop = 0, *chunks_downloaded, k = 0, tempfd = -1;
								long chunks, fileSize, num_chunks_downloaded = 0;
								char *start_pos_string, *requestSize, command[60];
								void* fileChunk;

								requestSize = (char*) malloc(sizeof(char) * 60);
								strcpy(requestSize, "SIZEOF ");
								strcat(requestSize, toks[1]);
								struct ServerIP *temp = retrieveIPbyConnId(client_head, 2);
								tempfd = temp->remoteFD;
								if (send(tempfd, requestSize, strlen(requestSize), 0) == -1) {
									perror("SEND");
								}
								free(requestSize);
								fd_set sizeof_fds;
								int sizeof_fdmax;
								FD_ZERO(&sizeof_fds);
								FD_SET(tempfd, &sizeof_fds);
								sizeof_fdmax = tempfd;
								while(1){
									select(sizeof_fdmax + 1, &sizeof_fds, NULL, NULL, NULL);
									if(FD_ISSET(tempfd, &sizeof_fds)){
										//get size of file
										char size_string[50];
										
										if ((nbytes = recv(tempfd, size_string, 50, 0)) <= 0) {
											rmt_srv_ip = (struct ServerIP*) (intptr_t) retrieveIPbyFD(client_head, tempfd);
											printf("\nThe peer: %s listening on port number: %d terminated the connection remotely\n", rmt_srv_ip->hostname, rmt_srv_ip->listen_port);
											removeIPbyConnId(client_head, rmt_srv_ip);
											num_conn_peers--;
											close(tempfd);
											FD_CLR(tempfd, &sizeof_fds);
											FD_CLR(tempfd, &master);
										} else {
											pch = strtok(size_string, " ");
											char** size_toks = (char **)calloc(5, sizeof(char*));
											for (l = 0; pch != NULL; l++) {
												size_toks[l] = pch;
												pch = strtok(NULL, " ");
											}
											//printf("Size received %s\n",size_toks[1]);
											fileSize = strtol(size_toks[1], NULL, 10);
											if(fileSize%chunk_size != 0 )
												chunks = (fileSize /chunk_size) + 1;
											else
												chunks = fileSize/chunk_size;
											chunks_downloaded = (int *)calloc(chunks, sizeof(int));
											//printf("File size is %ld \t total chunks is %ld", fileSize, chunks);
											break;
										}
									}
								}
								FD_ZERO(&sizeof_fds);
								fd_set download_fds, master_download_fds;
								int download_fdmax = 0;
								FD_ZERO(&download_fds);
								FD_ZERO(&master_download_fds);
								FD_SET(tempfd, &master_download_fds);
								download_fdmax = tempfd;
								time_t begin,end;
								begin = time(NULL);

								FILE *out_file;
								out_file = fopen(toks[1], "w+b");
								long y = 0;
								void *junk="0";
								while(y<fileSize){
									fwrite (junk , sizeof(char), 1, out_file);
									y++;
								}
								rewind(out_file);
								for (k = 2; k <= num_conn_peers; k++) {
									struct ServerIP *temp = retrieveIPbyConnId(client_head, k);
									if (temp != NULL) {
										tempfd = temp->remoteFD;
										char *request = (char*) calloc(60, sizeof(char));
										start_pos_string = (char*) calloc(10, sizeof(char));
										strcpy(request, "DOWNLOAD ");
										strcat(request, toks[1]);
										strcat(request, " ");
										strcat(request, toks[2]);
										strcat(request, " ");
										sprintf(start_pos_string, "%d", (k-2)*chunk_size);
										strcat(request, start_pos_string);
										if (send(tempfd, request, strlen(request), 0) == -1) {
											perror("send");
											free(start_pos_string);
											free(request);
											break;
										}
										free(start_pos_string);
										free(request);
										FD_SET(tempfd, &master_download_fds);
										if(tempfd > download_fdmax)
											download_fdmax = tempfd;
										chunks_downloaded[k-2] = -tempfd;
									}
								}
								
								int x;
								for(;break_outer_loop != 1;){
									download_fds = master_download_fds;
									select(download_fdmax + 1, &download_fds, NULL, NULL, NULL);
									for (k = 0; k <= download_fdmax; k++) {
										if (FD_ISSET(k, &download_fds)) {
											rmt_srv_ip = (struct ServerIP*) (intptr_t) retrieveIPbyFD(client_head, k);
											if ((nbytes = recv(k, command, sizeof(command), 0)) <= 0) {
												printf("The peer: %s listening on port number: on %d terminated the connection remotely\n", rmt_srv_ip->hostname, rmt_srv_ip->listen_port);
												//removeIPbyConnId(client_head, rmt_srv_ip);
												//num_conn_peers--;
												//close(k);
												FD_CLR(k, &master_download_fds);
												//FD_CLR(k, &master);
												long m = 0;
												for(; m < chunks; m++){
													if(chunks_downloaded[m] == -k){
														chunks_downloaded[m] = 0;
														break;
													}
												}
												memset(command, 0, 60);
											} else {
												if (command[59] == '\n')
													command[59] = '\0';
												if (strncasecmp(command, "FILE", strlen("FILE") - 1) == 0){
													pch = strtok(command, "~");
													char** file_toks = (char **)calloc(5, sizeof(char*));
													for (l = 0; pch != NULL; l++) {
														file_toks[l] = pch;
														pch = strtok(NULL, "~");
													}
													int rcvd_chunk_size = strtol(file_toks[2], NULL, 10);
													fileChunk = (void *)calloc(rcvd_chunk_size, sizeof(char));
													if ((nbytes = recv(k, fileChunk, rcvd_chunk_size, MSG_WAITALL)) <= 0) {
														printf("\nThe peer: %s listening on port number: %d terminated the connection remotely\n", rmt_srv_ip->hostname, rmt_srv_ip->listen_port);
														//removeIPbyConnId(client_head, rmt_srv_ip);
														//num_conn_peers--;
														//close(k);
														FD_CLR(k, &master_download_fds);
														//FD_CLR(k, &master);
														long m = 0;
														for(; m < chunks; m++){
															if(chunks_downloaded[m] == -k){
																chunks_downloaded[m] = 0;
																break;
															}
														}
														memset(command, 0, 60);
														free(fileChunk);
														continue;
													}

													long chunk_start = strtol(file_toks[1], NULL, 10);
													
													fseek (out_file, chunk_start, SEEK_SET);
													//printf ("Position received = %ld \t Seeked to position %ld\n", chunk_start, ftell(out_file));
													fwrite (fileChunk , sizeof(char), nbytes, out_file);
													rewind(out_file);
													free(file_toks);
													num_chunks_downloaded++;
													rmt_srv_ip->num_chunks = rmt_srv_ip->num_chunks + 1;
													long m = 0;
													for(; m < chunks; m++){
														if(chunks_downloaded[m] == -k){
															chunks_downloaded[m] = k;
															break;
														}
													}
													memset(command, 0, 60);
													free(fileChunk);
													if(num_chunks_downloaded == chunks){
														break_outer_loop = 1;
														break;
													}

													m = 0;
													for(; m < chunks; m++){
														if(chunks_downloaded[m] == 0){
															char *request = (char*) malloc(sizeof(char) * 60);
															start_pos_string = (char*) calloc(10, sizeof(char));
															strcpy(request, "DOWNLOAD ");
															strcat(request, toks[1]);
															strcat(request, " ");
															strcat(request, toks[2]);
															strcat(request, " ");
															sprintf(start_pos_string, "%d", m*chunk_size);
															strcat(request, start_pos_string);
															if (send(k, request, strlen(request), 0) == -1) {
																perror("send");
																free(start_pos_string);
																free(request);
																break;
															}
															chunks_downloaded[m] = -k;
															free(start_pos_string);
															free(request);
															break;
														}
													}
												}
											}
										}
									}
								}
								
								free(chunks_downloaded);
								FD_ZERO(&download_fds);
								FD_ZERO(&master_download_fds);
								fclose(out_file);
								end = time(NULL);
								printf("\nDownloaded %d chunks in %f seconds\n", num_chunks_downloaded , difftime(end, begin));
								printf("\n\nDOWNLOAD DETAILS\n\n");
								printf("id: %-35s\t%-15s\t%-6s\t%-27s\n", HOSTNAMESTRING, IPSTRING, PORTSTRING, NUM_CHUNKS_STRING);
								printf("---------------------------------------------------------------------------------------------------\n");
								for (k = 2; k <= num_conn_peers; k++) {
									struct ServerIP *temp = retrieveIPbyConnId(client_head, k);
									if (temp != NULL && temp->num_chunks > 0) {
										printf("%d: ", k);
										printf("%-35s\t", temp->hostname);
										printf("%-15s\t", temp->remoteIP);
										printf("%6d\t", temp->listen_port);
										printf("%27ld\n", temp->num_chunks);
										temp->num_chunks = 0;
									}
								}
							}
						}
						free(toks);
					}
					else
						fprintf(stderr, "\nInvalid Command!\n");
					memset(stdinbuf, 0, MAXDATASIZE);
					fflush(stdin);
				} else {
					memset(buf, 0, MAXDATASIZE);
					if ((nbytes = recv(i, buf, sizeof(buf), 0)) <= 0) {
						rmt_srv_ip = (struct ServerIP*) (intptr_t) retrieveIPbyFD(client_head, i);
						printf("\nThe peer: %s listening on port number: %d terminated the connection remotely.\n", rmt_srv_ip->hostname, rmt_srv_ip->listen_port);
						removeIPbyConnId(client_head, rmt_srv_ip);
						num_conn_peers--;
						close(i);
						FD_CLR(i, &master);
						if(i == server_fd)
							server_fd = -1;
					} else {
						if (strncasecmp(buf, "CONNECT", strlen("CONNECT") - 1) == 0){

							char* pch = strtok(buf, "~");
							char** toks = NULL;
							int l;
							toks = (char **)malloc(sizeof(char*) * 5);
							for (l = 0; pch != NULL; l++) {
								toks[l] = pch;
								pch = strtok(NULL, "~");
							}
							rmt_hostname = toks[1];
							strcpy(remoteIP, toks[2]);
							int rmt_listen_port = strtol(toks[3], NULL, 10);

							if (num_conn_peers > 5) {
								close(i);
								FD_CLR(i, &master);
								continue;
							}

							printf("\nConnected to peer: %s \t IP: %s \t on port number: %d\n", rmt_hostname, remoteIP, remotePort);
							// Add registered clients to the Client connection List
							addIPToList(client_head, rmt_hostname, remoteIP, remotePort, rmt_listen_port, i);
							char ip_port[256];
							strcpy(ip_port, "CONNECT~");
							strcat(ip_port, clienthost);
							strcat(ip_port, "~");
							strcat(ip_port, clientip);
							if (send(i, ip_port, strlen(ip_port), 0) == -1) {
								perror("SEND");
							}
						}
						else if (strncasecmp(buf, "SIZEOF", strlen("SIZEOF") - 1) == 0){
							begin = time(NULL);
							char** size_toks = NULL;
							char* pch = strtok(buf, " ");
							int l;
							size_toks = (char **)malloc(sizeof(char*) * 5);
							for (l = 0; pch != NULL; l++) {
								size_toks[l] = pch;
								pch = strtok(NULL, " ");
							}
							char *file_name = size_toks[1];
							struct stat file_stat;

							if (stat(file_name, &file_stat) == 0){
								char file_size[50], send_buffer[50];
								memset(file_size, 0, 50);
								memset(send_buffer, 0, 50);
								strcpy(send_buffer, "SIZEIS ");
								sprintf(file_size, "%ld", (long)file_stat.st_size);
								strcat(send_buffer, file_size);
								strcat(send_buffer, " ");
								//printf("file_size %s\n", send_buffer);
								
								if (send(i, send_buffer, strlen(send_buffer), 0) == -1) {
									perror("SEND: ");
								}
							}
							else
								perror("SIZEOF: ");
							free(size_toks);
						}
						else if (strncasecmp(buf, "DOWNLOAD", strlen("DOWNLOAD") - 1) == 0){						
							char** download_toks;
							char chunk_size_string[15];
							char* pch = strtok(buf, " ");
							int l;
							download_toks = (char **)malloc(sizeof(char*) * 5);
							for (l = 0; pch != NULL; l++) {
								download_toks[l] = pch;
								pch = strtok(NULL, " ");
							}
							char *file_name = download_toks[1], send_string[60];
							long chunk_size = strtol(download_toks[2], NULL, 10);
							long start_pos = strtol(download_toks[3], NULL, 10);
							void *file_chunk = calloc(chunk_size, sizeof(char));

							FILE *in_file = fopen(file_name, "rb");
							if (in_file == NULL) {
								perror("FILE IO: ");
							}
							struct stat file_stat;
							long file_size;
							if (stat(file_name, &file_stat) == 0)
								file_size = (long)file_stat.st_size;
							if((start_pos + chunk_size)>file_size)
								chunk_size = file_size - start_pos;
							fseek (in_file ,start_pos, SEEK_SET);
							fread (file_chunk, sizeof(char), chunk_size, in_file);
							fclose(in_file);
							strcpy(send_string, "FILE~");
							strcat(send_string, download_toks[3]);
							sprintf(chunk_size_string, "%d", chunk_size);
							strcat(send_string, "~");
							strcat(send_string, chunk_size_string);
							strcat(send_string, "~~~~~~~~");
							send_string[strlen(send_string)]='~';
							send_string[59] = '\0';
							if (send(i, send_string, 60, 0) == -1) {
								perror("SEND");
							}
							int nbytes_sent, len = sizeof(char) * chunk_size;
							// Custom send method to ensure entire data chunk is sent
							if ((nbytes_sent = sendall(i, file_chunk, &len)) == -1) {
								perror("SEND");
							}
							end = time(NULL);
							//printf ("Time = %f secs\n", difftime(end, begin));
							//printf ("Position sent = %ld \n", start_pos);
							free(file_chunk);
							free(download_toks);
							free(pch);
						}
						else if (strncasecmp(buf, "SERVER_HOSTNAME", strlen("SERVER_HOSTNAME") - 1) == 0){
							char *pch = strtok(buf, "~");
							char **toks = NULL;
							int l;
							toks = (char **)malloc(sizeof(char*) * 5);
							for (l = 0; pch != NULL; l++) {
								toks[l] = pch;
								pch = strtok(NULL, "~");
							}
							tokenize_server_list(toks[3]); // Tokenize the message received from server and display the available peers
							free(toks);
							free(pch);
						}
						else if (strncasecmp(buf, "REGISTER", strlen("REGISTER") - 1) == 0){
							char info [strlen("CANNOT REGISTER")];
							strcpy(info, "CANNOT REGISTER");
							if (send(i, info, strlen(info) - 1, 0) == -1) {
								perror("SEND");
							}
						}
					}
					//printf("proj1>>");
				}
			}
		}
	}
	return;
}

/* Tokenize the message received from server and display the available peers */
void tokenize_server_list(char* buf) {
	char* pch = strtok(buf, ";");
	char** client_toks = NULL;
	int i, j, num_avail_peers;
	client_toks = (char **)malloc(sizeof(char*) * 1024);
	for (i = 0; pch != NULL; i++) {
		client_toks[i] = pch;
		pch = strtok(NULL, ";");
	}
	num_avail_peers = i - 1;
	char** addr_toks[4] = { NULL };
	for (i = 1; i <= num_avail_peers; i++) {
		addr_toks[i - 1] = (char **)malloc(sizeof(char*) * 256);
		pch = strtok(client_toks[i], "|");
		for (j = 0; pch != NULL; j++) {
			addr_toks[i - 1][j] = pch;
			pch = strtok(NULL, "|");
		}
	}
	printf("\nLIST OF AVAILABLE PEERS\n\n");
	printf("id: %-35s\t%-15s\t%-6s\n", HOSTNAMESTRING, IPSTRING, PORTSTRING);
	printf("--------------------------------------------------------------------------\n");
	for (i = 0; i < num_avail_peers; i++) {
		printf("%d: ", i + 1);
		printf("%-35s\t", addr_toks[i][0]);
		printf("%-15s\t", addr_toks[i][1]);
		printf("%-6s\n", addr_toks[i][2]);
		int numbytes = sizeof(char*) * 256;
		addr_toks[i][0][numbytes] = '\0';
		addr_toks[i][1][numbytes] = '\0';
		addr_toks[i][2][numbytes] = '\0';
		char *remotehost = addr_toks[i][0];
		char *remoteip = addr_toks[i][1];
		char *remoteport = addr_toks[i][2];
		if (i == 0) {
			// Add available peers to the available Client List
			client_server_head = addIPToList(client_server_head, remotehost, remoteip, 0, atoi(remoteport), 0);
		} else {
			// Add available peers to the available Client List
			addIPToList(client_server_head, remotehost, remoteip, 0, atoi(remoteport), 0);
		}

	}
	printf("\n");
}

/* Validate if the host requested for connection is present in the server list and absent from client connection list*/
int validateHostPort(struct ServerIP *head, char* host, char* port) {
	struct ServerIP * cur;
	int isListed = 0;
	int i;
	cur = head;
	int remote_listen_port = strtol(port, NULL, 10);
	for (i = 1; cur != NULL; i++) {
		if (((strcmp(cur->hostname, host) == 0) || (strcmp(cur->remoteIP, host) == 0)) && cur->listen_port == remote_listen_port) {
			isListed = 1;
			break;
		} else if (cur->next == NULL) {
			cur = NULL;
			break;
		} else
			cur = cur->next;
	}
	return isListed;
}

/* Determine client's IP */
char* whatsclientsip() {
	int sockfd, port;
	struct addrinfo hints, *servinfo, *p;
	int rv, yes = 1;
	struct sockaddr_storage remoteaddr;

	socklen_t addrlen = sizeof(remoteaddr);

	memset(&hints, 0, sizeof(hints));
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
			inet_ntop(AF_INET, &s->sin_addr, clientip, INET6_ADDRSTRLEN);
		} else {
			struct sockaddr_in6 *s = (struct sockaddr_in6 *) &remoteaddr;
			port = ntohs(s->sin6_port);
			inet_ntop(AF_INET6, &s->sin6_addr, clientip, INET6_ADDRSTRLEN);
		}
	}
	freeaddrinfo(servinfo);

	close(sockfd);
	return clientip;
}

/* Custom send method to ensure entire data chunk is sent*/
int sendall(int s, void *buf, int *len)
{
	int total = 0;
	int bytesleft = *len;
	int n;
	while(total < *len) {
		n = send(s, buf+total, bytesleft, 0);
		if (n == -1){
			break;
		}
		total += n;
		bytesleft -= n;
	}
	*len = total;
	return n==-1?-1:0;
}
