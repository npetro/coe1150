/*
 * Server.c
 * Kevin Gilboy and Nick Petro
 *
 * This program is a client that sends a request to the server to encode or 
 * decode any string input by the user at runtime. The runtime args are:
 * -e for encode or -d for decode followed by a string in quotes. For ex
 * ./client -e "hello world" would encode the string "hello world" and
 * ./client -d "ifmmp xpsme" would decode the string "ifmmp xpsme"
 *
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>

#define PORT 1150
#define URL "127.0.0.1"

#define RESPONSE_PEEK_SIZE 800
#define REQUEST_HEADER_SIZE 128

/*
 * Constants needed for content and content length extraction
 */
#define TAG_CONTENT_LENGTH_START "Content-Length: "
#define TAG_CONTENT_LENGTH_END "Connection: "
#define TAG_LAST_HEADER_LINE "Connection: keep-alive\r\n\r\n"

int main(int argc, char *argv[]){
	/*
	 * Check input args for tag and msg
	 */
	if(argc!=3){
		printf("Invalid. Usage examples: \n",argv[0]);
		printf("\tEncode: %s -d \"hello world\"\n",argv[0]);
		printf("\tDecode: %s -e \"ifmmp xpsme\"\n",argv[0]);
		exit(-1);
	} 
	
	char msg[strlen(argv[2])+3];
	if(strcmp(argv[1],"-d")==0){
		//Decode
		strcpy(msg,"d:");
	}
	else if(strcmp(argv[1],"-e")==0){
		//Encode
		strcpy(msg,"e:");
	}
	else{
		printf("Sorry, \"%s\" is not a supported tag",argv[1]);
		exit(-1);
	}
	strcat(msg,argv[2]);

	sendToServer(msg);

	return 0;
}

int sendToServer(char *msg){
	int recv_bytes = 0;
	int content_length = 0;
	int header_length = 0;
	char peek_msg[RESPONSE_PEEK_SIZE];
	char get_command[strlen(msg)+REQUEST_HEADER_SIZE];
	struct sockaddr_in server_addr;
	char *curr_ptr;
	
	/*
	 * Open a socket and connect to the addr
	 */
	int client_socket = socket(AF_INET,
		SOCK_STREAM,0);
	if(client_socket<0) {
		printf("Socket error\n");
		return -1;
	}

	memset(&server_addr, '0', sizeof(server_addr));

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT);

	if(inet_pton(AF_INET, URL, &server_addr.sin_addr) <= 0) {
		fprintf(stderr,"Invalid addr\n");
		return -1;
	}

	// Connect to server
	int connection_status = connect(client_socket,
		(struct sockaddr *)&server_addr, sizeof(server_addr));
	if(connection_status<0){
		fprintf(stderr,"Connection failed\n");
		return -1;
	}
	
	/*
	 * Build a GET request and send it
	 */
	sprintf(get_command,"POST / HTTP/1.1\r\n"
		"Host: "URL"\r\n"
		"Content-Length: %d\r\n"
		"Connection: keep-alive\r\n\r\n"
		"%s\r\n",
		strlen(msg),msg);
	send(client_socket,get_command,strlen(get_command),0);
	
	/*
	 * Peek at response to get content length and size of header
	 */
	curr_ptr = peek_msg;
	do{
		recv_bytes = recv(client_socket, curr_ptr,
			sizeof(peek_msg)-1,MSG_PEEK);
		if(recv_bytes<=0){
			fprintf(stderr,"No message received from host\n");
			return -1;
		}
		curr_ptr+=recv_bytes*sizeof(char);
	} while((curr_ptr-peek_msg)/sizeof(char)<RESPONSE_PEEK_SIZE-1);
	peek_msg[recv_bytes] = 0;

	/*
	 * Get content length
	 */
	char *content_length_ptr = strstr(peek_msg,TAG_CONTENT_LENGTH_START)+
		strlen(TAG_CONTENT_LENGTH_START);
	
	char *content_length_end = strstr(peek_msg,TAG_CONTENT_LENGTH_END) -
		(2*sizeof(char)); //Rm 2 chars since the previous line ends in \r\n
	
	while(content_length_ptr<content_length_end){
		content_length = (content_length*10) + (content_length_ptr[0]-'0');
		content_length_ptr+=sizeof(char);
	}

	/*
	 * Get size of header
	 */
	char *response_start = strstr(peek_msg,TAG_LAST_HEADER_LINE)+
		strlen(TAG_LAST_HEADER_LINE);
	header_length = (response_start-peek_msg) / sizeof(char);

	/*
	 * Recv header and discard it
	 */
	char *header = malloc(sizeof(char) * (header_length+1));
	curr_ptr = header;
	do{
		recv_bytes = recv(client_socket, curr_ptr, header_length,0);
		if(recv_bytes<=0){
			printf("No message received from host\n");
			return -1;
		}
		curr_ptr+=recv_bytes*sizeof(char);
	} while((curr_ptr-header)/sizeof(char)<header_length-1);
	header[recv_bytes] = 0;
	
	/*
	 * Recv content
	 */
	char *content = malloc(sizeof(char) * (content_length+1));
	curr_ptr = content;
	// We must keep recv'ing if the recv gives less bytes than we ask for
	do{
		recv_bytes = recv(client_socket, curr_ptr, content_length,0);
		if(recv_bytes<=0){
			printf("No message received from host\n");
			return -1;
		}
		curr_ptr+=recv_bytes*sizeof(char);
	} while((curr_ptr-content)/sizeof(char)<content_length-1);
	content[content_length] = 0;

	fprintf(stderr,"%s\n\n",content);

	close(client_socket);

	free(header);
	free(content);

	return 0;
}
