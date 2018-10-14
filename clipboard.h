#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/un.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>

#define SOCK_ADDR "/home/francisco/IST/PSis/Project/clipboard_dir4"

#define COPY 0
#define PASTE 1
#define ADDR 2
#define CLOSED 3
#define WAIT 4

#define MAXINPUTSIZE 128

#define max(A,B) ((A)>=(B)?(A):(B))

int clipboard_connect(char * clipboard_dir);
int clipboard_copy(int clipboard_id, int region, void *buf, size_t count);
int clipboard_paste(int clipboard_id, int region, void *buf, size_t count);
void clipboard_close(int clipboard_id);
int clipboard_wait(int clipboard_id, int region, void *buf, size_t count);

typedef struct clip {
	char *message;
	size_t size;
} clip;

typedef struct input {
	int type;
	int region;
	size_t size;
} input;

typedef struct accept_connections_args {
	struct clip *clipboard;
	struct sockaddr_un locals;
	struct sockaddr_in remotes;
	int sock_fd;
	int son_fd;
} accept_connections_args;

typedef struct connection_thread {
	pthread_t thread_id;
	struct connection_thread *next;
} connection_thread;

typedef struct client_thread_args {
	struct clip *clipboard;
	int client_fd;
	struct connection *son_head;
	struct connection *client_head;
	/0/ struct connection *client_this;
} client_thread_args;

typedef struct father_thread_args {
	struct clip *clipboard;
	struct connection *son_head;
	struct connection *client_head;
} father_thread_args;

typedef struct son_thread_args {
	struct clip *clipboard;
	int son_fd;
	struct connection *son_head;
	struct connection *client_head;
	// struct connection *client_this;
} son_thread_args;

typedef struct connection {
	int nwait[10];
	int fd;
	struct connection *next;
} connection;


