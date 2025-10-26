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

// ---- Small utils ----
static int send_all(int fd, const char *buf, size_t len) {
    size_t off = 0;
    while (off < len) {
        ssize_t n = send(fd, buf + off, len - off, 0);
        if (n < 0) { if (errno == EINTR) continue; return -1; }
        off += (size_t)n;
    }
    return 0;
}

// TODO-3: Check if a NAT port already exists in the mapping table file.
// Format of each line: "<client_ip> <client_port> <nat_port>\n"
// Parameters:
//   fp        : pointer to the opened mapping table file
//   nat_port  : NAT port to check
//
// Returns: 1 if nat_port is already present, 0 if free, -1 on error.
//
static int nat_port_in_table(FILE *fp, unsigned short nat_port)
{

}

// TODO-4: Append one NAT mapping entry to the end of an open mapping table file.
// Format of each line: "<client_ip> <client_port> <nat_port>\n"
//
// Parameters:
//   fp           : pointer to the opened mapping table file (already LOCK_EX locked)
//   client_ip    : string containing the client's IP address
//   client_port  : client's source port
//   nat_port     : NAT-assigned public port
//
// Returns: 0 on success, -1 on failure

static int append_mapping_locked(FILE *fp,
                                 const char *client_ip,
                                 unsigned short client_port,
                                 unsigned short nat_port)
{

}





// Bind a socket to a RANDOM local port in [40000,60000).
// Works fv4 or IPv6 based on `family`.
// Returns 0 on success; sets *chosen_port_out. -1 on failure.
static int bind_random_port(int sockfd, int cfd, int family, unsigned short *chosen_port_out) {
    
    // TODO-2: Get client's IP/port from the accepted socket 'cfd'
    char client_ip[INET_ADDRSTRLEN] = {0};
    unsigned short client_port = 0;

    // 2) Open/lock the mapping table file (create if absent)
    FILE *fp = fopen("nat_table.txt", "a+");  // read/write, create if not exist
    if (!fp) {
        return -1;
    }
    // Lock the file for the entire selection+append critical section
    if (flock(fileno(fp), LOCK_EX) != 0) {
        fclose(fp);
        return -1;
    }

    unsigned short p = 40000;

    for (; p <= 65535; ++p) {
	// TODO-3: Write function nat_port_in_table() to check whether port number p has been already assigned, more details see line 31
        if (nat_port_in_table(fp, p)) continue;

        int bound_ok = -1;
        if (family == AF_INET) {
            struct sockaddr_in local4;
            memset(&local4, 0, sizeof(local4));
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
            // TODO-4: Write function append_mapping_locked(), to append a new entry to the table, more details see line 44
            if (append_mapping_locked(fp, client_ip, client_port, p) != 0) {
                // If write fails, return error.
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
    	// No port found
    	flock(fileno(fp), LOCK_UN);
    	fclose(fp);
    	errno = EAGAIN;
    	return -1;
}
    


// Resolve target and connect using a socket that we first bind to a RANDOM port.
// Returns connected socket fd, or -1 on failure. Fills *used_port with the bound port.
static int connect_target_pat(int cfd, const char *host, const char *port, unsigned short *used_port) {
    struct addrinfo hints, *res = NULL, *rp = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;   // allow v4 or v6
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
            break; // success
        }

        close(sfd); sfd = -1;
    }
    freeaddrinfo(res);
    return sfd;
}

// ---- Child handler ----
static void handle_one_client(int cfd) {
    // 1) Parse "<ip> <port>\n"
    char buf[BUFSZ];
    ssize_t n = recv(cfd, buf, sizeof(buf), 0);
   

    // TODO-1: extract the server's IP address and port number from the first line.
    //         - The remaining message (after the first '\n') should be saved in the variable 'rem'.
    char host[128] = {0}, port[16] = {0};
    char rem[BUFSZ]; 


    printf("[NAT] Parsed destination -> host: %s, port: %s\n", host, port);

    // 2) Connect to real server using an outgoing socket bound to a RANDOM port (PAT)
    unsigned short nat_port = 0;
    int sfd = connect_target_pat(cfd, host, port, &nat_port);
    if (sfd < 0) {
        const char *msg = "ERR connect failed\n";
        send(cfd, msg, strlen(msg), 0);
        close(cfd);
        return;
    }


    fprintf(stderr, "[NAT child %d] mapped -> local NAT port %u\n", getpid(), nat_port);

    // 3) Forward the actual message to real server (unchanged)
    size_t rem_len = strlen(rem);
    if (rem_len > 0) { if (send_all(sfd, rem, rem_len) != 0) { close(sfd); close(cfd); return; } }
    for (;;) {
        char buf[BUFSZ];
        ssize_t n = recv(cfd, buf, sizeof(buf), 0);
        if (n == 0) break; // client finished sending
        if (n < 0) { if (errno == EINTR) continue; close(sfd); close(cfd); return; }
        if (send_all(sfd, buf, (size_t)n) != 0) { close(sfd); close(cfd); return; }
    }
    shutdown(sfd, SHUT_WR); // end-of-message to server

    // 4) Relay server's response back to client (unchanged)
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

    // Seed randomness for translated port selection (unique per process)
    srand((unsigned)time(NULL) ^ (unsigned)getpid());

    // Listener (IPv4/IPv6)
    int server_fd = -1;
    struct addrinfo hints, *res = NULL, *rp = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM; hints.ai_flags = AI_PASSIVE;
    char port_str[16]; snprintf(port_str, sizeof(port_str), "%d", port);

    if (getaddrinfo(NULL, port_str, &hints, &res) != 0) { perror("getaddrinfo"); exit(EXIT_FAILURE); }
    for (rp = res; rp; rp = rp->ai_next) {
        server_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (server_fd < 0) continue;
        int one = 1; setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
        if (bind(server_fd, rp->ai_addr, rp->ai_addrlen) == 0 && listen(server_fd, BACKLOG) == 0) break;
        close(server_fd); server_fd = -1;
    }
    freeaddrinfo(res);
    if (server_fd < 0) { perror("listen"); exit(EXIT_FAILURE); }

    // Avoid zombies from children
    signal(SIGCHLD, SIG_IGN);

    printf("[NAT] listening on %d\n", port);

    for (;;) {
        int cfd = accept(server_fd, NULL, NULL);
        if (cfd < 0) { if (errno == EINTR) continue; perror("accept"); continue; }

        pid_t pid = fork();
        if (pid == 0) {
            // Child
            close(server_fd);
            handle_one_client(cfd);
            _exit(0);
        } else if (pid > 0) {
            // Parent
            close(cfd);
        } else {
            perror("fork");
            close(cfd);
        }
    }
}