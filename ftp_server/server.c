#include "server.h"

#define FTP_PORT  8021
#define FILE_SIZE 8192

int main()
{
	struct sockaddr_in client_addr;
	socklen_t len = sizeof(client_addr);
	int connection, bytes_read, pid; 
	char buffer[BUFSIZE];
	command_t *cmd = malloc(sizeof(command_t));
	state_t *state	= malloc(sizeof(state_t));
	int sock = create_socket(FTP_PORT);

	while(1) {
		char welcome[20] = "220 Welcome\n";

		connection = accept(sock,
							(struct sockaddr*)&client_addr,
							&len);
		pid = fork();
		memset(buffer, 0, BUFSIZE);

		if(pid < 0) {
			fprintf(stderr, "Can not create child process");
			exit(1);
		}

		if(pid == 0) {
			close(sock);
			write(connection, welcome, strlen(welcome));
			
			// read command from client
			while(bytes_read = read(connection, buffer, BUFSIZE)) {
				signal(SIGCHLD, signal_hanlder);

				if(bytes_read < BUFSIZE) {
					buffer[BUFSIZE-1]='\0';
					printf("User send command %s", buffer);
					// parse command 
					sscanf(buffer, "%s %s", cmd->cmd, cmd->arg);
					state->connection = connection;
					// handle client command
					response(cmd, state);
					memset(buffer, 0, BUFSIZE);
					memset(cmd, 0, sizeof(cmd));
				} else {
					fprintf(stderr, "read error");
				}
			}
			printf("client disconnect\n");
			exit(0);
		} else {
			close(connection);
		}
	} 
	return 0;
}


int create_socket(uint32_t port)
{
	int sock;
	int reuse = 1;
	struct sockaddr_in server_addr = (struct sockaddr_in) {
		AF_INET,
		htons(port),
		INADDR_ANY
	};

	// Create socket
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		fprintf(stderr, "cannot open socket");
		exit(1);
	}

	// Address can be reuse
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

	// Bind to server address
	if(bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
		fprintf(stderr, "cannot bind socket to server address");
		exit(1);
	}	
	
	listen(sock, 5);

	return sock;
}

void response(command_t *command, state_t *state)
{
	uint8_t count = sizeof(cmd_list_str) / sizeof(char *);

	switch(lookup(command->cmd, cmd_list_str, count)) {
		case ABOR:
			if(state->logged_in) {
				state->message = "226 Closing connection\n";
			} else {
				state->message = "530 Not log in\n";
			}
			break;

		case USER:
			{
				uint8_t uname_count = sizeof(user_name)/sizeof(char*);
				if (lookup(command->arg, user_name, uname_count) >= 0) {
					state->uname = malloc(16);
					memset(state->uname, 0, 16);
					strcpy(state->uname, command->arg);
					state->message = "331 Username valid, send password\n";
					state->uname_is_valid = 1;
				} else {
					state->message = "530 Username invalid\n";
				}
			}
			break;

		case PASS:
			if(state->uname_is_valid) {
				state->pw = malloc(16);
				memset(state->pw, 0, 16);
				strcpy(state->pw, command->arg);
				state->message = "230 Logged in\n";
				state->logged_in = 1;
			} else {
				state->message = "500 Log in fail\n";
			}
			break;
		
		case TYPE:
			if(state->logged_in) {
				if(command->arg[0] == 'I') {
					state->message = "200 Switching to binary mode\n";
				} else if (command->arg[0] == 'A') {
					state->message = "200 Switching to ascii mode\n";
				} else {
					state->message = "504 Not support\n";
				}
			} else {
				state->message = "530 Not log in\n";
			}
			break;

		case PASV:
			if(state->logged_in) {
				int ip[4];
				struct sockaddr_in server_addr;
				socklen_t addr_size = sizeof(struct sockaddr_in);
				char *server_addr_str;
				char buffer[255];
			
				// generate a random port for passive mode
				port_t *port = malloc(sizeof(port_t));	
				srand(time(NULL));
				port->p1 = rand() / 256;
				port->p2 = rand() % 256;
				
				close(state->sock_pasv);
				state->sock_pasv = create_socket(256*port->p1 + port->p2);
				getsockname(state->connection, 
							(struct sockaddr*)&server_addr, 
							&addr_size);
				server_addr_str = inet_ntoa(server_addr.sin_addr);
				sscanf(server_addr_str, "%d.%d.%d.%d",
						&ip[0],&ip[1],&ip[2],&ip[3]);
				sprintf(buffer, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)\n",
						ip[0],ip[1],ip[2],ip[3],port->p1,port->p2);
				state->message = buffer;
			} else {
				state->message = "530 Not log in\n";
			}
			break;

		case PWD:
			if(state->logged_in) {
				char cwd[127];
				char buffer[255];
				if(getcwd(cwd, sizeof(cwd)) != NULL) {
					printf("current working dir %s\n", cwd);
					sprintf(buffer, "257 %s\n", cwd);
					state->message = buffer;
				} else {
					state->message = "550 Failed to get current working directory\n";
				}
			} else {
				state->message = "530 Not log in\n";
			}
			break;

		case LIST:
			{
				struct dirent *dr;
				struct stat stat_buf;
				time_t time;
				char cwd_ex[255], cwd[255];
				char buffer[BUFSIZE], time_buf[80];
				char perm[10];
				DIR *dp;
				int connection;
				struct sockaddr_in client_addr;
				socklen_t len = sizeof(struct sockaddr_in);

				if (state->logged_in) {
					memset(cwd, 0, 255);
					memset(cwd_ex, 0, 255);
					memset(buffer, 0, BUFSIZE);
					memset(time_buf, 0, 80);
					// save origin cwd
					getcwd(cwd_ex,sizeof(cwd_ex));

					if(strlen(command->arg) > 0) {
						chdir(command->arg);
					}

					getcwd(cwd, sizeof(cwd));
					dp = opendir(cwd);
					if(!dp) {
						state->message = "550 Fail to open dir\n";
					} else {
						connection = accept(state->sock_pasv,
											(struct sockaddr*)&client_addr,
											&len);
						// loop through directory
						while((dr = readdir(dp)) != NULL) {
							if(stat(dr->d_name, &stat_buf) == -1) {
								fprintf(stderr, "Error reading file stats\n");
							} else {
								//convert permission
								memset(perm, 0, 10);
								perm_to_str((stat_buf.st_mode & ALLPERMS), perm);
								// convert time
								time= stat_buf.st_mtime;
								strftime(time_buf, 80, "%b %d %H:%M", localtime(&time));

								sprintf(buffer, "%c%s %5ld %4d %4d %8ld %s %s\r\n", 
											(dr->d_type==DT_DIR)?'d':'-',
											perm,
											stat_buf.st_nlink,
											stat_buf.st_uid, 
											stat_buf.st_gid,
											stat_buf.st_size,
											time_buf,
											dr->d_name);
								write(connection, buffer, strlen(buffer));
							}
						}
						state->message = "226 Directory sent\n";
						close(connection);
						close(state->sock_pasv);

					}
					closedir(dp);
					chdir(cwd_ex);
				} else {
					state->message = "530 Not log in\n";
				}
			}
			break;

		case CWD:
			if(state->logged_in) {
				if(chdir(command->arg) == 0) {
					state->message = "250 Directory changed\n";
				} else {
					state->message = "550 Fail to change directory\n";
				}
			} else {
				state->message = "530 Not log in\n";
			}
			break;

		case DELE:
			if (state->logged_in) {
				if (unlink(command->arg) == -1) {
					state->message = "550 File unavailable\n";
				} else {
					state->message = "250 Requested action done\n";
				}

			} else {
				state->message = "530 Not log in\n";
			}
			break;

		case RMD:
			if (state->logged_in) {
				if (rmdir(command->arg) != 0) {
					state->message = "550 Directory unavailable\n";
				} else {
					state->message = "250 Requested file action done\n";
				}
			} else {
				state->message = "530 Not log in\n";
			} 
	 		break;

		case SIZE:
			if (state->logged_in) {
				struct stat stat_buf;
				char buffer[255];

				if(stat(command->arg, &stat_buf) == -1) {
					state->message = "550 Could not get file size\n";
				} else {
					sprintf(buffer, "213 %ld\n", stat_buf.st_size);
					state->message = buffer;
				}
			} else {
				state->message = "530 Not log in\n";
			} 
	 		break;

		case MKD:
			if (state->logged_in) {
				char cwd[255];
				char buffer[BUFSIZE * 2];

				if (command->arg[0] == '/') {
					if (mkdir(command->arg, 0777) == 0) {
						sprintf(buffer, "257 %s New directory created\n", command->arg);
						state->message = buffer;
					} else {
						state->message = "550 Fail to create directory\n";
					}
				} else {
					getcwd(cwd, sizeof(cwd));
					if (mkdir(command->arg, 0777) == 0) {
						sprintf(buffer, "257 %s %s New directory created\n",
										cwd, command->arg);
						state->message = buffer;
					} else {
						state->message = "550 Fail to create directory\n";
					}
				}
			} else {
				state->message = "530 Not log in\n";
			} 
	 		break;

		case RETR:
			if(fork() == 0) {
				int fd;
				struct stat stat_buf;
				int connection;
				off_t offset = 0;
				int file_sent = 0;
				struct sockaddr_in client_addr;
				socklen_t len = sizeof(struct sockaddr_in);

				if(state->logged_in) {
					fd = open(command->arg, O_RDONLY);
					if((fd > 0) && (access(command->arg, R_OK) == 0)) {
						fstat(fd, &stat_buf);
						state->message = "150 Open BINARY mode connection\n";
						write(state->connection, state->message, strlen(state->message));

						connection = accept(state->sock_pasv,
											(struct sockaddr*)&client_addr,
											&len);
						close(state->sock_pasv);

						file_sent = sendfile(connection, fd, &offset, stat_buf.st_size);
						if (file_sent != stat_buf.st_size) {
							fprintf(stderr, "file sent fail\n");
							state->message = "550 Fail to get file\n";
							exit(0);
						} else {
							state->message = "226 File sent\n";
						}
					} else {
						state->message = "550 Fail to read file\n";
					}

				} else {
					state->message = "530 Not log in\n";
				}
				close(fd);
				close(connection);
				write(state->connection, state->message, strlen(state->message));
				exit(0);			
			}
			close(state->sock_pasv);
			break;

		case STOR:
			if(fork() == 0) {
				int connection, fd;
				int pipefd[2];
				struct sockaddr_in client_addr;
				socklen_t len = sizeof(struct sockaddr_in);
				int res = 1;
				uint32_t flag = SPLICE_F_MOVE | SPLICE_F_MORE;

				FILE *fp = fopen(command->arg, "w");

				if(fp == NULL) {
					fprintf(stderr, "file open fail\n");
					state->message = "550 No such file\n";
					exit(1);
				}

				if(state->logged_in) {
					fd = fileno(fp);
					if(pipe(pipefd) < 0) {
						fprintf(stderr,"fail to create pipe\n");
						exit(1);
					}
					connection = accept(state->sock_pasv,
											(struct sockaddr*)&client_addr,
											&len);
					close(state->sock_pasv);
					state->message = "125 Start tranfer file\n";
					write(state->connection, state->message, strlen(state->message));

					while((res = splice(connection, 0, pipefd[1], NULL, FILE_SIZE, flag))) {
						splice(pipefd[0], NULL, fd, 0, FILE_SIZE, flag);
					}
					
					if(res < 0) {
						fprintf(stderr, "splice fail\n");
						exit(1);
					}
					state->message = "226 File received\n";
					close(connection);
					close(fd);
				}	
			}
			close(state->sock_pasv);
			break;

		case QUIT:
			state->message = "221 Quit\n";
			write(state->connection, state->message, strlen(state->message));
			close(state->connection);
			exit(0);
			break;

		default:
			state->message = "500 Unknown command\n";
			break;
	}
	write(state->connection, state->message, strlen(state->message));
}

int lookup(char *needle, const char **haystack, uint8_t count)
{
	for (uint8_t i = 0; i < count; i++) {
		if(strcmp(needle, haystack[i]) == 0) {
			return i;
		}
	}

	return -1;
}

void signal_hanlder(int signal_number)
{
	int status;
	wait(&status);
}

void perm_to_str(int perm, char *perm_str)
{
	uint8_t r,w,x;
	uint32_t tmp;
	char buffer[4];

	for (int i = 6; i >= 0;i -= 3) {
		tmp = ((perm & ALLPERMS) >> i) & 0x7;
		memset(buffer, 0, 4);
		r = (tmp >> 2) & 1;
		w = (tmp >> 1) & 1;
		x = (tmp >> 0) & 1;
		sprintf(buffer, "%c%c%c", r?'r':'-', w?'w':'-', x?'x':'-');
		strcat(perm_str, buffer);
	}
}

ssize_t sendfile(int out_fd, int in_fd, off_t *offset, size_t count)
{
	off_t org;
	char buffer[FILE_SIZE];
	size_t to_read, num_read, num_sent, to_sent;

	if(offset != NULL) {
		// save current file offset 
		org = lseek(in_fd, 0, SEEK_CUR);
		if (org == -1) {
			return -1;
		}
		if(lseek(in_fd, *offset, SEEK_SET) == -1) {
			return -1;
		}
	}

	to_sent = 0;
	while(count > 0) {
		to_read = count < FILE_SIZE ? count : FILE_SIZE;
		num_read = read(in_fd, buffer, to_read);
		if(num_read == -1) {
			return -1;
		} else if(num_read == 0) {
			break;
		}

		num_sent = write(out_fd, buffer, num_read);
		if (num_sent == -1) {
			return -1;
		} else if(num_sent ==0) {
			fprintf(stderr,"write tranfer 0B\n");
			exit(1);
		}

		count -= num_sent;
		to_sent += num_sent;
	}

	if(offset != NULL) {
		*offset = lseek(in_fd, 0, SEEK_CUR);
		if(*offset == -1) {
			return -1;
		}
		if(lseek(in_fd, 0, SEEK_SET) == -1) {
			return -1;
		}
	}

	return to_sent;
}