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

int main(int argc, char const* argv[]) {

    int server_fd, new_socket, valread;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    int port = 12000;  // Default port

    // TODO-1: Read port from command-line (argv[1]), validate 1024-65535, prompt if invalid
    if (argc > 1) {

        port = atoi(argv[1]);
    }
    while (port < 1024 || port > 65535) {

        printf("Invalid port. Enter port (1024-65535): ");
        if (scanf("%d", &port) != 1) {

            printf("Error reading port.\n");
            exit(EXIT_FAILURE);
        }
    }

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {

        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Set socket options
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {

        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // Bind and listen
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {

        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 3) < 0) {

        perror("listen");
        exit(EXIT_FAILURE);
    }
    printf("The server is ready to receive\n");

    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {

            perror("accept");
            exit(EXIT_FAILURE);
        }
        char sentence[1024] = { 0 };
        valread = read(new_socket, sentence, 1024);
        printf("Received sentence:\n");
        printf("%s\n", sentence);

        // TODO-2: Convert to uppercase and print
        for (int i = 0; sentence[i]; i++) {
			
            sentence[i] = toupper(sentence[i]);
        }
        printf("Modified sentence:\n");
        printf("%s\n", sentence);

        // TODO-3: Send modified sentence back
        send(new_socket, sentence, strlen(sentence), 0);

        close(new_socket);
    }
    shutdown(server_fd, SHUT_RDWR);
    return 0;
}