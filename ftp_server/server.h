#ifndef SERVER_H_
#define SERVER_H_

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <time.h>
#include <dirent.h>
#include <fcntl.h>

#define BUFSIZE 1024

typedef struct state {
	// Check user logged in
	uint8_t logged_in;

	// Check user name valid
	uint8_t uname_is_valid;
	char *uname;
	char *pw;
	// connection mode
	uint8_t mode;
	// connection handle
	int connection;
	// socket for passive connection
	int sock_pasv;
	// response message
	char *message;
} state_t;

typedef struct command {
	char cmd[5];
	char arg[BUFSIZE];
} command_t;

typedef struct port {
	uint8_t p1;
	uint8_t p2;
} port_t;

typedef enum cmd_list {
	ABOR,
	CWD,
	DELE,
	LIST,
	MDTM,
	MKD,
	NLST,
	PASS,
	PASV,
	PORT,
	PWD,
	QUIT,
	RETR,
	RMD,
	RNFR,
	RNTO,
	SITE,
	SIZE,
	STOR,
	TYPE,
	USER,
	REST
} cmd_list_t;

const char *cmd_list_str[] = {
	"ABOR",
	"CWD",
	"DELE",
	"LIST",
	"MDTM",
	"MKD",
	"NLST",
	"PASS",
	"PASV",
	"PORT",
	"PWD",
	"QUIT",
	"RETR",
	"RMD",
	"RNFR",
	"RNTO",
	"SITE",
	"SIZE",
	"STOR",
	"TYPE",
	"USER",
	"REST"
};

const char *user_name[] = {
	"test",
	"anonymous",
	"public"
};

// Server function
int create_socket(uint32_t port);
void response(command_t *command, state_t *state);
void signal_hanlder(int signal_number);
int lookup(char *needle, const char **haystack, uint8_t count);
void perm_to_str(int perm, char *perm_str);

ssize_t sendfile(int out_fd, int in_fd, off_t *offset, size_t count);

#endif
