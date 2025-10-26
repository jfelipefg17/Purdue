// Server side C/C++ program to demonstrate Socket
// programming
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
// #define PORT 12000
int main(int argc, char const* argv[])
{
	int server_fd, new_socket, valread;
	struct sockaddr_in address;
	int opt = 1;
	int addrlen = sizeof(address);
	int port = 12000;

	// TODO-1: replace the port value above with the input number from command-line argument
	//          - Read port number from command-line arguments.
	//          - Validate that the port number is within 1024–65535.
	//          - If not valid, print an error message and ask for new input.
	//	    - Repeat this process until you get a valid port number.
	

	// Creating socket file descriptor
	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket failed");
		exit(EXIT_FAILURE);
	}

	// Forcefully attaching socket to the port 
	if (setsockopt(server_fd, SOL_SOCKET,
				SO_REUSEADDR, &opt,
				sizeof(opt))) {
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port);

	if (bind(server_fd, (struct sockaddr*)&address,
			sizeof(address))
		< 0) {
		perror("bind failed");
		exit(EXIT_FAILURE);
	}
	if (listen(server_fd, 3) < 0) {
		perror("listen");
		exit(EXIT_FAILURE);
	}
	printf("The server is ready to receive\n");


	while (1) {
		if ((new_socket
			= accept(server_fd, (struct sockaddr*)&address,
			(socklen_t*)&addrlen)) < 0){
			perror("accept");
			exit(EXIT_FAILURE);
		}
		char sentence[1024] = { 0 };
		valread = read(new_socket, sentence, 1024);
		printf("Received sentence:\n");
		printf("%s\n", sentence);

		// TODO-2: Convert the received sentence to uppercase, and print the modified result to the terminal


		printf("Modified sentence:\n");


		// TODO-3: send the modified sentence back to the client
		

		// closing the connected socket
		close(new_socket);
	}
	// closing the listening socket
	shutdown(server_fd, SHUT_RDWR);
	return 0;
}