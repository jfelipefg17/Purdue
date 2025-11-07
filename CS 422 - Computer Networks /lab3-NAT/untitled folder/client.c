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

int main(int argc, char const* argv[]) {
	
    int sock = 0, valread;
    struct sockaddr_in serv_addr;
    int port = 12000;
    char* ip = "127.0.0.1";

    // TODO-1: Read IP and port from command-line (argv[1] = IP, argv[2] = port)
    if (argc > 2) {

        ip = (char*)argv[1];
        port = atoi(argv[2]);
    } else {

        printf("Usage: %s [server_ip] [server_port]\n", argv[0]);
        return -1;
    }

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {

        printf("\n Socket creation error \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {

        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {

        printf("\nConnection Failed \n");
        return -1;
    }

    printf("Input lowercase sentence (press ctrl D to signal the end of input):\n");

    // TODO-2: Read multi-line input from stdin until EOF (Ctrl+D), store in sentence
    char sentence[1024] = {0};
    char line[256];
    size_t len = 0;
    while (fgets(line, sizeof(line), stdin)) {

        size_t line_len = strlen(line);
        if (len + line_len >= sizeof(sentence)) {

            printf("Input too long!\n");
            break;
        }
        strcpy(sentence + len, line);
        len += line_len;
    }

    send(sock, sentence, strlen(sentence), 0);
    shutdown(sock, SHUT_WR);

    // TODO-3: Receive and print modified message
    printf("Modified sentence received from server:\n");
    char buffer[1024] = {0};
    while ((valread = read(sock, buffer, 1024)) > 0) {
		
        printf("%s", buffer);
        memset(buffer, 0, 1024);
    }

    close(sock);
    return 0;
}