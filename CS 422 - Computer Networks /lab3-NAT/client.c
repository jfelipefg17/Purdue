// Client side C/C++ program to demonstrate Socket
// programming
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
// #define PORT 12000


int main(int argc, char const* argv[])
{
	int sock = 0, valread, client_fd;
	struct sockaddr_in serv_addr;
	int port = 12000;
	char* ip = "127.0.0.1";

	// TODO-1: replace the port value and ip value above with the input ones from command-line argument
	//          - Read port number and ip address from command-line arguments.


	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) { /* SOCKET_STREAM for TCP, SOCKET_DGRAM for UDP*/ /* Protocol: 0 to let system choose*/
		printf("\n Socket creation error \n");
		return -1;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);

	// Convert IPv4 and IPv6 addresses from text to binary
	// form
	if (inet_pton(AF_INET, ip, &serv_addr.sin_addr)
		<= 0) {
		printf(
			"\nInvalid address/ Address not supported \n");
		return -1;
	}


	if ((client_fd
		= connect(sock, (struct sockaddr*)&serv_addr,
				sizeof(serv_addr)))
		< 0) {
		printf("\nConnection Failed \n");
		return -1;
	}


	printf("Input lowercase sentence (press ctrl D to signal the end of input):\n");

	// TODO-2: Read the input messages from the user.
	//          - Each message line is separated by a newline character ('\n').
	//          - The end of input is indicated by pressing Ctrl+D (EOF).
	//          - Store all lines together into the 'sentence' buffer.
	char sentence[256];
	
	send(sock, sentence, strlen(sentence), 0);
	shutdown(sock,SHUT_WR);

	// TODO-3: Receive the modified message from the server and print it on the terminal.
	printf("Modified sentence received from server:\n");


	// closing the connected socket
	close(sock);

	return 0;
}
