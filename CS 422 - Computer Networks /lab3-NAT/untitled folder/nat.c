#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <sys/file.h>

#define BACKLOG 64
#define BUFSZ   8192
#define HDRMAX  256

// Small utils
static int send_all(int fd, const char *buf, size_t len) {

    size_t off = 0;
    while (off < len) {

        ssize_t n = send(fd, buf + off, len - off, 0);
        if (n < 0) { if (errno == EINTR) continue; return -1; }
        off += (size_t)n;
    }
    return 0;
}

// TODO-3: Check if NAT port exists in table
static int nat_port_in_table(FILE *fp, unsigned short nat_port) {

    char line[256];
    rewind(fp);  // Go to start of file
    while (fgets(line, sizeof(line), fp)) {

        char cip[INET_ADDRSTRLEN];
        unsigned short cp, np;
        if (sscanf(line, "%s %hu %hu", cip, &cp, &np) == 3) {

            if (np == nat_port) return 1;
        }
    }
    return 0;  // Not found
}

// TODO-4: Append mapping to table
static int append_mapping_locked(FILE *fp, const char *client_ip, unsigned short client_port, unsigned short nat_port) {

    fseek(fp, 0, SEEK_END);  // Go to end
    if (fprintf(fp, "%s %hu %hu\n", client_ip, client_port, nat_port) < 0) {
        
        return -1;
    }
    fflush(fp);
    return 0;
}

// Bind to random port [40000, 60000)
static int bind_random_port(int sockfd, int cfd, int family, unsigned short *chosen_port_out) {
    // TODO-2: Get client IP/port from cfd
    char client_ip[INET_ADDRSTRLEN] = {0};
    unsigned short client_port = 0;
    struct sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);
    if (getpeername(cfd, (struct sockaddr*)&client_addr, &len) < 0) {

        perror("getpeername");
        return -1;
    }
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    client_port = ntohs(client_addr.sin_port);

    FILE *fp = fopen("nat_table.txt", "a+");
    if (!fp) {

        perror("fopen");
        return -1;
    }
    if (flock(fileno(fp), LOCK_EX) != 0) {

        perror("flock");
        fclose(fp);
        return -1;
    }

    unsigned short p = 40000;
    for (; p < 60000; ++p) {

        if (nat_port_in_table(fp, p)) continue;  // Skip if in table

        int bound_ok = -1;
        if (family == AF_INET) {

            struct sockaddr_in local4 = {0};
            local4.sin_family = AF_INET;
            local4.sin_addr.s_addr = htonl(INADDR_ANY);
            local4.sin_port = htons(p);
            if (bind(sockfd, (struct sockaddr*)&local4, sizeof(local4)) == 0) {

                bound_ok = 0;
            }
        } else {

            errno = EAFNOSUPPORT;
            bound_ok = -1;
        }

        if (bound_ok == 0) {

            if (append_mapping_locked(fp, client_ip, client_port, p) != 0) {

                flock(fileno(fp), LOCK_UN);
                fclose(fp);
                return -1;
            }
            if (chosen_port_out) *chosen_port_out = p;
            flock(fileno(fp), LOCK_UN);
            fclose(fp);
            return 0;
        }
    }
    flock(fileno(fp), LOCK_UN);
    fclose(fp);
    errno = EAGAIN;
    return -1;
}

// Connect to target with PAT
static int connect_target_pat(int cfd, const char *host, const char *port, unsigned short *used_port) {

    struct addrinfo hints, *res = NULL, *rp = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int ret = getaddrinfo(host, port, &hints, &res);
    if (ret != 0) {

        fprintf(stderr, "getaddrinfo(%s,%s): %s\n", host, port, gai_strerror(ret));
        return -1;
    }

    int sfd = -1;
    for (rp = res; rp; rp = rp->ai_next) {

        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd < 0) continue;

        unsigned short local_port = 0;
        if (bind_random_port(sfd, cfd, rp->ai_family, &local_port) != 0) {

            close(sfd); sfd = -1; continue;
        }

        if (connect(sfd, rp->ai_addr, rp->ai_addrlen) == 0) {
            
            if (used_port) *used_port = local_port;
            break;
        }

        close(sfd); sfd = -1;
    }
    freeaddrinfo(res);
    return sfd;
}

// Handle client
static void handle_one_client(int cfd) {

    char buf[BUFSZ];
    ssize_t n = recv(cfd, buf, sizeof(buf) - 1, 0);
    if (n <= 0) {

        close(cfd);
        return;
    }
    buf[n] = '\0';

    // TODO-1: Parse first line for host/port, rest in rem
    char host[128] = {0}, port[16] = {0};
    char rem[BUFSZ] = {0};
    char *first_nl = strchr(buf, '\n');
    if (first_nl) {

        *first_nl = '\0';
        sscanf(buf, "%s %s", host, port);
        strcpy(rem, first_nl + 1);
    } else {
        
        close(cfd);
        return;
    }

    printf("[NAT] Parsed destination -> host: %s, port: %s\n", host, port);

    unsigned short nat_port = 0;
    int sfd = connect_target_pat(cfd, host, port, &nat_port);
    if (sfd < 0) {

        const char *msg = "ERR connect failed\n";
        send(cfd, msg, strlen(msg), 0);
        close(cfd);
        return;
    }

    fprintf(stderr, "[NAT child %d] mapped -> local NAT port %u\n", getpid(), nat_port);

    size_t rem_len = strlen(rem);
    if (rem_len > 0) { if (send_all(sfd, rem, rem_len) != 0) { close(sfd); close(cfd); return; } }
    for (;;) {
        char buf[BUFSZ];
        ssize_t n = recv(cfd, buf, sizeof(buf), 0);
        if (n == 0) break;
        if (n < 0) { if (errno == EINTR) continue; close(sfd); close(cfd); return; }
        if (send_all(sfd, buf, (size_t)n) != 0) { close(sfd); close(cfd); return; }
    }
    shutdown(sfd, SHUT_WR);

    for (;;) {

        char buf[BUFSZ];
        ssize_t n = recv(sfd, buf, sizeof(buf), 0);
        if (n == 0) break;
        if (n < 0) { if (errno == EINTR) continue; break; }
        if (send_all(cfd, buf, (size_t)n) != 0) break;
    }

    close(sfd);
    close(cfd);
}

int main(int argc, char const *argv[]) {

    int port = 7000;
    if (argc > 1) {
        errno = 0; char *end = NULL; long p = strtol(argv[1], &end, 10);
        if (errno == 0 && end && *end == '\0' && p > 0 && p <= 65535) port = (int)p;
        else fprintf(stderr, "Invalid port; using default %d\n", port);
    }

    srand((unsigned)time(NULL) ^ (unsigned)getpid());

    struct addrinfo hints, *res = NULL, *rp = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM; hints.ai_flags = AI_PASSIVE;
    char port_str[16]; snprintf(port_str, sizeof(port_str), "%d", port);

    if (getaddrinfo(NULL, port_str, &hints, &res) != 0) { perror("getaddrinfo"); exit(EXIT_FAILURE); }
    int server_fd = -1;
    for (rp = res; rp; rp = rp->ai_next) {
        server_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (server_fd < 0) continue;
        int one = 1; setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
        if (bind(server_fd, rp->ai_addr, rp->ai_addrlen) == 0 && listen(server_fd, BACKLOG) == 0) break;
        close(server_fd); server_fd = -1;
    }
    freeaddrinfo(res);
    if (server_fd < 0) { 
        
        perror("listen"); exit(EXIT_FAILURE); 
    }

    signal(SIGCHLD, SIG_IGN);

    printf("[NAT] listening on %d\n", port);

    for (;;) {

        int cfd = accept(server_fd, NULL, NULL);
        if (cfd < 0) { if (errno == EINTR) continue; perror("accept"); continue; }

        pid_t pid = fork();
        if (pid == 0) {

            close(server_fd);
            handle_one_client(cfd);
            _exit(0);
        } else if (pid > 0) {

            close(cfd);
        } else {
            
            perror("fork");
            close(cfd);
        }
    }
}