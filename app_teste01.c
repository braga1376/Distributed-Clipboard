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
*	app_teste01:
*		-> This test has an user interface that allows to copy, paste, wait for a change in a certain region and close the connection.
*			To copy, it reads from the terminal a certain input to send to a certain region.
*			To paste, the region and the size you want to read are chosen.
*			To wait, the region and the size you want to read are chosen, and it will be blocked until a change is made in the clipboard.
*/

int main(){

	char clipboard_dir[50] = SOCK_ADDR;
	char quest, cinfo[sizeof(input)], *ptr;
	char *message;
	input info;
	int flag = 1, nleft, nread, counter;
	fd_set fds;

	int clipboard_fd = clipboard_connect(clipboard_dir);
	if(clipboard_fd== -1) {
		printf("Could not connect with clipboard\n");
		return -1;
	}
	

	while(1){

		flag = 1;

		fprintf(stderr, "\nWould you like to copy or to paste? \n- c to copy; \n- v to paste; \n- w to wait for change; \n- f to finish the session.\n");

		FD_ZERO(&fds);
		FD_SET(0, &fds);
		FD_SET(clipboard_fd, &fds);

		counter = select(clipboard_fd+1, &fds, (fd_set *)NULL, (fd_set *)NULL, (struct timeval *)	NULL);
		if(counter <= 0) {
			fprintf(stderr, "Error counting fd's\n");
			break;
		}
		if(FD_ISSET(0, &fds)) {
			flag = 0;
			scanf("%c", &quest);
			if(quest == 'c' || quest == 'C')
			{
				getchar();
				printf("Data?\n");
				char *line = NULL;
			    size_t n = 0;
			    getline(&line, &n, stdin);
			    message = malloc(n+1);
				strcpy(message, line);
			    free(line);
			    message[n] = '\0';
				printf("Region? \n");
				if(!scanf("%d", &info.region)) {
					fprintf(stderr, "Region must be one number");
					free(message);
					continue;
				}
				if(info.region >= 0 && info.region < 10){
					if(clipboard_copy(clipboard_fd,info.region, message, strlen(message)+1)==0)
						printf("Error puting inside clipboard\n");
				} else {
					printf("Region should be between 0 and 9.\n");
				}
				free(message);
			}
			else if(quest == 'v' || quest == 'V')
			{
				printf("Region? \n");
				if(!scanf("%d", &info.region)) {
					fprintf(stderr, "Region must be one number");
					continue;
				}
				printf("Size? \n");
				if(!scanf("%zu", &info.size)) {
					fprintf(stderr, "Size must be one number");
					continue;
				}
				message = malloc(info.size+1);
				if(info.region >= 0 && info.region < 10){
					if(clipboard_paste(clipboard_fd,info.region, message, info.size+1)==0)
						printf("Error pulling from clipboard\n");
					else {
						message[info.size] = '\0';
						fprintf(stderr, "Message Received from region %d:\n%s\n", info.region, message);
					}		
				} else {
					printf("Region should be between 0 and 9.\n");
				}

				free(message);
			}
			else if(quest == 'f' || quest == 'F'){
				clipboard_close(clipboard_fd);
				fprintf(stderr, "Successfully closed clipboard connection.\n");
				break;
			} else if(quest == 'w' || quest == 'W'){
				printf("Region? \n");
				if(!scanf("%d", &info.region)) {
					fprintf(stderr, "Region must be one number");
					continue;
				}
				printf("Size? \n");
				if(!scanf("%zu", &info.size)) {
					fprintf(stderr, "Size must be one number");
					continue;
				}
				message = malloc(info.size+1);
				if(info.region >= 0 && info.region < 10){
					if(clipboard_wait(clipboard_fd, info.region, message, info.size+1) == 0)
						printf("Error pulling from clipboard (wait)\n");
					else{
						message[info.size] = '\0';
						fprintf(stderr, "Region %d changed:\n%s\n", info.region, message);
					}	
				} else {
					printf("Region should be between 0 and 9.\n");
				}

				free(message);
			}
			getchar();
		}
		if(flag == 1 && FD_ISSET(clipboard_fd, &fds)) {
			nleft = sizeof(cinfo);
			while(nleft>0) {	
				memset(cinfo, 0x00, sizeof(input));
				ptr = cinfo;
				nread = read(clipboard_fd,ptr,nleft);
				if(nread==-1)
					return 0;//error
				else if(nread==0)
					break;
				nleft-=nread;
				ptr+=nread;
			}
			if(nread == 0) {
				clipboard_close(clipboard_fd);
				fprintf(stderr, "Successfully closed clipboard connection.\n");
				break;
			}
			memset(&info, 0x00, sizeof(input));
			memcpy(&info, cinfo, sizeof(input));
			if(info.type == CLOSED) {
				clipboard_close(clipboard_fd);
				fprintf(stderr, "Successfully closed clipboard connection.\n");
				break;
			}
		}
	}	
	return 1;
}
