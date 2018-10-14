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
#include "clipboard.h"

/*
*	app_teste02:
*		-> This test create 8 processes usig 3 forks at the beggining in order to teste the clipboard to the limit.
*			It sends a string to a random region with a message with the type "Process <pid>: Hello World".
*			After sending, it will paste a random region from the clipboard.
*			After all it closes the connection with the clipboard.
*/

int main(){
	
	fork();
	fork();
	fork();

	char clipboard_dir[50] = SOCK_ADDR;
	input info;

	int pid = getpid();

	int clipboard_fd = clipboard_connect(clipboard_dir);
	if(clipboard_fd== -1) {
		printf("Process %d: Could not connect with clipboard\n", pid);
		return -1;
	}
	
	srand(pid);

    info.region = rand() % 10;
    char *message;
    
    message = malloc(sizeof(char)*120);
	
	// memset(message, 0x00, sizeof(char)*120);

    sprintf(message, "Process %d: Hello World", pid);
	info.size = strlen(message)+1;
    
    if(clipboard_copy(clipboard_fd, info.region, message, info.size)==0)
		printf("Error puting inside clipboard\n");


	info.region = rand() % 10;

	printf("Process %d: Asking for region %d\n", pid, info.region);

	memset(message, 0x00, sizeof(char)*120);
	info.size = sizeof(char)*120;

	if(clipboard_paste(clipboard_fd,info.region, message, info.size)==0)
		printf("Process %d: Error pulling from clipboard\n", pid);
	else {
		fprintf(stderr, "Process %d: Message Received from region %d:\n\t%s\n", pid, info.region, message);
	}

	clipboard_close(clipboard_fd);	

	free(message);

	return 1;
}
