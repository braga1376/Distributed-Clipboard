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

int clipboard_connect(char *clipboard_dir) {

	int csocket;
	struct sockaddr_un localc;

	if ((csocket = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror("client socket");
		return -1; // acho que é paara retornar -1
	}
	
	memset(&localc,0,sizeof(localc));
	localc.sun_family = AF_UNIX;
	strcpy(localc.sun_path, clipboard_dir);

	if(connect(csocket, (struct sockaddr *) &localc, sizeof(localc)) == -1) {
		perror("client connect");
		return -1;  // acho que é paara retornar -1
	}

	return csocket;	
}

int clipboard_copy(int clipboard_id, int region, void *buf, size_t count) {
	
	int nleft, nwritten;
	char cinfo[sizeof(input)], *ptr;
	input info;

	info.size = count;
	info.region = region;
	info.type = COPY;
	// printf("Message: %s  Size: %ld   Region: %d  Type: %d\n", (char *)buf, info.size, info.region, info.type);

	memset(cinfo, 0x00, sizeof(input));
	memcpy(cinfo, &info, sizeof(input));

	ptr = cinfo;
	nleft = sizeof(cinfo);
	while(nleft>0) {
		nwritten=write(clipboard_id,ptr,nleft);
		if(nwritten<=0)return 0;//error
		else if(nwritten==0)break;
		nleft=nleft - nwritten;
		ptr+=nwritten;
	}
	
	ptr = buf;
	nleft = info.size;
	while(nleft>0) {
		nwritten=write(clipboard_id,ptr,nleft);
		if(nwritten<=0)return 0;//error
		else if(nwritten==0)break;
		nleft=nleft - nwritten;
		ptr+=nwritten;
	}

	if(nleft == -1)
		return 0;
	else
		return nwritten;
}

int clipboard_paste(int clipboard_id, int region, void *buf, size_t count) {
		
	int nleft, nwritten;
	int nread;
	char cinfo[sizeof(input)], *ptr;
	input info;

	info.region = region;
	info.type = PASTE;
	info.size = -1;

	memset(cinfo, 0x00, sizeof(input));
	memcpy(cinfo, &info, sizeof(input));

	ptr = cinfo;
	nleft = sizeof(cinfo);
	while(nleft>0) {
		nwritten=write(clipboard_id,ptr,nleft);
		if(nwritten<=0)return 0;//error
		else if(nwritten==0)break;
		nleft=nleft - nwritten;
		ptr+=nwritten;
	}

	memset(cinfo, 0x00, sizeof(input));
	ptr = cinfo;
	
	nleft = sizeof(cinfo);
	while(nleft>0) {	
		nread = read(clipboard_id,ptr,nleft);
		if(nread==-1)
			return 0;//error
		else if(nread==0)
			break;
		nleft-=nread;
		ptr+=nread;
	}
	if(nread == 0) {
		return 0;
	}

	memset(&info, 0x00, sizeof(input));
	memcpy(&info, cinfo, sizeof(input));

	if(info.type == PASTE) {
		if(info.size == -1) {
			printf("There is nothing in that region\n");
			return 0;
		} else {
			if(info.size>count)
				nleft = count;
			else
				nleft = info.size;

			ptr = buf;
			while(nleft>0) {	
				nread = read(clipboard_id,ptr,nleft);
				if(nread==-1)
					return 0;//error
				else if(nread==0)
					return 0;
				nleft-=nread;
				ptr+=nread;
			}
		}
	} else {
		printf("Received wrong message type.\n");
		return 0;
	}
	
	return nread;
}	

int clipboard_wait(int clipboard_id, int region, void *buf, size_t count) {

	int nleft, nwritten;
	int nread;
	char cinfo[sizeof(input)], *ptr;
	input info;

	info.size = count;
	info.region = region;
	info.type = WAIT;

	memset(cinfo, 0x00, sizeof(input));
	memcpy(cinfo, &info, sizeof(input));

	ptr = cinfo;
	nleft = sizeof(cinfo);
	while(nleft>0) {
		nwritten=write(clipboard_id,ptr,nleft);
		if(nwritten<=0)return 0;//error
		else if(nwritten==0)break;
		nleft=nleft - nwritten;
		ptr+=nwritten;
	}

	memset(cinfo, 0x00, sizeof(input));
	ptr = cinfo;
	
	nleft = sizeof(input);
	while(nleft>0) {	
		nread = read(clipboard_id,ptr,nleft);
		if(nread==-1)
			return 0;//error
		else if(nread==0)
			break;
		nleft-=nread;
		ptr+=nread;
	}
	if(nread == 0) {
		return 0;
	}

	memset(&info, 0x00, sizeof(input));
	memcpy(&info, cinfo, sizeof(input));

	if(info.type == WAIT) {
		// buf = realloc(buf, info.size);
		// nleft = info.size;
		if(info.size>count)
			nleft = count;
		else
			nleft = info.size;
		ptr = buf;
		while(nleft>0) {	
			nread = read(clipboard_id,ptr,nleft);
			if(nread==-1)
				return 0;//error
			else if(nread==0)
				return 0;
			nleft-=nread;
			ptr+=nread;
		}
	} else {
		printf("Received wrong message type.\n");
		return 0;
	}

	return nread;
}

void clipboard_close(int clipboard_id) {

	close(clipboard_id);
	return;
}
