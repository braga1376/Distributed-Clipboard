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

pthread_mutex_t m[10], s, c, f, p;

int father_fd = -1, father_port = -1;
char father_ip[128];

/*
 * Function: free_clipboard
 * 		Loop that frees all the allocated memory for the clipboard
 *
 *   clipboard : pointer to the first position of the clipboard
 *
 */
void free_clipboard(struct clip *clipboard){
	int i;
	for(i = 0; i < 10; i++)
		free(clipboard[i].message);

	free(clipboard);

	return;
}

/*
 * Function: free_list_thread
 *   Receives a list of connection threads (with sons or with apps) and frees it
 *
 *   head : head of the list of the threads
 *
 */
void free_list_thread(struct connection_thread *head){

	connection_thread *aux1;

	while(head != NULL) {
		aux1 = head;
		head = head->next;
		free(aux1);
	}

	return;
}

/*
 * Function: free_list_con
 *   Receives a list of connections (with sons or with apps) and frees it
 *
 *   head : head of the list of the threads
 *
 */
void free_list_con(struct connection *head){

	struct connection *aux1;

	while(head != NULL) {
		aux1 = head;
		head = head->next;
		free(aux1);
	}

	return;
}

/*
 * Function: remove_connection
 *   Receives a list of connections and a file descriptor and removes 
 *   from the list the connection correspondent to that file descriptor
 *   and in case of need refreshes the head of the list
 *
 *   head : head of the connections list
 *   fd : file descriptor of a connection 
 *
 */
void remove_connection(struct connection *head, int fd) {

	struct connection *aux1 = head, *aux2;

	if(head->fd == fd) {
		if(head->next != NULL) {
			head->fd = head->next->fd;
			aux1 = head->next;
			head->next = head->next->next;
			free(aux1);

		} else {
			head->fd = -1;
		}
	} else {
		aux2 = head;
		aux1 = head->next;
		while(aux1 != NULL) {
			if(aux1->fd == fd) {
				aux2->next = aux2->next->next;
				free(aux1);
				break;
			} else {
				aux2 = aux1;
				aux1 = aux1->next;	
			}
		}

	}

	return;
}

/*
 * Function: wait connection
 *   Receives the connections list of the applications, a file descriptor 
 *   and a region. Activates the wait flag on the specified region from the connection
 *   identified by the file descriptor.
 *
 */
void wait_connection(struct connection *head, int fd, int region) {

	struct connection *aux = head;

	if(head->fd == fd) {
		head->nwait[region] = 1;
	} else {
		aux = head;
		while(aux!= NULL) {
			if(aux->fd == fd) {
				aux->nwait[region] = 1;
			}
			aux = aux->next;
		}
	}
	return;
}

/*
 * Function: print_clipboard
 *   Receives a pointer to the clipboard and prints it to the terminal
 *   
 */
void print_clipboard(struct clip *clipboard){
	int i;
	pthread_mutex_lock(&p);
	fprintf(stderr, "\nPresent Clipboard:\n");
	for(i = 0; i < 10; i++) {
		if(clipboard[i].message != NULL){
			pthread_mutex_lock(&(m[i]));
			if(clipboard[i].size!=-1)
				fprintf(stderr, "\tRegion: %d  Size: %ld  Data: %s\n", i, clipboard[i].size, clipboard[i].message);
			pthread_mutex_unlock(&(m[i]));
		}
	}
	pthread_mutex_unlock(&p);


	return;
}

/*
 * Function: send_message
 *   Sends a message through tcp sockets
 *
 *   fd : file descriptor for the connection 
 *   message : pointer to char where the message to send is stored
 *   input fields: all the fields to be send case the message sends an input structure 
 *
 */
void send_message(int fd, char *message, int region, int type, size_t size){

	int nleft, nwritten;
	char cinfo[sizeof(input)], *ptr;
	struct input info;

	info.size = size;
	info.region = region;
	info.type = type;

	memset(cinfo, 0x00, sizeof(input));
	memcpy(cinfo, &info, sizeof(input));

	ptr = cinfo;
	nleft = sizeof(cinfo);
	while(nleft>0) {
		nwritten=write(fd,ptr,nleft);
		if(nwritten<=0) {
			perror("\nError in write");
			break;
		}
		else if(nwritten==0)break;
		nleft=nleft - nwritten;
		ptr+=nwritten;
	}

	if(type != CLOSED) {
		ptr = message;
		nleft = info.size;
		while(nleft>0) {
			nwritten=write(fd,ptr,nleft);
			if(nwritten<=0){
				perror("\nError in write");
				break;
			}
			else if(nwritten==0)break;
			nleft=nleft - nwritten;
			ptr+=nwritten;
		}
	}
	
	
	return;
}

/*
 * Function: receive_message
 *   receives a tcp message on a determined file descriptor
 *
 *   fd : file descriptor where the connection is set 
 *   info : adress of an input structure that will manage communications
 *
 *   returns: char with a content of a message received 
 */
char *receive_message(int fd, struct input *info){
	
	int nleft, nread;
	char cinfo[sizeof(input)], *ptr, *message;

	ptr = cinfo;
	
	nleft = sizeof(input);
	while(nleft>0) {	
		nread = read(fd,ptr,nleft);
		if(nread==-1) {
			(*info).type = CLOSED;
			return NULL;
		}
		else if(nread==0){
			(*info).type = CLOSED;
			return NULL;
		}
		nleft-=nread;
		ptr+=nread;
	}

	memset(info, 0x00, sizeof(input));
	memcpy(info, cinfo, sizeof(input));

	if((*info).type != PASTE && (*info).type != WAIT && (*info).type != CLOSED) {
		message = malloc((*info).size);
		nleft = (*info).size;
		ptr = message;
		while(nleft>0) {	
			nread = read(fd,ptr,nleft);
			if(nread==-1) {
				(*info).type = CLOSED;
				return NULL;
			}
			else if(nread==0){;
				(*info).type = CLOSED;
				return NULL;
			}
			nleft-=nread;
			ptr+=nread;
		}
	}

	return message;
}

/*
 * Function: send_clipboard
 *   Sends the whole clipboard to the son set in the file descriptor son_fd
 *
 *   clipboard: pointer to the clipboard to be sent 
 *   son_fd: file descriptor of the son that will receive the clipboard
 *
 */
void send_clipboard(struct clip *clipboard, int son_fd){
	int i;
	for(i = 0; i < 10; i++) {
		if((clipboard[i].size) != -1){
			pthread_mutex_lock(&(m[i]));
			send_message(son_fd, clipboard[i].message, i, COPY, clipboard[i].size);
			pthread_mutex_unlock(&(m[i]));
		}
	}

	return;
}

/*
 * Function: send_father_to_sons
 *   Sets the messages that will be sent to the sons with the the information of their grandfather
 *
 *   son_head : pointer to the head of the son's list
 *   fields with information about the father 
 *
 */
void send_father_to_sons(struct connection *son_head, int father_port, char father_ip[128]){
	
	struct connection *aux;

	pthread_mutex_lock(&s);
	if(son_head->fd != -1) {
		if(son_head->next == NULL && son_head->fd != -1) { //só haver um filho
			aux = son_head;
			send_message(aux->fd, father_ip, father_port, ADDR, sizeof(char)*128);
		} else {
			aux = son_head;
			while(aux != NULL) { //varios filhos percorrer a lista
				if(aux->fd != -1) {
					send_message(aux->fd, father_ip, father_port, ADDR, sizeof(char)*128);
				}
				aux = aux->next;
			}
		}
	}
	pthread_mutex_unlock(&s);
}

/*
 * Function: send_clients
 *   Sends region of clipboard to any application that has the wait flag on for that region
 *   
 *   client_head : pointer to the head of the client's list
 *   message : content of clipboard's region
 *   region/size : input parameters 
 *
 */
void send_clients(struct connection *client_head, char *message, int region, size_t size){

	struct connection *aux;
	pthread_mutex_lock(&c);
	if(client_head->fd != -1) {
		if(client_head->next == NULL && client_head->fd != -1) {
			if(client_head->nwait[region] == 1){
				send_message(client_head->fd, message, region, WAIT, size);
				client_head->nwait[region] = 0;
			}
		} else {
			aux = client_head;
			while(aux != NULL) { //varios filhos percorrer a lista
				if(aux->fd != -1) {
					if(aux->nwait[region] == 1){
						send_message(aux->fd, message, region, WAIT, size);
						aux->nwait[region] = 0;
					}
				}
				aux = aux->next;
			}
		}
	}
	pthread_mutex_unlock(&c);
	return;
}

/*
 * Function: send_clients
 *   Sends region of clipboard to any application that has the wait flag on for that region
 *   
 *   client_head : pointer to the head of the client's list
 *   message : content of clipboard's region
 *   region/size : input parameters 
 *
 */
void send_close_clients(struct connection *client_head){

	struct connection *aux;

	if(client_head->fd != -1) {
		if(client_head->next == NULL && client_head->fd != -1) {
			send_message(client_head->fd, NULL, -1, CLOSED, -1);
		} else {
			aux = client_head;
			while(aux != NULL) { //varios filhos percorrer a lista
				if(aux->fd != -1) {
					send_message(aux->fd, NULL, -1, CLOSED, -1);
				}
				aux = aux->next;
			}
		}
	}
	return;
}

/*
 * Function: send_relatives
 *   Sends a region of the clipboard to his relatives (father and sons)
 *
 *   send_father : flag to see if it's supposed to send to father 
 *   son_head : pointer to the list of sons
 *   message : content to be sent
 *   input fields : to be sent on the message 
 *
 *   returns: the square of the larger of n1 and n2 
 */
void send_relatives(int send_father, struct connection *son_head, char *message, int region, size_t size){

	struct connection *aux;
	
	pthread_mutex_lock(&f);
	if(father_fd != -1  && send_father == 1){
		send_message(father_fd, message, region, COPY, size);
	}
	pthread_mutex_unlock(&f);
	pthread_mutex_lock(&s);
	if(son_head != NULL) {
		if(son_head->next == NULL && son_head->fd != -1) {
			send_message(son_head->fd, message, region, COPY, size);
		} else {
			aux = son_head;
			while(aux != NULL) { //varios filhos percorrer a lista
				if(aux->fd != -1) {
					send_message(aux->fd, message, region, COPY, size);
				}
				aux = aux->next;
			}
		}
	}
	pthread_mutex_unlock(&s);
	return;
}

/*
 * Function: ftaher_connect
 *   creates a TCP connection to the father of clipboard 
 *
 *   ip: father ip
 *   port : father port
 *
 *   returns: returns the file descriptor to connect with father 
 */
int father_connect(char ip[128], int port){

	int fd;
	struct sockaddr_in addr;

	// Create TCP SOCKET (receive)

	memset((void*)&addr,(int)'\0',sizeof(addr));
	addr.sin_family=AF_INET;
	inet_aton(ip, &addr.sin_addr);
	addr.sin_port=htons(port);

	if((fd=socket(AF_INET,SOCK_STREAM,0))==-1) {
		perror("tcp socket");
		return -1;
	}
	
	if(connect(fd,(struct sockaddr*)&addr, sizeof(addr)) == -1) {
		perror("tcp connect");
		return -1;
	}

	return fd;
}

/*
 * Function: father_Session
 *   Runs on a thread that is responsible for managing all the connections with clipboard's father
 *   Has an loop that is waiting to receive messages on father's file descriptor and react according to 
 *   the message's content
 *
 *   arguments : structure with all the fields needed to deal with father's connections 
 *
 */
void *father_session(void *arguments){

	struct father_thread_args *args = (struct father_thread_args *) arguments;
	struct clip *clipboard = args->clipboard;
	struct connection *son_head = args->son_head;
	struct connection *client_head = args->client_head;

	int alter = 0;
	char *message;
	struct input info;

	while(1) {
		memset(&info, 0x00, sizeof(input));
		
		message = receive_message(father_fd, &info);
		
		if(info.type == COPY) {
			if(info.region >= 0 && info.region < 10) {

				pthread_mutex_lock(&(m[info.region]));
				if(memcmp(clipboard[info.region].message, message, info.size) != 0) {
					clipboard[info.region].size = info.size;
					clipboard[info.region].message = realloc(clipboard[info.region].message, info.size);
					memcpy(clipboard[info.region].message, message, info.size);
					send_relatives(0, son_head, message, info.region, info.size);
					send_clients(client_head, message, info.region, info.size);
					alter = 1;
				}
				pthread_mutex_unlock(&(m[info.region]));
				free(message);
				if(alter == 1) {
					print_clipboard(clipboard);
					alter = 0;
				}
			} else {
				if(message != NULL)
					free(message);
				fprintf(stderr, "Region shoul be between 0 and 9\n");
			}

		} else if(info.type == ADDR) {
			printf("Closing father connection\n");
			if(info.region == -1) {
				pthread_mutex_lock(&f);
				father_port = info.region;
				send_message(father_fd, NULL, -1, CLOSED, 0);
				close(father_fd);
				father_fd = -1;
				pthread_mutex_unlock(&f);
				free(message);
				break;
			} else {
				pthread_mutex_lock(&f);
				send_message(father_fd, NULL, -1, CLOSED, 0);
				close(father_fd);
				strcpy(father_ip, message);
				father_port = info.region;
				father_fd = father_connect(father_ip, father_port);
				pthread_mutex_unlock(&f);
				free(message);
				printf("New father connection\n");
			} 
		} else if(info.type == CLOSED) {
			pthread_mutex_lock(&f);
			close(father_fd);
			father_port = -1;
			father_fd = -1;
			pthread_mutex_unlock(&f);
			break;
		}
	
	}
	return NULL;
}

/*
 * Function: son_Session
 *   Runs on a thread (for each one of the sons) that is responsible for managing all the connections with
 *   one of the clipboard sons.
 *   Has an loop that is waiting to receive messages on a determined son's file descriptor and react according to 
 *   the message's content
 *
 *   arguments : structure with all the fields needed to deal with son's connections 
 *
 */
void *son_session(void *arguments){

	struct son_thread_args *args = (struct son_thread_args *) arguments;

	struct clip *clipboard  = args->clipboard;
	int son_fd = args->son_fd;
	struct connection *son_head = args->son_head;
	struct connection *client_head = args->client_head;	
	
	pthread_mutex_unlock(&s);

	char *message;
	input info;

	send_clipboard(clipboard, son_fd);

	while(1) {
		memset(&info, 0x00, sizeof(input));
		
		message = receive_message(son_fd, &info);
		
		if(info.type == COPY) {
			if(info.region >= 0 && info.region < 10) {

				pthread_mutex_lock(&(m[info.region]));
				clipboard[info.region].size = info.size;
				clipboard[info.region].message = realloc(clipboard[info.region].message, info.size);
				memcpy(clipboard[info.region].message, message, info.size);
				pthread_mutex_unlock(&(m[info.region]));
				send_relatives(1, son_head, message, info.region, info.size);
				send_clients(client_head, message, info.region, info.size);
				free(message);
			} else {
				if(message != NULL)
					free(message);
				fprintf(stderr, "Region shoul be between 0 and 9\n");
			}
			print_clipboard(clipboard);
		} else if(info.type == CLOSED) {
			pthread_mutex_lock(&s);
			remove_connection(son_head, son_fd);
			pthread_mutex_unlock(&s);
			close(son_fd);
			break;
		}
	}
	return NULL;
}

/*
 * Function: client_Session
 *   Runs on a thread that is responsible for managing all the connections with clipboard's applications
 *   Has an loop that is waiting to receive messages on every application 's file descriptor and react according to 
 *   the message's content
 *
 *   arguments : structure with all the fields needed to deal with application's connections 
 *
 */
void *client_session(void *arguments){
	
	struct client_thread_args *args = (struct client_thread_args *) arguments;

	struct clip *clipboard = args->clipboard;
	int client_fd = args->client_fd;
	struct connection *son_head = args->son_head;
	struct connection *client_head = args->client_head;
	pthread_mutex_unlock(&c);

	char *message;
	input info;
		
	while(1) {

		memset(&info, 0x00, sizeof(input));
		
		message = receive_message(client_fd, &info);
		if(info.type == COPY) {
			if(info.region >= 0 && info.region < 10) {
				pthread_mutex_lock(&(m[info.region]));
				clipboard[info.region].size = info.size;
				clipboard[info.region].message = realloc(clipboard[info.region].message, info.size);
				memcpy(clipboard[info.region].message, message, info.size);
				pthread_mutex_unlock(&(m[info.region]));
				send_relatives(1, son_head, message, info.region, info.size);
				send_clients(client_head, message, info.region, info.size);
				free(message);
			} else {
				if(message != NULL)
					free(message);
				fprintf(stderr, "Region shoul be between 0 and 9\n");
			}
			
			print_clipboard(clipboard);
		} else if(info.type == PASTE) {
			if(info.region >= 0 && info.region < 10) {
				pthread_mutex_lock(&(m[info.region]));
				send_message(client_fd, clipboard[info.region].message, info.region, PASTE, clipboard[info.region].size);
				pthread_mutex_unlock(&(m[info.region]));	
			} else {
				fprintf(stderr, "Region shoul be between 0 and 9\n");
			}
		} else if(info.type == CLOSED) {
			pthread_mutex_lock(&c);
			remove_connection(client_head, client_fd);
			pthread_mutex_unlock(&c);
			close(client_fd);
			break;
		} else if(info.type == WAIT) {
			if(info.region >= 0 && info.region < 10) {
				wait_connection(client_head, client_fd, info.region);
			} else {
				fprintf(stderr, "Region shoul be between 0 and 9\n");
			}
		}
	}

	return NULL;
}

/*
 * Function: server_client_socket
 *   Creates a socket for connetions between applications and clipboard 
 *   and returns a file descriptor
 *
 *   locals :  input values to create a local unix socket
 *
 *   returns: file descriptor for the socket
 */
int server_client_socket(struct sockaddr_un *locals){

	int sock_fd;

	if ((sock_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror("server socket");
		return -1;
	}

	memset(locals,0,sizeof(*locals));
	unlink(SOCK_ADDR);

	(*locals).sun_family = AF_UNIX;
	strcpy((*locals).sun_path, SOCK_ADDR);

	if(bind(sock_fd,(struct sockaddr *)locals,sizeof(*locals)) == -1) {
		perror("server bind");
		return -1;
	}

	if(listen(sock_fd, 5) == -1) {
		perror("server listen");
		return -1;
	}

	return sock_fd;
}

/*
 * Function: server_son_socket
 *   Creates a socket for connetions between clipboards 
 *   and returns a file descriptor
 *
 *   remotes :  input values to create a tcp socket
 *   port : tcp port for the connection
 *
 *   returns: file descriptor for the socket
 */
int server_son_socket(struct sockaddr_in *remotes, int port){

	int son_fd;

	memset((void*)remotes,(int)'\0',sizeof(*remotes));
	(*remotes).sin_family=AF_INET;
	(*remotes).sin_addr.s_addr=htonl(INADDR_ANY);
	(*remotes).sin_port=htons(port);

	if((son_fd=socket(AF_INET,SOCK_STREAM,0))==-1) {
		perror("tcp server socket");
		return -1;
	}
	if(bind(son_fd, (struct sockaddr *)remotes,sizeof(*remotes)) == -1) {
	    perror("tcp server bind");
		return -1;
	}
	if(listen(son_fd,5)==-1) {
		perror("tcp server listen");
		return -1;
	}

	return son_fd;
}

/*
 * Function: create_clipboard
 *   Allocates memory for the clipboard
 *
 *   returns: structureof type clipboard pointing to the clipboard
 */
struct clip *create_clipboard(){
	struct clip *clipboard;
	int i;
	clipboard = malloc(sizeof(struct clip)*10);
	for(i = 0; i < 10; i++) {
		clipboard[i].size = -1;
		clipboard[i].message = malloc(sizeof(char)*128);
		memset(clipboard[i].message, 0x00, sizeof(char)*128);
	}

	return clipboard;
}

/*
 * Function: accept_connections
 *   Allocates List for the sons and for the the applications.
 *   Connects with father if there is one.
 *   Loop that manages all the available file descriptors through select function
 *
 *   arguments: pointer with all the information needed to start the connections
 *
 */
void *accept_connections(void *arguments){
	
	struct accept_connections_args *acargs = (struct accept_connections_args *) arguments;
	int sock_fd = acargs->sock_fd;
	int son_fd = acargs->son_fd;
	struct sockaddr_un locals = acargs->locals;
	struct sockaddr_in remotes = acargs->remotes;
	struct clip *clipboard = acargs->clipboard;
	
	int new_son_fd, new_client_fd, counter, t_client_flag = 0, t_clip_flag = 0, i;
	socklen_t size_locals, size_remotes;
	char tbuffer[128];
	memset(tbuffer, 0x00, sizeof(char)*128);
	fd_set fds;

	size_remotes = sizeof(remotes);
	size_locals = sizeof(locals);
	struct client_thread_args cargs;
	struct son_thread_args sargs;
	struct father_thread_args fargs;

	struct connection_thread *t_clip_head = NULL, *t_clip_aux;
	t_clip_head = malloc(sizeof(connection_thread));
	t_clip_head->next = NULL;

	struct connection_thread *t_client_head = NULL, *t_client_aux;
	t_client_head = malloc(sizeof(connection_thread));
	t_client_head->next = NULL;

	struct connection *son_head = NULL, *son_aux;
	son_head = malloc(sizeof(struct connection));
	if (son_head == NULL)
	    return NULL;
	son_head->fd = -1;
	son_head->next = NULL;
	son_aux = son_head;

	struct connection *client_head = NULL, *client_aux;
	client_head = malloc(sizeof(struct connection));
	if (client_head == NULL)
	    return NULL;
	client_head->fd = -1;
	for(i=0; i<10; i++)
		client_head->nwait[i] = 0;

	client_head->next = NULL;
	client_aux = client_head;

	if(father_fd != -1) {
		fargs.son_head = son_head;
		fargs.clipboard = clipboard;
		fargs.client_head = client_head;
		t_clip_head->thread_id = -1;
		t_clip_aux = t_clip_head;
		t_clip_aux->next = NULL;
		t_clip_flag = 1;
		pthread_create(&(t_clip_aux->thread_id), NULL, father_session, &fargs);
	}

	printf("Enter exit to leave.\n");
	while(1){

		FD_ZERO(&fds);
		FD_SET(0, &fds);
		FD_SET(sock_fd, &fds);
		FD_SET(son_fd, &fds);

		counter = select(max(sock_fd, son_fd)+1, &fds, (fd_set *)NULL, (fd_set *)NULL, (struct timeval *)	NULL);
		if(counter <= 0) {
			fprintf(stderr, "Error counting fd's\n");
			break;
		}

		if(FD_ISSET(0, &fds)) {
			if(read(0, tbuffer , 128) < 0) {
				write(2, "An error occurred in the read.\n", 31);		
			}
			if(strstr(tbuffer, "exit") != NULL) {
				if(father_fd != -1) {
					send_father_to_sons(son_head, father_port, father_ip);
					pthread_mutex_lock(&f);
					send_message(father_fd, NULL, -1, CLOSED, 0);
					pthread_mutex_unlock(&f);
				} else {
					send_father_to_sons(son_head, -1, father_ip);
				}
				pthread_mutex_lock(&c);
				send_close_clients(client_head);
				pthread_mutex_unlock(&c);

				if(t_clip_flag == 1) {
					t_clip_aux = t_clip_head;
					while(t_clip_aux->next != NULL){
						pthread_join(t_clip_aux->thread_id, NULL);
						t_clip_aux = t_clip_aux->next;
					}
					pthread_join(t_clip_aux->thread_id, NULL);
				}

				if(t_client_flag == 1) {
					t_client_aux = t_client_head;
					while(t_client_aux->next != NULL){
						pthread_join(t_client_aux->thread_id, NULL);
						t_client_aux = t_client_aux->next;
					}
					pthread_join(t_client_aux->thread_id, NULL);
				}
				break;
			}
		}
		if(FD_ISSET(son_fd, &fds)) {
		
			pthread_mutex_lock(&s);
			if((new_son_fd =accept(son_fd,(struct sockaddr *)&remotes,&size_remotes)) ==-1) {
				perror("tcp session");
				exit(-1);
			}
			fprintf(stderr, "New Son Connection\n");

			if(son_head->fd == -1) {
				son_head->fd = new_son_fd;
				son_head->next = NULL;
				son_aux = son_head;
			} else {
				son_aux = son_head;
				while(son_aux->next != NULL) {
					son_aux = son_aux->next;
				}
				son_aux->next = malloc(sizeof(struct connection));
				son_aux = son_aux->next;
				// son_aux->nwait = NULL;
				son_aux->fd = new_son_fd;
				son_aux->next = NULL;
			}

			sargs.son_fd = new_son_fd;
			sargs.son_head = son_head;
			sargs.client_head = client_head;
			sargs.clipboard = clipboard;

			if(t_clip_flag == 0) {
				t_clip_head->thread_id = -1;
				t_clip_aux = t_clip_head;
				t_clip_aux->next = NULL;
				t_clip_flag = 1;
			} else {
				t_clip_aux->next = malloc(sizeof(connection_thread));
				t_clip_aux = t_clip_aux->next;
				t_clip_aux->thread_id = -1;
				t_clip_aux->next = NULL;
			}

			pthread_create(&(t_clip_aux->thread_id), NULL, son_session, &sargs);
		}
		if(FD_ISSET(sock_fd, &fds)) {
			pthread_mutex_lock(&c);
			if((new_client_fd = accept(sock_fd, (struct sockaddr *)&locals, &size_locals)) == -1) {
				perror("accept");
				exit(1);
			}
			fprintf(stderr, "New Client Connection\n");

			memset(&cargs, 0x00, sizeof(struct client_thread_args));

			if(client_head->fd == -1) {
				client_head->fd = new_client_fd;
				client_head->next = NULL;
				client_aux = client_head;
			} else {
				client_aux = client_head;
				while(client_aux->next != NULL) {
					client_aux = client_aux->next;
				}
				client_aux->next = malloc(sizeof(struct connection));
				client_aux = client_aux->next;
				client_aux->fd = new_client_fd;
				client_aux->next = NULL;
				// client_aux->nwait = malloc(sizeof(int)*10);
				for(i=0; i<10; i++)
					client_aux->nwait[i] = 0;
				// client_aux->next = NULL;
			}

			cargs.clipboard = clipboard;
			cargs.client_fd = new_client_fd;
			cargs.son_head = son_head;
			cargs.client_head = client_head;
			// cargs.client_this = client_aux;
			

			if(t_client_flag == 0) {
				t_client_head->thread_id = -1;
				t_client_head->next = NULL;
				t_client_aux = t_client_head;
				t_client_flag = 1;
			} else {
				t_client_aux->next = malloc(sizeof(connection_thread));;
				t_client_aux = t_client_aux->next;
				t_client_aux->thread_id = -1;
				t_client_aux->next = NULL;
			}
			pthread_create(&(t_client_aux->thread_id), NULL, client_session, &cargs);
		}
	}

	free_list_con(son_head);

	free_list_con(client_head);

	free_list_thread(t_client_head);

	free_list_thread(t_clip_head);



	return NULL;
}

/*
 * Function: main
 *   Receives the tcp port where he's gonna receive connections and the information needed to connect to his father
 *   Creates the clipboard
 *   Connects with father
 *   Creates all Sockets
 *   
 *
 *   returns: the square of the larger of n1 and n2 
 */
int main(int argc, char *argv[]){

	int sock_fd, son_fd, lport, checkl = 0, i;
	struct clip *clipboard;
	struct sockaddr_un locals;
	struct sockaddr_in remotes;
	struct accept_connections_args args;
	strcpy(father_ip, "");

	clipboard = create_clipboard();

	for(i = 0; i < 10; i++) {
		pthread_mutex_init(&m[i], NULL);
	}
	pthread_mutex_init(&c, NULL);
	pthread_mutex_init(&s, NULL);
	pthread_mutex_init(&f, NULL);
	pthread_mutex_init(&p, NULL);


	if(argc > 1){
		for(i = 1; i < argc; i++) {
			if(strcmp(argv[i],"-c") == 0) {	
				strcpy(father_ip, argv[++i]);
				father_port = atoi(argv[++i]);							
				father_fd = father_connect(father_ip, father_port);
			} else if(strcmp(argv[i],"-l") == 0) {
				lport = atoi(argv[++i]);
				checkl = 1;
			} else {
				fprintf(stderr, "Invalid parameter : %s\n", argv[i]);
				return 0;
			}
		}
	}
	else {
		fprintf(stderr, "Usage :\nConnect to a clipboard:\n\t./clipboard -c <IP from clipboard to connect> <Port> -l <Port to receive connections>\nStart alone:\n\t./clipboard -l <Port to receive connections>\n");
		return 0;
	}
	if(checkl == 0) {
		fprintf(stderr, "Usage :\nConnect to a clipboard:\n\t./clipboard -c <IP from clipboard to connect> <Port> -l <Port to receive connections>\nStart alone:\n\t./clipboard -l <Port to receive connections>\n");
		return 0;
	}

	son_fd = server_son_socket(&remotes, lport);

	sock_fd = server_client_socket(&locals);

	args.clipboard = clipboard;
	args.sock_fd = sock_fd;
	args.son_fd = son_fd;
	args.locals = locals;
	args.remotes = remotes;

	if(son_fd != -1 && sock_fd != -1) 
		accept_connections((void *)&args);
	
	free_clipboard(clipboard);

	for(i = 0; i < 10; i++) {
		pthread_mutex_destroy(&m[i]);
	}
	pthread_mutex_destroy(&c);
	pthread_mutex_destroy(&s);
	pthread_mutex_destroy(&f);
	pthread_mutex_destroy(&p);

	return 1;
}